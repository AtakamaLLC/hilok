#include "hilok.hpp"
#include "psplit.hpp"

bool lock_with_params(HiMutex &mut, bool block, int timeout) {
    if (!block) {
        return mut.try_lock();
    } else if (timeout) {
        return mut.try_lock_for(timeout);
    } else {
        mut.lock();
        return true;
    }
}

bool shared_lock_with_params(HiMutex &mut, bool block, int timeout) {
    if (!block) {
        return mut.try_lock_shared();
    } else if (timeout) {
        return mut.try_lock_shared_for(timeout);
    } else {
        mut.lock_shared();
        return true;
    }
}

void HiHandle::release() {
    for (auto it = m_refs.begin(); it!= m_refs.end(); ) {
        auto &kref = *it;
        ++it;
        if (m_shared || it != m_refs.end()) {
            kref.m_mut->unlock_shared();
        } else {
            kref.m_mut->unlock();
        }

        m_mgr.erase_safe(kref);
    }
}

HiHandle HiLok::read(std::string_view path, bool block, int timeout) {
    void *cur = nullptr;
    std::pair<void *, std::string> key;
    std::vector<HiKeyRef> refs;
    try {
        for (auto it = PathSplit(path, m_sep); it != it.end(); ++it) {
            key = {cur, *it};
            std::shared_ptr<HiMutex> mut = _get_mutex(key);
            bool ok = shared_lock_with_params(*mut, block, timeout);
            if (!ok) {
                mut.reset();
                throw HiErr("failed to lock");
            }
            refs.emplace_back(mut, key);
            cur = (void *) mut.get();
        }
    } catch (...) {
        auto hh = HiHandle(*this, true, refs);
        refs.clear();
        hh.release();
        throw;
    }
    return HiHandle(*this, true, refs);
}

std::shared_ptr<HiMutex> HiLok::_get_mutex(std::pair<void *, std::string> key) {
    std::lock_guard<std::mutex> guard(m_mutex);
    auto it = m_map.find(key);
    if (it == m_map.end()) {
        return m_map[key] = std::make_shared<HiMutex>(m_recursive);
    } else {
        return it->second;
    }
}


HiHandle HiLok::write(std::string_view path, bool block, int timeout) {
    void *cur = nullptr;
    std::pair<void *, std::string> key;
    std::vector<HiKeyRef>refs;
    try {
        for (auto it = PathSplit(path, m_sep); it != it.end(); ) {
            key = {cur, *it};
            std::shared_ptr<HiMutex> mut = _get_mutex(key);
            
            ++it;
            bool ok;
            if (it != it.end())
                ok = shared_lock_with_params(*mut, block, timeout);
            else
                ok = lock_with_params(*mut, block, timeout);

            if (!ok) {
                mut.reset();
                throw HiErr("failed to lock");
            }

            refs.emplace_back(mut, key);
            cur = (void *) mut.get();
        }
    } catch (...) {
        auto hh = HiHandle(*this, true, refs);
        refs.clear();
        hh.release();
        throw;
    }

    return HiHandle(*this, false, refs);
}


void HiLok::erase_safe(HiKeyRef &ref) {
    std::lock_guard<std::mutex> guard(m_mutex);
    if (ref.m_mut.use_count() <= 3) {
        // no one else is trying a new read or write, except maybe caller
        if (ref.m_mut->try_lock()) {
            // we now have an exclusive lock, so we really know nobody is using it
            if (ref.m_mut.use_count() <= 3) {
                // map + ref 
                auto it = m_map.find(ref.m_key);
                if (it != m_map.end()) {
                    if (it->second.get() == ref.m_mut.get()) {
                        // i will only ever erase my own
                        m_map.erase(it);
                    }
                }
            }
            ref.m_mut->unlock();
        }
    }
}
