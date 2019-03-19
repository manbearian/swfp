#include <stdint.h>
#include <memory>
#include <type_traits>
#include <string>
#include <sstream>
#include <assert.h>
#include <algorithm>
#include <intrin.h>

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

    using uint_t = typename fp_traits::uint_t;
    using int_t = std::make_signed_t<uint_t>;
    using hwfp_t = typename fp_traits::hwfp_t;

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

    //
    // construction from hw fp types
    //

    template<typename T = hwfp_t, typename = std::enable_if_t<std::is_same_v<T, hwfp_t>> > // triggers SFINAE and disables ctor if hwfp_t is void
    constexpr floatbase_t(T hwf) {
        memcpy(&raw_value, &hwf, sizeof(T));
    }

    explicit constexpr floatbase_t(float hwf)
    {
        if constexpr (format == fp_format::binary16)
        {
            *this = float16_t(float32_t(hwf));
        }
        else if constexpr (format == fp_format::binary32)
        {
            memcpy(&raw_value, &hwf, sizeof(float));
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
            memcpy(&raw_value, &hwf, sizeof(float));
        }
        else
        {
            static_assert(false, "nyi: convert from hw float64");
        }
    }

private:
    // initialize from raw bits
    explicit constexpr floatbase_t(uint_t val) :
        raw_value(val)
    { }

    constexpr floatbase_t(uint_t sign, uint_t exponent, uint_t significand) :
        raw_value((sign << (bitsize-1)) | (exponent << significand_bitsize) | significand)
    {
        assert((sign & 1) == sign);
        assert((significand & significand_mask) == significand);
        assert((exponent & exponent_mask) == exponent);
    }

    constexpr floatbase_t(uint_t sign, exponent_t exponent, uint_t significand) :
        floatbase_t(sign, static_cast<uint_t>(static_cast<uexponent_t>(exponent)), significand)
    { }


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

    template<typename T> typename T::uint_t narrow_significand(uint_t significand) {
        significand >>= (significand_bitsize - T::significand_bitsize);
        return static_cast<T::uint_t>(significand);
    }
    explicit operator floatbase_t<fp_format::binary16>()
    {
        static_assert(format != fp_format::binary16, "convert from T to T is impossible");

        // convert from bigger FP to smaller FP
        if constexpr (format == fp_format::binary32
            || format == fp_format::binary64
            || format == fp_format::binary128)
        {
            uint8_t sign = raw_value >> (bitsize - 1);
            exponent_t exponent = (raw_value >> significand_bitsize) & exponent_mask;
            uint_t significand = raw_value & significand_mask;

            if (exponent == 0) {
                // denormals round to 0
                significand = 0;
            }
            else if (exponent == exponent_mask) {
                exponent = float16_t::exponent_mask;
                //if (significand == 0) {
                //    return float16_t::infinity(sign);
                //}
                //else {
                //    return float16_t::indeterminate_nan();
                //}
            }
            else {
                exponent -= bias;
                significand |= (1 << significand_bitsize);

                if (exponent < float16_t::emin) {
                    while ((exponent < float16_t::emin) && significand) {
                        // see if we can make this into a denormal
                        exponent++;
                        significand >>= 1;
                    }
                    exponent = 0;
                }
                else {
                    exponent += float16_t::bias;
                }
            }

            auto significand16 = narrow_significand<float16_t>(significand);
            significand16 &= float16_t::significand_mask;

            return float16_t{ sign, exponent, significand16 };
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
            uint8_t sign = raw_value >> (bitsize-1);
            exponent_t exponent = (raw_value >> significand_bitsize) & exponent_mask;
            uint_t significand = raw_value & significand_mask;

            if (exponent == 0) {
                if (significand != 0) {
                    exponent = emin;
                    int distance = signficand_adjustment(significand);
                    assert(distance > 0);
                    significand <<= distance;
                    exponent -= distance; // cannot underflow
                    exponent += float32_t::bias;
                }
            }
            else if (exponent == exponent_mask) {
                exponent = float32_t::exponent_mask;
                //if (significand == 0) {
                //    return float32_t::infinity(sign);
                //}
                //else {
                //    return float32_t::indeterminate_nan();
                //}
            }
            else {
                exponent -= bias;
                exponent += float32_t::bias;
            }

            auto significand32 = widen_significand<float32_t>(significand);
            return float32_t{sign, exponent, (significand32 & float32_t::significand_mask) };
        }
        else
        {
            static_assert(false, "nyi: convert to hw float32");
            return float32_t::indeterminate_nan();
        }
    }

    explicit operator floatbase_t<fp_format::binary64>()
    {
        static_assert(false, "nyi: convert to hw float64");
        return float64_t::indeterminate_nan();
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
        exponent_t exponent;
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
                components.exponent = emin;
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

public:
    static constexpr floatbase_t indeterminate_nan() {
        return floatbase_t{ 1, exponent_mask, uint_t(1) << (significand_bitsize-1) };
    }

    static constexpr floatbase_t infinity(uint8_t sign = 0) {
        return floatbase_t{ sign, exponent_mask, 0};
    }

    static constexpr floatbase_t zero(uint8_t sign = 0) {
        return floatbase_t{ sign, 0, 0 };
    }

    static constexpr floatbase_t denormal(uint8_t sign, uint_t significand) {
        return floatbase_t{ sign, 0, significand };
    }


    //
    // arithmetic
    //

public:

    floatbase_t operator+(floatbase_t addend)
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

            if (roundoff_bits != bits_kind::zero)
            {
                // we have round-off bits in the subtrahend
                // subtracting out these bits would cause a borrow, so simulate this by subtracing 1
                significand--;

                // invert the reaminder bits
                if (roundoff_bits == bits_kind::high)
                {
                    roundoff_bits = bits_kind::low;
                }
                else if (roundoff_bits == bits_kind::low)
                {
                    roundoff_bits = bits_kind::high;
                }

                if ((roundoff_bits == bits_kind::mid)
                    && (significand & (significand_mask + 1)) == 0)
                {
                    // if we subtracted off the leading one, shift over and fill in the LSB
                    // the "roundoff_bits" are exactly equal to 1000..., so shift left should
                    // fill in that one bit.
                    // TODO: does this happens if the signficands are equal and exponent is off-by-one
                    //  this is just "x - x/2" so could just hard code return "x/2"?
                    // TOOD this speical caase bothers me... is this full of bugs?
                    significand <<= 1;
                    if (roundoff_bits == bits_kind::mid || roundoff_bits == bits_kind::high) {
                        significand |= 1;
                    }
                    exponent--;
                }
                else {
                    // round the value
                    round_signficant(significand, roundoff_bits);
                }
            }
        }
        else
        {
            significand = l.significand + r.significand;
            round_signficant(significand, roundoff_bits);
            sign = l.sign;
        }

        int distance = signficand_adjustment(significand);

        if (distance > 0)
        {
            // underflow--decrease exponent and fill signficand with 0s
            if (int underflow_amount = decrease_exponent(exponent, distance))
            {
                // underflow in exponent -> change to denormal
                int shift_amount = distance - underflow_amount;

                if (shift_amount > 0) {
                    assert((distance - underflow_amount) < significand_bitsize);
                    significand <<= shift_amount;
                }
                else if (shift_amount < 0) {
                    if (-shift_amount < (sizeof(significand) * 8)) {
                        significand >>= -shift_amount;
                    }
                    else {
                        return zero();
                    }
                }
                return denormal(sign, significand);
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
                return infinity(sign);
            }
            else
            {
                auto roundoff_bits2 = decrease_significand(significand, roundoff_bitsize);
                round_signficant(significand, roundoff_bits2);
            }
        }

        // mask off implied leading 1.
        assert((~significand_mask & significand) == (significand_mask + 1)); // only implied bit set outside of significand
        significand &= significand_mask;
        exponent += bias;

        return floatbase_t{ sign, exponent, significand };
    }

    floatbase_t operator-(floatbase_t addend)
    {
        // TODO: optimize this...
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

    floatbase_t operator*(floatbase_t addend)
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
            this->raw_value ^= (sign_mask & addend.raw_value);
            return *this;
        }
        else if (r.class_ == fp_class::infinity) {
            if (l.class_ == fp_class::zero) {
                return indeterminate_nan();
            }
            addend.raw_value ^= (sign_mask & this->raw_value);
            return addend;
        }

        uint8_t sign = l.sign ^ r.sign;

        if (l.class_ == fp_class::zero || r.class_ == fp_class::zero) {
            return zero(sign);
        }

        exponent_t exponent = l.exponent + r.exponent;

        if (exponent > emax) {
            return infinity(sign);
        }
        else if (exponent < emin) {
            return zero(sign);
        }
 
        constexpr uint_t midpoint = uint_t(1) << (significand_bitsize - 1);
        uint_t significand = 0;
        uint_t roundoff_bits = 0;

        if constexpr (sizeof(uint_t) == 2) {
            uint32_t z = uint32_t(l.significand) * uint32_t(r.significand);
            roundoff_bits = static_cast<uint_t>(z & significand_mask);
            significand = static_cast<uint_t>(z >> significand_bitsize);
        }
        else if constexpr (sizeof(uint_t) == 4) {
            uint64_t z = uint64_t(l.significand) * uint64_t(r.significand);
            roundoff_bits = static_cast<uint_t>(z & significand_mask);
            significand = static_cast<uint_t>(z >> significand_bitsize);
        }
        else if constexpr (sizeof(uint_t) == 8) {
            constexpr auto bitdiff = bitsize - significand_bitsize;
            uint64_t zhi, z = _umul128(x, y, &zhi);
            roundoff_bits = static_cast<uint_t>(z & significand_mask);
            significand = (zhi << bitdiff) | (z >> significand_bitsize);
        }
        else {
            static_assert(false, "NYI: signficand_adjustment for this size");
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

        // there should be an intersting bit somewhere (since we weeded out zero multiplicands)
        assert(significand != 0);

        int distance = signficand_adjustment(significand);

        if (distance > 0)
        {
            // underflow in signficand (there was a denormal in the input)

            if (int underflow_amount = decrease_exponent(exponent, distance))
            {
                // underflow in exponent -> result is a denormal
                distance -= underflow_amount;

                if (distance < 0) {
                    return zero(sign);
                }
                else if (distance == 0) {
                    return denormal(sign, significand);
                }
            }

            // shift signficand up and pull in bits from roundoff
            significand <<= distance;
            significand |= (roundoff_bits >> (significand_bitsize - distance));
            roundoff_bits = (roundoff_bits << distance) & significand_mask;
        }
        else if (distance < 0)
        {
            // max signficands can overflow only by a single bit
            assert(distance == -1);

            // move the LSB from product to roundoff
            if (significand & 1) {
                roundoff_bits |= uint_t(1) << significand_bitsize;
            }
            significand >>= 1;
            roundoff_bits >>= 1;
            if (increase_exponent(exponent, 1)) {
                return infinity(sign);
            }
        }

        if (roundoff_bits > midpoint)
            significand++;
        else if (roundoff_bits == midpoint)
            significand += (significand & 1); // round-to-even

        significand &= significand_mask;
        exponent += bias;

        return floatbase_t{ sign, exponent, significand };
    }

    // handle a division where the whole number portion can be > 1
    uint_t long_division(int_t& dividend, int_t divisor)
    {
        int_t quotient = 0;

        int bit = significand_bitsize;

        while (dividend && (dividend < divisor) && (bit-- >= 0)) {
            dividend <<= 1;
        }

        // compute the whole number portion
        if (dividend) {
            auto result = std::div(dividend, divisor);
            quotient = (result.quot << bit);
            dividend = result.rem << 1;
        }

        // convert remainder into binary 
        quotient |= remainder_division(dividend, divisor, bit-1);

        return static_cast<uint_t>(quotient);
    }

    // handle a division where the whole number portion is < 1
    uint_t remainder_division(int_t& dividend, int_t divisor, int bitcount = significand_bitsize)
    {
        int_t quotient = 0;

        for (int bit = bitcount; dividend && (bit >= 0); bit--)
        {
            if (dividend >= divisor) {
                quotient |= (int_t(1) << bit);
                dividend -= divisor;
            }

            dividend <<= 1;
        }

        return static_cast<uint_t>(quotient);
    }

    floatbase_t operator/(floatbase_t denomenator)
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

        exponent_t exponent = l.exponent - r.exponent;

        int_t dividend = static_cast<int_t>(l.significand);
        int_t divisor = static_cast<int_t>(r.significand);

        if (exponent > emax) {
            return infinity(sign);
        }

        // bring denormal inputs into range if possible
        while (exponent < emin) {
            exponent++; // cannot overflow
            dividend >>= 1;
            if (dividend == 0) {
                return zero(sign);
            }
        }

        // ensure we compute exactly the right amount of significand digits
        while ((dividend < divisor) && (exponent != emin)) {
            dividend <<= 1;
            --exponent;
            continue;
        }

        uint_t significand = long_division(dividend, divisor);

        int distance = signficand_adjustment(significand);

        if (distance > 0)
        {
            // TODO: do we need rounding bits here?
            assert(exponent == emin);
            return denormal(sign, significand);
        }

        uint_t roundoff_bits = remainder_division(dividend, divisor);

        if (distance < 0)
        {
            auto shift_amount = -distance;

            uint_t shift_out = significand & ((uint_t(1) << shift_amount) - 1);
            significand >>= shift_amount;
            roundoff_bits >>= shift_amount;
            roundoff_bits |= shift_out << (significand_bitsize - shift_amount + 1);
            if (increase_exponent(exponent, shift_amount)) {
                return infinity(sign);
            }

            distance = signficand_adjustment(roundoff_bits);
            if (distance < 0) {
                roundoff_bits >>= -distance;
            }
        }

        constexpr uint_t midpoint = uint_t(1) << significand_bitsize;
        if (roundoff_bits > midpoint) {
            significand++; // round up
        }
        else if (roundoff_bits == midpoint) {
            significand += (significand & 1); // round-to-even
        }

        significand &= significand_mask;
        exponent += bias;

        return floatbase_t{ sign, exponent, significand };
    }

    floatbase_t operator-()
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
            return (((x & exponent_mask) >> significand_bitsize) == exponent_mask) && ((x & significand_mask) != 0);
        };

        if (this->raw_value == other.raw_value)
        {
            if (is_nan(this->raw_value) || is_nan(other.raw_value))
                return false;
            return true;
        }
        return false;
    }

    bool operator!=(floatbase_t other) { return !operator==(other); }


    //
    // public utitliy functions
    //
 public:

     // publicly expose private constructors as factory functions. This is done
     // to keep floatbase_t interface similar to built-in float/double while
     // still allowing easy creation from integer values.
     static floatbase_t from_bitstring(uint_t t) { return floatbase_t{ t }; }
     static floatbase_t from_triplet(bool sign, exponent_t exponent, uint_t significand) {
         return floatbase_t{ static_cast<uint_t>(sign), static_cast<uexponent_t>(exponent), significand };
     }
 };


using float16_t = floatbase_t<fp_format::binary16>;
using float32_t = floatbase_t<fp_format::binary32>;
using float64_t = floatbase_t<fp_format::binary64>;
