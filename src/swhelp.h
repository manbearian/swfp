
#pragma once

#include <intrin.h>
#include <type_traits>
#if BIT_CAST_EXISTS
#include <bit>
#endif

namespace details {

#if IS_CONSTANT_EVALUATED_EXISTS
    using std::is_constant_evaluated();
#else
    bool is_constant_evaluated() { return false; }
#endif
 
    // select T if true, U if false
    template<bool, typename T, typename U> struct selector;
    template <typename T, typename U> struct selector<true, T, U> { using type = T; };
    template<bool, typename T, typename U> struct selector;
    template <typename T, typename U> struct selector<false, T, U> { using type = U; };
    template<bool a, typename T, typename U> using selector_t = typename selector<a, T, U>::type;

    // bit_cast operation provided for pre-C++20
#if BIT_CAST_EXISTS
    using std::bit_cast;
#else
    template< typename to_t, typename from_t, typename = std::enable_if_t<
        (sizeof(to_t) == sizeof(from_t))
        && std::is_trivially_copyable_v<from_t>
        && std::is_trivial_v<to_t>
    >>
    to_t bit_cast(const from_t& from) noexcept {
        to_t to;
        memcpy(&to, &from, sizeof(from_t));
        return to;
    }
#endif

    // wrapper for _BitScanReverse intrinsics that use C++ overloading and supports constexpr evaluation
    template<typename integral_t, typename = std::enable_if_t<std::is_integral_v<integral_t>>>
    static constexpr bool reverse_bit_scan(unsigned long *index, integral_t mask)
    {
        if (is_constant_evaluated())
        {
            // brain-dead implementation of bitscan for constexpr evaluation
            for (int i = (sizeof(integral_t) * 8) - 1; i >= 0; --i)
            {
                if (((integral_t(1) << i) & mask) != 0) {
                    *index = i;
                    return true;
                }
            }

            *index = 0;
            return false;
        }
        else
        {
            if constexpr (sizeof(integral_t) <= 4) {
                return _BitScanReverse(index, static_cast<std::make_unsigned_t<integral_t>>(mask));
            }
            else if constexpr ((sizeof(integral_t) == 8) && (sizeof(void*) == 8)) {
                return _BitScanReverse64(index, mask);
            }
            else {
                struct wrapper_t { uint32_t values[sizeof(integral_t) / sizeof(uint32_t)]; };
                wrapper_t wrapper = bit_cast<wrapper_t>(mask);
                for (int i = 0; i < _countof(wrapper.values); ++i) {
                    if (_BitScanReverse(index, wrapper.values[i])) {
                        return true;
                    }
                }
                return false;
            }
        }
    }

    template<typename integral_t, typename = std::enable_if_t<std::is_integral_v<integral_t>>>
    constexpr bool is_pow_2(integral_t mask) {
        bool bitfound = false;
        for (int i = 0; i < (sizeof(integral_t) * 8); ++i)
        {
            if (mask & (integral_t(1) << i)) {
                if (bitfound)
                    return false;
                bitfound = true;
            }
        }
        return bitfound;
    }
}
