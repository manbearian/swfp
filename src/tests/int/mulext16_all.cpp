
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

void validate_mul(uint16_t x, uint16_t y)
{
    {
        uint16sw_t a = static_cast<uint16sw_t>(x);
        uint16sw_t b = static_cast<uint16sw_t>(y);

        uint16sw_t chi, clo = uint16sw_t::multiply_extended(a, b, chi);

        uint32_t z = uint32_t(x) * uint32_t(y);
        uint16_t zhi = static_cast<uint16_t>(z >> 16);
        uint16_t zlo = static_cast<uint16_t>(z);

        if (memcmp(&clo, &zlo, sizeof(uint16_t))
            || memcmp(&chi, &zhi, sizeof(uint16_t)))
        {
            cout << std::hex << "hw: x=0x" << x << ", y=0x" << y << ", zhi=0x" << zhi << ", zlo=0x" << zlo << endl;
            cout << std::hex << "sw: x=0x" << (uint16_t)a << ", y=0x" << (uint16_t)b << ", zhi=0x" << (uint16_t)chi << ", zlo=0x" << (uint16_t)clo  << endl;

            throw std::exception("bad unsigned extended multiply");
        }
    }

    {
        int16sw_t a = static_cast<int16sw_t>(x);
        int16sw_t b = static_cast<int16sw_t>(y);
        int16_t sx = static_cast<int16sw_t>(x);
        int16_t sy = static_cast<int16sw_t>(y);

        int16sw_t chi, clo = int16sw_t::multiply_extended(a, b, chi);

        int32_t z = int32_t(sx) * int32_t(sy);
        int16_t zhi = static_cast<int16_t>((z >> 16) & 0xFFFF);
        int16_t zlo = static_cast<int16_t>(z);

        if (memcmp(&clo, &zlo, sizeof(uint16_t))
            || memcmp(&chi, &zhi, sizeof(uint16_t)))
        {
            cout << std::hex << "hw: x=0x" << x << ", y=0x" << y << ", zhi=0x" << zhi << ", zlo=0x" << zlo << endl;
            cout << std::hex << "sw: x=0x" << (uint16_t)a << ", y=0x" << (uint16_t)b << ", zhi=0x" << (uint16_t)chi << ", zlo=0x" << (uint16_t)clo  << endl;

            throw std::exception("bad signed extended multiply");
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
