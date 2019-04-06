#include <stdint.h>
#include <memory>
#include <type_traits>
#include <string>
#include <sstream>
#include <assert.h>
#include <algorithm>

enum class fp_format
{
   bfloat16,
   binary16,
   binary32,
   binary64,
   binary128
};

template<fp_format f> struct fp_traits { };
// todo: static assert only support fp_formats are instantiated
template<> struct fp_traits<fp_format::binary16>
{
   using uint_t = uint16_t;
   using hwfp_t = void;
   static constexpr uint_t exponent_bitsize = 5;
   static constexpr uint_t bias = 15;
};
template<> struct fp_traits<fp_format::binary32>
{
   using uint_t = uint32_t;
   using hwfp_t = float;
   static constexpr uint_t exponent_bitsize = 8;
   static constexpr uint_t bias = 127;
};
template<> struct fp_traits<fp_format::binary64>
{
   using uint_t = uint64_t;
   using hwfp_t = double;
   static constexpr uint_t exponent_bitsize = 11;
   static constexpr uint_t bias = 1023;
};
#if 0
template<> struct fp_traits<fp_format::binary128>
{
   using uint_t = uint128_t;
   using hwfp_t = long double;
   static constexpr uint_t exponent_bitsize = 15;
   static constexpr uint_t bias = 16383;
#endif

template<fp_format format>
class floatbase_t
{
private:
    using fp_traits = fp_traits<format>;
public:
    using uint_t = typename fp_traits::uint_t;
    using int_t = std::make_signed_t<uint_t>;
    using hwfp_t = typename fp_traits::hwfp_t;

    static constexpr uint_t bitsize = sizeof(uint_t) * 8;
    static constexpr uint_t exponent_bitsize = fp_traits::exponent_bitsize;
    static constexpr uint_t significand_bitsize = bitsize - exponent_bitsize - 1;
    static constexpr uint_t exponent_mask = (uint_t(1) << exponent_bitsize) - 1;
    static constexpr uint_t significand_mask = (uint_t(1) << significand_bitsize) - 1;
    static constexpr int_t bias = fp_traits::bias;
    static constexpr int_t emax = bias;
    static constexpr int_t emin = 1 - emax;

private:
    uint_t raw_value;

public:

    //
    // construction
    //

    // default to 0 value
    floatbase_t() : raw_value(0) { }

    // initialize from raw bits
    explicit floatbase_t(uint_t val) {
        static_assert(sizeof(uint_t) == sizeof(*this));
        memcpy(this, &val, sizeof(uint_t));
    }
    
    // initialize from any floating-point type
    template<typename T = hwfp_t, typename = std::enable_if_t<std::is_floating_point_v<T>>>
    floatbase_t(T fval)
    {
        if constexpr(!std::is_void_v<hwfp_t>) {
            hwfp_t hwf = static_cast<hwfp_t>(fval);
            static_assert(sizeof(hwfp_t) == sizeof(*this));
            memcpy(this, &hwf, sizeof(hwfp_t));
        }
        else {
            static_assert(false, "NYI: initiailization for this type");
        }  
    }

    //
    // convert to floating hw point type
    //

public:

    template<typename T = hwfp_t, typename = std::enable_if_t<std::is_same_v<T, hwfp_t> && !std::is_void_v<hwfp_t>>>
    explicit operator hwfp_t()
    {
        hwfp_t hwf;
        static_assert(sizeof(hwfp_t) == sizeof(*this));
        memcpy(&hwf, this, sizeof(hwfp_t));
        return hwf;
    }

    //
    // printing utilites
    //

public:

    std::string to_triplet_string() 
    {
        fp_components x = decompose();
        std::stringstream ss;
        ss << "{" << (x.sign ? "-" : "+") << ", " << x.exponent << ", 0x" << std::hex << x.significand << "}";
        return ss.str();
    }

    std::string to_hex_string() {
        std::stringstream ss;
        ss << "0x" << std::hex << raw_value;
        return ss.str();
    }

    //
    // utilities
    //

private:
    
    // decrease significand by N power-of-two
    // handles underflow with flush-to-zero
    // rounds value (to-neareset)
    enum class bits_kind { zero = 0, low, mid, high };
    static bits_kind decrease_significand(uint_t& significand, int amount)
    {
        if (amount > sizeof(uint_t)*8) {
            auto result = significand == 0 ? bits_kind::zero : bits_kind::low;
            significand = 0;
            return result;
        }

        uint_t mask = (uint_t(1) << amount) - 1;
        uint_t midpoint = uint_t(1) << (amount - 1);
        uint_t shifted_out_value = significand & mask;

        significand >>= amount;

        if (shifted_out_value == 0) {
            return bits_kind::zero;
        }
        if (shifted_out_value > midpoint) {
            return bits_kind::high;
        }
        if (shifted_out_value < midpoint) {
            return bits_kind::low;
        }

        return bits_kind::mid;
    }

    static void round_signficant(uint_t &significand, bits_kind roundoff_bits)
    {
        // IEEE794 says to treat value as infinite long and then round-to-nearest
        // we know the signficand bits that cannot be represented so use them to
        // round the value we're keeping up or down
        if (roundoff_bits == bits_kind::high) {
            significand++; // round-up
        }
        else if (roundoff_bits == bits_kind::mid) {
            significand += (significand & 1); // round-to-even
        }
    }

    // number of bits significand needs to shift to get implied-one
    // into the right position. Positive means left shift.
    static int32_t signficand_adjustment(uint_t significand)
    {
        // index of the implied leading `1.`
        constexpr unsigned long keybit = significand_bitsize;

        unsigned long index;
        if constexpr (sizeof(uint_t) <= 4) {
            if (!_BitScanReverse(&index, significand)) {
                return keybit;
            }
        }
        else if constexpr (sizeof(uint_t) == 8) {
            if (!_BitScanReverse64(&index, significand)) {
                return keybit;
            }
        }
        else {
            static_assert(false, "NYI: signficand_adjustment for this size");
        }

        return keybit - index;
    }

    // increase the exponent value and report if overflow occured
    static bool increase_exponent(int32_t& exponent, uint_t amount)
    {
        exponent += amount;

        if (exponent > emax) {
            exponent = exponent_mask;
            return true;
        }

        return false;
    }

    // decrease the exponent value and report if underflow occure
    // if return value is non-zero underflow occured--the value
    // returned is the amount of the underflow.
    static int decrease_exponent(int32_t& exponent, uint_t amount)
    {
        exponent -= amount;

        int diff = exponent - emin;

        if (diff < 0) {
            exponent = 0;
            return -diff;
        }

        return 0;
    }

    //
    // decompisition, composition, classification
    //

private:

    enum class fp_class { nan, infinity, zero, normal, denormal };
    struct fp_components { 
        fp_class class_;
        uint8_t sign;
        int32_t exponent;   // save some space for large fp (binary128 has only 15 bits)
        uint_t significand;
    };

    fp_components decompose()
    {
        fp_components components;
        components.class_ = fp_class::nan;
        components.sign = raw_value >> (bitsize - 1);
        components.exponent = (raw_value & ~(1 << (bitsize - 1))) >> significand_bitsize;
        components.significand = (raw_value & ((1 << significand_bitsize) - 1));

        if (components.exponent == 0)
        {
            if (components.significand == 0)
            {
                components.class_ = fp_class::zero;
            }
            else
            {
                components.exponent = (uint_t)emin;
                components.class_ = fp_class::denormal;
            }
        }
        else if (components.exponent == exponent_mask)
        {
            if (components.significand == 0)
            {
                components.class_ = fp_class::infinity;
            }
            else
            {
                components.class_ = fp_class::nan;
            }
        }
        else
        {
            components.exponent -= bias;
            components.class_ = fp_class::normal;
            components.significand |= (1 << significand_bitsize);
        }

        return components;
    }

    static floatbase_t compose(const fp_components &components)
    {
        auto sign = components.sign;
        auto exponent = components.exponent;
        auto significand = components.significand;

        // finite, non-zero values need special care
        if ((components.exponent != exponent_mask)
            && !(components.exponent == 0 && components.significand == 0))
        {
            int distance = signficand_adjustment(significand);
            bool is_normal = true;

            if (distance > 0)
            {
                // underflow--decrease exponent and fill signficand with 0s
                if (int underflow_amount = decrease_exponent(exponent, distance))
                {
                    // underflow in exponent -> change to denormal
                    significand <<= distance - underflow_amount;
                    is_normal = false;
                }
                else
                {
                    significand <<= distance;
                }
            }
            else if (distance < 0)
            {
                // overflow--increase exponent and round signficand
                auto roundoff_bitsize = -distance;
                if (increase_exponent(exponent, roundoff_bitsize))
                {
                    // overflow in exponent -> change to infinity
                    significand = 0;
                    is_normal = false;
                }
                else
                {
                    auto roundoff_bits = decrease_significand(significand, roundoff_bitsize);
                    round_signficant(significand, roundoff_bits);
                }
            }

            if (is_normal)
            {
                // mask off implied leading 1.
                assert((~significand_mask & significand) == (significand_mask+1)); // only implied bit set outside of significand
                significand &= significand_mask;

                // encode exponent with bias
                exponent += bias;
            }
        }

        assert(sign == 1 || sign == 0);
        assert((significand & significand_mask) == significand);
        assert(exponent >= 0 && exponent <= exponent_mask);
        return floatbase_t{ (sign << (bitsize - 1)) | (exponent << significand_bitsize) | significand };
    }

    //
    // arithmetic
    //

public:

    floatbase_t operator+(floatbase_t addend)
    {
        fp_components l = this->decompose();
        fp_components r = addend.decompose();

        if (l.class_ == fp_class::zero) {
            if (r.class_ == fp_class::zero && (r.sign != l.sign)) {
                return floatbase_t{};
            }
            return addend;
        }
        else if (r.class_ == fp_class::zero) {
            return *this;
        }

        if (l.class_ == fp_class::nan) {
            return *this;
        }
        else if (r.class_ == fp_class::nan) {
            return addend;
        }

        if (l.class_ == fp_class::infinity) {
            if (r.class_ == fp_class::infinity && (r.sign != l.sign)) {
                return floatbase_t{ (1 << (bitsize - 1)) | (exponent_mask << significand_bitsize) | 0x400000 };
            }
            return *this;
        }
        else if (r.class_ == fp_class::infinity) {
            return addend;
        }

        fp_components sum;

        bits_kind roundoff_bits = bits_kind::zero;

        // make exponents match
        auto exponent_diff = l.exponent - r.exponent;
        if (exponent_diff > 0)
        {
            auto roundoff_bitsize = exponent_diff;
            r.exponent += roundoff_bitsize;
            roundoff_bits = decrease_significand(r.significand, roundoff_bitsize);
        }
        else if (exponent_diff < 0)
        {
            auto roundoff_bitsize = -exponent_diff;
            l.exponent += roundoff_bitsize;
            roundoff_bits = decrease_significand(l.significand, roundoff_bitsize);
        }

        sum.exponent = l.exponent;

        if (l.sign != r.sign)
        {
            if (l.significand > r.significand)
            {
                sum.significand = l.significand - r.significand;
                sum.sign = l.sign;

            }
            else if (l.significand < r.significand)
            {
                sum.significand = r.significand - l.significand;
                sum.sign = r.sign;
            }
            else
            {
                // a - a => 0
                return floatbase_t{ 0 };
            }

            if (roundoff_bits != bits_kind::zero)
            {
                // we have round-off bits in the subtrahend
                // subtracting out these bits would cause a borrow, so simulate this by subtracing 1
                sum.significand--;

                // invert the reaminder bits
                if (roundoff_bits == bits_kind::high)
                    roundoff_bits = bits_kind::low;
                else if (roundoff_bits == bits_kind::low)
                    roundoff_bits = bits_kind::high;

                if ((sum.significand & (significand_mask + 1)) == 0) {
                    // if we subtracted off the leading one, shift over and fill in the LSB
                    // todo: move this into compose?
                    sum.significand <<= 1;
                    if (roundoff_bits == bits_kind::mid || roundoff_bits == bits_kind::high) {
                        sum.significand |= 1;
                    }
                    sum.exponent--;
                }
                else {
                    // round the value
                    round_signficant(sum.significand, roundoff_bits);
                }
            }
        }
        else
        {
            sum.significand = l.significand + r.significand;
            round_signficant(sum.significand, roundoff_bits);
            sum.sign = l.sign;
        }

        return compose(sum);
    }
};


using float16_t = floatbase_t<fp_format::binary16>;
using float32_t = floatbase_t<fp_format::binary32>;
using float64_t = floatbase_t<fp_format::binary64>;
