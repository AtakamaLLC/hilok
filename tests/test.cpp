#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <hilok.hpp>
#include <psplit.hpp>

#include <thread>
#include <iostream>
#include <array>

void slow_increment(int &ctr) {
    int x = ctr;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ctr = x + 1; 
}


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
    REQUIRE_THROWS_AS(h->write(h, "a", false), HiErr);
    REQUIRE_THROWS_AS(h->write(h, "a/b", false), HiErr);

    INFO("read lock while write");
    auto l2 = h->read(h, "a/b", false);
    
    INFO("write lock on sibling");
    auto l3 = h->write(h, "a/b/d", false);
    l1->release();
    
    INFO("write lock root after partial release");
    REQUIRE_THROWS_AS(h->write(h, "a", false), HiErr);
    
    l3->release();
    INFO("write lock root after partial release");
    REQUIRE_THROWS_AS(h->write(h, "a", false), HiErr);
    
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
    REQUIRE_THROWS_AS(h->read(h, "a/b", false), HiErr);
    l4->release();

    INFO("read child ok now");
    h->read(h, "a/b", false);
}

void check_read_locked(std::shared_ptr<HiLok> h, bool &ok, std::string path) {
    h->read(h, path, false)->release();
    try {
        std::cout << "check read locked" << std::endl;
        auto l1 = h->write(h, path, false);
        std::cout << "not read locked" << std::endl;
    } catch (HiErr &) {
        ok = true;
    }
}

bool thread_check_read_locked(std::shared_ptr<HiLok> h, std::string path) {
    bool ok = false;
    auto thread = std::thread([&h, &ok, path] () { check_read_locked(h, ok, path); } );
    thread.join();
    return ok;
}


void check_write_locked(std::shared_ptr<HiLok> h, bool &ok, std::string path) {
    try {
        auto l1 = h->read(h, path, false);
    } catch (HiErr &) {
        ok = true;
    }
}

bool thread_check_write_locked(std::shared_ptr<HiLok> h, std::string path) {
    bool ok = false;
    auto thread = std::thread([&h, &ok, path] () { check_write_locked(h, ok, path); } );
    thread.join();
    return ok;
}

TEST_CASE( "lock-escalate-deescalate", "[basic]" ) {
    auto h = std::make_shared<HiLok>('/', true);
    std::cout << "write a" << std::endl;
    auto l1 = h->write(h, "a");
    REQUIRE( h->find_node("a") );
    std::cout << "read a" << std::endl;
    auto l2 = h->read(h, "a");
    l1->release();
    auto nod = h->find_node("a");
    REQUIRE(nod);
    CHECK(thread_check_read_locked(h, "a"));
    auto l3 = h->write(h, "a");
    l2->release();
    CHECK(thread_check_write_locked(h, "a"));
    l3->release();
}

void dump_map(HiLok &h) {
    for (auto &it : h.m_map) {
        std::cout << it.first.first << "/" << it.first.second << ":" << it.second << std::endl;
    }
}

TEST_CASE( "rename-lock", "[basic]" ) {
    auto h = std::make_shared<HiLok>('/', false);
    auto l1 = h->write(h, "a/b/c/d");
    h->rename("a/b/c/d", "a/b/r/x", false);
   
    dump_map(*h);
    REQUIRE(h->size() == 4);

    // a/b/r now locked
    REQUIRE_THROWS_AS(h->write(h, "a/b/r", false), HiErr);

    // a/b/c not locked anymore
    auto l2 = h->write(h, "a/b/c", false);
    l2->release();

    l1->release();

    // release does the right thing
    l2 = h->write(h, "a/b/r/x", false);
    l2->release();

    CHECK(h->size() == 0);
}


TEST_CASE( "rename-on-top", "[basic]" ) {
    auto h = std::make_shared<HiLok>('/', false);
    auto l1 = h->write(h, "a/b/c/d");
    auto l2 = h->write(h, "a/b/c");
    h->rename("a/b/c/d", "a/b/c", false);
   
    // a/b/c write locked
    REQUIRE_THROWS_AS(h->read(h, "a/b/c", false), HiErr);

    l1->release();
    l2->release();

    CHECK(h->size() == 0);
}



TEST_CASE( "rlock-simple", "[basic]" ) {
    HiMutex h(true);
    h.lock();
    h.lock();
    CHECK(h.try_lock());
    h.unlock();
    h.unlock();
}

