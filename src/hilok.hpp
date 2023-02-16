#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <map>
#include <vector>
#include <thread>
#include <atomic>
#include <cassert>
#include <shared_mutex>

#include "recsh.hpp"
#include "hierr.hpp"

#define mut_op(op) (is_recursive() ? m_r_mut.op() : m_t_mut.op())
#define mut_op_1(op, a) (is_recursive() ? m_r_mut.op(a) : m_t_mut.op(a))

enum HiFlags { 
     STRICT = 0,                // no recursion, strict release
     RECURSIVE_WRITE = 1,       // allow recursive write/write locks only
     RECURSIVE_ONEWAY = 2,      // allow recursive write then read, but not vice versa
     RECURSIVE = 3,             // allow all recursion
     RECURSIVE_MODE_MASK = 7,   // mask that covers recursive modes
     LOOSE_READ_UNLOCK = 8,     // allow unlocks for read handles to come from other threads
     LOOSE_WRITE_UNLOCK = 16,   // allow unlocks for write handles to come from other threads
};

#define RECURSIVE_MODE(f) (f & RECURSIVE_MODE_MASK)

class HiMutex {
private: 
    recursive_shared_mutex m_r_mut;
    std::shared_timed_mutex m_t_mut;
public:
    std::thread::id m_ex_id;
    std::atomic<int> m_num_r;
    int m_rec_flags;
    bool m_is_ex;

    HiMutex(int rec_flags) : m_r_mut(RECURSIVE_MODE(rec_flags) == RECURSIVE_WRITE, RECURSIVE_MODE(rec_flags) == RECURSIVE_ONEWAY), m_num_r(0), m_rec_flags(rec_flags), m_is_ex(false) {
    }
    HiMutex(bool) = delete;

    bool is_locked() {
        return (m_num_r > 0) || m_is_ex;
    }

    bool is_recursive() {
        return RECURSIVE_MODE(m_rec_flags) != 0;
    }


    bool unsafe_clone_lock_shared(HiMutex &src, bool block, double secs) {
        auto num = (m_num_r + src.m_num_r + (src.m_is_ex ? 1 : 0));
        while (m_num_r < num) {
            if (!lock_shared(block, secs)) {
                return false;
            }
        }
        return true;
    }

    void unsafe_clone_unlock_shared(HiMutex &src) {
        auto num = (src.m_num_r + (src.m_is_ex ? 1 : 0));
        while (num > 0) {
            unlock_shared(true);
            --num;
        }
    }

    bool internal_lock(bool block, double secs) {
        bool ret;
        if (!block) {
            ret = mut_op(try_lock);
        } else if (secs != 0.0) {
            ret = mut_op_1(try_lock_for, std::chrono::duration<double>(secs));
        } else {
            mut_op(lock);
            ret = true;
        }
        return ret;
    }
 
    bool lock(bool block, double secs) {
        bool ret = internal_lock(block, secs);
        if (ret)
            m_is_ex = true;
        return ret;
    }
 
    void lock() {
        mut_op(lock);
        m_is_ex = true;
    }

    bool try_lock() {
        if (mut_op(try_lock)) {
            m_is_ex = true;
            return true;
        }
        return false;
    }

    bool try_solo_lock() {
        if (is_recursive() ? m_r_mut.try_solo_lock() : m_t_mut.try_lock()) {
            m_is_ex = true;
            return true;
        }
        return false;
    }


    bool try_lock_for(double secs) {
        if (mut_op_1(try_lock_for, std::chrono::duration<double>(secs))) {
            m_is_ex = true;
            return true;
        }
        return false;
    }

    void unlock() {
        m_is_ex = false;
        mut_op(unlock);
    }

    void unlock(std::thread::id tid) {
        assert(is_recursive());
        m_r_mut.unlock(tid);
        --m_num_r;
    }
   
    void lock_shared() {
        mut_op(lock_shared);
        ++m_num_r;
    }

    bool lock_shared(bool block, double secs) {
        bool ret;
        if (!block) {
            ret = mut_op(try_lock_shared);
        } else if (secs != 0.0) {
            ret = mut_op_1(try_lock_shared_for, std::chrono::duration<double>(secs));
        } else {
            mut_op(lock_shared);
            ret = true;
        }
        if (ret)
            ++m_num_r;
        return ret;
    }
 
    bool try_lock_shared() {
        auto ret = mut_op(try_lock_shared);
        if (ret)
            ++m_num_r;
        return ret;
    }

    bool try_lock_shared_for(double secs) {
        auto ret = mut_op_1(try_lock_shared_for, std::chrono::duration<double>(secs));
        if (ret)
            ++m_num_r;
        return ret;
    }

    void unlock_shared(bool any_thread = 0) {
        if(is_recursive() && any_thread) {
            m_r_mut.unlock_any_shared();
        } else {
            mut_op(unlock_shared);
        }
        --m_num_r;
    }

    void unlock_shared(std::thread::id tid) {
        assert(is_recursive());
        m_r_mut.unlock_shared(tid);
        --m_num_r;
    }
};

class HiKeyNode {
public:
    std::pair<std::shared_ptr<HiKeyNode>, std::string> m_key;
    HiMutex m_mut;
    std::atomic<int> m_inref;
    HiKeyNode(std::pair<std::shared_ptr<HiKeyNode>, std::string> key, int flags) : m_key(key), m_mut(flags), m_inref(0) {
    }
    HiKeyNode(std::pair<std::shared_ptr<HiKeyNode>, std::string>, bool) = delete;
};

class HiLok;

class HiHandle {
    bool m_shared;
    std::shared_ptr<HiKeyNode> m_ref;
    std::shared_ptr<HiLok> m_mgr;
    bool m_released;
    std::thread::id m_src_thread;

public:
    HiHandle(std::shared_ptr<HiLok> mgr, bool shared, std::shared_ptr<HiKeyNode> ref) :
        m_shared(shared), m_ref(ref), m_mgr(mgr), m_released(false), m_src_thread(std::this_thread::get_id()) {
    }

    HiHandle ( HiHandle && ) = default;
    HiHandle &  operator= ( HiHandle && ) = default;
    HiHandle ( const HiHandle & ) = delete;
    HiHandle & operator= ( const HiHandle & ) = delete;

    virtual ~HiHandle() {
        try {
            release();
        } catch (HiErr &) {
        }
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
    int m_flags;
    std::shared_ptr<HiKeyNode> _get_node(const std::pair<std::shared_ptr<HiKeyNode>, std::string> &key);

public:

    HiLok(char sep = '/', int flags=HiFlags::RECURSIVE) : m_sep(sep), m_flags(flags) {
    }
    
    HiLok(char, bool) = delete;
    
    virtual ~HiLok() {
    }

    bool is_recursive() {return m_flags & (HiFlags::RECURSIVE_MODE_MASK);}

    std::shared_ptr<HiKeyNode> find_node(std::string_view path_from);

    std::shared_ptr<HiHandle> read(std::shared_ptr<HiLok> mgr, std::string_view path, bool block = true, double timeout = 0);
    
    std::shared_ptr<HiHandle> write(std::shared_ptr<HiLok> mgr, std::string_view path, bool block = true, double timeout = 0);

    void rename(std::string_view from, std::string_view to, bool block = true, double timeout = 0);

    void erase_safe(std::shared_ptr<HiKeyNode> &ref);
    void erase_unsafe(std::shared_ptr<HiKeyNode> &ref);

    size_t size() const { return m_map.size(); };
};
