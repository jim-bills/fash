/*  fash.c
    Douglas Crockford
    2018-03-19

    Public Domain

    This contains the C implementation of

        fash64
        fash256
        rash64
        srash64

    This implementation trades away performance for portability.
*/

#include "uint64.h"
#include "fash.h"

#define prime_11 (11111111111111111027LL)
#define prime_9   (9999999999999999961LL)
#define prime_8   (8888888888888888881LL)
#define prime_7   (7777777777777777687LL)
#define prime_6   (6666666666666666619LL)
#define prime_5   (5555555555555555533LL)
#define prime_4   (4444444444444444409LL)
#define prime_3   (3333333333333333271LL)
#define prime_2   (2222222222222222177LL)
#define prime_1   (1111111111111111037LL)

static uint64 high_umul64(uint64 a, uint64 b) {

/*
    Produce the upper 64 bits of a 64 bit * 64 bit unsigned multiplication.
    This is done by breaking the operands into 32 bit pieces, and using those
    pieces in 4 separate multiplications that are added together.

    Since C is unable to correctly compute the product a * b,

                  xxxx xxxx   a
                * xxxx xxxx   b
        -------------------
        xxxx xxxx xxxx xxxx   no can do

    we instead total 4 sub products.

                  xxxx xxxx   a
                * xxxx xxxx   b
        -------------------
                  xxxx xxxx   a_low * b_low
             xxxx xxxx        a_high * b_low
             xxxx xxxx        a_low * b_high
      + xxxx xxxx             a_high * b_high
        -------------------
        xxxx xxxx xxxx xxxx   done the hard way

    There may be architecturally-dependent ways to do this more efficiently.
    For example, Arm64 has an instruction that implements this function. X64
    has a multiplication instruction that can deliver the entire 128 bit
    product in rax and rdx.
*/

/*
    Make the four pieces.
*/
    uint64 a_low = a & 0xFFFFFFFF;
    uint64 a_high = a >> 32;
    uint64 b_low = b & 0xFFFFFFFF;
    uint64 b_high = b >> 32;
/*
    Make the four sub products.
*/
    uint64 low = a_low * b_low;
    uint64 ab = a_high * b_low;
    uint64 ba = a_low * b_high;
    uint64 high = a_high * b_high;
/*
    Add the two middle sub products. If there is carry, add the carried bit
    to the high sub product.
*/
    uint64 mid = ab + ba;
    if (ab > mid) {
        high += 0x100000000;
    }
/*
    We don't need the low part, but we do need to know if adding the mid part
    to the low part causes a carry.
*/
    if (low > low + ((mid & 0xFFFFFFFF) << 32)) {
        high += 1;
    }
/*
    Add the upper part of the mid sum to the high product.
*/
    high += (mid >> 32);

    return high;
}

static uint64 low_umul64(uint64 a, uint64 b) {
    return (a * b) & 0xFFFFFFFFFFFFFFFFLL;
}

static uint64 f64_product;
static uint64 f64_sum;

void fash64_begin() {
    f64_product = prime_8;
    f64_sum = prime_3;

}

void fash64_word(uint64 word) {
    word ^= f64_product;
    uint64 low = low_umul64(word, prime_11);
    uint64 high = high_umul64(word, prime_11);
    f64_sum += high;
    f64_product = f64_sum ^ low;
}

void fash64_block(uint64* block, uint64 length) {
    uint64 i;
    for (i = 0; i < length; i += 1) {
        fash64_word(block[i]);
    }
}

uint64 fash64_end() {
    return f64_product;
}

 static uint64 f256_a_result;
 static uint64 f256_b_result;
 static uint64 f256_c_result;
 static uint64 f256_d_result;

 static uint64 f256_a_sum;
 static uint64 f256_b_sum;
 static uint64 f256_c_sum;
 static uint64 f256_d_sum;


void fash256_begin() {
    f256_a_result = prime_8;
    f256_b_result = prime_6;
    f256_c_result = prime_4;
    f256_d_result = prime_2;

    f256_a_sum = prime_7;
    f256_b_sum = prime_5;
    f256_c_sum = prime_3;
    f256_d_sum = prime_1;
}

void fash256_word(uint64 word) {
/*
     Mix the word with the current state of the hash
    and multiply it with the big primes.
*/
    uint64 a_low = low_umul64(f256_a_result ^ word, prime_11);
    uint64 b_low = low_umul64(f256_b_result ^ word, prime_9);
    uint64 c_low = low_umul64(f256_c_result ^ word, prime_7);
    uint64 d_low = low_umul64(f256_d_result ^ word, prime_5);

    uint64 a_high = high_umul64(f256_a_result ^ word, prime_11);
    uint64 b_high = high_umul64(f256_b_result ^ word, prime_9);
    uint64 c_high = high_umul64(f256_c_result ^ word, prime_7);
    uint64 d_high = high_umul64(f256_d_result ^ word, prime_5);
/*
    Add the high parts to the sums.
*/
    f256_a_sum += a_high;
    f256_b_sum += b_high;
    f256_c_sum += c_high;
    f256_d_sum += d_high;
/*
    Mix the low parts with sums from another quadrant.
*/
    f256_a_result = a_low ^ f256_d_sum;
    f256_b_result = b_low ^ f256_a_sum;
    f256_c_result = c_low ^ f256_b_sum;
    f256_d_result = d_low ^ f256_c_sum;
}

