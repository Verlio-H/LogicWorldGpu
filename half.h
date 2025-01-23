#ifndef HALF_H
#define HALF_H

#include <stdbool.h>
#include <stdint.h>

float halfToFloat(uint16_t input);
int16_t halfToInt(uint16_t input);

uint16_t floatToHalf(float input, bool *exception);
uint16_t intToHalf(int16_t input, bool *exception);

uint16_t halfAdd(uint16_t inputA, uint16_t inputB, bool *exception);
uint16_t halfSub(uint16_t inputA, uint16_t inputB, bool *exception);
uint16_t halfMlt(uint16_t inputA, uint16_t inputB, bool *exception);
uint16_t halfDiv(uint16_t inputA, uint16_t inputB, bool *exception);
uint16_t halfSqrt(uint16_t inputA, bool *exception);

#endif
