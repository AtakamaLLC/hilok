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
        auto kref = *it;
        ++it;
        if (m_shared || it != m_refs.end()) {
            kref.m_mut->unlock_shared();
        } else {
            kref.m_mut->unlock();
        }

//        if (!kref.m_mut->is_locked()) {
//            m_mgr.erase_safe(kref);
//        }
    }
}

HiHandle HiLok::read(std::string_view path, bool block, int timeout) {
    void *cur = nullptr;
    std::pair<void *, std::string> key;
    std::vector<HiKeyRef> refs;
    try {
        for (auto it = PathSplit(path, m_sep); it != it.end(); ++it) {
            key = {cur, *it};
            HiMutex *mut;
            {
                std::lock_guard<std::mutex> guard(m_mutex);
                mut = &m_map[key];
            }
            bool ok = shared_lock_with_params(*mut, block, timeout);
            if (!ok)
                throw HiErr("failed to lock");
            refs.emplace_back(mut, key);
            cur = (void *) mut;
        }
    } catch (...) {
        HiHandle(*this, true, refs).release();
        throw;
    }
    return HiHandle(*this, true, refs);
}

HiHandle HiLok::write(std::string_view path, bool block, int timeout) {
    void *cur = nullptr;
    std::pair<void *, std::string> key;
    std::vector<HiKeyRef>refs;
    try {
        for (auto it = PathSplit(path, m_sep); it != it.end(); ) {
            key = {cur, *it};
            ++it;
            HiMutex *mut;
            {
                std::lock_guard<std::mutex> guard(m_mutex);
                mut = &m_map[key];
            }
            bool ok;

            if (it != it.end())
                ok = shared_lock_with_params(*mut, block, timeout);
            else
                ok = lock_with_params(*mut, block, timeout);

            if (!ok)
                throw HiErr("failed to lock");

            refs.emplace_back(mut, key);
            cur = (void *) mut;
        }
    } catch (...) {
        HiHandle(*this, true, refs).release();
        throw;
    }

    return HiHandle(*this, false, refs);
}


void HiLok::erase_safe(HiKeyRef &ref) {
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (!ref.m_mut->is_locked()) {
            m_map.erase(ref.m_key);
        }
    }
}
