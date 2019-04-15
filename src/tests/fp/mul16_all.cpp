
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
// Validate all 16-bit mul operations
//  compute in HW at 32-bit and compare to 16-bit output
//  tests both MUL and NARROW operations
//

void validate_mul(float16_t a, float16_t b)
{
    float x = (float)a;
    float y = (float)b;

    float16_t c = a * b;
    float z = x * y;
    float16_t z16 = (float16_t)z;

    if (std::isnan(x) || std::isnan(y))
    {
        // todo: understand NaN payloads and ensure they are enforced
        if (!std::isnan((float)c) || !std::isnan(z))
            throw std::exception("bad nan");
    }
    else if (memcmp(&c, &z16, sizeof(c)) != 0)
    {
        cout << "failed!" << endl;
        cout << "a-as-16: " << (float)a << " " << a.to_hex_string() << " " << a.to_triplet_string() << endl;
        cout << "b-as-16: " << (float)b << " " << b.to_hex_string() << " " << b.to_triplet_string() << endl;
        cout << "sw-at-16: " << (float)c << " " << c.to_hex_string() << " " << c.to_triplet_string() << endl;
        cout << "hw-to-16: " << (float)z16 << " " << z16.to_hex_string() << " " << z16.to_triplet_string() << endl;
        cout << "\n";
        cout << "a-to-32: " << x << " " << float32_t(x).to_hex_string() << " " << float32_t(x).to_triplet_string() << endl;
        cout << "b-to-32: " << y << " " << float32_t(y).to_hex_string() << " " << float32_t(y).to_triplet_string() << endl;
        cout << "hw-at-32: " << z << " " << float32_t(z).to_hex_string() << " " << float32_t(z).to_triplet_string() << endl;

        float32_t z32 = (float32_t)a * (float32_t)b;
        cout << "sw-at-32:  " << (float)z32 << " " << z32.to_hex_string() << " " << z32.to_triplet_string() << endl;

        throw std::exception("bad mul");
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
                validate_mul(a, b);
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
