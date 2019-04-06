
#include <stdint.h>
#include <iostream>
#include <memory>
#include <string>
#include <algorithm>
#include <execution>
#include <atomic>

#include <limits>

#include "fpcore.h"

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

template<typename integral_t>
void validate_int_to_fp(integral_t a)
{
    using display_t = mint32_t<(sizeof(integral_t) < sizeof(int)), integral_t>::int_t;

    float16_t f = static_cast<float16_t>(a);
    float fx = static_cast<float>(a);
    float16_t g = static_cast<float16_t>(fx);

    if (memcmp(&f, &g, sizeof(f)) != 0)
    {
        cout << "failed!" << endl;
        cout << "a: 0x" << std::hex << display_t(a) << endl;
        cout << "sw_to_f16: " << (float)f << " " << f.to_hex_string() << " " << f.to_triplet_string() << endl;
        cout << "hw_to_f32: " << (float)g << " " << g.to_hex_string() << " " << g.to_triplet_string() << endl;

        throw std::exception("bad conv");
    }
}

// fill array with values for std::for_each
uint32_t values[std::numeric_limits<uint16_t>::max() + 1];

std::atomic<int> count;

template<typename integral_t>
void testall()
{
    cout << "testing '"<< typeid(integral_t).name() << "'";

    // run through all possible values, parallelizing the outer loop
    std::for_each(std::execution::seq, std::begin(values), std::end(values), [](uint16_t a) {

        validate_int_to_fp<integral_t>(a);

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
        values[i] = uint16_t(i);
    }

    try
    {
#if 0
        testall<int8_t>();
        testall<uint8_t>();
        testall<int16_t>();
        testall<uint16_t>();
        testall<int32_t>();
        testall<uint32_t>();
        testall<int64_t>();
        testall<uint64_t>();
#endif
       // for ()
        validate_int_to_fp<short>(std::numeric_limits<short>::max());
    }
    catch (std::exception e)
    {
        cout << "test failed: " << e.what() << endl;
        return 1;
    }

    cout << "success!" << endl;
    return 0;
}