void fash256_block(uint64 *block, uint64 length) {
    uint64 i;
    for (i = 0; i < length; i += 1) {
        fash256_word(block[i]);
    }
}

void fash256_end(uint64 *result) {
    result[0] = f256_a_result + f256_c_sum;
    result[1] = f256_b_result + f256_d_sum;
    result[2] = f256_c_result + f256_a_sum;
    result[3] = f256_d_result + f256_b_sum;
}

static uint64 r64_counter;
static uint64 r64_result;
static uint64 r64_sum;

void rash64_seed(uint64 seed) {
    r64_counter = seed;
    r64_result = prime_8;
    r64_sum = prime_3;
}

uint64 rash64() {
    r64_result ^= r64_counter;
    r64_counter += 1;
    r64_sum += high_umul64(r64_result, prime_11);
    r64_result  = low_umul64(r64_result, prime_11) ^ r64_sum;
    return r64_result;
}

static uint64 sr_a_product;
static uint64 sr_a_sum;
static uint64 sr_b_product;
static uint64 sr_b_sum;
static uint64 sr_c_product;
static uint64 sr_c_sum;
static uint64 sr_d_product;
static uint64 sr_d_sum;
static uint64 sr_e_product;
static uint64 sr_e_sum;
static uint64 sr_f_product;
static uint64 sr_f_sum;
static uint64 sr_g_product;
static uint64 sr_g_sum;
static uint64 sr_h_product;
static uint64 sr_h_sum;
static uint64 sr_counter;

void srash64_seed(uint64 *seeds) {
    sr_a_product = seeds[0];
    sr_a_sum = seeds[1];
    sr_b_product = seeds[2];
    sr_b_sum = seeds[3];
    sr_c_product = seeds[4];
    sr_c_sum = seeds[5];
    sr_d_product = seeds[6];
    sr_d_sum = seeds[7];
    sr_e_product = seeds[8];
    sr_e_sum = seeds[9];
    sr_f_product = seeds[10];
    sr_f_sum = seeds[11];
    sr_g_product = seeds[12];
    sr_g_sum = seeds[13];
    sr_h_product = seeds[14];
    sr_h_sum = seeds[15];
    sr_counter = 0;
}

uint64 srash64() {
    sr_a_product ^= sr_counter;
    sr_counter += 1;

    uint64 a_low = low_umul64(sr_a_product, prime_11);
    uint64 b_low = low_umul64(sr_b_product, prime_9);
    uint64 c_low = low_umul64(sr_c_product, prime_8);
    uint64 d_low = low_umul64(sr_d_product, prime_7);
    uint64 e_low = low_umul64(sr_e_product, prime_6);
    uint64 f_low = low_umul64(sr_f_product, prime_5);
    uint64 g_low = low_umul64(sr_g_product, prime_4);
    uint64 h_low = low_umul64(sr_h_product, prime_3);

    sr_a_sum += high_umul64(sr_a_product, prime_11);
    sr_b_sum += high_umul64(sr_b_product, prime_9);
    sr_c_sum += high_umul64(sr_c_product, prime_8);
    sr_d_sum += high_umul64(sr_d_product, prime_7);
    sr_e_sum += high_umul64(sr_e_product, prime_6);
    sr_f_sum += high_umul64(sr_f_product, prime_5);
    sr_g_sum += high_umul64(sr_g_product, prime_4);
    sr_h_sum += high_umul64(sr_h_product, prime_3);

    sr_a_product = a_low ^ sr_h_sum;
    sr_b_product = b_low ^ sr_a_sum;
    sr_c_product = c_low ^ sr_b_sum;
    sr_d_product = d_low ^ sr_c_sum;
    sr_e_product = e_low ^ sr_d_sum;
    sr_f_product = f_low ^ sr_e_sum;
    sr_g_product = g_low ^ sr_f_sum;
    sr_h_product = h_low ^ sr_g_sum;

    return (
        ((sr_a_product + sr_e_product) ^ (sr_b_product + sr_f_product)) +
        ((sr_c_product + sr_g_product) ^ (sr_d_product + sr_h_product))
    );
}

void srash64_dump(uint64 *seeds) {
    seeds[0] = sr_a_product;
    seeds[1] = sr_a_sum;
    seeds[2] = sr_b_product;
    seeds[3] = sr_b_sum;
    seeds[4] = sr_c_product;
    seeds[5] = sr_c_sum;
    seeds[6] = sr_d_product;
    seeds[7] = sr_d_sum;
    seeds[8] = sr_e_product;
    seeds[9] = sr_e_sum;
    seeds[10] = sr_f_product;
    seeds[11] = sr_f_sum;
    seeds[12] = sr_g_product;
    seeds[13] = sr_g_sum;
    seeds[14] = sr_h_product;
    seeds[15] = sr_h_sum;
}
