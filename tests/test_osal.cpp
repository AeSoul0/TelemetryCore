#include <gtest/gtest.h>

#include <atomic>

#include "osal/host.h"
#include "osal/osal.h"

namespace {

class OsalTest : public ::testing::Test {
  protected:
    void SetUp() override { osal::host::resetKernel(); }
    void TearDown() override {
        osal::requestShutdown();
        osal::host::joinAllTasks();
        osal::host::resetKernel();
    }
};

struct Item {
    uint32_t id;
    float value;
};

} // namespace

TEST_F(OsalTest, QueueIsFifo) {
    osal::Queue<Item, 3> queue;
    ASSERT_EQ(queue.create(), ErrorCode::OK);
    EXPECT_EQ(queue.send({1, 1.0f}), ErrorCode::OK);
    EXPECT_EQ(queue.send({2, 2.0f}), ErrorCode::OK);
    EXPECT_EQ(queue.count(), 2U);

    Item out{};
    EXPECT_EQ(queue.receive(out, osal::kNoWait), ErrorCode::OK);
    EXPECT_EQ(out.id, 1U);
    EXPECT_EQ(queue.receive(out, osal::kNoWait), ErrorCode::OK);
    EXPECT_EQ(out.id, 2U);
    EXPECT_EQ(queue.count(), 0U);
}

TEST_F(OsalTest, QueueFullWhenProducerFasterThanConsumer) {
    osal::Queue<Item, 2> queue;
    queue.create();
    EXPECT_EQ(queue.send({1, 0}), ErrorCode::OK);
    EXPECT_EQ(queue.send({2, 0}), ErrorCode::OK);
    EXPECT_EQ(queue.send({3, 0}), ErrorCode::QUEUE_FULL);
    EXPECT_EQ(queue.send({3, 0}, 20), ErrorCode::QUEUE_FULL) << "still full after the timeout";
}

TEST_F(OsalTest, QueueWrapsAround) {
    osal::Queue<Item, 2> queue;
    queue.create();
    Item out{};
    for (uint32_t i = 0; i < 10; ++i) {
        ASSERT_EQ(queue.send({i, 0}), ErrorCode::OK);
        ASSERT_EQ(queue.receive(out, osal::kNoWait), ErrorCode::OK);
        EXPECT_EQ(out.id, i);
    }
}

TEST_F(OsalTest, ReceiveTimesOutWhenEmpty) {
    osal::Queue<Item, 2> queue;
    queue.create();
    Item out{};
    EXPECT_EQ(queue.receive(out, osal::kNoWait), ErrorCode::TIMEOUT);
    const uint32_t start = osal::nowMs();
    EXPECT_EQ(queue.receive(out, 30), ErrorCode::TIMEOUT);
    EXPECT_GE(osal::nowMs() - start, 25U);
}

TEST_F(OsalTest, UncreatedObjectsReportNotInitialized) {
    osal::Queue<Item, 2> queue;
    Item out{};
    EXPECT_EQ(queue.send({1, 0}), ErrorCode::NOT_INITIALIZED);
    EXPECT_EQ(queue.receive(out, osal::kNoWait), ErrorCode::NOT_INITIALIZED);
    osal::BinarySemaphore semaphore;
    EXPECT_EQ(semaphore.take(osal::kNoWait), ErrorCode::NOT_INITIALIZED);
}

TEST_F(OsalTest, SemaphoreSignalsOnce) {
    osal::BinarySemaphore semaphore;
    ASSERT_EQ(semaphore.create(), ErrorCode::OK);
    EXPECT_EQ(semaphore.take(osal::kNoWait), ErrorCode::TIMEOUT);
    semaphore.giveFromIsr();
    semaphore.give(); // binary: a second give does not accumulate
    EXPECT_EQ(semaphore.take(osal::kNoWait), ErrorCode::OK);
    EXPECT_EQ(semaphore.take(osal::kNoWait), ErrorCode::TIMEOUT);
}

namespace {
osal::Queue<Item, 4> pipeQueue;
std::atomic<uint32_t> consumed{0};

void consumerFn(void* /*arg*/) {
    Item item{};
    while (pipeQueue.receive(item, osal::kWaitForever) == ErrorCode::OK) {
        consumed.fetch_add(item.id);
    }
}
} // namespace

TEST_F(OsalTest, TasksStartOnlyWithTheSchedulerAndStopOnShutdown) {
    pipeQueue.create();
    consumed = 0;
    osal::Task<128> consumer;
    ASSERT_EQ(consumer.start("Consumer", osal::Priority::Normal, consumerFn), ErrorCode::OK);

    pipeQueue.send({5, 0});
    osal::delayMs(30);
    EXPECT_EQ(consumed.load(), 0U) << "tasks must not run before startScheduler()";

    osal::startScheduler();
    pipeQueue.send({7, 0}, 100);
    for (int i = 0; i < 100 && consumed.load() != 12U; ++i) {
        osal::delayMs(5);
    }
    EXPECT_EQ(consumed.load(), 12U);

    // The consumer is blocked forever in receive(): shutdown must wake it up.
    osal::requestShutdown();
    osal::host::joinAllTasks();
    SUCCEED();
}

TEST_F(OsalTest, DelayUntilKeepsAFixedPeriod) {
    uint32_t lastWake = osal::nowMs();
    const uint32_t start = lastWake;
    for (int i = 0; i < 5; ++i) {
        osal::delayMs(3); // variable work inside the loop must not shift the schedule
        osal::delayUntil(lastWake, 20);
    }
    EXPECT_EQ(lastWake - start, 100U) << "schedule advances by exactly the period";
    const uint32_t elapsed = osal::nowMs() - start;
    EXPECT_GE(elapsed, 99U);
    EXPECT_LT(elapsed, 160U);
}

TEST_F(OsalTest, MutexAndLockGuard) {
    osal::Mutex mutex;
    ASSERT_EQ(mutex.create(), ErrorCode::OK);
    {
        osal::LockGuard guard(mutex);
    }
    osal::LockGuard again(mutex); // would deadlock if the guard had not released it
    SUCCEED();
}
