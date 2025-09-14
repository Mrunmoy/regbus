#pragma once

#include <tuple>
#include <type_traits>
#include <cstdint>

#include "DBReg.hpp"
#include "CmdReg.hpp"

namespace regbus
{

    // Kind of register (Data = double-buffer latest; Cmd = edge-trigger command)
    enum class Kind
    {
        Data,
        Cmd
    };

    // Users provide: template<Key K> struct Traits { using type = ...; static constexpr Kind kind = Kind::Data; }

    namespace detail
    {

        // helper for dependent false in static_assert
        template <typename T>
        struct dependent_false : std::false_type
        {
        };

        // PRIMARY TEMPLATE DECLARATION
        template <typename Key, Key K, Key... Keys>
        struct index_of;

        // MATCH AT HEAD: if the first element of the pack equals K, index is 0
        template <typename Key, Key K, Key... Tail>
        struct index_of<Key, K, K, Tail...>
        {
            static constexpr std::size_t value = 0;
        };

        // NOT A MATCH: drop Head and continue searching in Tail...
        template <typename Key, Key K, Key Head, Key... Tail>
        struct index_of<Key, K, Head, Tail...>
        {
            static constexpr std::size_t value = 1 + index_of<Key, K, Tail...>::value;
        };

        // EMPTY PACK: key not found
        template <typename Key, Key K>
        struct index_of<Key, K>
        {
            static_assert(dependent_false<Key>::value, "Key not present in Registry key list");
            static constexpr std::size_t value = 0;
        };

        // Select storage type for a key based on Traits::kind<K>
        template <typename Key, template <Key> class Traits, Key K>
        struct storage_for
        {
            using T = typename Traits<K>::type;
            static constexpr Kind kind = Traits<K>::kind;
            using type = std::conditional_t<kind == Kind::Cmd, CmdReg<T>, DBReg<T>>;
        };

    } // namespace detail

    // Registry: strongly-typed, header-only, no heap/RTTI
    template <typename Key, template <Key> class Traits, Key... Keys>
    class Registry
    {
    public:
        template <Key K>
        using value_t = typename Traits<K>::type;
        template <Key K>
        static constexpr Kind kind = Traits<K>::kind;

        // ---- Data registers (double-buffered latest) ----
        template <Key K, typename = std::enable_if_t<kind<K> == Kind::Data>>
        inline void write(const value_t<K> &v) { get<K>().write(v); }

        template <Key K, typename = std::enable_if_t<kind<K> == Kind::Data>>
        inline bool read(value_t<K> &out, uint32_t *seq = nullptr) const { return cget<K>().read(out, seq); }

        template <Key K, typename = std::enable_if_t<kind<K> == Kind::Data>>
        inline bool has() const { return cget<K>().has(); }

        // ---- Command registers (edge-trigger) ----
        template <Key K, typename = std::enable_if_t<kind<K> == Kind::Cmd>>
        inline void post(const value_t<K> &v) { get<K>().post(v); }

        template <Key K, typename = std::enable_if_t<kind<K> == Kind::Cmd>>
        inline bool consume(value_t<K> &out) { return get<K>().consume(out); }

        // Size accounting (compile-time, useful for budgets)
        static constexpr std::size_t bytes() { return sizeof(Registry); }

    private:
        template <Key K>
        static constexpr std::size_t idx() { return detail::index_of<Key, K, Keys...>::value; }

        template <Key K>
        using storage_t = typename detail::storage_for<Key, Traits, K>::type;

        template <Key K>
        inline storage_t<K> &get() { return std::get<idx<K>()>(storage_); }
        template <Key K>
        inline const storage_t<K> &cget() const { return std::get<idx<K>()>(storage_); }

        // One storage member per key (type-selected at compile time)
        std::tuple<storage_t<Keys>...> storage_;
    };

} // namespace regbus