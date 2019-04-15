
#pragma once

#include <stdint.h>
#include <cstddef>

#include "swhelp.h"

#if !defined(UINT128_MAX) // detect existence of HW 128-bit integer types
#define USE_SW_INT128 1
#endif


// define software implementation of integral typess
namespace details
{
template<size_t byte_size> struct int_traits { };
template<> struct int_traits<16> { using halfint_t = uint64_t; };
template<> struct int_traits<8> { using halfint_t = uint32_t; };
template<> struct int_traits<4> { using halfint_t = uint16_t; };
template<> struct int_traits<2> { using halfint_t = uint8_t; };
}


template<size_t byte_size, bool is_signed>
class intbase_t
{
    static_assert(byte_size > 1, "expecting 2 bytes or more");
//    static_assert(is_pow_2(byte_size), "expecting power of 2");

private:
    using halfint_t = typename details::int_traits<byte_size>::halfint_t;
    static constexpr size_t half_bitsize = sizeof(halfint_t) * 8;
    static constexpr halfint_t topbit_mask = halfint_t(1) << (half_bitsize - 1);
    static constexpr halfint_t allones_mask = static_cast<halfint_t>(~0);

    halfint_t lower_half;
    halfint_t upper_half;

    constexpr intbase_t(halfint_t lower_half, halfint_t upper_half) : lower_half(lower_half), upper_half(upper_half) {

    }

public:

    intbase_t() = default;

    template<typename integral_t, typename = std::enable_if_t<std::is_integral_v<integral_t>>>
    constexpr intbase_t(integral_t val) {
        if constexpr (sizeof(integral_t) == sizeof(intbase_t))
        {
            *this = details::bit_cast<intbase_t>(val);
        }
        else if constexpr (sizeof(integral_t) <= sizeof(halfint_t))
        {
            lower_half = val;
            if constexpr (std::is_signed_v<integral_t>) {
                upper_half = (val & topbit_mask) ? allones_mask : 0;
            }
            else {
                upper_half = 0;
            }
        }
        else
        {
            lower_half = static_cast<halfint_t>(val);
            upper_half = static_cast<halfint_t>(val >> half_bitsize);
        }
    }


    //
    // conversion
    //

public:

    template<typename integral_t, typename = std::enable_if_t<std::is_integral_v<integral_t>>>
    constexpr operator integral_t() const
    {
        if constexpr (sizeof(integral_t) == sizeof(intbase_t)) {
            return details::bit_cast<integral_t>(*this);
        }
        else if constexpr (sizeof(integral_t) <= sizeof(halfint_t)) {
            return static_cast<integral_t>(lower_half);
        }
        else {
            return (static_cast<integral_t>(upper_half) << half_bitsize) | static_cast<integral_t>(lower_half);
        }
    }

    //
    // arithmetic
    //

public:

    constexpr intbase_t operator-(intbase_t other) const
    {
        // todo: handle borrowing
        return intbase_t(this->upper_half - other.upper_half, this->lower_half - other.lower_half);
    }

    constexpr intbase_t operator+(intbase_t other) const
    {
        // todo: handle carry
        return intbase_t(this->upper_half + other.upper_half, this->lower_half + other.lower_half);
    }

    constexpr intbase_t operator*(intbase_t other) const
    {
        // todo: handle carry
        return intbase_t(this->upper_half * other.upper_half, this->lower_half * other.lower_half);
    }

    constexpr intbase_t operator/(intbase_t other) const
    {
        // todo: handle carry
        return intbase_t(this->upper_half / other.upper_half, this->lower_half / other.lower_half);
    }

    constexpr intbase_t operator%(intbase_t other) const
    {
        // todo: handle carry
        return intbase_t(this->upper_half % other.upper_half, this->lower_half % other.lower_half);
    }

    //
    // increment
    //
public:
    constexpr intbase_t operator--() {
        auto t = *this;
        *this = this->operator-(1);
        return t;
    }
    constexpr intbase_t operator--(int) {
        *this = this->operator-(1);
        return *this;
    }
    constexpr intbase_t operator++() {
        auto t = *this;
        *this = this->operator+(1);
        return t;
    }
    constexpr intbase_t operator++(int) {
        *this = this->operator+(1);
        return *this;
    }

    //
    // op-eq operators
    //
    
public:

    constexpr intbase_t operator+=(intbase_t other) {
        *this = this->operator+(other);
        return *this;
    }
    constexpr intbase_t operator-=(intbase_t other) {
        *this = this->operator-(other);
        return *this;
    }
    constexpr intbase_t operator*=(intbase_t other) {
        *this = this->operator-(other);
        return *this;
    }
    constexpr intbase_t operator/=(intbase_t other) {
        *this = this->operator-(other);
        return *this;
    }

