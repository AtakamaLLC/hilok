#include <string>
#include <string_view>
#include <shared_mutex>
#include <unordered_map>
#include <map>
#include <vector>
#include <thread>

#include <iostream>

class HiErr : public std::runtime_error {
    using std::runtime_error::runtime_error;
};


class HiMutex {
public:
    std::thread::id m_ex_id;
    int m_ex_cnt;
    std::map<std::thread::id, int> m_shared;
    std::shared_timed_mutex m_mut;

    HiMutex() : m_ex_cnt(0) {
    }

    bool _has_lock() {
        return m_ex_cnt > 0 && m_ex_id == std::this_thread::get_id();
    }

    void lock() {
        if (_has_lock()) {
            ++m_ex_cnt;
            return;
        }
        m_mut.lock();
        _start_lock();
    }

    void _start_lock() {
        m_ex_id = std::this_thread::get_id();
        m_ex_cnt = 1;
    }

    bool try_lock() {
        if (_has_lock()) {
            ++m_ex_cnt;
            return true;
        }
        if (m_mut.try_lock()) {
            _start_lock();
            return true;
        }
        return false;
    }

    bool try_lock_for(int secs) {
        if (_has_lock()) {
            ++m_ex_cnt;
            return true;
        }
        if (m_mut.try_lock_for(std::chrono::seconds(secs))) {
            _start_lock();
            return true;
        }
        return false;
    }

    void unlock() {
        if (!_has_lock()) throw HiErr("invalid unlock");
        --m_ex_cnt;
        m_mut.unlock();
    }
    
    bool is_locked() {
        return m_ex_cnt > 0 || m_shared.size();
    }


    bool _has_lock_shared() {
        return m_shared[std::this_thread::get_id()] > 0;
    }

    void lock_shared() {
        if (_has_lock_shared()) {
            ++m_shared[std::this_thread::get_id()];
            return;
        }
        m_mut.lock_shared();
        _start_lock_shared();
    }

    void _start_lock_shared() {
        m_shared[std::this_thread::get_id()] = 1;
    }

    bool try_lock_shared() {
        if (_has_lock_shared()) {
            ++m_shared[std::this_thread::get_id()];
            return true;
        }
        if (m_mut.try_lock_shared()) {
            _start_lock_shared();
            return true;
        }
        return false;
    }

    bool try_lock_shared_for(int secs) {
        if (_has_lock_shared()) {
            ++m_shared[std::this_thread::get_id()];
            return true;
        }
        if (m_mut.try_lock_shared_for(std::chrono::seconds(secs))) {
            _start_lock_shared();
            return true;
        }
        return false;
    }

    void unlock_shared() {
        if (!_has_lock_shared()) throw HiErr("invalid shared unlock");

        --m_shared[std::this_thread::get_id()];

        if (!m_shared[std::this_thread::get_id()]) {
            m_shared.erase(std::this_thread::get_id());
        }

        m_mut.unlock_shared();
    }
};

class HiKeyRef {
public:
    HiMutex *m_mut;
    std::pair<void *, std::string> m_key;
    HiKeyRef(HiMutex *mut, std::pair<void *, std::string> key) : m_mut(mut), m_key(key) {
    }
};

class HiLok;

class HiHandle {
    std::vector<HiKeyRef> m_refs;
    bool m_shared;
    HiLok &m_mgr;

public:
    HiHandle(HiLok &mgr, bool shared, std::vector<HiKeyRef> refs) : m_shared(shared), m_refs(refs), m_mgr(mgr) {
    }

    virtual ~HiHandle() {
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
    std::unordered_map<std::pair<void *, std::string>, HiMutex, pair_hash> m_map;
    std::mutex m_mutex;
    char m_sep;
public:

    HiLok(char sep = '/') : m_sep (sep) {
    }
    
    virtual ~HiLok() {
    }

    HiHandle read(std::string_view path, bool block = true, int timeout = 0);
    
    HiHandle write(std::string_view path, bool block = true, int timeout = 0);

    void erase_safe(HiKeyRef &ref);
};
