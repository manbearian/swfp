
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
#pragma warning(disable:4309)
#pragma warning(disable:4310)

using std::cout;
using std::endl;


void test128left()
{
    //                  0     1     2     3     4     5     6     7     8     9    10    11    12    13    14    15
    //                  0     8    16    24    32    40    48    56    64    72    80    88    96   104   112   120
    uint8_t r1[] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t r2[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t r3[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t r4[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80 };
    uint8_t r5[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00 };
    uint8_t r6[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };


    uint128sw_t ua = uint128sw_t(1);
    uint128sw_t ub = ua << 1;
    uint128sw_t uc = ub << 62;
    uint128sw_t ud = uc << 64;
    uint128sw_t ue = ua << 127;
    uint128sw_t uf = ub << 96;
    uint128sw_t ug = uf << 127;

    if (memcmp(&r1, &ua, 16)) {
        throw std::exception("failed to match 'a' for unsigned shift left");
    }
    if (memcmp(&r2, &ub, 16)) {
        throw std::exception("failed to match 'b' for unsigned shift left");
    }
    if (memcmp(&r3, &uc, 16)) {
        throw std::exception("failed to match 'c' for unsigned shift left");
    }
    if (memcmp(&r4, &ud, 16)) {
        throw std::exception("failed to match 'd' for unsigned shift left");
    }
    if (memcmp(&r4, &ue, 16)) {
        throw std::exception("failed to match 'e' for unsigned shift left");
    }
    if (memcmp(&r5, &uf, 16)) {
        throw std::exception("failed to match 'f' for unsigned shift left");
    }
    if (memcmp(&r6, &ug, 16)) {
        throw std::exception("failed to match 'g' for unsigned shift left");
    }


    int128sw_t sa = int128sw_t(1);
    int128sw_t sb = sa << 1;
    int128sw_t sc = sb << 62;
    int128sw_t sd = sc << 64;
    int128sw_t se = sa << 127;
    int128sw_t sf = sb << 96;
    int128sw_t sg = sf << 127;

    if (memcmp(&r1, &sa, 16)) {
        throw std::exception("failed to match 'a' for signed shift left");
    }
    if (memcmp(&r2, &sb, 16)) {
        throw std::exception("failed to match 'b' for signed shift left");
    }
    if (memcmp(&r3, &sc, 16)) {
        throw std::exception("failed to match 'c' for signed shift left");
    }
    if (memcmp(&r4, &sd, 16)) {
        throw std::exception("failed to match 'd' for signed shift left");
    }
    if (memcmp(&r4, &se, 16)) {
        throw std::exception("failed to match 'e' for signed shift left");
    }
    if (memcmp(&r5, &sf, 16)) {
        throw std::exception("failed to match 'f' for signed shift left");
    }
    if (memcmp(&r6, &sg, 16)) {
        throw std::exception("failed to match 'g' for signed shift left");
    }
}

void test128right()
{
    //                  0     1     2     3     4     5     6     7     8     9    10    11    12    13    14    15
    //                  0     8    16    24    32    40    48    56    64    72    80    88    96   104   112   120
    uint8_t u1[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80 };
    uint8_t u2[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40 };
    uint8_t u3[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t u4[] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t u5[] = { 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t u6[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t s1[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80 };
    uint8_t s2[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0 };
    uint8_t s3[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    uint8_t s4[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    uint8_t s5[] = { 0x00, 0x00, 0x00, 0xC0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    uint8_t s6[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20 };


    uint128sw_t ua = uint128sw_t(1) << 127;
    uint128sw_t ub = ua >> 1;
    uint128sw_t uc = ub >> 62;
    uint128sw_t ud = uc >> 64;
    uint128sw_t ue = ua >> 127;
    uint128sw_t uf = ub >> 96;
    uint128sw_t ug = uf >> 127;

    if (memcmp(&u1, &ua, 16)) {
        throw std::exception("failed to match 'a' for unsigned shift right");
    }
    if (memcmp(&u2, &ub, 16)) {                                       
        throw std::exception("failed to match 'b' for unsigned shift right");
    }
    if (memcmp(&u3, &uc, 16)) {
        throw std::exception("failed to match 'c' for unsigned shift right");
    }
    if (memcmp(&u4, &ud, 16)) {
        throw std::exception("failed to match 'd' for unsigned shift right");
    }
    if (memcmp(&u4, &ue, 16)) {
        throw std::exception("failed to match 'e' for unsigned shift right");
    }
    if (memcmp(&u5, &uf, 16)) {
        throw std::exception("failed to match 'f' for unsigned shift right");
    }
    if (memcmp(&u6, &ug, 16)) {
        throw std::exception("failed to match 'g' for unsigned shift right");
    }


    int128sw_t sa = int128sw_t(1) << 127;
    int128sw_t sb = sa >> 1;
    int128sw_t sc = sb >> 62;
    int128sw_t sd = sc >> 64;
    int128sw_t se = sa >> 127;
    int128sw_t sf = sb >> 96;;
    int128sw_t sg = sf >> 127;

    if (memcmp(&s1, &sa, 16)) {
        throw std::exception("failed to match 'a' for signed shift right");
    }
    if (memcmp(&s2, &sb, 16)) {
        throw std::exception("failed to match 'b' for signed shift right");
    }
    if (memcmp(&s3, &sc, 16)) {
        throw std::exception("failed to match 'c' for signed shift right");
    }
    if (memcmp(&s4, &sd, 16)) {
        throw std::exception("failed to match 'd' for signed shift right");
    }
    if (memcmp(&s4, &se, 16)) {
        throw std::exception("failed to match 'e' for signed shift right");
    }
    if (memcmp(&s5, &sf, 16)) {
        throw std::exception("failed to match 'f' for signed shift right");
    }
    if (memcmp(&s4, &sg, 16)) {
        throw std::exception("failed to match 'g' for signed shift right");
    }


    int128sw_t sa_z = int128sw_t(1) << 126; // 0x7ff..
    int128sw_t sb_z = sa_z >> 1;
    int128sw_t sc_z = sa_z >> 62;
    int128sw_t sd_z = sc_z >> 64;
    int128sw_t se_z = sa_z >> 126;
    int128sw_t sf_z = sb_z >> 95;
    int128sw_t sg_z = sf_z >> 127;

    if (memcmp(&u2, &sa_z, 16)) {
        throw std::exception("failed to match 'a' for signed shift right (zero)");
    }
    if (memcmp(&s6, &sb_z, 16)) {
        throw std::exception("failed to match 'b' for signed shift right (zero)");
    }
    if (memcmp(&u3, &sc_z, 16)) {
        throw std::exception("failed to match 'c' for signed shift right (zero)");
    }
    if (memcmp(&u4, &sd_z, 16)) {
        throw std::exception("failed to match 'd' for signed shift right (zero)");
    }
    if (memcmp(&u4, &se_z, 16)) {
        throw std::exception("failed to match 'e' for signed shift right (zero)");
    }
    if (memcmp(&u5, &sf_z, 16)) {
        throw std::exception("failed to match 'f' for signed shift right (zero)");
    }
    if (memcmp(&u6, &sg_z, 16)) {
        throw std::exception("failed to match 'g' for signed shift right (zero)");
    }
}

template<int bit_size, typename integral_t>
void testleft(integral_t *x)
{
    x[0] = integral_t(1);
    x[1] = x[0] << 1;
    x[2] = x[1] << ((bit_size / 2) - 2);
    x[3] = x[2] << (bit_size / 2);
    x[4] = x[3] << (bit_size - 1);
    x[5] = x[1] << ((bit_size * 3) / 4);
    x[6] = x[2] << (bit_size - 1);
}

template<int bit_size, typename integral_t>
void testright(integral_t *x)
{
    x[0] = integral_t(1) << (bit_size - 1);
    x[1] = x[0] >> 1;
    x[2] = x[1] >> ((bit_size / 2) - 2);
    x[3] = x[2] >> (bit_size / 2);
    x[4] = x[3] >> (bit_size - 1);
    x[5] = x[1] << ((bit_size * 3) / 4);
    x[6] = x[2] >> (bit_size - 1);

    x[7]  = integral_t(1) << (bit_size - 2); // 0x7ff..
    x[8]  = x[7] >> 1;
    x[9]  = x[8] >> ((bit_size / 2) - 2);
    x[10] = x[9] >> (bit_size / 2);
    x[11] = x[10] >> (bit_size - 2);
    x[12] = x[7] >> (((bit_size * 3) / 4) - 1);
    x[13] = x[8] >> (bit_size - 1);
}

void test64left()
{
    uint64_t uhw[7];
    uint64sw_t usw[7];
    int64_t shw[7];
    int64sw_t ssw[7];

    testleft<64>(uhw);
    testleft<64>(usw);
    if (memcmp(uhw, usw, sizeof(uhw))) {
        throw std::exception("failed unsigned shift left)");
    }

    testleft<64>(shw);
    testleft<64>(ssw);
    if (memcmp(shw, ssw, sizeof(uhw))) {
        throw std::exception("failed signed shift left)");
    }
}
void test64right()
{
    uint64_t uhw[14];
    uint64sw_t usw[14];
    int64_t shw[14];
    int64sw_t ssw[14];

    testright<64>(uhw);
    testright<64>(usw);
    if (memcmp(uhw, usw, sizeof(uhw))) {
        throw std::exception("failed unsigned shift right");
    }

    testright<64>(shw);
    testright<64>(ssw);
    if (memcmp(shw, ssw, sizeof(uhw))) {
        throw std::exception("failed signed shift right");
    }
}

void test32left()
{
    uint32_t uhw[7];
    uint32sw_t usw[7];
    int32_t shw[7];
    int32sw_t ssw[7];

    testleft<32>(uhw);
    testleft<32>(usw);
    if (memcmp(uhw, usw, sizeof(uhw))) {
        throw std::exception("failed unsigned shift left)");
    }

    testleft<32>(shw);
    testleft<32>(ssw);
    if (memcmp(shw, ssw, sizeof(uhw))) {
        throw std::exception("failed signed shift left)");
    }
}

void test32right()
{
    uint32_t uhw[14];
    uint32sw_t usw[14];
    int32_t shw[14];
    int32sw_t ssw[14];

    testright<32>(uhw);
    testright<32>(usw);
    if (memcmp(uhw, usw, sizeof(uhw))) {
        throw std::exception("failed unsigned shift right");
    }

    testright<32>(shw);
    testright<32>(ssw);
    if (memcmp(shw, ssw, sizeof(uhw))) {
        throw std::exception("failed signed shift right");
    }
}

void test16left()
{
    uint16_t uhw[7];
    uint16sw_t usw[7];
    int16_t shw[7];
    int16sw_t ssw[7];

    testleft<16>(uhw);
    testleft<16>(usw);
    if (memcmp(uhw, usw, sizeof(uhw))) {
        throw std::exception("failed unsigned shift left)");
    }

    testleft<16>(shw);
    testleft<16>(ssw);
    if (memcmp(shw, ssw, sizeof(uhw))) {
        throw std::exception("failed signed shift left)");
    }
}

void test16right()
{
    uint16_t uhw[14];
    uint16sw_t usw[14];
    int16_t shw[14];
    int16sw_t ssw[14];

    testright<16>(uhw);
    testright<16>(usw);
    if (memcmp(uhw, usw, sizeof(uhw))) {
        throw std::exception("failed unsigned shift right");
    }

    testright<16>(shw);
    testright<16>(ssw);
    if (memcmp(shw, ssw, sizeof(uhw))) {
        throw std::exception("failed signed shift right");
    }
}

int main()
{
    try
    {
        cout << "Testing 128 shift left..." << endl;
        test128left();
        cout << "...okay!" << endl;

        cout << "Testing 128 shift right..." << endl;
        test128right();
        cout << "...okay!" << endl;

        cout << "Testing 64 shift left..." << endl;
        test64left();
        cout << "...okay!" << endl;

        cout << "Testing 64 shift right..." << endl;
        test64right();
        cout << "...okay!" << endl;

        cout << "Testing 32 shift left..." << endl;
        test32left();
        cout << "...okay!" << endl;

        cout << "Testing 32 shift right..." << endl;
        test32right();
        cout << "...okay!" << endl;

        cout << "Testing 16 shift left..." << endl;
        test32left();
        cout << "...okay!" << endl;

        cout << "Testing 16 shift right..." << endl;
        test32right();
        cout << "...okay!" << endl;
    }
    catch (std::exception e)
    {
        cout << "test failed: " << e.what() << endl;
        return 1;
    }

    cout << "success!" << endl;
    return 0;
}
