#include "recsh.hpp"
#include <mutex>
#include <iostream>

bool recursive_shared_mutex::try_lock_for(const std::chrono::duration<double> &secs)
{
    std::unique_lock<std::mutex> sync_lock(m_mtx);
    if (!m_cond_var.wait_for(sync_lock, secs, [this] { return can_exclusively_lock(); })) {
        return false;
    }
    if (is_exclusive_locked_on_this_thread())
    {
        increment_exclusive_lock();
    }
    else
    {
        start_exclusive_lock();
    }
    return true;
}

void recursive_shared_mutex::lock()
{
    std::unique_lock<std::mutex> sync_lock(m_mtx);
    m_cond_var.wait(sync_lock, [this] { return can_exclusively_lock(); });
    if (is_exclusive_locked_on_this_thread())
    {
        increment_exclusive_lock();
    }
    else
    {
        start_exclusive_lock();
    }
}

bool recursive_shared_mutex::try_lock()
{
    std::unique_lock<std::mutex> sync_lock(m_mtx);
    if (can_increment_exclusive_lock())
    {
        increment_exclusive_lock();
        return true;
    }
    if (can_start_exclusive_lock())
    {
        start_exclusive_lock();
        return true;
    }
    return false;
}

bool recursive_shared_mutex::try_solo_lock()
{
    std::unique_lock<std::mutex> sync_lock(m_mtx);
    if (can_start_solo_lock())
    {
        start_solo_lock();
        return true;
    }
    return false;
}

void recursive_shared_mutex::unlock()
{
    unlock(std::this_thread::get_id());
}

void recursive_shared_mutex::unlock(std::thread::id tid)
{
    {
        std::unique_lock<std::mutex> sync_lock(m_mtx);
        decrement_exclusive_lock(tid);
        m_solo_locked = false;
    }
    m_cond_var.notify_all();
}


void recursive_shared_mutex::lock_shared()
{
    std::unique_lock<std::mutex> sync_lock(m_mtx);
    m_cond_var.wait(sync_lock, [this] { return can_lock_shared(); });
    increment_shared_lock();
}

bool recursive_shared_mutex::try_lock_shared_for(const std::chrono::duration<double> &secs)
{
    std::unique_lock<std::mutex> sync_lock(m_mtx);
    if(!m_cond_var.wait_for(sync_lock, secs, [this] { return can_lock_shared(); })) {
        return false;
    }
    increment_shared_lock();
    return true;
}


bool recursive_shared_mutex::try_lock_shared()
{
    std::unique_lock<std::mutex> sync_lock(m_mtx);
    if (can_lock_shared())
    {
        increment_shared_lock();
        return true;
    }
    return false;
}

void recursive_shared_mutex::unlock_shared()
{
    unlock_shared(std::this_thread::get_id());
}

void recursive_shared_mutex::unlock_shared(std::thread::id id)
{
    {
        std::unique_lock<std::mutex> sync_lock(m_mtx);
        decrement_shared_lock(id);
    }
    m_cond_var.notify_all();
}

void recursive_shared_mutex::unlock_any_shared()
{
    {
        std::unique_lock<std::mutex> sync_lock(m_mtx);
        decrement_any_shared_lock(std::this_thread::get_id());
    }
    m_cond_var.notify_all();
}
