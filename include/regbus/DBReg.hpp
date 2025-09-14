#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace regbus
{
    template <typename T>
    class DBReg
    {
        static_assert(std::is_trivially_copyable<T>::value,
                      "DBReg<T>: T must be trivially copyable (no heap, fast copy).");

    public:
        DBReg() : idx_(0), has_(false)
        {
            seq_[0].store(0);
            seq_[1].store(0);
        }

        inline void write(const T &v)
        {
            uint32_t cur = idx_.load(std::memory_order_acquire), nxt = cur ^ 1u;
            buf_[nxt] = v; // single POD copy
            uint32_t s = seq_ctr_.fetch_add(1, std::memory_order_acq_rel) + 1;
            seq_[nxt].store(s, std::memory_order_release);
            idx_.store(nxt, std::memory_order_release);
            has_.store(true, std::memory_order_release);
        }

        inline bool read(T &out, uint32_t *out_seq = nullptr) const
        {
            if (!has_.load(std::memory_order_acquire))
                return false;
            for (;;)
            {
                uint32_t i1 = idx_.load(std::memory_order_acquire);
                uint32_t s1 = seq_[i1].load(std::memory_order_acquire);
                T tmp = buf_[i1];
                uint32_t i2 = idx_.load(std::memory_order_acquire);
                if (i1 == i2 && s1 == seq_[i1].load(std::memory_order_acquire))
                {
                    out = tmp;
                    if (out_seq)
                        *out_seq = s1;
                    return true;
                }
            }
        }

        inline bool has() const { return has_.load(std::memory_order_acquire); }

    private:
        alignas(16) T buf_[2]{}; // avoid false sharing / misalignment
        std::atomic<uint32_t> seq_[2];
        std::atomic<uint32_t> seq_ctr_{0};
        std::atomic<uint32_t> idx_;
        std::atomic<bool> has_;
    };
} // namespace regbus
