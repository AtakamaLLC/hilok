#include <string_view>
#include <string>
#include <assert.h>

class PathSplit {
  
    std::string_view m_vw;
    char m_sep;
    int next;


public:


    PathSplit(std::string_view path, char sep='/') : m_vw(path), m_sep(sep) {
        while (!m_vw.empty() && m_vw[0] == m_sep) {
            m_vw.remove_prefix(1);
        }
        next = m_vw.find(m_sep);
    }

    std::string operator *() {
        if (next != std::string_view::npos) {
            return std::string(m_vw.substr(0, next));
        } else {
            return std::string(m_vw);
        }
    }

    PathSplit & operator ++() {
        if (next != std::string_view::npos) {
            m_vw.remove_prefix(next + 1);
            while (!m_vw.empty() && m_vw[0] == m_sep) {
                m_vw.remove_prefix(1);
            }
            next = m_vw.find(m_sep);
        } else {
            m_vw.remove_prefix(m_vw.size());
        }
        return *this;
    }

    bool operator != (const void *ptr) {
        // we only support comparison with null
        assert(!ptr);
        return !m_vw.empty(); 
    }

    const void *end() {
        return nullptr;
    }

};
