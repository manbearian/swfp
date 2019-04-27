
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

template<typename uint_t, typename = std::enable_if_t<std::is_integral_v<uint_t> && std::is_unsigned_v<uint_t>>>
constexpr uint_t add_carry(uint_t a, uint_t b, uint8_t &carry)
{
    if constexpr (sizeof(uint_t) == sizeof(uint8_t)) {
        uint_t sum;
        carry = _addcarry_u8(carry, a, b, &sum);
        return sum;
    }
    else if constexpr (sizeof(uint_t) == sizeof(uint16_t)) {
        uint_t sum;
        carry = _addcarry_u16(carry, a, b, &sum);
        return sum;
    }
    else if constexpr (sizeof(uint_t) == sizeof(uint32_t)) {
        uint_t sum;
        carry = _addcarry_u32(carry, a, b, &sum);
        return sum;
    }
    else if constexpr ((sizeof(uint_t) == sizeof(uint64_t)) && (sizeof(void*) == 8)) {
        uint_t sum;
        carry = _addcarry_u64(carry, a, b, &sum);
        return sum;
    }
    else
    {
        static_assert("larger add_carry not implemented");
    }
}

template<typename uint_t, typename = std::enable_if_t<std::is_integral_v<uint_t> && std::is_unsigned_v<uint_t>>>
constexpr uint_t sub_borrow_slow(uint_t a, uint_t b, uint8_t &borrow) {
    uint_t diff = a - b;
    diff -= borrow;
    borrow = (b > a) || ((b == a) && borrow);
    return diff;
}

template<typename uint_t, typename = std::enable_if_t<std::is_integral_v<uint_t> && std::is_unsigned_v<uint_t>>>
constexpr uint_t sub_borrow(uint_t a, uint_t b, uint8_t &borrow)
{
    if (details::is_constant_evaluated())
    {
        return sub_borrow_slow(a, b, borrow);
    }
    else if constexpr (sizeof(uint_t) == sizeof(uint8_t)) {
        uint_t diff;
        borrow = _subborrow_u8(borrow, a, b, &diff);
        return diff;
    }
    else if constexpr (sizeof(uint_t) == sizeof(uint16_t)) {
        uint_t diff;
        borrow = _subborrow_u16(borrow, a, b, &diff);
        return diff;
    }
    else if constexpr (sizeof(uint_t) == sizeof(uint32_t)) {
        uint_t diff;
        borrow = _subborrow_u32(borrow, a, b, &diff);
        return diff;
    }
    else if constexpr ((sizeof(uint_t) == sizeof(uint64_t)) && (sizeof(void*) == 8)) {
        uint_t diff;
        borrow = _subborrow_u64(borrow, a, b, &diff);
        return diff;
    }
    else {
        return sub_borrow_slow(a, b, borrow);
    }
}

template<typename uint_t, typename = std::enable_if_t<std::is_integral_v<uint_t> && std::is_unsigned_v<uint_t>>>
constexpr uint_t mul_extended(uint_t a, uint_t b, uint_t &upper)
{
    using mask_t = details::selector_t<(sizeof(uint_t) < sizeof(int)), int, int64_t>;

    if constexpr (sizeof(uint_t) <= sizeof(uint32_t))
    {
        constexpr int bitsize = sizeof(uint_t) * 8;
        constexpr mask_t lower_mask = (mask_t(1) << bitsize) - 1;
        constexpr mask_t upper_mask = lower_mask << bitsize;

        if constexpr (sizeof(uint_t) < sizeof(uint32_t)) {
            int full = a * b;
            upper = (full & upper_mask) >> bitsize;
            return full & lower_mask;
        }
        else if constexpr (sizeof(uint_t) == sizeof(uint32_t)) {
            int64_t full = __emulu(a, b);
            upper = (full & upper_mask) >> bitsize;
            return full & lower_mask;
        }
    }
    else if constexpr (sizeof(uint_t) == sizeof(uint64_t))
    {
        return _umul128(a, b, &upper);
    }
    else
    {
        static_assert("extended multiply not implemented for this size/arch");
    }
}

}


template<size_t byte_size, bool is_signed>
class intbase_t
{
    static_assert(byte_size > 1, "expecting 2 bytes or more");
    static_assert(details::is_pow_2(byte_size), "expecting power of 2");

public:

    static constexpr bool is_signed = is_signed;

private:
    using halfint_t = typename details::int_traits<byte_size>::halfint_t;
    static constexpr size_t bitsize = byte_size * 8;
    static constexpr size_t half_bitsize = sizeof(halfint_t) * 8;
    static constexpr halfint_t topbit_mask = halfint_t(1) << (half_bitsize - 1);
    static constexpr halfint_t allones_mask = static_cast<halfint_t>(~halfint_t(0));

    halfint_t lower_half;
    halfint_t upper_half;

