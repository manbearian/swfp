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
    static constexpr uint_t exponent_mask = (1 << exponent_bitsize) - 1;
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
    static uint_t decrease_significand(uint_t& significand, int amount)
    {
        uint_t mask = (uint_t(1) << amount) - 1;
        uint_t shifted_out_value = significand & mask;
        significand >>= amount;

        return shifted_out_value;
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
        uint_t sign = components.sign;
        uint_t exponent = components.exponent;
        auto significand = components.significand;

        // finite, non-zero values need special care
        if ((components.exponent != exponent_mask)
            && !(components.exponent == 0 && components.significand == 0))
        {
            // underflow, decrease exponent and add 0s
            while ((significand < (uint_t(1) << (significand_bitsize + 1)))
                && (exponent != emin))
            {
                // transfer power-of-two from exponent to significand
                exponent--;
                significand <<= 1;
            }

            // check for overflow, increase exponent are remove sig-digits until appropriate
            while (significand >= (uint_t(1) << (significand_bitsize + 1)))
            {
                // transfer power-of-two from significand to exponent
                exponent++;
                if (decrease_significand(significand, 1)) {
                    significand += (significand & 1); // round-to-even
                }
            }

            // mask off implied topbit
            if (significand >= (uint_t(1) << significand_bitsize))
            {
                // normal
                assert((significand & (~((uint_t(1) << (significand_bitsize + 2)) - 1))) == 0); // no higher bits should be set
                significand &= ((uint_t(1) << significand_bitsize) - 1);

                // todo: validate exponent before adding bias
                exponent += bias;
            }
            else
            {
                // subnormal
                exponent = 0;
            }
        }

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
            return addend;
        }

        if (r.class_ == fp_class::zero) {
            return *this;
        }

        if (l.class_ == fp_class::infinity) {
            return *this;
        }

        if (r.class_ == fp_class::infinity) {
            return addend;
        }

        fp_components sum;

        uint_t roundoff_bits = 0;
        int roundoff_bitsize = 0;

        // make exponents match
        auto exponent_diff = l.exponent - r.exponent;
        if (exponent_diff > 0)
        {
            roundoff_bitsize = exponent_diff;
            r.exponent += roundoff_bitsize;
            roundoff_bits = decrease_significand(r.significand, roundoff_bitsize);
        }
        else if (exponent_diff < 0)
        {
            roundoff_bitsize = -exponent_diff;
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
                return floatbase_t{};
            }
        }
        else
        {
            sum.significand = l.significand + r.significand;

            // IEEE794 says to treat value as infinite long and then round-to-nearest
            // we know the signficand bits that cannot be represented so use them to
            // round the value we're keeping up or down
            uint_t midpoint = uint_t(1) << (roundoff_bitsize - 1);
            if (roundoff_bits > midpoint) {
                sum.significand++; // round-up
            }
            else if (roundoff_bits == midpoint) {
                sum.significand += (sum.significand & 1); // round-to-even
            }

            sum.sign = l.sign;
        }

        return compose(sum);
    }
};


using float16_t = floatbase_t<fp_format::binary16>;
using float32_t = floatbase_t<fp_format::binary32>;
using float64_t = floatbase_t<fp_format::binary64>;
