// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <string.h>

int main(void)
{
    if (OPENSSL_init_crypto(0, NULL) != 1 || OPENSSL_init_ssl(0, NULL) != 1)
        return 2;

    if (OPENSSL_version_major() != 3 || OPENSSL_version_minor() != 6 || OPENSSL_version_patch() != 5)
        return 3;

    static const unsigned char expected[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
    };
    static const unsigned char input[] = "abc";
    unsigned char              digest[EVP_MAX_MD_SIZE];
    unsigned int               digestLength  = 0;
    EVP_MD_CTX*                digestContext = EVP_MD_CTX_new();
    if (!digestContext)
        return 4;
    int digestValid = EVP_DigestInit_ex(digestContext, EVP_sha256(), NULL) == 1 &&
                      EVP_DigestUpdate(digestContext, input, sizeof(input) - 1) == 1 &&
                      EVP_DigestFinal_ex(digestContext, digest, &digestLength) == 1;
    EVP_MD_CTX_free(digestContext);
    if (!digestValid || digestLength != sizeof(expected) || memcmp(digest, expected, sizeof(expected)) != 0)
        return 5;

    SSL_CTX* context = SSL_CTX_new(TLS_method());
    if (!context)
        return 6;
    SSL_CTX_free(context);
    return 0;
}
