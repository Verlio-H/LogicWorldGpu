// This file implements a custom variation of ieee-754 half precision floats
// Same as ieee-754 half precision floats, but:
//  1. No subnormals (except for 0), any underflow will underflow to 0
//  2. No infinity/NaN, but the max number will be returned in the event of an overflow
//  3. Bias changed from -15 to -16 to better accomodate the previous changes
//  4. 1s complement used instead of sign + magnitude in order to allow signed integer comparisons to work for floats
// This float format is nearly equivalent to that used in the IRIS redstone computer

// Exceptions are thrown on any overflows or NaN result but not on underflows

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

float halfToFloat(uint16_t input) {
    if (input == 0) return 0.0f;

    float sign;
    if (input & 0x8000) {
        sign = -1;
        input = ~input;
    } else {
        sign = 1;
    }

    int16_t exp = ((input & 0x7C00) >> 10) - 16;
    uint16_t imantissa = (input & 0x03FF) + 0x0400;
    float fmantissa = ldexpf((float)imantissa, -10);
    float result = ldexpf(fmantissa, exp) * sign;
    return result;
}

int16_t halfToInt(uint16_t input) {
    return (int16_t)halfToFloat(input);
}

uint16_t floatToHalf(float input, bool *exception) {
    int exp;
    float mantissa = frexpf(input, &exp) * 2;
    --exp; //exp from frexpf is off by one from normalized value

    uint16_t imantissa = (uint16_t)(fabs(mantissa) * 0x400 + 0.5);
    if (imantissa >= 0x800) {
        imantissa >>= 1;
        ++exp;
    }

    imantissa &= 0x3FF;

    if (isinf(mantissa) || isnan(mantissa) || exp > 15) {
        *exception = true;
        if (mantissa < 0) {
            return 0x8000;
        }
        return 0x7FFF;
    }
    if (mantissa == 0 || exp < -16) {
        return 0x0000;
    }
    
    uint16_t result = ((exp + 16) << 10) + imantissa;
    if (mantissa < 0) {
        result = ~result;
    }
    return result;
}

uint16_t intToHalf(int16_t input, bool *exception) {
    return floatToHalf(input, exception);
}

uint16_t halfAdd(uint16_t inputA, uint16_t inputB, bool *exception) {
    return floatToHalf(halfToFloat(inputA) + halfToFloat(inputB), exception);
}

uint16_t halfSub(uint16_t inputA, uint16_t inputB, bool *exception) {
    return floatToHalf(halfToFloat(inputA) - halfToFloat(inputB), exception);
}
uint16_t halfMlt(uint16_t inputA, uint16_t inputB, bool *exception) {
    return floatToHalf(halfToFloat(inputA) * halfToFloat(inputB), exception);
}

uint16_t halfDiv(uint16_t inputA, uint16_t inputB, bool *exception) {
    return floatToHalf(halfToFloat(inputA) / halfToFloat(inputB), exception);
}

uint16_t halfSqrt(uint16_t inputA, bool *exception) {
    return floatToHalf(sqrtf(halfToFloat(inputA)), exception);
}