    constexpr intbase_t(halfint_t upper_half, halfint_t lower_half) : lower_half(lower_half), upper_half(upper_half) {

    }

public:

    intbase_t() = default;

    template<typename integral_t, typename = std::enable_if_t<std::is_integral_v<integral_t>>>
    constexpr intbase_t(integral_t val) : upper_half(0), lower_half(0) {
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
        else
        {
            integral_t value = (static_cast<integral_t>(upper_half) << half_bitsize) | static_cast<integral_t>(lower_half);

            constexpr integral_t bitdiff = (sizeof(integral_t) * 8) - bitsize;
            if constexpr (is_signed && (bitdiff > 0)) {
                if (upper_half & topbit_mask) {
                    // sign extend...
                    constexpr integral_t signext_mask = ((integral_t(1) << bitdiff) - 1) << bitsize;
                    value |= signext_mask;
                }
            }

            return value;
        }
    }

    constexpr operator bool() const {
        return this->lower_half != 0 || this->upper_half != 0;
    }

    //
    // arithmetic
    //

public:

    constexpr intbase_t operator+(intbase_t other) const
    {
        uint8_t carry = 0;
        halfint_t lower_sum = details::add_carry(this->lower_half, other.lower_half, carry);
        return intbase_t(this->upper_half + other.upper_half + carry, lower_sum);
    }

    constexpr intbase_t operator-(intbase_t other) const
    {
        uint8_t borrow = 0 ;
        halfint_t lower_diff = details::sub_borrow(this->lower_half, other.lower_half, borrow);
        return intbase_t(this->upper_half - other.upper_half - borrow, lower_diff);
    }

    constexpr intbase_t operator*(intbase_t other) const
    {
        halfint_t carry = 0, ll = details::mul_extended(this->lower_half, other.lower_half, carry);
        halfint_t lu = this->lower_half * other.upper_half;
        halfint_t ul = this->upper_half * other.lower_half;
        return intbase_t(lu + ul + carry, ll);
    }

    constexpr intbase_t operator/(intbase_t other) const
    {
        intbase_t dividend = *this;
        intbase_t divisor = other;

        int qsign = 0;
        if constexpr (is_signed) {
            if (dividend.upper_half & topbit_mask) {
                qsign ^= 1;
                dividend = -dividend;
            }
            if (divisor.upper_half & topbit_mask) {
                qsign ^= 1;
                divisor = -divisor;
            }
        }

        intbase_t quot = 0;

        if (other.upper_half == 0 && this->upper_half == 0) {
            quot.lower_half = this->lower_half / other.lower_half;
        }
        else {
            // implement via repeated subtraction
            // todo: optimize this
            while (dividend >= divisor) {
                dividend -= divisor;
                ++quot;
            }
        }
        
        if (qsign) {
            quot = -quot;
        }

        return quot;
    }

    constexpr intbase_t operator%(intbase_t other) const
    {
        // todo: handle carry
        return intbase_t(this->upper_half % other.upper_half, this->lower_half % other.lower_half);
    }

    constexpr intbase_t operator-() const
    {
        halfint_t l = -this->lower_half;
        halfint_t h = -this->upper_half;
        h -= (l != 0) ? 1 : 0;
        return intbase_t(h, l);
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
        halfint_t h = this->upper_half;
        halfint_t l = this->lower_half;

        if (amount >= half_bitsize) {
            amount -= half_bitsize;
            return intbase_t(l << amount, 0);
        }

        h <<= amount;
        h |= l >> (half_bitsize - amount);
        l <<= amount;
        return intbase_t(h, l);
    }

    constexpr intbase_t operator>>(int amount) const
    {
        halfint_t mask = ((halfint_t(1) << amount) - 1);

        details::selector_t<is_signed, details::make_signed_t<halfint_t>, halfint_t> h = this->upper_half;
        halfint_t l = this->lower_half;

        if (amount >= half_bitsize) {
            amount -= half_bitsize;
            if constexpr (is_signed) {
                if (h & topbit_mask) {
                    return intbase_t(allones_mask, h >> amount);
                }
            }
            return intbase_t(0, h >> amount);
        }

        l >>= amount;
        l |= (h & mask) << (half_bitsize - amount);
        h >>= amount;

        return intbase_t(h, l);
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
        if (this->upper_half < other.upper_half)
            return true;
        if (this->upper_half > other.upper_half)
            return false;
        return this->lower_half < other.lower_half; 
    }
    constexpr bool operator<=(intbase_t other) const {
        if (this->upper_half < other.upper_half)
            return true;
        if (this->upper_half > other.upper_half)
            return false;
        return this->lower_half <= other.lower_half;
    }

    constexpr bool operator>(intbase_t other) const { return other.operator<(*this); }
    constexpr bool operator>=(intbase_t other) const { return other.operator<=(*this); }


