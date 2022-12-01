#include <catch2/catch_test_macros.hpp>

#include <hilok.hpp>
#include <psplit.hpp>

#include <thread>

#include <iostream>

TEST_CASE( "path-split-basic", "[basic]" ) {
    std::vector<std::string> res;
    auto expect = std::vector<std::string>({"a", "b", "c"});
    for (auto it=PathSplit("/a/b/c"); it != it.end(); ++it) {
        res.push_back(*it);
    }
    REQUIRE(res == expect);
}

TEST_CASE( "path-split-trims", "[basic]" ) {
    std::vector<std::string> res;
    auto expect = std::vector<std::string>({"a", "b", "c"});
    for (auto it=PathSplit("//a//b//c//"); it != it.end(); ++it) {
        res.push_back(*it);
    }
    REQUIRE(res == expect);
}


TEST_CASE( "path-split-empty", "[basic]" ) {
    std::vector<std::string> res;
    auto expect = std::vector<std::string>({});
    for (auto it=PathSplit("::::", ':'); it != it.end(); ++it) {
        res.push_back(*it);
    }
    REQUIRE(res == expect);
}

TEST_CASE( "path-split-one", "[basic]" ) {
    std::vector<std::string> res;
    auto expect = std::vector<std::string>({"x"});
    for (auto it=PathSplit("x"); it != it.end(); ++it) {
        res.push_back(*it);
    }
    REQUIRE(res == expect);
}

TEST_CASE( "ex-lock-unlock", "[basic]" ) {
    HiLok h;
    std::cout << "here 1" << std::endl;
    auto l1 = h.write("a");
    l1.release();
    
    auto l2 = h.write("a", false);
    l2.release();
}

TEST_CASE( "sh-lock-unlock", "[basic]" ) {
    HiLok h;
    std::cout << "here 1" << std::endl;
    auto l1 = h.read("a");
    l1.release();
    
    auto l2 = h.read("a");
    l2.release();
}

TEST_CASE( "rd-in-wr", "[basic]" ) {
    HiLok h;
    std::cout << "here 1" << std::endl;
    auto l1 = h.write("a/b/c");
    std::cout << "here 2" << std::endl;
    REQUIRE_THROWS(h.write("a", false));
    REQUIRE_THROWS(h.write("a/b", false));
    std::cout << "here 3" << std::endl;

    INFO("read lock while write");
    auto l2 = h.read("a/b", false);
    
    INFO("write lock on sibling");
    auto l3 = h.write("a/b/d", false);
    l1.release();
    
    INFO("write lock root after partial release");
    REQUIRE_THROWS(h.write("a", false));
    
    l3.release();
    INFO("write lock root after partial release");
    REQUIRE_THROWS(h.write("a", false));
    
    l2.release();

    INFO("write lock root after all 3 released");
    h.write("a", false);
}

TEST_CASE( "wr-after-rel", "[basic]" ) {
    HiLok h;
    auto l1 = h.write("a/b/c");
    l1.release();

    INFO("write lock root after full release");
    auto l4 = h.write("a", false);
    

    INFO("read child fails after write root");
    REQUIRE_THROWS(h.read("a/b", false));
    l4.release();

    INFO("read child ok now");
    h.read("a/b", false);
}


void worker(int i, HiLok &h, int &ctr) {
    auto l1 = h.write("a/b/c/d/e");
    INFO("got write lock" << i);
    ctr++;
    l1.release();
}

TEST_CASE( "many-threads", "[basic]" ) {
    HiLok h;
    int ctr = 0;
    int pool_size = 100;
    std::vector<std::thread> threads;
    for(unsigned int i = 0; i < pool_size; ++i)
    {
        threads.emplace_back(std::thread([&] () { worker(i, h, ctr); } ));
    }

    for (auto& thread : threads) {
        thread.join();
    }
    REQUIRE(ctr == pool_size);
}

