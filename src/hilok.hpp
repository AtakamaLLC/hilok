#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <shared_mutex>
#include <unordered_map>
#include <map>
#include <vector>
#include <thread>
#include <atomic>

class HiErr : public std::runtime_error {
    using std::runtime_error::runtime_error;
};


class HiMutex {
    
public:
    std::thread::id m_ex_id;
    int m_ex_cnt;
    std::map<std::thread::id, int> m_shared;
    std::shared_timed_mutex m_mut;
    std::mutex m_internal_mut;
    bool m_recursive;

    HiMutex(bool recursive) : m_ex_cnt(0), m_recursive(recursive) {
    }

    bool _has_lock() {
        return m_ex_cnt > 0 && m_ex_id == std::this_thread::get_id();
    }

    bool _do_lock() {
        if (!m_recursive)
            return false;
        std::lock_guard<std::mutex> guard(m_internal_mut);
        if (_has_lock()) {
            ++m_ex_cnt;
            return true;
        }
        return false;
    }

    bool internal_shared_lock(bool block, double secs) {
        bool ret;
        if (!block) {
            ret = m_mut.try_lock_shared();
        } else if (secs != 0.0) {
            ret = m_mut.try_lock_shared_for(std::chrono::duration<double>(secs));
        } else {
            m_mut.lock_shared();
            ret = true;
        }
        return ret;
    }
 
    bool unsafe_clone_lock_shared(HiMutex &src, bool block, double secs) {
        // lock number of times matching src
        for (const auto &[key, cnt] : src.m_shared) {
            for (int i = 0; i < cnt; ++i) {
                if (!internal_shared_lock(block, secs)) {
                    return false;
                }
                m_shared[key] += 1;
            }
        }
        for (int i = 0; i < src.m_ex_cnt; ++i) {
            if (!internal_shared_lock(block, secs)) {
                return false;
            }
            m_shared[src.m_ex_id] += 1;
        }
        return true;
    }

    void unsafe_clone_unlock_shared(HiMutex &src) {
        // unlock number of times matching src
        for (const auto &[key, cnt] : src.m_shared) {
            for (int i = 0; i < cnt; ++i) {
                m_mut.unlock_shared();
                m_shared[key] -= 1;
                if (!m_shared[key]) {
                    m_shared.erase(key);
                }
            }
        }
        for (int i = 0; i < src.m_ex_cnt; ++i) {
            m_mut.unlock_shared();
            m_shared[src.m_ex_id] -= 1;
            if (!m_shared[src.m_ex_id]) {
                m_shared.erase(src.m_ex_id);
            }
        }
    }

    void lock() {
        if (_do_lock()) {
            return;
        }
        m_mut.lock();
        _start_lock();
    }

    void _start_lock() {
        std::lock_guard<std::mutex> guard(m_internal_mut);
        m_ex_id = std::this_thread::get_id();
        m_ex_cnt = 1;
    }

    bool try_lock() {
        if (_do_lock()) {
            return true;
        }
        if (m_mut.try_lock()) {
            _start_lock();
            return true;
        }
        return false;
    }

    bool try_lock_for(double secs) {
        if (_do_lock()) {
            return true;
        }
        if (m_mut.try_lock_for(std::chrono::duration<double>(secs))) {
            _start_lock();
            return true;
        }
        return false;
    }

    void unlock() {
        std::lock_guard<std::mutex> guard(m_internal_mut);
        if (!_has_lock()) throw HiErr("invalid unlock");
        if (m_recursive) {
            --m_ex_cnt;
            if (m_ex_cnt == 0) {
                m_mut.unlock();
            }
        } else {
            m_mut.unlock();
        }
    }
    
    bool is_locked() {
        return m_ex_cnt > 0 || m_shared.size();
    }


    bool _has_lock_shared() {
        auto it = m_shared.find(std::this_thread::get_id());
        return it != m_shared.end() && it->second > 0;
    }

    void lock_shared() {
        m_mut.lock_shared();
        _add_lock_shared();
    }

    void _add_lock_shared() {
        std::lock_guard<std::mutex> guard(m_internal_mut);
        m_shared[std::this_thread::get_id()] += 1;
    }

    bool try_lock_shared() {
        if (m_mut.try_lock_shared()) {
            _add_lock_shared();
            return true;
        }
        return false;
    }

    bool try_lock_shared_for(double secs) {
        if (m_mut.try_lock_shared_for(std::chrono::duration<double>(secs))) {
            _add_lock_shared();
            return true;
        }
        return false;
    }

    void unlock_shared() {
        std::lock_guard<std::mutex> guard(m_internal_mut);

        if (!_has_lock_shared()) throw HiErr("invalid shared unlock");

        --m_shared[std::this_thread::get_id()];

        if (!m_shared[std::this_thread::get_id()]) {
            m_shared.erase(std::this_thread::get_id());
        }

        m_mut.unlock_shared();
    }
};

class HiKeyNode {
public:
    std::pair<std::shared_ptr<HiKeyNode>, std::string> m_key;
    HiMutex m_mut;
    HiKeyNode(std::pair<std::shared_ptr<HiKeyNode>, std::string> key, bool recursive) : m_key(key), m_mut(recursive) {
    }
};

class HiLok;

class HiHandle {
    bool m_shared;
    std::shared_ptr<HiKeyNode> m_ref;
    std::shared_ptr<HiLok> m_mgr;
    bool m_released;

public:
    HiHandle(std::shared_ptr<HiLok> mgr, bool shared, std::shared_ptr<HiKeyNode> ref) :
        m_shared(shared), m_ref(ref), m_mgr(mgr), m_released(false) {
    }
    HiHandle ( HiHandle && ) = default;
    HiHandle &  operator= ( HiHandle && ) = default;
    HiHandle ( const HiHandle & ) = delete;
    HiHandle & operator= ( const HiHandle & ) = delete;

    virtual ~HiHandle() {
        release();
    }

    void release();
};

struct pair_hash
{
    template <class T1, class T2>
    std::size_t operator() (const std::pair<T1, T2> &pair) const {
        return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
    }
};


class HiLok {
public:
    std::unordered_map<std::pair<std::shared_ptr<HiKeyNode>, std::string>, std::shared_ptr<HiKeyNode>, pair_hash> m_map;
    std::mutex m_mutex;
    char m_sep;
    bool m_recursive;
    std::shared_ptr<HiKeyNode> _get_node(const std::pair<std::shared_ptr<HiKeyNode>, std::string> &key);


public:

    HiLok(char sep = '/', bool recursive=true) : m_sep(sep), m_recursive(recursive) {
    }
    
    virtual ~HiLok() {
    }

    std::shared_ptr<HiKeyNode> find_node(std::string_view path_from);

    std::shared_ptr<HiHandle> read(std::shared_ptr<HiLok> mgr, std::string_view path, bool block = true, double timeout = 0);
    
    std::shared_ptr<HiHandle> write(std::shared_ptr<HiLok> mgr, std::string_view path, bool block = true, double timeout = 0);

    void rename(std::string_view from, std::string_view to, bool block = true, double timeout = 0);

    void erase_safe(std::shared_ptr<HiKeyNode> &ref);
    void erase_unsafe(std::shared_ptr<HiKeyNode> &ref);

    size_t size() const { return m_map.size(); };
};
