// #define HILOK_TRACE

#include "hilok.hpp"
#include "psplit.hpp"

#ifdef HILOK_TRACE
#include <iostream>
#endif

#include <algorithm>
#include <cassert>

bool lock_with_params(HiMutex &mut, bool block, double timeout) {
    if (!block) {
        return mut.try_lock();
    } else if (timeout != 0.0) {
        return mut.try_lock_for(timeout);
    } else {
        mut.lock();
        return true;
    }
}

bool shared_lock_with_params(HiMutex &mut, bool block, double timeout) {
    if (!block) {
        return mut.try_lock_shared();
    } else if (timeout != 0.0) {
        return mut.try_lock_shared_for(timeout);
    } else {
        mut.lock_shared();
        return true;
    }
}


void HiHandle::release() {
    if (m_released) return;
    m_released = true;
    auto cur = m_ref;
    std::vector<std::shared_ptr<HiKeyNode>> refs;
    while ( cur ) {
        refs.push_back(cur);
        cur = cur->m_key.first;
    }
    for (auto it = refs.rbegin(); it!= refs.rend(); ) {
        auto &kref = *it;
        ++it;
        if (m_shared || it != refs.rend()) {
#ifdef HILOK_TRACE
            std::cout << "un: " << kref << " " << 0 << " " << m_shared << std::endl;
#endif
            if (m_mgr->m_flags & HiFlags::LOOSE_READ_UNLOCK)
                kref->m_mut.unlock_shared(m_src_thread);
            else 
                kref->m_mut.unlock_shared();
        } else {
#ifdef HILOK_TRACE
            std::cout << "un: " << kref << " " << 1 << " " << m_shared << std::endl;
#endif
            if (m_mgr->m_flags & HiFlags::LOOSE_WRITE_UNLOCK)
                kref->m_mut.unlock(m_src_thread);
            else 
                kref->m_mut.unlock();
        }
        m_mgr->erase_safe(kref);
    }
}

std::shared_ptr<HiHandle> HiLok::read(std::shared_ptr<HiLok> mgr, std::string_view path, bool block, double timeout) {
    std::shared_ptr<HiKeyNode> cur;   // root is an empty ptr
    try {
        std::pair<std::shared_ptr<HiKeyNode>, std::string> key;
        for (auto it = PathSplit(path, m_sep); it != it.end(); ++it) {
            key = {cur, *it};
            std::shared_ptr<HiKeyNode> nod = _get_node(key);
            bool ok = shared_lock_with_params(nod->m_mut, block, timeout);
            nod->m_inref--;
            if (!ok) {
                throw HiErr("failed to lock");
            }
#ifdef HILOK_TRACE
            std::cout << "lk: " << key.first << "/" << key.second << "->" << nod << " " << 0 << std::endl;
#endif
            cur = nod; 
        }
    } catch (...) {
        auto hh = HiHandle(mgr, true, cur);
        cur.reset(); // decrement refcount for erase_safe
        hh.release();
        throw;
    }
    return std::make_shared<HiHandle>(mgr, true, cur);
}

std::shared_ptr<HiKeyNode> HiLok::_get_node(const std::pair<std::shared_ptr<HiKeyNode>, std::string> &key) {
    std::lock_guard<std::mutex> guard(m_mutex);
    auto it = m_map.find(key);
    std::shared_ptr<HiKeyNode> ret;
    if (it == m_map.end()) {
        ret = m_map[key] = std::make_shared<HiKeyNode>(key, m_flags);
    } else {
        ret = it->second;
    }
    ret->m_inref++;
    return ret;
}


std::shared_ptr<HiHandle> HiLok::write(std::shared_ptr<HiLok> mgr, std::string_view path, bool block, double timeout) {
    std::shared_ptr<HiKeyNode> cur;
    try {
        std::pair<std::shared_ptr<HiKeyNode>, std::string> key;
        for (auto it = PathSplit(path, m_sep); it != it.end(); ) {
            key = {cur, *it};
            std::shared_ptr<HiKeyNode> nod = _get_node(key);
            
            ++it;
            bool ok;
            if (it != it.end())
                ok = shared_lock_with_params(nod->m_mut, block, timeout);
            else
                ok = lock_with_params(nod->m_mut, block, timeout);
            nod->m_inref--;

            if (!ok) {
                throw HiErr("failed to lock");
            }

#ifdef HILOK_TRACE
            std::cout << "lk: " << key.first << "/" << key.second << "->" << nod << " " << !(it != it.end()) << std::endl;
#endif
            cur = nod;
        }
    } catch (...) {
        auto hh = HiHandle(mgr, true, cur);
        cur.reset(); // decrement refcount for erase_safe
        hh.release();
        throw;
    }

    return std::make_shared<HiHandle>(mgr, false, cur);
}

std::shared_ptr<HiKeyNode> HiLok::find_node(std::string_view path_from) {
    auto it_from = PathSplit(path_from, m_sep);
    auto leaf_from = it_from;
    std::shared_ptr<HiKeyNode> cur;
    std::pair<std::shared_ptr<HiKeyNode>, std::string> key;
    while (it_from != it_from.end()) {
        leaf_from = it_from;
        ++it_from;

        key = {cur, *leaf_from};

        auto it = m_map.find(key);
        if (it == m_map.end()) {
            return {};
        } else {
            cur = it->second;
        }
    }
    return cur;
}

