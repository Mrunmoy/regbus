#include <iostream>
#include <thread>
#include <chrono>

#include "regbus/Registry.hpp"

// 1) Define your key set
enum class MyKey : uint8_t
{
    IMU_RAW,
    FUSION_STATE,
    CMD_RESET
};

// 2) Define your types (must be trivially copyable)
struct IMURaw
{
    uint64_t t_us;
    float ax, ay, az, gx, gy, gz;
};
struct FusionState
{
    uint64_t t_us;
    float qw, qx, qy, qz;
};

// 3) Map keys -> (type, kind)
template <MyKey K>
struct MyTraits;
template <>
struct MyTraits<MyKey::IMU_RAW>
{
    using type = IMURaw;
    static constexpr regbus::Kind kind = regbus::Kind::Data;
};
template <>
struct MyTraits<MyKey::FUSION_STATE>
{
    using type = FusionState;
    static constexpr regbus::Kind kind = regbus::Kind::Data;
};
template <>
struct MyTraits<MyKey::CMD_RESET>
{
    using type = bool;
    static constexpr regbus::Kind kind = regbus::Kind::Cmd;
};

// 4) Build a registry over a fixed key list
using MyReg = regbus::Registry<MyKey, MyTraits, MyKey::IMU_RAW, MyKey::FUSION_STATE, MyKey::CMD_RESET>;

int main()
{
    MyReg reg;

    // Writer thread: IMU samples
    std::thread writer([&]
                       {
    for (int i=0;i<10;i++){
      IMURaw s{(uint64_t)i, (float)i,0,0,0,0,0};
      reg.write<MyKey::IMU_RAW>(s);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    reg.post<MyKey::CMD_RESET>(true); });

    // Reader loop
    for (int i = 0; i < 12; i++)
    {
        IMURaw r{};
        if (reg.read<MyKey::IMU_RAW>(r))
        {
            std::cout << "IMU ax=" << r.ax << " t=" << r.t_us << "\n";
        }
        bool reset = false;
        if (reg.consume<MyKey::CMD_RESET>(reset))
        {
            std::cout << "CMD_RESET consumed\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    writer.join();
    return 0;
}
