
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

template<size_t byte_size> struct make_integral { using type = void; };
template<> struct make_integral<8> { using type = uint64_t;  };
template<> struct make_integral<4> { using type = uint32_t; };
template<> struct make_integral<2> { using type = uint16_t; };



template<typename sw_t, typename integral_t>
void test_value(integral_t x)
{
    using display_t = details::selector_t<(sizeof(sw_t) > sizeof(int)), int64_t, int>;

    using hw_t = typename make_integral<(sizeof(sw_t))>::type;

    if constexpr (std::is_integral_v<hw_t>)
    {
        hw_t hw = static_cast<hw_t>(x);
        sw_t sw = static_cast<sw_t>(x);

        if (memcmp(&hw, &sw, sizeof(hw_t))) {
            cout << std::hex << "original: 0x" << x << endl;
            cout << std::hex << "to_hw: 0x" << (display_t)hw << endl;
            cout << std::hex << "to_sw: 0x" << (display_t)sw << endl;
            cout << "integral_t: " << typeid(integral_t).name() << endl;
            cout << "sw_t: " << typeid(sw_t).name() << endl;
            cout << "hw_t: " << typeid(hw_t).name() << endl;
            throw std::exception("bad cast");
        }

        integral_t a = static_cast<integral_t>(hw);
        integral_t b = static_cast<integral_t>(sw);

        if (memcmp(&a, &b, sizeof(integral_t))) {
            cout << std::hex << "original: 0x" << x << endl;
            cout << std::hex << "from_hw: 0x" << (display_t)a << endl;
            cout << std::hex << "from_sw: 0x" << (display_t)b << endl;
            cout << "integral_t: " << typeid(integral_t).name() << endl;
            cout << "sw_t: " << typeid(sw_t).name() << endl;
            cout << "hw_t: " << typeid(hw_t).name() << endl;
            throw std::exception("bad cast");
        }
    }
    else
    {
        sw_t sw = static_cast<sw_t>(x);
        integral_t b = static_cast<integral_t>(sw);

        if (memcmp(&x, &b, sizeof(integral_t))) {
            cout << std::hex << "original: 0x" << x << endl;
            cout << std::hex << "to_sw: 0x" << (display_t)sw << endl;
            cout << std::hex << "from_sw: 0x" << (display_t)b << endl;
            cout << "integral_t: " << typeid(integral_t).name() << endl;
            cout << "sw_t: " << typeid(sw_t).name() << endl;
            throw std::exception("bad cast");
        }
    }
}

int main()
{
    try
    {
#if 1
        for (int i = 0; i < std::numeric_limits<uint16_t>::max() * 2; ++i) {
            test_value<int16sw_t>(static_cast<int8_t>(i));
            test_value<uint16sw_t>(static_cast<uint8_t>(i));
            test_value<int16sw_t>(static_cast<int16_t>(i));
            test_value<uint16sw_t>(static_cast<uint16_t>(i));
            test_value<int16sw_t>(static_cast<int32_t>(i));
            test_value<uint16sw_t>(static_cast<uint32_t>(i));
            test_value<int16sw_t>(static_cast<int64_t>(i));
            test_value<uint16sw_t>(static_cast<uint64_t>(i));

            test_value<int32sw_t>(static_cast<int8_t>(i));
            test_value<uint32sw_t>(static_cast<uint8_t>(i));
            test_value<int32sw_t>(static_cast<int16_t>(i));
            test_value<uint32sw_t>(static_cast<uint16_t>(i));
            test_value<int32sw_t>(static_cast<int32_t>(i));
            test_value<uint32sw_t>(static_cast<uint32_t>(i));
            test_value<int32sw_t>(static_cast<int64_t>(i));
            test_value<uint32sw_t>(static_cast<uint64_t>(i));

            test_value<int64sw_t>(static_cast<int8_t>(i));
            test_value<uint64sw_t>(static_cast<uint8_t>(i));
            test_value<int64sw_t>(static_cast<int16_t>(i));
            test_value<uint64sw_t>(static_cast<uint16_t>(i));
            test_value<int64sw_t>(static_cast<int32_t>(i));
            test_value<uint64sw_t>(static_cast<uint32_t>(i));
            test_value<int64sw_t>(static_cast<int64_t>(i));
            test_value<uint64sw_t>(static_cast<uint64_t>(i));

            test_value<int128sw_t>(static_cast<int8_t>(i));
            test_value<uint128sw_t>(static_cast<uint8_t>(i));
            test_value<int128sw_t>(static_cast<int16_t>(i));
            test_value<uint128sw_t>(static_cast<uint16_t>(i));
            test_value<int128sw_t>(static_cast<int32_t>(i));
            test_value<uint128sw_t>(static_cast<uint32_t>(i));
            test_value<int128sw_t>(static_cast<int64_t>(i));
            test_value<uint128sw_t>(static_cast<uint64_t>(i));
        }
#else
        test_value<int16sw_t, int>(0x10000);
#endif
    }
    catch (std::exception e)
    {
        cout << "test failed: " << e.what() << endl;
        return 1;
    }

    cout << "success!" << endl;
    return 0;
}
