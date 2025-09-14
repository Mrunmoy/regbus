#include <gtest/gtest.h>

#include "regbus/Registry.hpp"

enum class K : uint8_t
{
    A,
    B,
    CMD_GO
};

struct AType
{
    int a;
};
struct BType
{
    float b;
};

template <K KK>
struct Traits;
template <>
struct Traits<K::A>
{
    using type = AType;
    static constexpr regbus::Kind kind = regbus::Kind::Data;
};
template <>
struct Traits<K::B>
{
    using type = BType;
    static constexpr regbus::Kind kind = regbus::Kind::Data;
};
template <>
struct Traits<K::CMD_GO>
{
    using type = bool;
    static constexpr regbus::Kind kind = regbus::Kind::Cmd;
};

using R = regbus::Registry<K, Traits, K::A, K::B, K::CMD_GO>;

TEST(Registry, DataRoundTrip)
{
    R r;
    r.write<K::A>({123});
    AType a{};
    uint32_t seq = 0;
    ASSERT_TRUE(r.read<K::A>(a, &seq));
    EXPECT_EQ(a.a, 123);
    EXPECT_GT(seq, 0u);

    r.write<K::B>({3.14f});
    BType b{};
    ASSERT_TRUE(r.read<K::B>(b));
    EXPECT_FLOAT_EQ(b.b, 3.14f);
}

TEST(Registry, CommandConsume)
{
    R r;
    bool go = false;
    EXPECT_FALSE(r.consume<K::CMD_GO>(go));
    r.post<K::CMD_GO>(true);
    EXPECT_TRUE(r.consume<K::CMD_GO>(go));
    EXPECT_TRUE(go);
    EXPECT_FALSE(r.consume<K::CMD_GO>(go)); // cleared
}

TEST(Registry, SizeBudgetIsSmall)
{
    // Ensure we stay tiny; this catches accidental bloat.
    static_assert(R::bytes() <= 4096, "Registry too large");
}
