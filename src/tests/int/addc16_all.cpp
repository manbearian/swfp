
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

void validate_add(uint16_t x, uint16_t y)
{
    for (int i = 0; i < 3; ++i)
    {
        {
            uint16sw_t a = static_cast<uint16sw_t>(x);
            uint16sw_t b = static_cast<uint16sw_t>(y);

            uint8_t swcarry = static_cast<uint8_t>(i);
            uint16sw_t c = uint16sw_t::add_carry(a, b, swcarry);

            uint8_t hwcarry = static_cast<uint8_t>(i);
            uint16_t z;
            hwcarry = _addcarry_u16(hwcarry, x, y, &z);

            if (memcmp(&c, &z, sizeof(uint16_t))
                || memcmp(&swcarry, &hwcarry, sizeof(uint8_t)))
            {
                cout << "carry_in: " << i << endl;
                cout << std::hex << "hw: x=0x" << x << ", y=0x" << y << ", c=0x" << z << ", carry_out=0x" << (int)hwcarry << endl;
                cout << std::hex << "sw: x=0x" << (uint16_t)a << ", y=0x" << (uint16_t)b << ", c=0x" << (uint16_t)c << ", carry_out=0x" << (int)swcarry << endl;

                throw std::exception("bad unsigned add w/ carry");
            }
        }

        {
            int16sw_t a = static_cast<int16sw_t>(x);
            int16sw_t b = static_cast<int16sw_t>(y);

            uint8_t swcarry = static_cast<uint8_t>(i);
            int16sw_t c = int16sw_t::add_carry(a, b, swcarry);

            uint8_t hwcarry = static_cast<uint8_t>(i);
            uint16_t z;
            hwcarry = _addcarry_u16(hwcarry, x, y, &z);

            if (memcmp(&c, &z, sizeof(uint16_t))
                || memcmp(&swcarry, &hwcarry, sizeof(uint8_t)))
            {
                cout << "carry_in: " << i << endl;
                cout << std::hex << "hw: x=0x" << x << ", y=0x" << y << ", c=0x" << z << ", carry=0x" << (int)hwcarry << endl;
                cout << std::hex << "sw: x=0x" << (uint16_t)a << ", y=0x" << (uint16_t)b << ", c=0x" << (uint16_t)c << ", carry=0x" << (int)swcarry << endl;

                throw std::exception("bad signed add w/ carry");
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
                for (int j = 0; j < std::numeric_limits<uint16_t>::max(); ++j) {
                    auto b = static_cast<uint16_t>(j);
                    validate_add(a, b);
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
            }
            catch (std::exception e)
            {
                cout << "test failed: " << e.what() << endl;
                std::terminate();
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
