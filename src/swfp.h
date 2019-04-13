#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <limits>
#include <type_traits>
#include <string>
#include <sstream>
#include <assert.h>
#include <algorithm>
#include <intrin.h>

// Emulate IA32/IA32-64 behavior when outside the bounds of IEEE754
#define EMULATE_INTEL 1


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
   static constexpr uint_t exponent_bitsize = 5;
   static constexpr uint_t bias = 15;
};
template<> struct fp_traits<fp_format::binary32>
{
   using uint_t = uint32_t;
   static constexpr uint_t exponent_bitsize = 8;
   static constexpr uint_t bias = 127;
};
template<> struct fp_traits<fp_format::binary64>
{
   using uint_t = uint64_t;
   static constexpr uint_t exponent_bitsize = 11;
   static constexpr uint_t bias = 1023;
};
#if 0
template<> struct fp_traits<fp_format::binary128>
{
   using uint_t = uint128_t;
   static constexpr uint_t exponent_bitsize = 15;
   static constexpr uint_t bias = 16383;
#endif

namespace details {
    // select T if true, U if false
    template<bool, typename T, typename U> struct selector;
    template <typename T, typename U> struct selector<true, T, U> { using type = T; };

    template<bool, typename T, typename U> struct mint32_t;
    template <typename T, typename U> struct selector<false, T, U> { using type = U; };

    template<bool a, typename T, typename U> using selector_t = typename selector<a, T, U>::type;

    template<typename integral_t, typename = std::enable_if_t<std::is_integral_v<integral_t>>>
    static bool bit_scan(unsigned long *index, integral_t mask)
    {
        if constexpr (sizeof(integral_t) <= 4) {
            return _BitScanReverse(index, static_cast<std::make_unsigned_t<integral_t>>(mask));
        }
        else if constexpr (sizeof(integral_t) == 8) {
            return _BitScanReverse64(index, mask);
        }
        else {
            static_assert(false, "NYI: significand_adjustment for this size");
            return false;
        }
    }
}

template<fp_format format>
class floatbase_t
{
private:
    using fp_traits = fp_traits<format>;

    using uint_t = typename fp_traits::uint_t;
    using int_t = std::make_signed_t<uint_t>;

    using exponent_t = int32_t; // use signed ints for performance for exponent
    using uexponent_t = std::make_unsigned_t<exponent_t>;

    static constexpr uint_t bitsize = sizeof(uint_t) * 8;
    static constexpr uint_t exponent_bitsize = fp_traits::exponent_bitsize;
    static constexpr uint_t significand_bitsize = bitsize - exponent_bitsize - 1;
    static constexpr uint_t exponent_mask = (uint_t(1) << exponent_bitsize) - 1;
    static constexpr uint_t significand_mask = (uint_t(1) << significand_bitsize) - 1;
    static constexpr uint_t sign_mask = uint_t(1) << (bitsize - 1);
    static constexpr exponent_t bias = fp_traits::bias;
    static constexpr exponent_t emax = bias;
    static constexpr exponent_t emin = 1 - emax;

    friend floatbase_t<fp_format::binary16>;
    friend floatbase_t<fp_format::binary32>;
    friend floatbase_t<fp_format::binary64>;
    friend floatbase_t<fp_format::binary128>;

private:

    uint_t raw_value;


    //
    // construction
    //

public:

    // default is uninit, just like built-in FP types
    floatbase_t() = default;

    // construct from 32-bit built-in floating-point type
    explicit constexpr floatbase_t(float hwf)
    {
        if constexpr (format == fp_format::binary16)
        {
            *this = float16_t(float32_t(hwf));
        }
        else if constexpr (format == fp_format::binary32)
        {
#if BIT_CAST_EXISTS
            *this = std::bit_cast<uint_t>(hwf);
#else
            memcpy(this, &hwf, sizeof(float));
#endif
        }
        else if constexpr (format == fp_format::binary64)
        {
            *this = float64_t(float32_t(hwf));
        }
        else
        {
            static_assert(false, "nyi: convert from hw float32");
        }
    }

    // construct from 64-bit built-in floating-point type
    explicit constexpr floatbase_t(double hwf)
    {
        if constexpr (format == fp_format::binary16)
        {
            *this = float16_t(float64_t(hwf));
        }
        else if constexpr (format == fp_format::binary32)
        {
            *this = float32_t(float64_t(hwf));
        }
        else if constexpr (format == fp_format::binary64)
        {
#if BIT_CAST_EXISTS
            *this = std::bit_cast<uint_t>(hwf);
#else
            memcpy(this, &hwf, sizeof(double));
#endif
        }
        else
        {
            static_assert(false, "nyi: convert from hw float64");
        }
    }

    // construct from built-in ingegral type
    template<typename integral_t, typename = std::enable_if_t<std::is_integral_v<integral_t>>>
    explicit constexpr floatbase_t(integral_t t)
    {
        using uintegral_t = std::make_unsigned_t<integral_t>;
        using intermediate_t = details::selector_t<(sizeof(uintegral_t) > sizeof(uint_t)), uintegral_t, uint_t>;

        if (t == 0) {
            raw_value = 0;
            return;
        }

        bool sign = false;
        intermediate_t intermediate_value;
        if constexpr (std::is_signed_v<integral_t>)
        {
            intermediate_value = static_cast<intermediate_t>(static_cast<uintegral_t>(abs(t)));
            sign = t < 0;
        }
        else
        {
            intermediate_value = static_cast<intermediate_t>(static_cast<uintegral_t>(t));
        }

        unsigned long index;
        if (!details::bit_scan(&index, intermediate_value))
        {
            assert(false); // 0 value checked earlier
        }

        exponent_t exponent = index;
        if (exponent > emax) {
            raw_value = floatbase_t::infinity(sign).raw_value;
            return;
        }

        uint_t signficand = static_cast<uint_t>(intermediate_value);
        int bitdiff = significand_bitsize - index;

        if (bitdiff < 0)
        {
            bitdiff = -bitdiff;
            intermediate_t intermediate_roundoff_bits = (intermediate_value & ((intermediate_t(1) << bitdiff) - 1)) << ((sizeof(intermediate_t) * 8) - bitdiff);
            if constexpr (sizeof(intermediate_t) > sizeof(uint_t)) {
                intermediate_roundoff_bits >>= (sizeof(intermediate_t) - sizeof(uint_t)) * 8;
            }
            uint_t roundoff_bits = static_cast<uint_t>(intermediate_roundoff_bits);
            signficand = static_cast<uint_t>(intermediate_value >> bitdiff);
            if (!round_significand(signficand, exponent, roundoff_bits)) {
                raw_value = floatbase_t::infinity(sign).raw_value;
                return;
            }
        }
        else
        {
            signficand <<= bitdiff;
        }

        raw_value = floatbase_t::normal(sign, exponent, signficand).raw_value;
    }

private:

    constexpr floatbase_t(uint_t sign, uint_t exponent, uint_t significand) :
        raw_value((sign << (bitsize - 1)) | (exponent << significand_bitsize) | significand)
    {
        assert((sign & 1) == sign);
        assert((significand & significand_mask) == significand);
        assert((exponent & exponent_mask) == exponent);
    }

    constexpr floatbase_t(uint_t sign, exponent_t exponent, uint_t significand) :
        floatbase_t(sign, static_cast<uint_t>(static_cast<uexponent_t>(exponent)), significand)
    { }

    //
    // convert to integral type
    //

#if defined(EMULATE_INTEL)
    template<typename integral_t, typename = std::enable_if_t<std::is_integral_v<integral_t>>>
    constexpr integral_t intel_bad_value()
    {
        if constexpr (sizeof(integral_t) < sizeof(int))
            return 0;
        else if constexpr (std::is_unsigned_v<integral_t> && (sizeof(integral_t) == sizeof(int)))
            return 0;
        else
            return std::numeric_limits<std::make_signed_t<integral_t>>::min();
    }
#endif

public:

    template<typename integral_t, typename = std::enable_if_t<std::is_integral_v<integral_t>>>
    explicit operator integral_t()
    {
        using intermediate_t = details::selector_t<(sizeof(int_t) > sizeof(integral_t)), int_t, std::make_signed_t<integral_t>>;

        fp_components components = decompose();

        switch (components.class_)
        {
        case fp_class::infinity:
            if constexpr (EMULATE_INTEL) {
                return intel_bad_value<integral_t>();
            }
            else {
                return components.sign ? std::numeric_limits<integral_t>::min() : std::numeric_limits<integral_t>::max();
            }
        case fp_class::nan:
            if constexpr (EMULATE_INTEL) {
                return intel_bad_value<integral_t>();
            }
            else {
                return 0;
            }
        case fp_class::zero:
        case fp_class::subnormal:
            return 0;
        }

        if (components.exponent < 0) {
            return 0;
        }

        intermediate_t value = static_cast<intermediate_t>(components.significand);

        int bitshift = significand_bitsize - components.exponent;

        if (bitshift > 0)
        {
            if (bitshift > (sizeof(intermediate_t) * 8)) {
                if constexpr (EMULATE_INTEL) {
                    return intel_bad_value<integral_t>();
                }
                else {
                    return 0;
                }
            }

            value >>= bitshift;
        }
        else if (bitshift < 0)
        {
            if (-bitshift > (sizeof(intermediate_t) * 8)) {
                if constexpr (EMULATE_INTEL) {
                    return intel_bad_value<integral_t>();
                }
                else {
                    return components.sign ? std::numeric_limits<integral_t>::min() : std::numeric_limits<integral_t>::max();
                }
            }

            value <<= -bitshift;
        }

        return components.sign ? static_cast<integral_t>(-value) : static_cast<integral_t>(value);
    }

    //
    // convert to floating hw point type
    //

public:

    explicit operator float()
    {
        if constexpr (format == fp_format::binary32)
        {
            float hwf;
            memcpy(&hwf, this, sizeof(float));
            return hwf;
        }
        else
        {
            return float(float32_t(*this));
        }
    }

    explicit operator double()
    {
        if constexpr (format == fp_format::binary64)
        {
            double hwf;
            memcpy(&hwf, this, sizeof(double));
            return hwf;
        }
        else
        {
            return double(float64_t(*this));
        }
    }

    template<typename T> typename T::uint_t widen_significand(uint_t significand) {
        T::uint_t wide_significand = significand;
        return wide_significand << (T::significand_bitsize - significand_bitsize);
    }

    template<typename widefp_t> widefp_t to_widefp()
    {
        constexpr int significand_bitdiff = widefp_t::significand_bitsize - significand_bitsize;

        uint8_t sign = raw_value >> (bitsize - 1);
        exponent_t exponent = (raw_value >> significand_bitsize) & exponent_mask;
        uint_t narrow_significand = raw_value & significand_mask;

        // special values
        if (exponent == exponent_mask) {
            if (narrow_significand == 0) {
                return widefp_t::infinity(sign);
            }
            // preserve NaN-payload
            return widefp_t{ sign, widefp_t::exponent_mask, static_cast<widefp_t::uint_t>(narrow_significand) << significand_bitdiff };
        }

        if (exponent == 0) {
            if (narrow_significand == 0) {
                return widefp_t::zero(sign);
            }

            // subnormals of smaller type will become normals of the larger type
            exponent = emin;
            int distance = significand_adjustment(narrow_significand);
            assert(distance > 0);
            narrow_significand <<= distance;
            narrow_significand &= significand_mask;
            exponent -= distance; // cannot underflow
        }
        else {
            exponent -= bias;
        }

        widefp_t::uint_t wide_significand = narrow_significand;
        wide_significand <<= significand_bitdiff;

        exponent += widefp_t::bias;

        return widefp_t{ sign, exponent, wide_significand };
    }

    template<typename narrowfp_t> narrowfp_t to_narrowfp()
    {
        constexpr int significand_bitdiff = significand_bitsize - narrowfp_t::significand_bitsize;
        constexpr uint_t mask = (uint_t(1) << significand_bitdiff) - 1;

        uint8_t sign = raw_value >> (bitsize - 1);
        exponent_t exponent = (raw_value >> significand_bitsize) & exponent_mask;
        uint_t wide_significand = raw_value & significand_mask;

        if (exponent == 0) {
            // subnormals round to 0
            return narrowfp_t::zero(sign);
        }
        else if (exponent == exponent_mask) {
            // special values
            if (wide_significand == 0) {
                return narrowfp_t::infinity(sign);
            }
            // preserve NaN-payload
            return narrowfp_t{ sign, narrowfp_t::exponent_mask, static_cast<narrowfp_t::uint_t>(wide_significand >> significand_bitdiff) };
        }

        exponent -= bias;
        wide_significand |= (uint_t(1) << significand_bitsize);

        narrowfp_t::uint_t narrow_significand = static_cast<narrowfp_t::uint_t>(wide_significand >> significand_bitdiff);
        narrowfp_t::uint_t roundoff_bits = static_cast<narrowfp_t::uint_t>((wide_significand & mask) << (narrowfp_t::bitsize - significand_bitdiff));

        // if the exponent is outside the range of the narrower FP
        // type see if it could be a denomral of tha narrrow FP type otherwise its zero
        if (exponent < narrowfp_t::emin) {
            while (exponent < narrowfp_t::emin) {
                ++exponent;
                roundoff_bits >>= 1;
                roundoff_bits |= (narrow_significand & 1) << (narrowfp_t::bitsize - 1);
                narrow_significand >>= 1;

                if (narrow_significand == 0 && roundoff_bits == 0) {
                    return narrowfp_t::zero(sign);
                }
            }

            if (narrowfp_t::round_subnormal_significand(narrow_significand, roundoff_bits)) {
                return narrowfp_t::subnormal(sign, narrow_significand);
            }
        }
        else if (exponent > narrowfp_t::emax) {
            return narrowfp_t::infinity(sign);
        }
        else if (!narrowfp_t::round_significand(narrow_significand, exponent, roundoff_bits)) {
            return narrowfp_t::infinity(sign);
        }

        return narrowfp_t::normal(sign, exponent, narrow_significand);
    }

    explicit operator floatbase_t<fp_format::binary16>()
    {
        static_assert(format != fp_format::binary16, "convert from T to T is impossible");

        // convert from bigger FP to smaller FP
        if constexpr (format == fp_format::binary32
            || format == fp_format::binary64
            || format == fp_format::binary128)
        {
            return to_narrowfp<float16_t>();
        }
        else
        {
            static_assert(false, "nyi: convert to hw float16");
            return float16_t::indeterminate_nan();
        }
    }

    explicit operator floatbase_t<fp_format::binary32>()
    {
        static_assert(format != fp_format::binary32, "convert from T to T is impossible");

        // convert smaller FP to bigger FP
        if constexpr (format == fp_format::binary16)
        {
            return to_widefp<float32_t>();
        }
        else if constexpr ((format == fp_format::binary64)
            || (format == fp_format::binary128))
        {
            return to_narrowfp<float32_t>();
        }
        else
        {
            static_assert(false, "nyi: convert to hw float32");
            return float32_t::indeterminate_nan();
        }
    }

    explicit operator floatbase_t<fp_format::binary64>()
    {
        static_assert(format != fp_format::binary64, "convert from T to T is impossible");

        if constexpr (format == fp_format::binary128)
        {
            return to_narrowfp<float64_t>();
        }
        else if constexpr (format == fp_format::binary16
            || format == fp_format::binary32)
        {
            return to_widefp<float64_t>();
        }
        else
        {
            static_assert(false, "nyi: convert to hw float64");
            return float64_t::indeterminate_nan();
        }
    }

#if 0
    explicit operator floatbase_t<fp_format::binary128>()
    {
        static_assert(format != fp_format::binary128, "convert from T to T is impossible");

        return to_widefp<float128_t>();
    }
#endif


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

    static uint_t decrease_significand(uint_t& significand, int amount)
    {
        auto shiftout_bits = significand;

        // handle shift-out case
        if (amount >= bitsize)
        {
            significand = 0;
            amount -= bitsize;
            if (amount >= bitsize) {
                return 0;
            }
            return shiftout_bits >> amount;
        }
        else {
            significand >>= amount;
        }

        uint_t mask = (uint_t(1) << amount) - 1;
        shiftout_bits &= mask;
        shiftout_bits <<= (bitsize - amount);
        return shiftout_bits;
    }

    // round significand appropriately

    static void round_significand_core(uint_t &significand, uint_t roundoff_bits)
    {
        constexpr uint_t midpoint = uint_t(1) << (bitsize - 1);

        // IEEE794 says to treat value as infinite long and then round-to-nearest
        // we know the significand bits that cannot be represented so use them to
        // round the value we're keeping up or down
        if (roundoff_bits > midpoint) {
            significand++; // round-up
        }
        else if (roundoff_bits == midpoint) {
            significand += (significand & 1); // round-to-even
        }
    }

    static bool round_significand(uint_t &significand, exponent_t& exponent, uint_t roundoff_bits)
    {
        assert((significand & ~significand_mask) == (significand_mask+1));

        round_significand_core(significand, roundoff_bits);

        // cheeck for all-ones rounded-up to overflow
        constexpr uint_t mask = uint_t(1) << (significand_bitsize + 1);
        if (significand == mask) {
            significand >>= 1;
            if (increase_exponent(exponent, 1)) {
                // overflow in exponent -> change to infinity
                return false;
            }
        }

        return true;
    }

    // helper function to round subnormal values. If the value rounds up and becomes
    // normal this function returns `false` to indicate that to the caller
    static bool round_subnormal_significand(uint_t &significand, uint_t roundoff_bits)
    {
        assert((significand & ~significand_mask) == 0);

        round_significand_core(significand, roundoff_bits);

        // cheeck for all-ones rounded-up changing subnormal to normal
        constexpr uint_t mask = uint_t(1) << significand_bitsize;
        if (significand == mask) {
            return false;
        }
 
        return true;
    }

    // number of bits significand needs to shift to get implied-one
    // into the right position. Positive means left shift.
    static int32_t significand_adjustment(uint_t significand)
    {
        // index of the implied leading `1.`
        constexpr unsigned long keybit = significand_bitsize;

        unsigned long index;
        if (details::bit_scan(&index, significand))
        {
            return keybit - index;
        }

        return keybit;

    }

    // increase the exponent value and report if overflow occured
    static bool increase_exponent(exponent_t& exponent, int amount)
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
    static int decrease_exponent(exponent_t& exponent, int amount)
    {
        exponent -= amount;

        int diff = exponent - emin;

        if (diff < 0) {
            exponent = emin;
            return -diff;
        }

        return 0;
    }

    //
    // decompisition, composition, classification
    //

private:

    enum class fp_class { nan, infinity, zero, normal, subnormal };
    struct fp_components {
        fp_class class_;
        uint8_t sign;
        exponent_t exponent;
        uint_t significand;
    };

    fp_components decompose() const
    {
        fp_components components;
        components.class_ = fp_class::nan;
        components.sign = raw_value >> (bitsize - 1);
        components.exponent = (raw_value & ~(uint_t(1) << (bitsize - 1))) >> significand_bitsize;
        components.significand = (raw_value & ((uint_t(1) << significand_bitsize) - 1));

        if (components.exponent == 0)
        {
            if (components.significand == 0)
            {
                components.class_ = fp_class::zero;
            }
            else
            {
                components.exponent = emin;
                components.class_ = fp_class::subnormal;
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
            components.significand |= (uint_t(1) << significand_bitsize);
        }

        return components;
    }

public:
    static constexpr floatbase_t indeterminate_nan() {
        return floatbase_t{ 1, exponent_mask, uint_t(1) << (significand_bitsize - 1) };
    }

    static constexpr floatbase_t infinity(uint8_t sign = 0) {
        return floatbase_t{ sign, exponent_mask, 0 };
    }

    static constexpr floatbase_t zero(uint8_t sign = 0) {
        return floatbase_t{ sign, 0, 0 };
    }

    static constexpr floatbase_t subnormal(uint8_t sign, uint_t significand) {
        return floatbase_t{ sign, 0, significand };
    }

    static constexpr floatbase_t normal(uint8_t sign, exponent_t exponent, uint_t significand) {
        assert(exponent >= emin && exponent <= emax);
        assert((significand & ~significand_mask) == (significand_mask + 1));
        return floatbase_t{ sign, exponent + bias, static_cast<uint_t>(significand & significand_mask) };
    }


    //
    // arithmetic
    //

public:

    floatbase_t operator+(floatbase_t addend) const
    {
        fp_components l = this->decompose();
        fp_components r = addend.decompose();

        if (l.class_ == fp_class::nan) {
            return *this;
        }
        else if (r.class_ == fp_class::nan) {
            return addend;
        }

        if (l.class_ == fp_class::zero) {
            if (r.class_ == fp_class::zero && (r.sign != l.sign)) {
                return zero();
            }
            return addend;
        }
        else if (r.class_ == fp_class::zero) {
            return *this;
        }

        if (l.class_ == fp_class::infinity) {
            if (r.class_ == fp_class::infinity && (r.sign != l.sign)) {
                return indeterminate_nan();
            }
            return *this;
        }
        else if (r.class_ == fp_class::infinity) {
            return addend;
        }

        uint_t roundoff_bits = 0;

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

        uint8_t sign = 0;
        exponent_t exponent = l.exponent;
        uint_t significand = 0;

        if (l.sign != r.sign)
        {
            if (l.significand > r.significand)
            {
                significand = l.significand - r.significand;
                sign = l.sign;

            }
            else if (l.significand < r.significand)
            {
                significand = r.significand - l.significand;
                sign = r.sign;
            }
            else
            {
                // a - a => 0
                return zero();
            }

            if (roundoff_bits > 0)
            {
                // we have round-off bits in the subtrahend
                // subtracting out these bits would cause a borrow, so simulate this by subtracing 1
                significand--;

                // invert roundoff_bits (small value becomes large during subtraction)
                roundoff_bits = 0 - roundoff_bits; // rely on unsigned wrap-around

                // if we lost the top-bit shift everything over
                if ((significand & (significand_mask + 1)) == 0) {
                    significand <<= 1;
                    significand |= roundoff_bits >> (bitsize - 1);
                    roundoff_bits <<= 1;
                    exponent--; // TODO: can this underflow???
                }

                if (!round_significand(significand, exponent, roundoff_bits)) {
                    return infinity(sign);
                }
            }


            int distance = significand_adjustment(significand);
            assert(distance >= 0);

            if (distance > 0)
            {
                // underflow--decrease exponent and fill significand with 0s
                if (int underflow_amount = decrease_exponent(exponent, distance))
                {
                    // underflow in exponent -> change to subnormal
                    int shift_amount = distance - underflow_amount;

                    if (shift_amount > 0) {
                        assert((distance - underflow_amount) < significand_bitsize);
                        significand <<= shift_amount;
                    }
                    else if (shift_amount < 0) {
                        if (-shift_amount < bitsize) {
                            significand >>= -shift_amount;
                        }
                        else {
                            return zero();
                        }
                    }
                    return subnormal(sign, significand);
                }
                else
                {
                    significand <<= distance;
                }
            }
        }
        else
        {
            significand = l.significand + r.significand;
            sign = l.sign;

            constexpr uint_t topbit = (uint_t(1) << significand_bitsize);
            constexpr uint_t overflowbit = topbit << 1;

            // check for overflow
            if ((significand & overflowbit) != 0) {
                roundoff_bits >>= 1;
                roundoff_bits |= (significand & 1) << (bitsize - 1);
                significand >>= 1;
                if (increase_exponent(exponent, 1)) {
                    // overflow in exponent -> change to infinity
                    return infinity(sign);
                }
            }

            if (!round_significand(significand, exponent, roundoff_bits)) {
                return infinity(sign);
            }

            if ((significand & topbit) == 0) {
                return subnormal(sign, significand);
            }
        }

        return normal(sign, exponent, significand);
    }

    floatbase_t operator-(floatbase_t addend) const
    {
        // TODO: optimize this to avoid calling decompose here and then again in operator+
        fp_components l = this->decompose();
        fp_components r = addend.decompose();

        if (l.class_ == fp_class::nan) {
            return *this;
        }
        else if (r.class_ == fp_class::nan) {
            return addend;
        }

        return this->operator+(-addend);
    }

    floatbase_t operator*(floatbase_t addend) const
    {
        fp_components l = this->decompose();
        fp_components r = addend.decompose();

        if (l.class_ == fp_class::nan) {
            return *this;
        }
        else if (r.class_ == fp_class::nan) {
            return addend;
        }

        if (l.class_ == fp_class::infinity) {
            if (r.class_ == fp_class::zero) {
                return indeterminate_nan();
            }
            return floatbase_t::from_bitstring(this->raw_value ^ (sign_mask & addend.raw_value));

        }
        else if (r.class_ == fp_class::infinity) {
            if (l.class_ == fp_class::zero) {
                return indeterminate_nan();
            }

            return floatbase_t::from_bitstring(addend.raw_value ^ (sign_mask & this->raw_value));
        }

        uint8_t sign = l.sign ^ r.sign;

        if (l.class_ == fp_class::zero || r.class_ == fp_class::zero) {
            return zero(sign);
        }

        exponent_t exponent = l.exponent + r.exponent;

        if (exponent > emax) {
            return infinity(sign);
        }

        constexpr uint_t midpoint = uint_t(1) << (significand_bitsize - 1);
        uint_t significand = 0;
        uint_t roundoff_bits = 0;

        if constexpr (sizeof(uint_t) == 2) {
            uint32_t z = uint32_t(l.significand) * uint32_t(r.significand);
            roundoff_bits = static_cast<uint_t>(z & significand_mask) << (bitsize - significand_bitsize);
            significand = static_cast<uint_t>(z >> significand_bitsize);
        }
        else if constexpr (sizeof(uint_t) == 4) {
            uint64_t z = uint64_t(l.significand) * uint64_t(r.significand);
            roundoff_bits = static_cast<uint_t>(z & significand_mask) << (bitsize - significand_bitsize);;
            significand = static_cast<uint_t>(z >> significand_bitsize);
        }
        else if constexpr (sizeof(uint_t) == 8) {
            constexpr auto bitdiff = bitsize - significand_bitsize;
            uint64_t zhi, z = _umul128(x, y, &zhi);
            roundoff_bits = static_cast<uint_t>(z & significand_mask) << (bitsize - significand_bitsize);;
            significand = (zhi << bitdiff) | (z >> significand_bitsize);
        }
        else {
            static_assert(false, "NYI: multiply for this size");
        }

        if (significand == 0)
        {
            significand = roundoff_bits;
            roundoff_bits = 0;

            if (decrease_exponent(exponent, significand_bitsize)) {
                // there are no interesting bits...
                return zero(sign);
            }

            // fallthrough and continue adjustment
        }

        // there should be an intersting bit as we exited on zero multiplicands
        assert(significand != 0);

        int distance = significand_adjustment(significand);

        if (distance > 0)
        {
            // underflow in significand (there was a subnormal in the input)

            if (int underflow_amount = decrease_exponent(exponent, distance))
            {
                // underflow in exponent -> result is a subnormal
                distance -= underflow_amount;

                if (distance < 0) {
                    roundoff_bits >>= -distance;
                    roundoff_bits |= significand << (bitsize - -distance);
                    significand >>= -distance;
                }
                else if (distance > 0) {
                    significand <<= distance;
                    significand |= roundoff_bits >> (bitsize - distance);
                    roundoff_bits <<= distance;
                }

                if (!round_subnormal_significand(significand, roundoff_bits)) {
                    return normal(sign, emin, significand);
                }

                return subnormal(sign, significand);
            }
            else
            {
                // shift significand up and pull in bits from roundoff
                significand <<= distance;
                significand |= roundoff_bits >> (bitsize - distance);
                roundoff_bits <<= distance;
            }
        }
        else if (distance < 0)
        {
            // max significands can overflow only by a single bit
            assert(distance == -1);

            // move the LSB from product to roundoff

            roundoff_bits >>= 1;
            roundoff_bits |= significand << (bitsize - 1);
            significand >>= 1;

            if (increase_exponent(exponent, 1)) {
                return infinity(sign);
            }
        }

        if (exponent < emin) {
            while (exponent < emin) {
                ++exponent;
                roundoff_bits >>= 1;
                roundoff_bits |= (significand & 1) << (bitsize - 1);
                significand >>= 1;

                if (significand == 0 && roundoff_bits == 0) {
                    return zero(sign);
                }
            }

            if (round_subnormal_significand(significand, roundoff_bits)) {
                return subnormal(sign, significand);
            }
        }
        else if (!round_significand(significand, exponent, roundoff_bits)) {
            return infinity(sign);
        }

        return normal(sign, exponent, significand);
    }

    static void long_division(int_t dividend, int_t divisor, uint_t& quotient, uint_t& remainder)
    {
        quotient = long_division_loop(dividend, divisor);
        remainder = long_division_loop(dividend, divisor);

        remainder <<= (bitsize - (significand_bitsize + 1));

        // if this is a repeating fractional part, force round-up at midpoint
        if (dividend && (remainder == (uint_t(1) << (bitsize - 1)))) {
            remainder += 1;
        }
    }

    // compute binary long division
    static uint_t long_division_loop(int_t& dividend, int_t divisor)
    {
        int_t quotient = 0;

        for (int bit = significand_bitsize; dividend && (bit >= 0); bit--)
        {
            if (dividend >= divisor) {
                quotient |= (int_t(1) << bit);
                dividend -= divisor;
            }

            dividend <<= 1;
        }

        return static_cast<uint_t>(quotient);
    }

    floatbase_t operator/(floatbase_t denomenator) const
    {
        fp_components l = this->decompose();
        fp_components r = denomenator.decompose();

        if (l.class_ == fp_class::nan) {
            return *this;
        }
        else if (r.class_ == fp_class::nan) {
            return denomenator;
        }

        uint8_t sign = l.sign ^ r.sign;

        if (l.class_ == fp_class::zero) {
            if (r.class_ == fp_class::zero) {
                return indeterminate_nan();
            }
            return zero(sign);
        }
        else if (r.class_ == fp_class::zero) {
            return infinity(sign);
        }

        if (l.class_ == fp_class::infinity) {
            if (r.class_ == fp_class::infinity) {
                return indeterminate_nan();
            }
            return infinity(sign);
        }
        else if (r.class_ == fp_class::infinity) {
            return zero(sign);
        }

        // convert subnormal inputs into normal so that values are close
        // to each other during division to avoid overflowing the quotient
        if (l.class_ == fp_class::subnormal) {
            int adjustment = significand_adjustment(l.significand);
            l.significand <<= adjustment;
            l.exponent -= adjustment;
        }
        if (r.class_ == fp_class::subnormal) {
            int adjustment = significand_adjustment(r.significand);
            r.significand <<= adjustment;
            r.exponent -= adjustment;
        }

        exponent_t exponent = l.exponent - r.exponent;

        int_t dividend = static_cast<int_t>(l.significand);
        int_t divisor = static_cast<int_t>(r.significand);

        // ensure we compute exactly the right amount of significand digits
        assert(dividend && (divisor <= (uint_t(1) << (significand_bitsize + 1)))); // ensure loop terminates
        while (dividend < divisor) {
            dividend <<= 1;
            --exponent; // underflow okay, handled later
        }

        uint_t significand, roundoff_bits;
        long_division(dividend, divisor, significand, roundoff_bits);

        if (significand < (significand_mask + 1))
        {
            assert(exponent == emin);
            if (round_subnormal_significand(significand, roundoff_bits)) {
                return subnormal(sign, significand);
            }
        }
        else if (exponent < emin) {
            while (exponent < emin) {
                ++exponent;
                roundoff_bits >>= 1;
                roundoff_bits |= (significand & 1) << (bitsize - 1);
                significand >>= 1;

                if (significand == 0 && roundoff_bits == 0) {
                    return zero(sign);
                }
            }

            if (round_subnormal_significand(significand, roundoff_bits)) {
                return subnormal(sign, significand);
            }
        }
        else if (exponent > emax) {
            return infinity(sign);
        }
        else if (!round_significand(significand, exponent, roundoff_bits)) {
            return infinity(sign);
        }

        return normal(sign, exponent, significand);
    }

    floatbase_t operator-() const
    {
        floatbase_t neg = *this;
        neg.raw_value ^= (uint_t(1) << (bitsize - 1));
        return neg;
    }

    //
    // Comparison
    //
public:

    bool operator==(floatbase_t other)
    {
        auto is_nan = [](uint_t x) {
            constexpr uint_t exp_mask = (exponent_mask << significand_bitsize);
            return ((x & exp_mask) == exp_mask) && ((x & significand_mask) != 0);
        };

        if (this->raw_value == other.raw_value)
        {
            if (is_nan(this->raw_value) || is_nan(other.raw_value))
                return false;
            return true;
        }

        if ((this->raw_value == 0) && (other.raw_value == floatbase_t::zero(1).raw_value)) {
            return true;
        }
            
        if ((other.raw_value == 0) && (this->raw_value == floatbase_t::zero(1).raw_value)) {
            return true;
        }

        return false;
    }

    bool operator!=(floatbase_t other) { return !operator==(other); }

 private:
 
    template<bool check_eq>
    static bool compare_lt(const fp_components &l, const fp_components &r)
    {
        // Special case 0 to handle -0.0 and the fact that 0 has bigger exponent then values < 1.0f
        if (l.class_ == fp_class::zero)
        {
            if (r.class_ == fp_class::zero) {
                return check_eq;
            }

            return !r.sign;
        }
        else if (r.class_ == fp_class::zero)
        {
            return l.sign;
        }

        // negatives always less than positives
        if (l.sign && !r.sign) {
            return true;
        }
        else if (r.sign && !l.sign) {
            return false;
        }

        if (l.exponent < r.exponent) {
            return !l.sign;
        }
        else if (l.exponent > r.exponent) {
            return l.sign;
        }

        if (l.significand < r.significand) {
            return !l.sign;
        }
        else if (l.significand > r.significand) {
            return l.sign;
        }

        return check_eq;
    }

 public:

    bool operator<(floatbase_t other)
    {
        fp_components l = this->decompose();
        fp_components r = other.decompose();

        // NaN's always compare false
        if (l.class_ == fp_class::nan || r.class_ == fp_class::nan) {
            return false;
        }

        return compare_lt<false>(l, r);
    }

    bool operator<=(floatbase_t other)
    {
        fp_components l = this->decompose();
        fp_components r = other.decompose();

        // NaN's always compare false
        if (l.class_ == fp_class::nan || r.class_ == fp_class::nan) {
            return false;
        }

        return compare_lt<true>(l, r);
    }

    bool operator>(floatbase_t other)
    {
        fp_components l = this->decompose();
        fp_components r = other.decompose();

        // NaN's always compare false
        if (l.class_ == fp_class::nan || r.class_ == fp_class::nan) {
            return false;
        }
        return compare_lt<false>(r, l);
    }

    bool operator>=(floatbase_t other)
    {
        fp_components l = this->decompose();
        fp_components r = other.decompose();

        // NaN's always compare false
        if (l.class_ == fp_class::nan || r.class_ == fp_class::nan) {
            return false;
        }

        return compare_lt<true>(r, l);
    }

    //
    // public utitliy functions
    //

public:

    // publicly expose private constructors as factory functions. This is done
    // to keep floatbase_t interface similar to built-in float/double while
    // still allowing easy creation from integer values.
    static floatbase_t from_bitstring(uint_t t) { auto f = floatbase_t{}; f.raw_value = t; return f; }
    static floatbase_t from_triplet(bool sign, exponent_t exponent, uint_t significand) {
        return floatbase_t{ static_cast<uint_t>(sign), exponent, significand };
    }
};


using float16_t = floatbase_t<fp_format::binary16>;
using float32_t = floatbase_t<fp_format::binary32>;
using float64_t = floatbase_t<fp_format::binary64>;
