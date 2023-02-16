#ifndef _RECURSIVE_SHARED_MUTEX_H
#define _RECURSIVE_SHARED_MUTEX_H

// from https://stackoverflow.com/a/60046372/627042

#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include "hierr.hpp"


struct recursive_shared_mutex
{
public:

    recursive_shared_mutex(bool rec_write_only = false, bool rec_one_way = false) :
        m_mtx{}, m_exclusive_thread_id{}, m_exclusive_count{ 0 }, m_shared_locks{}, m_solo_locked{0}, m_wr_only(rec_write_only), m_one_way(rec_one_way)
    {}


    void lock();
    bool try_lock();
    bool try_solo_lock();
    bool try_lock_for( const std::chrono::duration<double>& secs);
    void unlock();
    void unlock(std::thread::id id);

    void lock_shared();
    bool try_lock_shared();
    bool try_lock_shared_for(const std::chrono::duration<double>& secs);
    void unlock_shared();
    void unlock_any_shared();
    void unlock_shared(std::thread::id id);

    recursive_shared_mutex(const recursive_shared_mutex&) = delete;
    recursive_shared_mutex& operator=(const recursive_shared_mutex&) = delete;

private:

    inline bool is_exclusive_locked()
    {
        return m_exclusive_count > 0;
    }

    inline bool is_shared_locked()
    {
        return m_shared_locks.size() > 0;
    }

    inline bool can_exclusively_lock()
    {
        return can_start_exclusive_lock() || can_increment_exclusive_lock();
    }

    inline bool can_start_exclusive_lock()
    {
        return !is_exclusive_locked() && (!is_shared_locked() || (!m_wr_only && !m_one_way && is_shared_locked_only_on_this_thread()));
    }

    inline bool can_start_solo_lock()
    {
        return !is_exclusive_locked() && !is_shared_locked();
    }

    inline bool can_increment_exclusive_lock()
    {
        return is_exclusive_locked_on_this_thread() && !m_solo_locked && (!m_one_way || !is_shared_locked());
    }

    inline bool can_lock_shared()
    {
        return !is_exclusive_locked() || (!m_wr_only && is_exclusive_locked_on_this_thread());
    }

    inline bool is_shared_locked_only_on_this_thread()
    {
        return is_shared_locked_only_on_thread(std::this_thread::get_id());
    }

    inline bool is_shared_locked_only_on_thread(std::thread::id id)
    {
        return m_shared_locks.size() == 1 && m_shared_locks.find(id) != m_shared_locks.end();
    }

    inline bool is_exclusive_locked_on_this_thread()
    {
        return is_exclusive_locked_on_thread(std::this_thread::get_id());
    }

    inline bool is_exclusive_locked_on_thread(std::thread::id id)
    {
        return m_exclusive_count > 0 && m_exclusive_thread_id == id;
    }

    inline void start_exclusive_lock()
    {
        m_exclusive_thread_id = std::this_thread::get_id();
        m_exclusive_count++;
    }

    inline void start_solo_lock()
    {
        start_exclusive_lock();
        m_solo_locked = true;
    }

    inline void increment_exclusive_lock()
    {
        m_exclusive_count++;
    }

    inline void decrement_exclusive_lock()
    {
        decrement_exclusive_lock(std::this_thread::get_id());
    }

    inline void decrement_exclusive_lock(std::thread::id tid)
    {
        if (m_exclusive_count == 0)
        {
            throw HiErr("Not exclusively locked, cannot exclusively unlock");
        }
        if (m_exclusive_thread_id == tid)
        {
            m_exclusive_count--;
        }
        else
        {
            throw HiErr("Calling exclusively unlock from the wrong thread");
        }
    }


    inline void increment_shared_lock()
    {
        increment_shared_lock(std::this_thread::get_id());
    }

    inline void increment_shared_lock(std::thread::id id)
    {
        if (m_shared_locks.find(id) == m_shared_locks.end())
        {
            m_shared_locks[id] = 1;
        }
        else
        {
            m_shared_locks[id] += 1;
        }
    }

    inline void decrement_shared_lock()
    {
        decrement_shared_lock(std::this_thread::get_id());
    }

    inline void decrement_any_shared_lock(std::thread::id id)
    {
        if (m_shared_locks.size() == 0)
        {
            throw HiErr("Not shared locked, cannot shared unlock");
        }
        auto pos = m_shared_locks.find(id);
        if (pos == m_shared_locks.end())
            pos = m_shared_locks.begin();
        if (pos->second == 1)
        {
            m_shared_locks.erase(pos);
        }
        else
        {
            pos->second -= 1;
        }
    }

    inline void decrement_shared_lock(std::thread::id id)
    {
        if (m_shared_locks.size() == 0)
        {
            throw HiErr("Not shared locked, cannot shared unlock");
        }
        if (m_shared_locks.find(id) == m_shared_locks.end())
        {
            throw HiErr("Calling shared unlock from the wrong thread");
        }
        else
        {
            if (m_shared_locks[id] == 1)
            {
                m_shared_locks.erase(id);
            }
            else
            {
                m_shared_locks[id] -= 1;
            }
        }
    }

    std::mutex m_mtx;
    std::thread::id m_exclusive_thread_id;
    size_t m_exclusive_count;
    std::map<std::thread::id, size_t> m_shared_locks;
    std::condition_variable m_cond_var;
    bool m_solo_locked;
    bool m_wr_only;
    bool m_one_way;
};

#endif