void rlock_worker(int, HiMutex &h, int &ctr) {
    h.lock();
    h.lock();
    slow_increment(ctr);
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

TEST_CASE( "shared-simple", "[basic]" ) {
    HiMutex h(true);
    h.lock_shared();
    h.unlock_shared();
    CHECK(!h.is_locked());
}

void hold_lock_until(std::shared_ptr<HiLok> h, std::string p1, std::string p2) {
    auto wr1 = h->write(h, p1);
    auto wr2 = h->write(h, p2);
    wr1->release();
    wr2->release();
}


TEST_CASE( "timed-hlock", "[basic]" ) {
    auto i = GENERATE(true, false);
    DYNAMIC_SECTION("recursive " << i) {
    auto h = std::make_shared<HiLok>('/', i);

    auto thread_lock = h->write(h, "y");
    auto thread = std::thread([&h] () { hold_lock_until(h, "a/b", "y"); } );
   
    // wait until thread lock is obtained
    while (true) {
        try {
            h->read(h, "a/b", false)->release();
        } catch (HiErr & err) {
            break;
        }
    }

    INFO("timed read while write");
    {
    auto start = std::chrono::steady_clock::now();
    REQUIRE_THROWS_AS(h->read(h, "a/b", true, 0.01), HiErr);
    auto end = std::chrono::steady_clock::now();
    auto dur = std::chrono::duration<double>(end - start);
    CHECK(dur.count() >= 0.01);
    }
    
    REQUIRE_THROWS_AS(h->read(h, "a/b", false), HiErr);

    INFO("timed write while write");
    {
    auto start = std::chrono::steady_clock::now();
    REQUIRE_THROWS_AS(h->write(h, "a/b", true, 0.01), HiErr);
    auto end = std::chrono::steady_clock::now();
    auto dur = std::chrono::duration<double>(end - start);
    CHECK(dur.count() >= 0.01);
    }
    
    thread_lock->release();
    thread.join();
    }
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


void nest_lock_worker(int &ctr, HiMutex &h1, HiMutex &h2) {
    // simulates the kind of locking that can happen in a nested set of locks, with reentrance
    // lock mutex shared, then lock write child, then unlock both
    h2.lock_shared();
    h1.lock();
    slow_increment(ctr);
    h1.unlock();
    h2.unlock_shared();
}

TEST_CASE( "simulate-nest", "[basic]" ) {
    HiMutex mut1(true);
    HiMutex mut2(true);
    int ctr = 0;
    int pool_size = 100;
    std::vector<std::thread> threads;
    for(int i = 0; i < pool_size; ++i)
    {
        threads.emplace_back(std::thread([&mut1, &mut2, &ctr] () { nest_lock_worker(ctr, mut1, mut2); } ));
    }

    for (auto& thread : threads) {
        thread.join();
    }
    CHECK(mut1.is_locked() == false);
    CHECK(mut2.is_locked() == false);
    CHECK(ctr == pool_size);
}

void worker(int, std::shared_ptr<HiLok> h, int &ctr) {
    auto l1 = h->write(h, "a/b/c/d/e");
    auto l2 = h->write(h, "a/b/c/d/e");
    slow_increment(ctr);
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
    slow_increment(ctr);
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
    slow_increment(ctr);
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

void rename_worker(int i, std::shared_ptr<HiLok> &h, std::vector<int> &ctr) {
    std::array<const char *, 2>paths{"a/x", "a/b"};
    auto l1 = h->write(h, paths[i%2]);
    try {
        h->rename(paths[i%2], paths[(i+1)%2]);
    } catch (HiErr &) {
        // some other thread already renamed me... that's ok, ignore it
    }
    // i still have a lock
    ctr[i%2]++;
    l1->release();
}

TEST_CASE( "rename-threads", "[basic]" ) {
    auto h = std::make_shared<HiLok>();
    int pool_size = 100;
    std::vector<std::thread> threads;
    std::vector<int> ctr(2);
    for(int i = 0; i < pool_size; ++i)
    {
        threads.emplace_back(std::thread([&h, &ctr, i] () { rename_worker(i, h, ctr); } ));
    }

    for (auto& thread : threads) {
        thread.join();
    }
    CHECK(ctr[0] + ctr[1] == pool_size);
    CHECK(h->size() == 0);
}

