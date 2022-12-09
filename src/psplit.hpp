#pragma once
#include <string_view>
#include <string>

class PathSplit {
  
    std::string_view m_vw;
    char m_sep;
    size_t m_next;


public:

    void trim_seps() {
        while (!m_vw.empty() && m_vw[0] == m_sep) {
            m_vw.remove_prefix(1);
        }
    }

    void seek_next() {
        trim_seps();
        m_next = m_vw.find(m_sep);
    }



    PathSplit(std::string_view path, char sep='/') : m_vw(path), m_sep(sep) {
        seek_next();
    }

    std::string operator *() {
        if (m_next != std::string_view::npos) {
            return std::string(m_vw.substr(0, m_next));
        } else {
            return std::string(m_vw);
        }
    }

    PathSplit & operator ++() {
        if (m_next != std::string_view::npos) {
            m_vw.remove_prefix(m_next + 1);
            seek_next();
        } else {
            m_vw.remove_prefix(m_vw.size());
        }
        return *this;
    }

    bool operator != (const void *) {
        // we only support comparison with null
        return !m_vw.empty(); 
    }

    const void *end() {
        return nullptr;
    }

};
