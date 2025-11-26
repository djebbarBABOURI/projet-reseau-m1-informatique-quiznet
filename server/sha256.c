#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "sqlite/sqlite3.h"
#include "handlers.h"
#include "db_utils.h"
#include "utils.h"

// ==== Implémentation SHA-256 (public domain) ====
// typedef struct {
//     unsigned int state[8];
//     unsigned long long bitcount;
//     unsigned char buffer[64];
// } sha256_ctx;

//
static const unsigned int sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};


//
void sha256_init(sha256_ctx *ctx) {
    ctx->bitcount = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

//
void sha256_update(sha256_ctx *ctx, const unsigned char *data, size_t len) {
    unsigned int i;
    for (i = 0; i < len; ++i) {
        ctx->buffer[ctx->bitcount & 0x3F] = data[i];
        ctx->bitcount++;
        if ((ctx->bitcount & 0x3F) == 0) {
            sha256_transform(ctx, ctx->buffer);
        }
    }
}


//
void sha256_final(sha256_ctx *ctx, unsigned char hash[32]) {
    unsigned int i = ctx->bitcount & 0x3F;
    ctx->buffer[i++] = 0x80;

    if (i > 56) {
        while (i < 64) ctx->buffer[i++] = 0x00;
        sha256_transform(ctx, ctx->buffer);
        i = 0;
    }

    while (i < 56) ctx->buffer[i++] = 0x00;

    unsigned long long bitcount_be = ctx->bitcount << 3;
    for (int j = 7; j >= 0; --j) {
        ctx->buffer[i++] = (unsigned char)((bitcount_be >> (j * 8)) & 0xFF);
    }

    sha256_transform(ctx, ctx->buffer);

    for (int j = 0; j < 8; ++j) {
        hash[j * 4] = (ctx->state[j] >> 24) & 0xFF;
        hash[j * 4 + 1] = (ctx->state[j] >> 16) & 0xFF;
        hash[j * 4 + 2] = (ctx->state[j] >> 8) & 0xFF;
        hash[j * 4 + 3] = ctx->state[j] & 0xFF;
    }
}


//
void sha256_transform(sha256_ctx *ctx, const unsigned char data[]) {
    unsigned int m[64];
    unsigned int a,b,c,d,e,f,g,h;

    for (unsigned int i = 0, j = 0; i < 16; ++i, j += 4) {
        m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
    }

    for (unsigned int i = 16; i < 64; ++i) {
        unsigned int s0 = rotr(m[i - 15], 7) ^ rotr(m[i - 15], 18) ^ (m[i - 15] >> 3);
        unsigned int s1 = rotr(m[i - 2], 17) ^ rotr(m[i - 2], 19) ^ (m[i - 2] >> 10);
        m[i] = m[i - 16] + s0 + m[i - 7] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (unsigned int i = 0; i < 64; ++i) {
        unsigned int s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        unsigned int ch = (e & f) ^ ((~e) & g);
        unsigned int temp1 = h + s1 + ch + sha256_k[i] + m[i];
        unsigned int s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        unsigned int maj = (a & b) ^ (a & c) ^ (b & c);
        unsigned int temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

//
unsigned int rotr(unsigned int value, unsigned int count) {
    return (value >> count) | (value << (32 - count));
}