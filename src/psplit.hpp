#include <string_view>
#include <string>

class PathSplit {
  
    std::string_view m_vw;
    int next;


public:


    PathSplit(std::string_view path) {
        m_vw = path;
        while (!m_vw.empty() && m_vw[0] == '/') {
            m_vw.remove_prefix(1);
        }
        next = m_vw.find('/');
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
            while (!m_vw.empty() && m_vw[0] == '/') {
                m_vw.remove_prefix(1);
            }
            next = m_vw.find('/');
        } else {
            m_vw.remove_prefix(m_vw.size());
        }
        return *this;
    }

    bool operator != (const void *ptr) {
        return !m_vw.empty(); 
    }

    const void *end() {
        return nullptr;
    }

};
