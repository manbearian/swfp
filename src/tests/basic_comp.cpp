
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
// Validate basic comparison operations by choosing various interesting values
// and then comparing them for various sized floats.
//


template <typename fp_t>
void fail(fp_t a, fp_t b, char const *what)
{
    cout << "failed!" << endl;
    cout << "a: " << (float)a << " " << a.to_hex_string() << " " << a.to_triplet_string() << endl;
    cout << "b: " << (float)b << " " << b.to_hex_string() << " " << b.to_triplet_string() << endl;

    auto err = std::string{ "Failure: '" } + what + "'";
    throw std::exception(err.c_str());
}


template<typename fp_t>
void validate_compares(fp_t a, fp_t b)
{
    using hwfp_t = details::selector_t<(sizeof(fp_t) > sizeof(float)), double, float>;

    hwfp_t ax = (hwfp_t)a;
    hwfp_t bx = (hwfp_t)b;

    if (ax == bx) {
        if (!(a == b)) {
            fail(a, b, "bad ==");
        }
    } else {
        if (a == b) {
            fail(a, b, "bad ==");
        }
    }

    if (ax != bx) {
        if (!(a != b)) {
            fail(a, b, "bad !=");
        }
    }
    else {
        if (a != b) {
            fail(a, b, "bad !=");
        }
    }

    if (ax < bx) {
        if (!(a < b)) {
            fail(a, b, "bad <");
        }
    }
    else {
        if (a < b) {
            fail(a, b, "bad <");
        }
    }

    if (ax <= bx) {
        if (!(a <= b)) {
            fail(a, b, "bad <=");
        }
    }
    else
    {
        if (a <= b) {
            fail(a, b, "bad <=");
        }
    }

    if (ax > bx) {
        if (!(a > b)) {
            fail(a, b, "bad >");
        }
    }
    else
    {
        if (a > b) {
            fail(a, b, "bad >");
        }
    }

    if (ax >= bx) {
        if (!(a >= b)) {
            fail(a, b, "bad >=");
        }
    }
    else
    {
        if (a >= b) {
            fail(a, b, "bad >=");
        }
    }
}


float16_t values16[] = {
    static_cast<float16_t>(0.0f),
    static_cast<float16_t>(0.5f),
    static_cast<float16_t>(1.0f),
    static_cast<float16_t>(2.0f),
    static_cast<float16_t>(3.0f),
    static_cast<float16_t>(3.14159f),
    static_cast<float16_t>(100.0f),
    static_cast<float16_t>(12345.0f),
    static_cast<float16_t>(std::numeric_limits<float>::denorm_min()),
    static_cast<float16_t>(std::numeric_limits<float>::epsilon()),
    static_cast<float16_t>(std::numeric_limits<float>::min()),
    static_cast<float16_t>(std::numeric_limits<float>::max() / 2),
    static_cast<float16_t>(std::numeric_limits<float>::max()),
    static_cast<float16_t>(std::numeric_limits<float>::infinity()),
    static_cast<float16_t>(std::numeric_limits<float>::quiet_NaN())
};

float32_t values32[] = {
    static_cast<float32_t>(0.0f),
    static_cast<float32_t>(0.5f),
    static_cast<float32_t>(1.0f),
    static_cast<float32_t>(2.0f),
    static_cast<float32_t>(3.0f),
    static_cast<float32_t>(3.14159f),
    static_cast<float32_t>(100.0f),
    static_cast<float32_t>(12345.0f),
    static_cast<float32_t>(std::numeric_limits<float>::denorm_min()),
    static_cast<float32_t>(std::numeric_limits<float>::epsilon()),
    static_cast<float32_t>(std::numeric_limits<float>::min()),
    static_cast<float32_t>(std::numeric_limits<float>::max()/2),
    static_cast<float32_t>(std::numeric_limits<float>::max()),
    static_cast<float32_t>(std::numeric_limits<float>::infinity()),
    static_cast<float32_t>(std::numeric_limits<float>::quiet_NaN())
};

float64_t values64[] = {
    static_cast<float64_t>(0.0),
    static_cast<float64_t>(0.5),
    static_cast<float64_t>(1.0),
    static_cast<float64_t>(2.0),
    static_cast<float64_t>(3.0),
    static_cast<float64_t>(3.14159),
    static_cast<float64_t>(100.0),
    static_cast<float64_t>(12345.0),
    static_cast<float64_t>(std::numeric_limits<double>::denorm_min()),
    static_cast<float64_t>(std::numeric_limits<double>::epsilon()),
    static_cast<float64_t>(std::numeric_limits<double>::min()),
    static_cast<float64_t>(std::numeric_limits<double>::max() / 2),
    static_cast<float64_t>(std::numeric_limits<double>::max()),
    static_cast<float64_t>(std::numeric_limits<double>::infinity()),
    static_cast<float64_t>(std::numeric_limits<double>::quiet_NaN())
};


int main()
{
    try
    {
        for (int i = 0; i < _countof(values16); ++i) {
            for (int j = 0; j < _countof(values16); ++j) {
                validate_compares(values16[i], values16[j]);
                validate_compares(-values16[i], values16[j]);
                validate_compares(values16[i], -values16[j]);
                validate_compares(-values16[i], -values16[j]);
            }
        }

        for (int i = 0; i < _countof(values32); ++i) {
            for (int j = 0; j < _countof(values32); ++j) {
                validate_compares(values32[i], values32[j]);
                validate_compares(-values32[i], values32[j]);
                validate_compares(values32[i], -values32[j]);
                validate_compares(-values32[i], -values32[j]);
            }
        }

        for (int i = 0; i < _countof(values64); ++i) {
            for (int j = 0; j < _countof(values64); ++j) {
                validate_compares(values64[i], values64[j]);
                validate_compares(-values64[i], values64[j]);
                validate_compares(values64[i], -values64[j]);
                validate_compares(-values64[i], -values64[j]);
            }
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
