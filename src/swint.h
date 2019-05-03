
#pragma once

#include <stdint.h>
#include <cstddef>

#include "swhelp.h"

#if !defined(UINT128_MAX) // detect existence of HW 128-bit integer types
#define USE_SW_INT128 1
#endif

// forward declare
template<size_t byte_size, bool is_signed> class intbase_t;

// define software implementation of integral typess
namespace details
{

template<size_t byte_size> struct int_traits { using halfint_t = typename intbase_t<byte_size / 2, false>; };
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
        static_assert(false, "larger add_carry not implemented");
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

#pragma optimize("", off)
__declspec(noreturn) __declspec(noinline) void divide_by_zero() {

    static volatile int divide_by_zero_global = 0;
    divide_by_zero_global = 100 / divide_by_zero_global;
}
#pragma optimize("", on)

}


template<size_t byte_size, bool is_signed>
class intbase_t
{
    static_assert(byte_size > 1, "expecting 2 bytes or more");
    static_assert(details::is_pow_2(byte_size), "expecting power of 2");

public:

    static constexpr bool is_signed = is_signed;

private:
    using signed_t = intbase_t<byte_size, true>;
    using unsigned_t = intbase_t<byte_size, false>;
    using halfint_t = typename details::int_traits<byte_size>::halfint_t;
    using shalfint_t = details::selector_t<std::is_integral_v<halfint_t>,
        std::make_signed_t<halfint_t>,
        intbase_t<sizeof(halfint_t), true>
    >;

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
    constexpr explicit intbase_t(integral_t val) : upper_half(0), lower_half(0) {
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
    // limits
    //

public:

    static constexpr intbase_t max() {
        constexpr halfint_t allones = static_cast<halfint_t>(~halfint_t(0));
        if constexpr (is_signed) {
            return intbase_t(allones >> 1, allones);
        }
        else {
            return intbase_t(allones, allones);
        }
    }

    static constexpr intbase_t min() {
        if constexpr (is_signed) {
            return intbase_t(static_cast<halfint_t>(halfint_t(1) << (half_bitsize - 1)), 0);
        }
        else {
            return intbase_t(0);
        }
    }

    //
    // conversion
    //

public:

    // combine template for both integral and bool to avoid compiler prefering
    // non-template version over template verion if non-template operator bool.
    template<typename integral_t,
        typename = std::enable_if_t<
            std::is_integral_v<integral_t> || std::is_same_v<integral_t, bool>
    >>
    constexpr explicit operator integral_t() const
    {
        if constexpr (std::is_same_v<integral_t, bool>) {
            return this->upper_half || this->lower_half;
        }
        else if constexpr (sizeof(integral_t) == sizeof(intbase_t)) {
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

private:

    struct div_t { intbase_t quot; intbase_t rem; };
    static constexpr div_t div(intbase_t dividend, intbase_t divisor)
    {
        div_t result{};

        if (divisor == intbase_t(0)) {
            // handle divide-by-zero
            details::divide_by_zero();
        }

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

        if (dividend.upper_half == 0 && divisor.upper_half == 0) {
            result.quot.lower_half = dividend.lower_half / divisor.lower_half;
            result.rem.lower_half = dividend.lower_half % divisor.lower_half;
        }
        else {

#if 0
            // Q = N / D, R = remainder

            halfint_t q = halfint_t(0), r = halfint_t(0);

            int bit = bitsize - 1;
            for (; bit >= bit / 2; --bit)
            {
                r <<= 1;
                r |= (dividend.lower_half & (halfint_t(1) << bit)) >> bit;
                if (r >= divisor) {
                    r -= divisor;
                    q|= (halfint_t(1) << bit);
                }

            }
#elif 1
            int bit = bitsize - 1;
            for (; bit >= 0; --bit)
            {
                result.rem <<= 1;
                result.rem |= (dividend & (intbase_t(1) << bit)) >> bit;
                if (result.rem >= divisor) {
                    result.rem -= divisor;
                    result.quot |= (intbase_t(1) << bit);
                }
            }

#else
            // implement via repeated subtraction
            // todo: optimize this
            while (dividend >= divisor) {
                dividend -= divisor;
                ++result.quot;
            }
#endif
        }
        

        if constexpr (is_signed) {
            if (qsign) {
                result.quot = -result.quot;
            }
        }
        else {
            (qsign);
        }

        return result;
    }

public:

    constexpr intbase_t operator/(intbase_t other) const
    {
        auto result = div(*this, other);
        return result.quot;
    }

    constexpr intbase_t operator%(intbase_t other) const
    {
        // todo: handle carry
        return intbase_t(this->upper_half % other.upper_half, this->lower_half % other.lower_half);
    }

    constexpr intbase_t operator-() const
    {
        halfint_t l = static_cast<halfint_t>(-static_cast<shalfint_t>(this->lower_half));
        halfint_t h = static_cast<halfint_t>(-static_cast<shalfint_t>(this->upper_half));
        h = static_cast<halfint_t>(h - ((l != 0) ? 1 : 0));
        return intbase_t(h, l);
    }

    //
    // increment
    //
public:
    constexpr intbase_t operator--() {
        auto t = *this;
        *this = this->operator-(intbase_t(1));
        return t;
    }
    constexpr intbase_t operator--(int) {
        *this = this->operator-(intbase_t(1));
        return *this;
    }
    constexpr intbase_t operator++() {
        auto t = *this;
        *this = this->operator+(intbase_t(1));
        return t;
    }
    constexpr intbase_t operator++(int) {
        *this = this->operator+(intbase_t(1));
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
        *this = this->operator*(other);
        return *this;
    }
    constexpr intbase_t operator/=(intbase_t other) {
        *this = this->operator/(other);
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
        if constexpr (sizeof(intbase_t) <= sizeof(uint64_t))
        {
            using inthw_t = details::make_integral_t<byte_size, false>;
            using shift_type_t = details::selector_t<(byte_size < sizeof(int32_t)), uint32_t, inthw_t>;

            shift_type_t all = static_cast<inthw_t>(*this);
            all <<= amount;
            return static_cast<intbase_t>(all);
        }
        else if constexpr (sizeof(intbase_t) == 16)
        {
            if ((amount & 63) != amount) {
                return intbase_t(this->lower_half << (amount - 64), 0);
            }

            intbase_t out;
            out.lower_half = this->lower_half << amount;
            out.upper_half = __shiftleft128(this->lower_half, this->upper_half, static_cast<unsigned char>(amount));
            return out;
        }
        else
        {
            // geneal purpose left shift
            // todo: hand optimize for larger shifts as optimzer does a bad job of this

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
    }

    constexpr intbase_t operator>>(int amount) const
    {
        if constexpr (sizeof(intbase_t) <= sizeof(uint64_t))
        {
            using inthw_t = details::make_integral_t<byte_size, is_signed>;
            using shift_type_t = details::selector_t<(byte_size < sizeof(int32_t)), int32_t, inthw_t>;

            shift_type_t all = static_cast<inthw_t>(*this);
            all >>= amount;
            return static_cast<intbase_t>(all);
        }
        else if constexpr (sizeof(intbase_t) == 16)
        {
            using shift_type_t = details::selector_t<is_signed, shalfint_t, halfint_t>;
            if ((amount & 63) != amount) {
                halfint_t top_half = 0;
                if constexpr (is_signed) {
                    if (this->upper_half & topbit_mask) {
                        top_half = allones_mask;
                    }
                }

                return intbase_t(top_half, static_cast<shift_type_t>(this->upper_half) >> amount);
            }

            intbase_t out;
            out.upper_half = static_cast<shift_type_t>(this->upper_half) >> amount;
            out.lower_half = __shiftright128(this->lower_half, this->upper_half, static_cast<unsigned char>(amount));
            return out;
        }
        else {
            halfint_t mask = ((halfint_t(1) << amount) - 1);

            details::selector_t<is_signed, shalfint_t, halfint_t> h = this->upper_half;
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

    constexpr bool operator!() const {
        return !this->upper_half && !this->lower_half;
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

    
    //
    // to string
    //

 public:

    static std::string to_string(intbase_t sw) {
        if (!sw.upper_half) {
            if constexpr (std::is_integral_v<halfint_t>) {
                return std::to_string(sw.lower_half);
            }
            else {
                return halfint_t::to_string(sw.upper_half);
            }
        }

        std::string s;
        while (sw)
        {
            //intbase_t digit = sw % intbase_t(10);
            sw /= intbase_t(10);
            s += '0' + 0;// (char)digit;
        }

        return s;
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


//
// literals
//

#define MAKE_LITERAL_OPERATOR(swtype, name)                                 \
    inline constexpr swtype operator "" name(unsigned long long int val) {  \
        if (val > static_cast<unsigned long long int>(swtype::max()))       \
            throw std::exception("literal out of range");                   \
        return swtype(val);                                                 \
    }                                                                       \

MAKE_LITERAL_OPERATOR(int16sw_t, _i16sw)
MAKE_LITERAL_OPERATOR(int32sw_t, _i32sw)
MAKE_LITERAL_OPERATOR(int64sw_t, _i64sw)
MAKE_LITERAL_OPERATOR(uint16sw_t, _u16sw)
MAKE_LITERAL_OPERATOR(uint32sw_t, _u32sw)
MAKE_LITERAL_OPERATOR(uint64sw_t, _u64sw)

#undef MAKE_LITERAL_OPERATOR

inline int128sw_t operator "" _i128sw(const char * val)
{
    // cannot use strlen because its not constexpr???
    size_t len = 0;
    for (auto p = val; *p; ++p, ++len);

    int128sw_t out = int128sw_t(0);

    // non-decimal
    if (val[0] == '0')
    {
        if (val[1] == 'x' || val[1] == 'X')
        {
            len -= 2;
            val += 2;

            if (len > (sizeof(int128sw_t) * 2)) {
                throw std::exception("literal out of range");
            }

            for (int i = 0; i < len; ++i)
            {
                char c = val[len - 1 - i];
                if (c >= '1' && c <= '9') {
                    out |= (static_cast<int128sw_t>(c - '0')) << (i * 4);
                }
                else if (c >= 'a' && c <= 'f') {
                    out |= (static_cast<int128sw_t>(10 + c - 'a')) << (i * 4);
                }
                else if (c >= 'A' && c <= 'F') {
                    out |= (static_cast<int128sw_t>(10 + c - 'A')) << (i * 4);
                }
                else if (c != '0') {
                    throw std::exception("invalid hexadecimal literal");
                }
            }
        }
        else if (val[1] == 'b' || val[1] == 'B')
        {
            len -= 2;
            val += 2;

            if (len > (sizeof(int128sw_t) * 8)) {
                throw std::exception("literal out of range");
            }

            for (int i = 0; i < len; ++i)
            {
                char c = val[len - 1 - i];
                if (c == '1') {
                    out |= int128sw_t(1) << i;
                }
                else if (c != '0') {
                    throw std::exception("invalid binary literal");
                }
            }
        }
        else
        {
            len -= 1;
            val += 1;

            if (len > (sizeof(int128sw_t) * 3)) {
                throw std::exception("literal out of range");
            }

            for (int i = 0; i < len; ++i)
            {
                char c = val[len - 1 - i];
                if (c >= '1' && c <= '7') {
                    out |= (static_cast<int128sw_t>(c - '0')) << (i * 3);
                }
                else if (c != '0') {
                    throw std::exception("invalid octal literal");
                }
            }
        }
    }
    else
    {
        int128sw_t j = int128sw_t(1);
        for (int i = 0; i < len; ++i, j *= int128sw_t(10))
        {
            char c = val[len - 1 - i];
            if (c >= '1' && c <= '9') {
                uint8_t carry = 0;
                out = int128sw_t::add_carry(out, static_cast<int128sw_t>(c - '0') * j, carry);
                if (carry) {
                    throw std::exception("literal out of range");
                }
            }
            else if (c != '0') {
                throw std::exception("invalid decimal literal");
            }
        }
    }

    if (out > int128sw_t::max()) {
        throw std::exception("literal out of range");
    }

    return out;
}                                                                     

inline std::string to_string(int16sw_t sw) { return std::to_string(static_cast<int16_t>(sw)); }
inline std::string to_string(int32sw_t sw) { return std::to_string(static_cast<int32_t>(sw)); }
inline std::string to_string(int64sw_t sw) { return std::to_string(static_cast<int64_t>(sw)); }
inline std::string to_string(uint16sw_t sw) { return std::to_string(static_cast<uint16_t>(sw)); }
inline std::string to_string(uint32sw_t sw) { return std::to_string(static_cast<uint32_t>(sw)); }
inline std::string to_string(uint64sw_t sw) { return std::to_string(static_cast<uint64_t>(sw)); }
inline std::string to_string(int128sw_t sw)
{
    return int128sw_t::to_string(sw);
}
inline std::string to_string(uint128sw_t)
{
    return "err";
}

inline std::wstring to_wstring(int16sw_t sw) { return std::to_wstring(static_cast<int16_t>(sw)); }
inline std::wstring to_wstring(int32sw_t sw) { return std::to_wstring(static_cast<int32_t>(sw)); }
inline std::wstring to_wstring(int64sw_t sw) { return std::to_wstring(static_cast<int64_t>(sw)); }
inline std::wstring to_wstring(uint16sw_t sw) { return std::to_wstring(static_cast<uint16_t>(sw)); }
inline std::wstring to_wstring(uint32sw_t sw) { return std::to_wstring(static_cast<uint32_t>(sw)); }
inline std::wstring to_wstring(uint64sw_t sw) { return std::to_wstring(static_cast<uint64_t>(sw)); }
inline std::wstring to_wstring(int128sw_t)
{
    return L"err";
}
inline std::wstring to_wstring(uint128sw_t)
{
    return L"err";
}

#if USE_SW_INT128 // detect existence of HW 128-bit integer types

using int128_t = intbase_t<16, true>;
using uint128_t = intbase_t<16, false>;

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


