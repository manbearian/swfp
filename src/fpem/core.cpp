
#include <stdint.h>
#include <iostream>
#include <memory>

#include <limits>

#include "fpcore.h"


using std::cout;
using std::endl;

void validate_add(float a, float b)
{
    float c = a + b;
    float32_t x = a;
    float32_t y = b;
    float32_t z = x + y;

    if (memcmp((char*)&c, (char*)&z, sizeof(float)) != 0)
    {
        cout << "addition failed!" << endl;
        cout << "x: " << a << " " << x.to_hex_string() << " " << x.to_triplet_string() << endl;
        cout << "y: " << b << " " << y.to_hex_string() << " " << y.to_triplet_string() << endl;

        float32_t z_ = c;
        cout << "expected: " << c << " " << z_.to_hex_string() << " " << z_.to_triplet_string() << endl;
        cout << "actual:   " << (float)z << " " << z.to_hex_string() << " " << z.to_triplet_string() << endl;

        throw std::exception("Failure: 'add'");
    }
}

void validate_add()
{
    cout << "Validating 'add'" << endl;

    // testing all values is too slow
    // test some suggested values

    cout << "testing a few denormals...";

    for (uint32_t i = 1; i < 0xff; ++i) {
        for (uint32_t j = 0x7f; j < 0x0fff; ++j) {
            float x = *(float *)&i;
            float y = *(float *)&j;
            validate_add(x, y);
        }
    }
    cout << "okay!" << endl;

    cout << "testing a few negative denormals...";
    for (uint32_t i = 0x80000001; i < 0x800000ff; ++i) {
        for (uint32_t j = 0x8000007f; j < 0x80000fff; ++j) {
            float x = *(float *)&i;
            float y = *(float *)&j;
            validate_add(x, y);
        }
    }
    cout << "okay!" << endl;

    cout << "testing a neg+pos denormals...";
    for (uint32_t i = 0x80000001; i < 0x800000ff; ++i) {
        for (uint32_t j = 1; j < 0xff; ++j) {
            float x = *(float *)&i;
            float y = *(float *)&j;
            validate_add(x, y);
        }
    }
    cout << "okay!" << endl;

    cout << "testing a pos+neg denormals...";
    for (uint32_t i = 1; i < 0xff; ++i) {
        for (uint32_t j = 0x80000001; j < 0x800000ff; ++j) {
            float x = *(float *)&i;
            float y = *(float *)&j;
            validate_add(x, y);
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
                validate_add(x, y);
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
                validate_add(x, y);
            }
        }
    }
    cout << "okay!" << endl;

    cout << "testing a neg+pos denormals...";
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
                validate_add(x, y);
            }
        }
    }
    cout << "okay!" << endl;

    cout << "testing a pos+neg denormals...";
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
                validate_add(x, y);
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
                validate_add(x, y);
            }
        }
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
                validate_add(x, y);
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
        validate_add();
#else
        int i = 0xbf800040;
        int j = 0xc348007f;
        float x = *(float*)&i;
        float y = *(float*)&j;
        //float x = -2.0f;
        //float y = 1.0f;
        validate_add(x, y);
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
