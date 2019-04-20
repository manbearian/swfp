
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

void validate_shift(uint16_t x)
{
    for (int i = 0; i < (sizeof(uint16_t) * 8); ++i)
    {
        {
            uint16sw_t a = x;
            auto c = a << i;
            auto z = x << i;

            if (memcmp(&c, &z, sizeof(uint16_t))) {
                cout << std::hex << "0x" << x << " << " << i << endl;
                cout << std::hex << "hw: 0x" << z << endl;
                cout << std::hex << "sw: 0x" << (uint16_t)c << endl;
                throw std::exception("bad unsigned shl");
            }
        }

        {
            uint16sw_t a = x;
            auto c = a >> i;
            auto z = x >> i;

            if (memcmp(&c, &z, sizeof(uint16_t))) {
                cout << std::hex << "0x" << x << " >> " << i << endl;
                cout << std::hex << "hw: 0x" << z << endl;
                cout << std::hex << "sw: 0x" << (uint16_t)c << endl;
                throw std::exception("bad unsigned shr");
            }
        }

        {
            int16sw_t a = static_cast<int16sw_t>(x);
            int16_t sx = static_cast<int16sw_t>(x);

            int16sw_t c = a << i;
            int16_t z = sx << i;;

            if (memcmp(&c, &z, sizeof(uint16_t))) {
                cout << std::hex << "0x" << sx << " << " << i << endl;
                cout << std::hex << "hw: 0x" << z << endl;
                cout << std::hex << "sw: 0x" << (uint16_t)c << endl;
                throw std::exception("bad signed shl");
            }
        }

        {
            int16sw_t a = static_cast<int16sw_t>(x);
            int16_t sx = static_cast<int16sw_t>(x);

            int16sw_t c = a >> i;
            int16_t z = sx >> i;

            if (memcmp(&c, &z, sizeof(uint16_t))) {
                cout << std::hex << "0x" << sx << " >> " << i << endl;
                cout << std::hex << "hw: 0x" << z << endl;
                cout << std::hex << "sw: 0x" << (uint16_t)c << endl;
                throw std::exception("bad signed shr");
            }
        }
    }
}

std::atomic<int> count = 0;

int main()
{
    try
    {
        // fill array with values for std::for_each
        uint16_t values[std::numeric_limits<uint16_t>::max() + 1];
        for (int i = 0; i < std::numeric_limits<uint16_t>::max(); ++i) {
            values[i] = uint16_t(i);
        }

        // run through all possible values, parallelizing the outer loop
        std::for_each(std::execution::par_unseq, std::begin(values), std::end(values), [](uint16_t a) {
            try
            {
                validate_shift(a);
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
