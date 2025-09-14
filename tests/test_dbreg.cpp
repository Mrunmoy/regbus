#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <type_traits>

#include "regbus/DBReg.hpp"

// Correlated fields make any torn read obvious.
struct S
{
    uint32_t a;
    uint32_t b;
};
static_assert(std::is_trivially_copyable<S>::value, "S must be trivially copyable");

static inline void prime_first_write(regbus::DBReg<S> &r)
{
    // Wait until at least one write has happened so reads don't fail spuriously.
    S tmp{};
    while (!r.read(tmp))
    {
        std::this_thread::yield();
    }
}

// 1) Coherence under constant flips: b must always equal ~a.
TEST(DBReg, NoTearPattern_CoherentUnderFlip)
{
    regbus::DBReg<S> r;
    std::atomic<bool> run{true};

    std::thread w([&]
                  {
        uint32_t i = 0;
        while (run.load(std::memory_order_relaxed)) {
            r.write(S{i, ~i});
            ++i;
        } });

    prime_first_write(r);

    for (int k = 0; k < 50000; ++k)
    {
        S s{};
        uint32_t seq = 0;
        ASSERT_TRUE(r.read(s, &seq));
        ASSERT_EQ(s.b, ~s.a) << "Torn read detected at iter " << k;
        (void)seq;
    }

    run.store(false, std::memory_order_relaxed);
    w.join();
}

// 2) Sequence should never go backwards across successful reads.
TEST(DBReg, MonotonicSequence)
{
    regbus::DBReg<S> r;
    std::atomic<bool> run{true};

    std::thread w([&]
                  {
        uint32_t i = 0;
        while (run.load(std::memory_order_relaxed)) {
            r.write(S{i, ~i});
            ++i;
        } });

    prime_first_write(r);

    uint32_t last_seq = 0;
    for (int k = 0; k < 20000; ++k)
    {
        S s{};
        uint32_t seq = 0;
        ASSERT_TRUE(r.read(s, &seq));
        ASSERT_GE(seq, last_seq) << "Sequence decreased at iter " << k;
        last_seq = seq;
        // Still check coherence each time.
        ASSERT_EQ(s.b, ~s.a) << "Coherence failed at iter " << k;
    }

    run.store(false, std::memory_order_relaxed);
    w.join();
}

// 3) Two immediate reads should either see the same snapshot or a newer one.
//    Neither should be torn; both must satisfy the correlation.
TEST(DBReg, DoubleReadStability)
{
    regbus::DBReg<S> r;
    std::atomic<bool> run{true};

    std::thread w([&]
                  {
        uint32_t i = 0;
        while (run.load(std::memory_order_relaxed)) {
            r.write(S{i, ~i});
            ++i;
        } });

    prime_first_write(r);

    for (int k = 0; k < 20000; ++k)
    {
        S s1{}, s2{};
        uint32_t q1 = 0, q2 = 0;

        ASSERT_TRUE(r.read(s1, &q1));
        ASSERT_TRUE(r.read(s2, &q2));

        ASSERT_TRUE(q2 == q1 || q2 > q1)
            << "Second read yielded older seq at iter " << k
            << " (q1=" << q1 << ", q2=" << q2 << ")";

        // Both snapshots must be coherent.
        ASSERT_EQ(s1.b, ~s1.a) << "First read torn at iter " << k;
        ASSERT_EQ(s2.b, ~s2.a) << "Second read torn at iter " << k;
    }

    run.store(false, std::memory_order_relaxed);
    w.join();
}
