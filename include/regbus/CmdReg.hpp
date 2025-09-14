#pragma once

#include <atomic>

namespace regbus
{
    template <typename T>
    class CmdReg
    {
    public:
        void post(const T &v)
        {
            val_ = v;
            ready_.store(true, std::memory_order_release);
        }
        bool consume(T &out)
        {
            if (!ready_.load(std::memory_order_acquire))
                return false;
            out = val_;
            ready_.store(false, std::memory_order_release);
            return true;
        }
        bool pending() const { return ready_.load(std::memory_order_acquire); }

    private:
        T val_{};
        std::atomic<bool> ready_{false};
    };
} // namespace regbus
