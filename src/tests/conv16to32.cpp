
#include <stdint.h>
#include <iostream>
#include <memory>
#include <string>

using std::cout;
using std::endl;

#include <limits>

#include "fpcore.h"


void validate_cast(float16_t a, float16_t b, float32_t x)
{
    if (memcmp((char*)&a, (char*)&b, sizeof(float16_t)) != 0)
    {
        cout << "failed!" << endl;
        cout << "a: " << (float)a << " " << a.to_hex_string() << " " << a.to_triplet_string() << endl;
        cout << "x: " << (float)x << " " << x.to_hex_string() << " " << x.to_triplet_string() << endl;
        cout << "b: " << (float)b << " " << b.to_hex_string() << " " << b.to_triplet_string() << endl;

        auto err = std::string{ "Failure: '" } + "float16->float32->float16" + "'";
        throw std::exception(err.c_str());
    }
}

void validate_cast(float16_t a)
{
    float32_t x = (float32_t)a;
    float16_t b = (float16_t)x;

    validate_cast(a, b, x);
}

void validate_casts()
{
    for (int i = 0; i < std::numeric_limits<uint16_t>::max(); ++i)
    {
        validate_cast(float16_t::from_bitstring(uint16_t(i)));
    }

    // test special values
    validate_cast(float16_t::infinity(), (float16_t)float32_t::infinity(), float32_t::infinity());
    validate_cast(float16_t::infinity(1), (float16_t)float32_t::infinity(1), -float32_t::infinity(1));
    validate_cast(float16_t::zero(), (float16_t)float32_t::zero(), float32_t::zero());
    validate_cast(float16_t::zero(1), (float16_t)float32_t::zero(1), float32_t::zero(1));
    validate_cast(float16_t::indeterminate_nan(), (float16_t)float32_t::indeterminate_nan(), float32_t::indeterminate_nan());
}

int main()
{
    try
    {
        validate_casts();
    }
    catch (std::exception e)
    {
        cout << "test failed: " << e.what() << endl;
        return 1;
    }

    cout << "success!" << endl;
    return 0;
}
