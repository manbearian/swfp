
#include <stdint.h>
#include <iostream>
#include <memory>
#include <string>

#include <limits>

#include "fpcore.h"

// disable constant arithmetic warnings
#pragma warning(disable:4756)

using std::cout;
using std::endl;

//
// Validate all 16-bit add operations
//  compute in HW at 32-bit and compare to 16-bit output
//  tests both ADD and NARROW operations
//

void validate_add(float16_t a, float16_t b)
{
    float x = (float)a;
    float y = (float)b;

    float16_t c = a + b;
    float z = x + y;
    float16_t z16 = (float16_t)z;

    float32_t z32 = float32_t(x) + float32_t(y);

    if (std::isnan(x) || std::isnan(y))
    {
        // todo: understand NaN payloads and ensure they are enforced
        if (!std::isnan((float)c) || !std::isnan(z))
            throw std::exception("bad nan");
    }
    else if (memcmp(&c, &z16, sizeof(c)) != 0)
    {
        cout << "failed!" << endl;
        cout << "a: " << (float)a << " " << a.to_hex_string() << " " << a.to_triplet_string() << endl;
        cout << "b: " << (float)b << " " << b.to_hex_string() << " " << b.to_triplet_string() << endl;
        cout << "c: " << (float)c << " " << c.to_hex_string() << " " << c.to_triplet_string() << endl;
        cout << "z16: " << (float)z16 << " " << z16.to_hex_string() << " " << z16.to_triplet_string() << endl;
        cout << "\n";
        cout << "x: " << x << " " << float32_t(x).to_hex_string() << " " << float32_t(x).to_triplet_string() << endl;
        cout << "y: " << y << " " << float32_t(y).to_hex_string() << " " << float32_t(y).to_triplet_string() << endl;
        cout << "z: " << z << " " << float32_t(z).to_hex_string() << " " << float32_t(z).to_triplet_string() << endl;


        throw std::exception("bad add");
    }
}

void validate_add()
{
    for (int i = 0; i < std::numeric_limits<uint16_t>::max(); ++i) {
        if (i % 10000 == 0) {
            cout << "@";
        } else if (i % 1000 == 0) {
            cout << "$";
        }  else if (i % 100 == 0) {
            cout << ".";
        }
        for (int j = 0; j < std::numeric_limits<uint16_t>::max(); ++j) {
            auto a = float16_t::from_bitstring(uint16_t(i));
            auto b = float16_t::from_bitstring(uint16_t(j));
            validate_add(a, b);
        }
    }
    cout << "\n";

}

int main()
{
    try
    {
       validate_add();
    }
    catch (std::exception e)
    {
        cout << "test failed: " << e.what() << endl;
        return 1;
    }

    cout << "success!" << endl;
    return 0;
}
