/*
 * Copyright (C) 2025
 * Licensed under the Apache License, Version 2.0
 */

#include <gtest/gtest.h>
#include <systemc>
#include <tbb/concurrent_priority_queue.h>

#include "MainMemCosim.hpp"   // Must define MainMemCosim::Req and TEST_PQ

using namespace vpsim;
using namespace sc_core;

class compare_Req {
        public:
            bool operator()(const MainMemCosim::Req &u, const MainMemCosim::Req &v) const {
                return (u.epoch > v.epoch || (u.epoch == v.epoch && u.time_stamp > v.time_stamp));
            }
        };

tbb::concurrent_priority_queue<MainMemCosim::Req, compare_Req> TEST_PQ;
/*
 * GoogleTest entry point
 */
int sc_main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

/*
 * Helper to build Req objects easily
 */
static MainMemCosim::Req make_req(
    uint64_t epoch,
    uint64_t timestamp,
    void* phys = 0,
    uint32_t cpu = 0,
    bool write = false,
    uint32_t size = 0)
{
    MainMemCosim::Req r{};
    r.epoch = epoch;
    r.time_stamp = timestamp;
    r.phys = phys;
    r.id = cpu;
    r.write = write;
    r.size = size;
    r.fetch = 0;
    return r;
}

/*
 * Ensure the priority queue returns the correct element
 * first by epoch, then by timestamp.
 */
TEST(MainMemCosimPriorityQueueTest, OrderingByEpochThenTimestamp)
{
    // Clear the queue before testing
    while (true) {
        MainMemCosim::Req dump;
        if (!TEST_PQ.try_pop(dump)) break;
    }

    // Two Req elements with different epoch
    MainMemCosim::Req first  = make_req(/*epoch*/1, /*timestamp*/150);
    MainMemCosim::Req second = make_req(/*epoch*/0, /*timestamp*/500);

    // According to the rule: smallest epoch has highest priority
    // so "second" should come out first.

    TEST_PQ.push(first);
    TEST_PQ.push(second);

    MainMemCosim::Req out1, out2;

    ASSERT_TRUE(TEST_PQ.try_pop(out1));
    ASSERT_TRUE(TEST_PQ.try_pop(out2));

    EXPECT_EQ(out1.epoch, 0);
    EXPECT_EQ(out2.epoch, 1);
}

/*
 * Same-epoch ordering test: when epoch is equal, the queue must
 * order by time_stamp.
 */
TEST(MainMemCosimPriorityQueueTest, OrderingByTimestampWhenEpochEqual)
{
    // Clear queue
    while (true) {
        MainMemCosim::Req dump;
        if (!TEST_PQ.try_pop(dump)) break;
    }

    // Same epoch, different timestamps
    MainMemCosim::Req early = make_req(/*epoch*/42, /*timestamp*/100);
    MainMemCosim::Req later = make_req(/*epoch*/42, /*timestamp*/200);

    // Early timestamp should be returned first

    TEST_PQ.push(later);
    TEST_PQ.push(early);

    MainMemCosim::Req out1, out2;
    ASSERT_TRUE(TEST_PQ.try_pop(out1));
    ASSERT_TRUE(TEST_PQ.try_pop(out2));

    EXPECT_EQ(out1.epoch, 42);
    EXPECT_EQ(out1.time_stamp, 100);

    EXPECT_EQ(out2.epoch, 42);
    EXPECT_EQ(out2.time_stamp, 200);
}

/*
 * Mixed ordering: epoch is primary key, timestamp second.
 */
TEST(MainMemCosimPriorityQueueTest, ComplexOrdering)
{
    // Clear queue
    while (true) {
        MainMemCosim::Req dump;
        if (!TEST_PQ.try_pop(dump)) break;
    }

    MainMemCosim::Req r1 = make_req(5, 300);
    MainMemCosim::Req r2 = make_req(2, 900);
    MainMemCosim::Req r3 = make_req(2, 100);
    MainMemCosim::Req r4 = make_req(7,  50);

    // Expected order:
    // 1 → r3 (epoch=2, timestamp=100)
    // 2 → r2 (epoch=2, timestamp=900)
    // 3 → r1 (epoch=5)
    // 4 → r4 (epoch=7)

    TEST_PQ.push(r1);
    TEST_PQ.push(r2);
    TEST_PQ.push(r3);
    TEST_PQ.push(r4);

    MainMemCosim::Req out;

    ASSERT_TRUE(TEST_PQ.try_pop(out));
    EXPECT_EQ(out.epoch, 2);
    EXPECT_EQ(out.time_stamp, 100);

    ASSERT_TRUE(TEST_PQ.try_pop(out));
    EXPECT_EQ(out.epoch, 2);
    EXPECT_EQ(out.time_stamp, 900);

    ASSERT_TRUE(TEST_PQ.try_pop(out));
    EXPECT_EQ(out.epoch, 5);

    ASSERT_TRUE(TEST_PQ.try_pop(out));
    EXPECT_EQ(out.epoch, 7);
}
