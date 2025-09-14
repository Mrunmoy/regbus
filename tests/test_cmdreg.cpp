#include <gtest/gtest.h>

#include "regbus/CmdReg.hpp"

TEST(CmdReg, EdgeTriggered)
{
    regbus::CmdReg<int> c;
    int v = 0;
    EXPECT_FALSE(c.consume(v));
    c.post(42);
    EXPECT_TRUE(c.consume(v));
    EXPECT_EQ(v, 42);
    EXPECT_FALSE(c.consume(v)); // one-shot
}
