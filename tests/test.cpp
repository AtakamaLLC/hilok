#include <catch2/catch_test_macros.hpp>

#include <hilok.hpp>
#include <psplit.hpp>

#include <thread>
#include <array>

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

TEST_CASE( "path-split-very-empty", "[basic]" ) {
    std::vector<std::string> res;
    auto expect = std::vector<std::string>({});
    for (auto it=PathSplit(""); it != it.end(); ++it) {
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
    auto h = std::make_shared<HiLok>();
    auto l1 = h->write(h, "a");
    l1->release();
    
    auto l2 = h->write(h, "a", false);
    l2->release();
}

TEST_CASE( "sh-lock-unlock", "[basic]" ) {
    auto h = std::make_shared<HiLok>();
    auto l1 = h->read(h, "a");
    l1->release();
    
    auto l2 = h->read(h, "a");
    l2->release();
}

TEST_CASE( "ex-riaa", "[basic]" ) {
    auto h = std::make_shared<HiLok>();
    {
        h->write(h, "a");
    }

    {
        h->write(h, "a");
    }
}

TEST_CASE( "rd-in-wr", "[basic]" ) {
    auto h = std::make_shared<HiLok>('/', false);
    auto l1 = h->write(h, "a/b/c");
    REQUIRE_THROWS(h->write(h, "a", false));
    REQUIRE_THROWS(h->write(h, "a/b", false));

    INFO("read lock while write");
    auto l2 = h->read(h, "a/b", false);
    
    INFO("write lock on sibling");
    auto l3 = h->write(h, "a/b/d", false);
    l1->release();
    
    INFO("write lock root after partial release");
    REQUIRE_THROWS(h->write(h, "a", false));
    
    l3->release();
    INFO("write lock root after partial release");
    REQUIRE_THROWS(h->write(h, "a", false));
    
    l2->release();

    INFO("write lock root after all 3 released");
    h->write(h,"a", false);
}

TEST_CASE( "wr-after-rel", "[basic]" ) {
    auto h = std::make_shared<HiLok>('/', false);
    auto l1 = h->write(h, "a/b/c");
    l1->release();

    INFO("write lock root after full release");
    auto l4 = h->write(h, "a", false);
    
    INFO("read child fails after write root");
    REQUIRE_THROWS(h->read(h, "a/b", false));
    l4->release();

    INFO("read child ok now");
    h->read(h, "a/b", false);
}

TEST_CASE( "rlock-simple", "[basic]" ) {
    HiMutex h(true);
    h.lock();
    CHECK(h.try_lock());
    h.unlock();
    h.unlock();
}

void rlock_worker(int, HiMutex &h, int &ctr) {
    h.lock();
    h.lock();
    ctr++;
    h.unlock();
    h.unlock();
}

TEST_CASE( "rlock-thread", "[basic]" ) {
    HiMutex mut(true);
    int ctr = 0;
    int pool_size = 100;
    std::vector<std::thread> threads;
    for(int i = 0; i < pool_size; ++i)
    {
        threads.emplace_back(std::thread([&mut, &ctr, i] () { rlock_worker(i, mut, ctr); } ));
    }

    for (auto& thread : threads) {
        thread.join();
    }
    CHECK(ctr == pool_size);
    CHECK(mut.is_locked() == false);
}

void shared_lock_worker(int, HiMutex &h) {
    h.lock_shared();
    h.lock_shared();
    h.unlock_shared();
    h.unlock_shared();
}

TEST_CASE( "shared-lock-thread", "[basic]" ) {
    HiMutex mut(true);
    int pool_size = 100;
    std::vector<std::thread> threads;
    for(int i = 0; i < pool_size; ++i)
    {
        threads.emplace_back(std::thread([&mut, i] () { shared_lock_worker(i, mut); } ));
    }

    for (auto& thread : threads) {
        thread.join();
    }
    CHECK(mut.is_locked() == false);
}


void worker(int, std::shared_ptr<HiLok> h, int &ctr) {
    auto l1 = h->write(h, "a/b/c/d/e");
    auto l2 = h->write(h, "a/b/c/d/e");
    ctr++;
    l1->release();
    l2->release();
}

TEST_CASE( "deep-many-threads", "[basic]" ) {
    auto h = std::make_shared<HiLok>();
    int ctr = 0;
    int pool_size = 200;
    std::vector<std::thread> threads;
    for(int i = 0; i < pool_size; ++i)
    {
        threads.emplace_back(std::thread([&h, &ctr, i] () { worker(i, h, ctr); } ));
    }

    for (auto& thread : threads) {
        thread.join();
    }
    CHECK(ctr == pool_size);
    CHECK(h->size() == 0);
}


void nesty_worker(int, std::shared_ptr<HiLok> h, int &ctr) {
    auto l2 = h->read(h, "a/b/c");
    auto l1 = h->write(h, "a/b/c/d/e");
    ctr++;
    l1->release();
    l2->release();
}

TEST_CASE( "deep-nesty-threads", "[basic]" ) {
    auto h = std::make_shared<HiLok>();
    int ctr = 0;
    int pool_size = 200;
    std::vector<std::thread> threads;
    for(int i = 0; i < pool_size; ++i)
    {
        threads.emplace_back(std::thread([&h, &ctr, i] () { nesty_worker(i, h, ctr); } ));
    }

    for (auto& thread : threads) {
        thread.join();
    }
    CHECK(ctr == pool_size);
    CHECK(h->size() == 0);
}


void randy_worker(int i, std::shared_ptr<HiLok> &h, int &ctr) {
    std::array<const char *, 5>paths{"a", "a/b", "a/b/c", "a/b/c/d", "a/b/c/d/e"};
    int depth = (i % 5);
    auto l1 = h->write(h, paths[depth]);
    ctr++;
    l1->release();
}

TEST_CASE( "randy-threads", "[basic]" ) {
    auto h = std::make_shared<HiLok>();
    int ctr = 0;
    int pool_size = 100;
    std::vector<std::thread> threads;
    for(int i = 0; i < pool_size; ++i)
    {
        threads.emplace_back(std::thread([&h, &ctr, i] () { randy_worker(i, h, ctr); } ));
    }

    for (auto& thread : threads) {
        thread.join();
    }
    CHECK(ctr == pool_size);
    CHECK(h->size() == 0);
}

