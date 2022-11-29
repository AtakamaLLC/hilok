#include "hilok.hpp"
#include "psplit.hpp"

bool lock_with_params(std::shared_timed_mutex &mut, bool block, int timeout) {
    if (!block) {
        return mut.try_lock();
    } else if (timeout) {
        return mut.try_lock_for(std::chrono::seconds(timeout));
    } else {
        mut.lock();
        return true;
    }
}

bool shared_lock_with_params(std::shared_timed_mutex &mut, bool block, int timeout) {
    if (!block) {
        return mut.try_lock_shared();
    } else if (timeout) {
        return mut.try_lock_shared_for(std::chrono::seconds(timeout));
    } else {
        mut.lock_shared();
        return true;
    }
}

HiHandle HiLok::read(std::string_view path, bool block, int timeout) {
    void *cur = nullptr;
    std::pair<void *, std::string> key;
    std::vector<std::shared_timed_mutex *>refs;
    try {
        for (auto it = PathSplit(path); it != it.end(); ++it) {
            key = {cur, *it};
            std::shared_timed_mutex *mut;
            {
                std::lock_guard<std::mutex> guard(m_mutex);
                mut = &m_map[key];
            }
            bool ok = shared_lock_with_params(*mut, block, timeout);
            if (!ok)
                throw HiErr("failed to lock");
            refs.push_back(mut);
            cur = (void *) mut;
        }
    } catch (...) {
        HiHandle(true, refs).release();
        throw;
    }
    return HiHandle(true, refs);
}

HiHandle HiLok::write(std::string_view path, bool block, int timeout) {
    void *cur = nullptr;
    std::pair<void *, std::string> key;
    std::vector<std::shared_timed_mutex *>refs;
    try {
        for (auto it = PathSplit(path); it != it.end(); ) {
            key = {cur, *it};
            ++it;
            std::shared_timed_mutex *mut;
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
            
            refs.push_back(mut);
            cur = (void *) mut;
        }
    } catch (...) {
        HiHandle(true, refs).release();
        throw;
    }

    return HiHandle(false, refs);
}

