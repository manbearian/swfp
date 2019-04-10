
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
// Validate some int-to-fp operations
//  compute in HW and SW and compare
//  check all 16-bit ints, select larger integers
//

template<typename fp_t, typename integral_t>
void validate_int_to_fp(integral_t a)
{
    using display_t = details::selector_t<(sizeof(integral_t) < sizeof(int)), int, integral_t>;
    using hwfp_t = details::selector_t<(sizeof(fp_t) > sizeof(float)), double, float>;

    auto f = static_cast<fp_t>(a);
    auto fx = static_cast<hwfp_t>(a);
    auto g = static_cast<fp_t>(fx);

    if (memcmp(&f, &g, sizeof(f)) != 0)
    {
        cout << "failed!" << endl;
        cout << "a: 0x" << std::hex << display_t(a)  << std::dec << " (" << display_t(a) << ")" << endl;
        cout << "sw_to_fp: " << (hwfp_t)f << " " << f.to_hex_string() << " " << f.to_triplet_string() << endl;
        cout << "hw_to_fp: " << (hwfp_t)g << " " << g.to_hex_string() << " " << g.to_triplet_string() << endl;

        throw std::exception("bad conv");
    }
}

int main()
{
    try
    {
        // validate the basics
        for (uint64_t i = 0; i < (std::numeric_limits<uint16_t>::max() * 10); ++i)
        {
            validate_int_to_fp<float32_t>(uint8_t(i));
            validate_int_to_fp<float32_t>(int8_t(i));
            validate_int_to_fp<float32_t>(uint16_t(i));
            validate_int_to_fp<float32_t>(int16_t(i));
            validate_int_to_fp<float32_t>(uint32_t(i));
            validate_int_to_fp<float32_t>(int32_t(i));
            validate_int_to_fp<float32_t>(-int32_t(i));
            validate_int_to_fp<float32_t>(uint64_t(i));
            validate_int_to_fp<float32_t>(int64_t(i));
            validate_int_to_fp<float32_t>(-int64_t(i));

            validate_int_to_fp<float64_t>(uint8_t(i));
            validate_int_to_fp<float64_t>(int8_t(i));
            validate_int_to_fp<float64_t>(uint16_t(i));
            validate_int_to_fp<float64_t>(int16_t(i));
            validate_int_to_fp<float64_t>(uint32_t(i));
            validate_int_to_fp<float64_t>(int32_t(i));
            validate_int_to_fp<float64_t>(-int32_t(i));
            validate_int_to_fp<float64_t>(uint64_t(i));
            validate_int_to_fp<float64_t>(int64_t(i));
            validate_int_to_fp<float64_t>(-int64_t(i));
        }

        // test select larger values

        constexpr uint64_t special_values[] = {
            static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()) + 1,
            static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()) * 2,
            static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()) * 5,
            static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()) * 10,
            static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) / 10,
            static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) / 5,
            static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) / 2,
            static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) - 1,
            static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()),
            static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1,
            static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) * 2,
            static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) * 5,
            static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) * 10,
            std::numeric_limits<uint64_t>::max() / 10,
            std::numeric_limits<uint64_t>::max() / 5,
            std::numeric_limits<uint64_t>::max() / 2,
            std::numeric_limits<uint64_t>::max() - 1,
            std::numeric_limits<uint64_t>::max()
        };

        for (int i = 0; i < _countof(special_values); ++i) {
            validate_int_to_fp<float32_t>(static_cast<uint32_t>(special_values[i]));
            validate_int_to_fp<float32_t>(static_cast<int32_t>(special_values[i]));
            validate_int_to_fp<float32_t>(-static_cast<int32_t>(special_values[i]));
            validate_int_to_fp<float32_t>(static_cast<uint64_t>(special_values[i]));
            validate_int_to_fp<float32_t>(static_cast<int64_t>(special_values[i]));
            validate_int_to_fp<float32_t>(-static_cast<int64_t>(special_values[i]));

            validate_int_to_fp<float64_t>(static_cast<uint32_t>(special_values[i]));
            validate_int_to_fp<float64_t>(static_cast<int32_t>(special_values[i]));
            validate_int_to_fp<float64_t>(-static_cast<int32_t>(special_values[i]));
            validate_int_to_fp<float64_t>(static_cast<uint64_t>(special_values[i]));
            validate_int_to_fp<float64_t>(static_cast<int64_t>(special_values[i]));
            validate_int_to_fp<float64_t>(-static_cast<int64_t>(special_values[i]));
        }
    }
    catch (std::exception e)
    {
        cout << "test failed: " << e.what() << endl;
        return 1;
    }

    cout << "success!" << endl;
    return 0;
}
