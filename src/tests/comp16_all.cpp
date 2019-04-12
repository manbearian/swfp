
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
// Validate all 16-bit comparison operations
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

std::atomic<int> count = 0;

int main()
{
    try
    {
        // fill array with values for std::for_each
        float16_t values[std::numeric_limits<uint16_t>::max() + 1];
        for (int i = 0; i < std::numeric_limits<uint16_t>::max(); ++i) {
            values[i] = float16_t::from_bitstring(uint16_t(i));
        }

        // run through all possible values, parallelizing the outer loop
        std::for_each(std::execution::par_unseq, std::begin(values), std::end(values), [](float16_t a) {
            for (int j = 0; j < std::numeric_limits<uint16_t>::max(); ++j) {
                auto b = float16_t::from_bitstring(uint16_t(j));
                validate_compares(a, b);
            }

            // output progress
            int old_value = count.fetch_add(1);
            if (old_value % 10000 == 0) {
                cout << "@";
            }
            else if (old_value % 1000 == 0) {
                cout << "$";
            }
            else if (old_value % 100 == 0) {
                cout << ".";
            }
        });

        cout << "\n";
    }
    catch (std::exception e)
    {
        cout << "test failed: " << e.what() << endl;
        return 1;
    }

    cout << "success!" << endl;
    return 0;
}
