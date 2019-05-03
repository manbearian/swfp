
#include <stdint.h>
#include <iostream>
#include <memory>
#include <string>
#include <algorithm>
#include <execution>
#include <atomic>

#include <limits>

#include "swint.h"

// disable constant arithmetic warnings
#pragma warning(disable:4756)

using std::cout;
using std::endl;

void validate_neg(uint16_t x)
{
    {
        uint16sw_t a = static_cast<uint16sw_t>(x);

        uint16sw_t c = -a;
        uint16_t z = -x;

        if (memcmp(&c, &z, sizeof(uint16_t))) {
            cout << std::hex << "hw: x=0x" << x << ", z=0x" << z << endl;
            cout << std::hex << "sw: x=0x" << (uint16_t)a << ", z=0x" << (uint16_t)c << endl;
            throw std::exception("bad signed neg");
        }
    }

    {
        int16sw_t a = static_cast<int16sw_t>(x);
        int16_t sx = static_cast<int16_t>(x);

        int16sw_t c = -a;
        int16_t z = -sx;

        if (memcmp(&c, &z, sizeof(uint16_t))) {
            cout << std::hex << "hw: x=0x" << x << ", z=0x" << z << endl;
            cout << std::hex << "sw: x=0x" << (uint16_t)a << ", z=0x" << (uint16_t)c << endl;
            throw std::exception("bad signed neg");
        }
    }
}

int main()
{
    try
    {
        // fill array with values for stdnumeric_limits::for_each
        uint16_t values[std::numeric_limits<uint16_t>::max() + 1];
        for (int i = 0; i < std::numeric_limits<uint16_t>::max(); ++i) {
            values[i] = uint16_t(i);
        }

        // run through all possible values in parallel
        std::for_each(std::execution::par_unseq, std::begin(values), std::end(values), [](uint16_t a) {
            try
            {
                validate_neg(a);
            }
            catch (std::exception e)
            {
                cout << "test failed: " << e.what() << endl;
                std::terminate();
            }
        });
    }
    catch (std::exception e)
    {
        cout << "test failed: " << e.what() << endl;
        return 1;
    }

    cout << "success!" << endl;
    return 0;
}
