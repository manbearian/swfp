
#include <stdint.h>
#include <iostream>
#include <memory>
#include <string>
#include <algorithm>
#include <execution>
#include <atomic>

#include <limits>

#include "swfp.h"

// disable constant arithmetic warnings
#pragma warning(disable:4756)

using std::cout;
using std::endl;

//
// Validate all 16-bit div operations
//  compute in HW at 32-bit and compare to 16-bit output
//  tests both DIV and NARROW operations
//

template<bool, typename T> struct mint32_t;
template <typename T> struct mint32_t<true, T> { using int_t = int; };
template<bool, typename T> struct mint32_t;
template <typename T> struct mint32_t<false, T> { using int_t = T; };

template<bool, typename T> struct mf32_t;
template <typename T> struct mf32_t<true, T> { using float_t = float; };
template<bool, typename T> struct mf32_t;
template <typename T> struct mf32_t<false, T> { using float_t = double; };

template<typename int_t, typename float_t>
void validate_to_conv(float_t a)
{
    using display_t = mint32_t<(sizeof(int_t) < sizeof(int)), int_t> ::int_t;
    using hwfloat_t = mf32_t<(sizeof(float_t) < sizeof(float)), float_t> ::float_t;

    hwfloat_t x = (hwfloat_t)a;

    int_t c = static_cast<int_t>(a);
    int_t z = static_cast<int_t>(x);

    if (memcmp(&c, &z, sizeof(c)) != 0)
    {
        cout << "failed!" << endl;
        cout << "a: " << x << " " << a.to_hex_string() << " " << a.to_triplet_string() << endl;
        cout << "sw conv: 0x" << std::hex << display_t(c) << endl;
        cout << "hw conv: 0x" << std::hex << display_t(z) << endl;

        throw std::exception("bad conv");
    }
}

// fill array with values for std::for_each
float16_t values[std::numeric_limits<uint16_t>::max() + 1];

std::atomic<int> count;

template<typename integral_t>
void testall()
{
    cout << "testing '"<< typeid(integral_t).name() << "'";
    // run through all possible values, parallelizing the outer loop
    std::for_each(std::execution::par_unseq, std::begin(values), std::end(values), [](float16_t a) {
        
        validate_to_conv<integral_t>(a);

        // output progress
        int old_value = count.fetch_add(1);
        if (old_value % 10000 == 0) {
            cout << ".";
        }
    });

    cout << "completed\n";
}

int main()
{
    // initialize values
    for (int i = 0; i < std::numeric_limits<uint16_t>::max(); ++i) {
        values[i] = float16_t::from_bitstring(uint16_t(i));
    }

    try
    {
        testall<int8_t>();
        testall<uint8_t>();
        testall<int16_t>();
        testall<uint16_t>();
        testall<int32_t>();
        testall<uint32_t>();
        testall<int64_t>();
        testall<uint64_t>();
;    }
    catch (std::exception e)
    {
        cout << "test failed: " << e.what() << endl;
        return 1;
    }

    cout << "success!" << endl;
    return 0;
}
