#include <string>
#include <string_view>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

class HiErr : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class HiHandle {
    std::vector<std::shared_timed_mutex *> m_refs;
    bool m_shared;

public:
    HiHandle(bool shared, std::vector<std::shared_timed_mutex *> refs) : m_shared(shared), m_refs(refs) {
    }

    virtual ~HiHandle() {
    }

    void release() {
        for (auto it = m_refs.begin(); it!= m_refs.end(); ) {
            auto mut = *it;
            ++it;
            if (m_shared || it != m_refs.end()) {
                mut->unlock_shared();
            } else {
                mut->unlock();
            }
        }
    }
};

struct pair_hash
{
    template <class T1, class T2>
    std::size_t operator() (const std::pair<T1, T2> &pair) const {
        return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
    }
};


class HiLok {
    std::unordered_map<std::pair<void *, std::string>, std::shared_timed_mutex, pair_hash> m_map;
    std::mutex m_mutex;
    char m_sep;
public:

    HiLok(char sep = '/') : m_sep (sep) {
    }
    
    virtual ~HiLok() {
    }

    HiHandle read(std::string_view path, bool block = true, int timeout = 0);
    
    HiHandle write(std::string_view path, bool block = true, int timeout = 0);
};
