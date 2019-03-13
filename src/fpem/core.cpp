
#include <stdint.h>
#include <iostream>
#include <memory>
#include <string>

#include <limits>

#include "fpcore.h"

// disbale constant arithmetic warnings
#pragma warning(disable:4756)

using std::cout;
using std::endl;

template<typename T>
struct validate_base
{
    static void check_binary(float a, float b, float c, float32_t x, float32_t y, float32_t z)
    {
        if (memcmp((char*)&c, (char*)&z, sizeof(float)) != 0)
        {
            cout << "failed!" << endl;
            cout << "x: " << a << " " << x.to_hex_string() << " " << x.to_triplet_string() << endl;
            cout << "y: " << b << " " << y.to_hex_string() << " " << y.to_triplet_string() << endl;

            float32_t z_ = c;
            cout << "expected: " << c << " " << z_.to_hex_string() << " " << z_.to_triplet_string() << endl;
            cout << "actual:   " << (float)z << " " << z.to_hex_string() << " " << z.to_triplet_string() << endl;

            auto err = std::string{ "Failure: '" } + T::name() + "'";
            throw std::exception(err.c_str());
        }
    }
};

struct validate_add : public validate_base<validate_add>
{
    static std::string name() { return "add"; }

    static void validate(float a, float b)
    {
        float c = a + b;
        float32_t x = a;
        float32_t y = b;
        float32_t z = x + y;

        check_binary(a, b, c, x, y, z);
    }
};

struct validate_sub : public validate_base<validate_sub>
{
    static std::string name() { return "sub"; }

    static void validate(float a, float b)
    {
        float c = a - b;
        float32_t x = a;
        float32_t y = b;
        float32_t z = x - y;

        check_binary(a, b, c, x, y, z);
    }
};

struct validate_mul : public validate_base<validate_mul>
{
    static std::string name() { return "mul"; }

    static void validate(float a, float b)
    {
        float c = a * b;
        float32_t x = a;
        float32_t y = b;
        float32_t z = x * y;

        check_binary(a, b, c, x, y, z);
    }
};

struct validate_div : public validate_base<validate_div>
{
    static std::string name() { return "div"; }

    static void validate(float a, float b)
    {
        float c = a / b;
        float32_t x = a;
        float32_t y = b;
        float32_t z = x / y;

        check_binary(a, b, c, x, y, z);
    }
};