    //
    // misc operations
    //
 
    static constexpr bool reverse_bit_scan(unsigned long *index, intbase_t value)
    {
        if constexpr (std::is_integral_v<halfint_t>) {
            if (details::reverse_bit_scan(index, value.upper_half)) {
                return index + sizeof(halfint_t);
            }
            return details::reverse_bit_scan(index, value.lower_half);
        } else {
            if (halfint_t::reverse_bit_scan(index, value.upper_half)) {
                return index + sizeof(halfint_t);
            }
            return halfint_t::reverse_bit_scan(index, value.lower_half);
        }
    }

    static constexpr intbase_t add_carry(intbase_t a, intbase_t b, uint8_t &carry)
    {
        if constexpr (std::is_integral_v<halfint_t>) {
            halfint_t lower_sum = details::add_carry(a.lower_half, b.lower_half, carry);
            halfint_t upper_sum = details::add_carry(a.upper_half, b.upper_half, carry);
            return intbase_t(upper_sum, lower_sum);
        } else {
            halfint_t lower_sum = halfint_t::add_carry(a.lower_half, b.lower_half, carry);
            halfint_t upper_sum = halfint_t::add_carry(a.upper_half, b.upper_half, carry);
            return intbase_t(upper_sum, lower_sum);
        }
    }

    static constexpr intbase_t sub_borrow(intbase_t a, intbase_t b, uint8_t &borrow)
    {
        if constexpr (std::is_integral_v<halfint_t>) {
            halfint_t lower_diff = details::sub_borrow(a.lower_half, b.lower_half, borrow);
            halfint_t upper_diff = details::sub_borrow(a.upper_half, b.upper_half, borrow);
            return intbase_t(upper_diff, lower_diff);
        } else {
            halfint_t lower_diff = halfint_t::sub_borrow(a.lower_half, b.lower_half, borrow);
            halfint_t upper_diff = halfint_t::sub_borrow(a.upper_half, b.upper_half, borrow);
            return intbase_t(upper_diff, lower_diff);
        }
    }

    static constexpr intbase_t multiply_extended(intbase_t a, intbase_t b, intbase_t &prod_hi)
    {
        int qsign = 0;

        if constexpr (is_signed) {
            if (a.upper_half & topbit_mask) {
                qsign ^= 1;
                a = -a;
            }
            if (b.upper_half & topbit_mask) {
                qsign ^= 1;
                b = -b;
            }
        }

        // we cannot use operator* because we need to compute the top-bits of
        // the multiply, so re-implement it here with that functionality.

        // use FOIL method (first-inside-outside-last)
        //   F -> lower-bits of upper-product, throw away overflow when computing this
        //   O -> upper-bits of lower-product, overflow goes to lower-bits of upper-product
        //   I -> upper-bits of lower-product, overflow goes to lower-bits of upper-product
        //   L -> lower-bits of lower-product, overflow goes to upper-bits of lower-product
        halfint_t carry_ll = 0, ll;
        halfint_t carry_lu = 0, lu;
        halfint_t carry_ul = 0, ul;
        halfint_t carry_uu = 0, uu;
        if constexpr (std::is_integral_v<halfint_t>) {
           ll = details::mul_extended(a.lower_half, b.lower_half, carry_ll);
           lu = details::mul_extended(a.lower_half, b.upper_half, carry_lu);
           ul = details::mul_extended(a.upper_half, b.lower_half, carry_ul);
           uu = details::mul_extended(a.upper_half, b.upper_half, carry_uu);
        }
        else {
           ll = halfint_t::multiply_extended(a.lower_half, b.lower_half, carry_ll);
           lu = halfint_t::multiply_extended(a.lower_half, b.upper_half, carry_lu);
           ul = halfint_t::multiply_extended(a.upper_half, b.lower_half, carry_ul);
           uu = halfint_t::multiply_extended(a.upper_half, b.upper_half, carry_uu);
        }

        uint8_t carry1 = 0;
        uint8_t carry2 = 0;

        auto prod_lo = intbase_t(carry_ll, ll);
        prod_lo = add_carry(prod_lo, intbase_t(lu, 0), carry1);
        prod_lo = add_carry(prod_lo, intbase_t(ul, 0), carry2);

        prod_hi = intbase_t(carry_uu, uu);
        prod_hi += intbase_t(0, carry_lu);
        prod_hi += intbase_t(0, carry_ul);
        prod_hi += intbase_t(0, carry1 + carry2);

        if constexpr (is_signed) {
            if (qsign) {
                prod_lo = -prod_lo;
                prod_hi = -prod_hi;
                if (prod_lo) {
                    --prod_hi;
                }
            }
        }
        else {
            (qsign);
        }

        return prod_lo;
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
        return uint128_t::reverse_bit_scan(index, mask);
    }
}


