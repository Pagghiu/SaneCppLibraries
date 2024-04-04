// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "sc_hashing.h"
#include <assert.h>
#include <string.h>

#define SC_TEST_EXPECT(a)                                                                                              \
    if (!(a))                                                                                                          \
    {                                                                                                                  \
        assert(a);                                                                                                     \
        return #a;                                                                                                     \
    }

const char* sc_hashing_test_init(sc_hashing_type type, const uint8_t* expected, const size_t expected_size, bool update)
{
    sc_hashing_t ctx;
    SC_TEST_EXPECT(sc_hashing_init(&ctx, type));
    SC_TEST_EXPECT(sc_hashing_add(&ctx, (sc_hashing_span_t){.data = "test", .length = strlen("test")}));
    if (update)
    {
        SC_TEST_EXPECT(sc_hashing_add(&ctx, (sc_hashing_span_t){.data = "test", .length = strlen("test")}));
    }
    sc_hashing_result_t result;
    SC_TEST_EXPECT(sc_hashing_get(&ctx, &result));
    SC_TEST_EXPECT(result.size == expected_size);
    SC_TEST_EXPECT(memcmp(result.hash, expected, result.size) == 0);
    sc_hashing_close(&ctx);
    return 0;
}

const char* sc_hashing_test(void)
{
    const char*   res;
    const uint8_t md5_expected[] = {0x09, 0x8F, 0x6B, 0xCD, 0x46, 0x21, 0xD3, 0x73,
                                    0xCA, 0xDE, 0x4E, 0x83, 0x26, 0x27, 0xB4, 0xF6};

    res = sc_hashing_test_init(SC_HASHING_TYPE_MD5, md5_expected, sizeof(md5_expected), false);
    if (res)
        return res;

    const uint8_t sha1_expected[] = {0xA9, 0x4A, 0x8F, 0xE5, 0xCC, 0xB1, 0x9B, 0xA6, 0x1C, 0x4C,
                                     0x08, 0x73, 0xD3, 0x91, 0xE9, 0x87, 0x98, 0x2F, 0xBB, 0xD3};

    res = sc_hashing_test_init(SC_HASHING_TYPE_SHA1, sha1_expected, sizeof(sha1_expected), false);
    if (res)
        return res;

    const uint8_t sha256_expected[] = {0x9F, 0x86, 0xD0, 0x81, 0x88, 0x4C, 0x7D, 0x65, 0x9A, 0x2F, 0xEA,
                                       0xA0, 0xC5, 0x5A, 0xD0, 0x15, 0xA3, 0xBF, 0x4F, 0x1B, 0x2B, 0x0B,
                                       0x82, 0x2C, 0xD1, 0x5D, 0x6C, 0x15, 0xB0, 0xF0, 0x0A, 0x08};

    res = sc_hashing_test_init(SC_HASHING_TYPE_SHA256, sha256_expected, sizeof(sha256_expected), false);
    if (res)
        return res;

    const uint8_t md5_expected2[] = {0x05, 0xA6, 0x71, 0xC6, 0x6A, 0xEF, 0xEA, 0x12,
                                     0x4C, 0xC0, 0x8B, 0x76, 0xEA, 0x6D, 0x30, 0xBB};

    res = sc_hashing_test_init(SC_HASHING_TYPE_MD5, md5_expected2, sizeof(md5_expected2), true);
    if (res)
        return res;

    const uint8_t sha1_expected2[] = {0x51, 0xAB, 0xB9, 0x63, 0x60, 0x78, 0xDE, 0xFB, 0xF8, 0x88,
                                      0xD8, 0x45, 0x7A, 0x7C, 0x76, 0xF8, 0x5C, 0x8F, 0x11, 0x4C};

    res = sc_hashing_test_init(SC_HASHING_TYPE_SHA1, sha1_expected2, sizeof(sha1_expected2), true);
    if (res)
        return res;

    const uint8_t sha256_expected2[] = {0x37, 0x26, 0x83, 0x35, 0xDD, 0x69, 0x31, 0x04, 0x5B, 0xDC, 0xDF,
                                        0x92, 0x62, 0x3F, 0xF8, 0x19, 0xA6, 0x42, 0x44, 0xB5, 0x3D, 0x0E,
                                        0x74, 0x6D, 0x43, 0x87, 0x97, 0x34, 0x9D, 0x4D, 0xA5, 0x78};

    res = sc_hashing_test_init(SC_HASHING_TYPE_SHA256, sha256_expected2, sizeof(sha256_expected2), true);
    if (res)
        return res;

    return 0;
}