    constexpr intbase_t operator<<=(int amount) {
        *this = this->operator<<(amount);
        return *this;
    }
    constexpr intbase_t operator>>=(int amount) {
        *this = this->operator>>(amount);
        return *this;
    }
    constexpr intbase_t operator|=(intbase_t other) {
        *this = this->operator|(other);
        return *this;
    }
    constexpr intbase_t operator&=(intbase_t other) {
        *this = this->operator&(other);
        return *this;
    }


    //
    // bitwise
    //

public:

    constexpr intbase_t operator<<(int amount) const
    {
        // todo: handle wrap
        return intbase_t(this->upper_half << amount, this->lower_half << amount);
    }

    constexpr intbase_t operator>>(int amount) const
    {
        // todo: handle wrap
        return intbase_t(this->upper_half >> amount, this->lower_half >> amount);
    }


    constexpr intbase_t operator&(intbase_t other) const {
        return intbase_t(this->upper_half & other.upper_half, this->lower_half & other.lower_half);
    }
    constexpr intbase_t operator|(intbase_t other) const {
        return intbase_t(this->upper_half | other.upper_half, this->lower_half | other.lower_half);
    }
    constexpr intbase_t operator^(intbase_t other) const {
        return intbase_t(this->upper_half ^ other.upper_half, this->lower_half ^ other.lower_half);
    }

    constexpr intbase_t operator~() const {
        return intbase_t(~this->upper_half, ~this->lower_half);
    }


    //
    // relational operators
    //

public:
    constexpr bool operator==(intbase_t other) const {
        return this->upper_half == other.upper_half && this->lower_half == other.lower_half;
    }
    constexpr bool operator!=(intbase_t other) const { return !this->operator==(other); }

    constexpr bool operator<(intbase_t other) const {
        if (this->upper_half < other.lower_half)
            return true;
        if (this->upper_half > other.lower_half)
            return false;
        return this->lower_half < other.lower_half; 
    }
    constexpr bool operator<=(intbase_t other) const {
        if (this->upper_half < other.lower_half)
            return true;
        if (this->upper_half > other.lower_half)
            return false;
        return this->lower_half <= other.lower_half;
    }

    constexpr bool operator>(intbase_t other) const { return other.operator<(*this); }
    constexpr bool operator>=(intbase_t other) const { return other.operator<=(*this); }


    //
    // misc operations
    //
public:
    constexpr bool reverse_bit_scan(unsigned long *index) {
        if (details::reverse_bit_scan(index, upper_half)) {
            return index + sizeof(halfint_t);
        }
        return details::reverse_bit_scan(index, lower_half);
    }
};

using int128sw_t = intbase_t<16, true>;
using uint128sw_t = intbase_t<16, false>;
using int64sw_t = intbase_t<8, true>;
using uint64sw_t = intbase_t<8, false>;
using int32sw_t = intbase_t<4, true>;
using uint32sw_t = intbase_t<4, false>;
using int16sw_t = intbase_t<2, true>;
using uint16sw_t = intbase_t<2, false>;

#if USE_SW_INT128 // detect existence of HW 128-bit integer types

using int128_t = intbase_t<16, true>;
using uint128_t = intbase_t<16, false>;

template<typename integral_t, typename = std::enable_if_t<std::is_integral_v<integral_t>>>
constexpr int128_t operator-(integral_t minuend, int128_t subtrahend)
{
    return int128_t(minuend) - subtrahend;
}
template<typename integral_t, typename = std::enable_if_t<std::is_integral_v<integral_t>>>
constexpr uint128_t operator-(integral_t minuend, uint128_t subtrahend)
{
    return uint128_t(minuend) - subtrahend;
}
template<typename integral_t, typename = std::enable_if_t<std::is_integral_v<integral_t>>>
constexpr bool operator==(integral_t a, int128_t b)
{
    return uint128_t(a) == b;
}
template<typename integral_t, typename = std::enable_if_t<std::is_integral_v<integral_t>>>
constexpr bool operator==(integral_t a, uint128_t b)
{
    return uint128_t(a) == b;
}


// todo: correct these values
#define INT128_MIN        (-9223372036854775807i64 - 1)
#define INT128_MAX        0xffffffffui32
#define UINT128_MAX       0xffffffffffffffffui64
#define INT128_C(x)       (x ## LL)
#define UINT128_C(x)      (x ## ULL)

#endif

namespace details
{
    template<typename T>
    struct make_signed { using type = typename std::make_signed_t<T>;  };
#if USE_SW_INT128 
    template<>
    struct make_signed<uint128_t> { using type = int128_t; };
#endif

    template<typename T>
    using make_signed_t = typename details::make_signed<T>::type;

    static constexpr bool reverse_bit_scan(unsigned long *index, uint128_t mask) {
        return mask.reverse_bit_scan(index);
    }
}

