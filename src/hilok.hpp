#include <string>
#include <string_view>

class HiHandle {
public:
    HiHandle() {
    }

    virtual ~HiHandle() {
    }

    void release() {
    }
};



class HiLok {
public:
    HiLok() {
    }
    
    virtual ~HiLok() {
    }

    HiHandle read(std::string_view path, bool block = true, int timeout = 0) {
        return HiHandle();
    }
    
    HiHandle write(std::string_view path, bool block = true, int timeout = 0) {
        return HiHandle();
    }
};