template<typename T>
void validate()
{
    cout << "Validating '" << T::name() << "'" << endl;

    // testing all values is too slow
    // test some suggested values

    cout << "testing a few denormals...";

    for (uint32_t i = 1; i < 0xff; ++i) {
        for (uint32_t j = 0x7f; j < 0x0fff; ++j) {
            float x = *(float *)&i;
            float y = *(float *)&j;
            T::validate(x, y);
        }
    }
    cout << "okay!" << endl;

    cout << "testing a few negative denormals...";
    for (uint32_t i = 0x80000001; i < 0x800000ff; ++i) {
        for (uint32_t j = 0x8000007f; j < 0x80000fff; ++j) {
            float x = *(float *)&i;
            float y = *(float *)&j;
            T::validate(x, y);
        }
    }
    cout << "okay!" << endl;

    cout << "testing a neg+pos denormals...";
    for (uint32_t i = 0x80000001; i < 0x800000ff; ++i) {
        for (uint32_t j = 1; j < 0xff; ++j) {
            float x = *(float *)&i;
            float y = *(float *)&j;
            T::validate(x, y);
        }
    }
    cout << "okay!" << endl;

    cout << "testing a pos+neg denormals...";
    for (uint32_t i = 1; i < 0xff; ++i) {
        for (uint32_t j = 0x80000001; j < 0x800000ff; ++j) {
            float x = *(float *)&i;
            float y = *(float *)&j;
            T::validate(x, y);
        }
    }
    cout << "okay!" << endl;

    cout << "testing a few normals...";
    {
        float start = 1.0f;
        uint32_t start_i = *(uint32_t *)&start;

        for (uint32_t i = 0; i < 0xff; ++i) {
            for (uint32_t j = 0x7f; j < 0xfff; ++j) {
                uint32_t x_i = start_i + i;
                uint32_t y_i = start_i + j;
                float x = *(float *)&x_i;
                float y = *(float *)&y_i;
                T::validate(x, y);
            }
        }
    }
    cout << "okay!" << endl;


    cout << "testing a few negative normals...";
    {
        float start1 = -1.0f;
        float start2 = -200.0f;
        uint32_t start_i = *(uint32_t *)&start1;
        uint32_t start_j = *(uint32_t *)&start2;

        for (uint32_t i = 0; i < 0xff; ++i) {
            for (uint32_t j = 0x7f; j < 0xfff; ++j) {
                uint32_t x_i = start_i + i;
                uint32_t y_i = start_j + j;
                float x = *(float *)&x_i;
                float y = *(float *)&y_i;
                T::validate(x, y);
            }
        }
    }
    cout << "okay!" << endl;

    cout << "testing a neg+pos normals...";
    {
        float start1 = -100.0f;
        float start2 = 100.0f;
        uint32_t start_i = *(uint32_t *)&start1;
        uint32_t start_j = *(uint32_t *)&start2;

        for (uint32_t i = 0; i < 0xff; ++i) {
            for (uint32_t j = 0x7f; j < 0xfff; ++j) {
                uint32_t x_i = start_i + i;
                uint32_t y_i = start_j + j;
                float x = *(float *)&x_i;
                float y = *(float *)&y_i;
                T::validate(x, y);
            }
        }
        T::validate(std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
        T::validate(std::numeric_limits<float>::max(), -(std::numeric_limits<float>::max() / 2));
        T::validate((std::numeric_limits<float>::max() / 2), -std::numeric_limits<float>::max());
    }
    cout << "okay!" << endl;

    cout << "testing a pos+neg normals...";
    {
        float start1 = 100.0f;
        float start2 = -100.0f;
        uint32_t start_i = *(uint32_t *)&start1;
        uint32_t start_j = *(uint32_t *)&start2;

        for (uint32_t i = 0; i < 0xff; ++i) {
            for (uint32_t j = 0x7f; j < 0xfff; ++j) {
                uint32_t x_i = start_i + i;
                uint32_t y_i = start_j + j;
                float x = *(float *)&x_i;
                float y = *(float *)&y_i;
                T::validate(x, y);
            }
        }
    }
    cout << "okay!" << endl;

    cout << "testing values far apart...";
    {
        float start1 = 1.0f;
        float start2 = 1234556789.0f;
        uint32_t start_i = *(uint32_t *)&start1;
        uint32_t start_j = *(uint32_t *)&start2;

        for (uint32_t i = 0; i < 0xff; ++i) {
            for (uint32_t j = 0x7f; j < 0xfff; ++j) {
                uint32_t x_i = start_i + i;
                uint32_t y_i = start_j + j;
                float x = *(float *)&x_i;
                float y = *(float *)&y_i;
                T::validate(x, y);
            }
        }
        T::validate(std::numeric_limits<float>::max(), std::numeric_limits<float>::min());
    }
    cout << "okay!" << endl;

    cout << "testing values really far apart...";
    {
        float start1 = 0.0000000001f;
        float start2 = 1234556789.0f;
        uint32_t start_i = *(uint32_t *)&start1;
        uint32_t start_j = *(uint32_t *)&start2;

        for (uint32_t i = 0; i < 0xff; ++i) {
            for (uint32_t j = 0x7f; j < 0xfff; ++j) {
                uint32_t x_i = start_i + i;
                uint32_t y_i = start_j + j;
                float x = *(float *)&x_i;
                float y = *(float *)&y_i;
                T::validate(x, y);
            }
        }
    }
    cout << "okay!" << endl;

    cout << "testing values that go to infinity...";
    T::validate(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    T::validate(std::numeric_limits<float>::max(), std::numeric_limits<float>::max() / 2);
    T::validate(std::numeric_limits<float>::max(), std::numeric_limits<float>::max() / 1000);
    T::validate(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    T::validate(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() / 2);
    T::validate(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() / 1000);
    cout << "okay!" << endl;

    cout << "testing special values...";
    {
        float values[] = { 0.0f, 1.0f,
            std::numeric_limits<float>::max() / 2,
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::min(),
            std::numeric_limits<float>::min() / 2,
            std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::signaling_NaN() };


        for (int i = 0; i < _countof(values); ++i) {
            for (int j = 0; j < _countof(values); ++j) {
                T::validate(values[i], values[j]);
                T::validate(-values[i], values[j]);
                T::validate(values[i], -values[j]);
                T::validate(-values[i], -values[j]);
            }
        }
    }
    cout << "okay!" << endl;
}

int main()
{
    try
    {
#if 1
        validate<validate_add>();
        validate<validate_sub>();
        validate<validate_mul>();
        validate<validate_div>();
#else
        int i = 0x3f800000;
        int j = 0x400000;
        float x = *(float*)&i;
        float y = *(float*)&j;
        //float x = std::numeric_limits<float>::max();
        //float y = std::numeric_limits<float>::max();
        validate_div::validate(x, y);
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