void HiLok::rename(std::string_view path_from, std::string_view path_to, bool block, double secs) {
    std::lock_guard<std::mutex> guard(m_mutex);
    
    auto leaf_from_node = find_node(path_from);
    if (!leaf_from_node)
        throw HiErr("rename source lock not found");

    std::pair<std::shared_ptr<HiKeyNode>, std::string> to_key;
    
    auto it_from = PathSplit(path_from, m_sep);
    
    bool common = true;
    auto it_to = PathSplit(path_to, m_sep);
    auto leaf_to = it_to;
    std::shared_ptr<HiKeyNode> cur_to;
    std::shared_ptr<HiKeyNode> cur_from;
    std::pair<std::shared_ptr<HiKeyNode>, std::string> from_key;
    while (it_to != it_to.end()) {
        leaf_to = it_to;
        to_key = {cur_to, *leaf_to};

        ++it_to;
        if (common) {
            // common ancestor nodes can be ignored
            from_key = {cur_to, *it_from};
            ++it_from;
            if (to_key == from_key) {
#ifdef HILOK_TRACE
                std::cout << "ig: " << to_key.first << "/" << to_key.second << std::endl;
#endif
                auto it = m_map.find(to_key);
                cur_to = it->second;
                cur_from = it->second;
                continue;
            }
            common = false;
        }

        if (it_to != it_to.end()) {
            // non-leaf destination
            // get or create node, as needed for the destination
            // clone lock counts && thread ids from the leaf

            auto it = m_map.find(to_key);
            if (it == m_map.end()) {
                cur_to = m_map[to_key] = std::make_shared<HiKeyNode>(to_key, m_flags);
            } else {
                cur_to = it->second;
            }

#ifdef HILOK_TRACE
            std::cout << "clon was: " << cur_to->m_mut.m_num_r << std::endl;
#endif

#ifdef HILOK_TRACE
            std::cout << "clon lk: " << to_key.first << "/" << to_key.second << ":" << cur_to << " " << leaf_from_node->m_mut.m_num_r + leaf_from_node->m_mut.m_is_ex << std::endl;
#endif
            // copy lock counts from the leaf to the ancestor
            if (!cur_to->m_mut.unsafe_clone_lock_shared(leaf_from_node->m_mut, block, secs)) {
                throw HiErr("unable to lock rename dest");
            }

#ifdef HILOK_TRACE
            std::cout << "clon new: " << cur_to->m_mut.m_num_r << std::endl;
#endif
        }
    }

    std::vector<std::shared_ptr<HiKeyNode>> to_erase;

    while (it_from != it_from.end()) {
        // uncommon ancestor of source must be released

        auto it = m_map.find(from_key);

        // we already tested for this above, and we have a mutex, should never happen
        assert(it != m_map.end());

        cur_from = it->second;

#ifdef HILOK_TRACE
        std::cout << "clon un: " << from_key.first << "/" << from_key.second << ":" << cur_from << " " << leaf_from_node->m_mut.m_num_r + leaf_from_node->m_mut.m_is_ex << std::endl;
#endif
        // unlock uncommon ancestors of the source
        cur_from->m_mut.unsafe_clone_unlock_shared(leaf_from_node->m_mut);

        to_erase.push_back(cur_from);

        from_key = {cur_from, *it_from};
        
        ++it_from;
    }
   
    // lower refs for erase hint
    from_key.first.reset();
    cur_to.reset();
    cur_from.reset();

    for (auto nod : to_erase) {
        erase_unsafe(nod);
    }

    // keep leaf locks, only change key
    m_map.erase(leaf_from_node->m_key);
    leaf_from_node->m_key = to_key;
    m_map[to_key] = leaf_from_node;        
}


void HiLok::erase_safe(std::shared_ptr<HiKeyNode> &ref) {
    std::lock_guard<std::mutex> guard(m_mutex);
    erase_unsafe(ref);
}

void HiLok::erase_unsafe(std::shared_ptr<HiKeyNode> &ref) {
    // lazy speedup, there can be lots of refs (parent->child ref, caller refs, etc.)
    // but if there are too many, we know the exclusive cannot work and is not worth even trying
    if (ref.use_count() <= 5 && ref->m_inref == 0) {
        // maybe no one else is using it?
        if (ref->m_mut.try_solo_lock() && ref->m_inref == 0) {
            // we now have an exclusive lock, so we really know nobody is using it
            // map + ref 
            try {
                auto it = m_map.find(ref->m_key);
                if (it != m_map.end()) {
                    if (it->second == ref) {
#ifdef HILOK_TRACE
                        std::cout << "erasing " << ref->m_key.second << std::endl;
#endif
                    // i will only ever erase my own
                        m_map.erase(it);
                    }
                }
            } catch (...) {
                        ref->m_mut.unlock();
                        throw;
            }
            ref->m_mut.unlock();
        }
    }
}
