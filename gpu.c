#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "include/c11threads.h"

#include "half.h"
#include "gpu.h"
#include "graphics.h"

#define BUFFER_COUNT 15

typedef struct execUnit {
    atomic_bool ready;
    atomic_llong absoluteMask;
    atomic_llong mask;
    atomic_short pc;
    uint64_t instructionCount;
    uint32_t decodeInst;
    uint64_t mstack[32];
    uint8_t msp;
    uint16_t cstack[16];
    uint8_t csp;
    uint16_t registers[WAVE_SIZE][64];
    uint16_t mem[256*WAVE_SIZE];
    atomic_short info[WAVE_SIZE][4];
    atomic_bool flags[WAVE_SIZE][16];
    uint16_t pipeliningBufferVals[WAVE_SIZE][BUFFER_COUNT];
    uint8_t pipeliningBufferDests[WAVE_SIZE][BUFFER_COUNT];
    thrd_t thread;
} execUnit;

#define FLAG_Z 0 //zero
#define FLAG_NZ 1 //not zero
#define FLAG_C 2 //carry out
#define FLAG_NC 3 //not carry out
#define FLAG_N 4 //negative
#define FLAG_NN 5 //not negative
#define FLAG_P 6
#define FLAG_NP 7
#define FLAG_V 8 //overflow
#define FLAG_NV 9 //not overflow
#define FLAG_O 10 //odd
#define FLAG_NO 11 //even
#define FLAG_LT 12 //signed less than
#define FLAG_GE 13 //signed greater than or equal to
#define FLAG_E 14 //exception (float)
#define FLAG_F 15 //false

#define BUFFER_FADD_0 0
#define BUFFER_FADD_1 1
#define BUFFER_FADD_2 2
#define BUFFER_FADD_3 3
#define BUFFER_FMLT_0 4
#define BUFFER_FMLT_1 5
#define BUFFER_FDIV_0 6
#define BUFFER_FDIV_1 7
#define BUFFER_FDIV_2 8
#define BUFFER_FDIV_3 9
#define BUFFER_FDIV_4 10
#define BUFFER_FSQRT_0 11
#define BUFFER_FSQRT_1 12
#define BUFFER_FSQRT_2 13
#define BUFFER_FSQRT_3 14

#define UNUSED(x) ((void)x)

//#define DEBUG 1
//#define DEBUG_COMMANDS 1

typedef enum gpuOp {
    OP_ADD, //0
    OP_SUB, //1
    OP_AND, //2
    OP_XOR, //3
    OP_ADDI, //4
    OP_SUBI, //5
    OP_BSL, //6
    OP_BSR, //7
    OP_FADD, //8
    OP_FSUB, //9
    OP_FMLT, //10
    OP_FDIV, //11
    OP_FSQRT, //12
    OP_FTOI, //13
    OP_ITOF, //14
    OP_IMM, //15
    OP_SIMM, //16
    OP_RNG, //17
    OP_WLOD, //18
    OP_WSTR, //19
    OP_CLOD, //20
    OP_ILOD, //21
    OP_DLODL, //22
    OP_DSTRL, //23
    OP_DLOD, //24
    OP_DSTR, //25
    OP_OUT, //26
    OP_ASTR, //27
    OP_AADD, //28

    OP_JMPMZ, //29
    OP_JMPMNZ, //30
    OP_CAL, //31
    OP_RET, //32
    OP_SETM, //33
    OP_SETAM, //34
    OP_PSHM, //35
    OP_POPM, //36
    OP_XORM, //37
    OP_FLGM, //38
    OP_HLT //39
} gpuOp;

atomic_int gpu_instructionMem[65536];
atomic_short gpu_constBuffer[65536];
atomic_short gpu_atomics[16];

atomic_llong gpu_instruction_count;

atomic_short gpu_deviceMemory[262144];

atomic_bool gpu_busy;
atomic_bool gpu_start;
atomic_bool gpu_stop; //exists for threading purposes

atomic_short gpu_commandBufferPtr;
atomic_short gpu_commandStride;
atomic_short gpu_atomicNumber;

execUnit gpu[CORE_COUNT];

#ifdef INSPECT
    atomic_short gpu_inspectX;
    atomic_short gpu_inspectY;
#endif

void updateFlags(execUnit *core, int thread, uint16_t result) {
    if (result == 0) {
        core->flags[thread][FLAG_Z] = true;
        core->flags[thread][FLAG_NZ] = false;
    } else {
        core->flags[thread][FLAG_Z] = false;
        core->flags[thread][FLAG_NZ] = true;
    }

    if (result & 0x8000) {
        core->flags[thread][FLAG_N] = true;
        core->flags[thread][FLAG_NN] = false;
    } else {
        core->flags[thread][FLAG_N] = false;
        core->flags[thread][FLAG_NN] = true;
    }

    if ((int16_t)result > 0) {
        core->flags[thread][FLAG_P] = true;
        core->flags[thread][FLAG_NP] = false;
    } else {
        core->flags[thread][FLAG_P] = false;
        core->flags[thread][FLAG_NP] = true;
    }

    if (result & 1) {
        core->flags[thread][FLAG_O] = true;
        core->flags[thread][FLAG_NO] = false;
    } else {
        core->flags[thread][FLAG_O] = false;
        core->flags[thread][FLAG_NO] = true;
    }

    core->flags[thread][FLAG_C] = false;
    core->flags[thread][FLAG_NC] = true;
    core->flags[thread][FLAG_V] = false;
    core->flags[thread][FLAG_NV] = true;
    core->flags[thread][FLAG_LT] = core->flags[thread][FLAG_N] ^ core->flags[thread][FLAG_V];
    core->flags[thread][FLAG_GE] = !core->flags[thread][FLAG_LT];
    core->flags[thread][FLAG_E] = false;
    core->flags[thread][FLAG_F] = false;
}

void execThreadedInstruction(execUnit *core, uint32_t op, uint32_t op1, uint32_t op2, uint32_t op3) {
    for (uint8_t thread = 0; thread < WAVE_SIZE; ++thread) {
        uint64_t threadBit = ((uint64_t)1) << thread;
        if (!((core->mask & threadBit) && (core->absoluteMask & threadBit))) {
            continue;
        }
        core->registers[thread][0] = 0;
        switch (op) {
            case (OP_ADD): {
                uint16_t input1 = core->registers[thread][op2];
                uint16_t input2 = core->registers[thread][op3];
                uint16_t result = input1 + input2;
                updateFlags(core, thread, result);
                if (result < core->registers[thread][op2]) {
                    core->flags[thread][FLAG_C] = true;
                    core->flags[thread][FLAG_NC] = false;
                } else {
                    core->flags[thread][FLAG_C] = false;
                    core->flags[thread][FLAG_NC] = true;
                }
                if ((input1 & 0x8000) == (input2 & 0x8000) && (result & 0x8000) != (input1 & 0x8000)) {
                    core->flags[thread][FLAG_V] = true;
                    core->flags[thread][FLAG_NV] = false;
                } else {
                    core->flags[thread][FLAG_V] = false;
                    core->flags[thread][FLAG_NV] = true;
                }
                core->flags[thread][FLAG_LT] = core->flags[thread][FLAG_N] ^ core->flags[thread][FLAG_V];
                core->flags[thread][FLAG_GE] = !core->flags[thread][FLAG_LT];
                core->registers[thread][op1] = result;
                break;
            }
            case (OP_SUB): {
                uint16_t input1 = core->registers[thread][op2];
                uint16_t input2 = core->registers[thread][op3];
                uint16_t result = input1 - input2;
                updateFlags(core, thread, result);
                if (result < core->registers[thread][op2]) {
                    core->flags[thread][FLAG_C] = true;
                    core->flags[thread][FLAG_NC] = false;
                } else {
                    core->flags[thread][FLAG_C] = false;
                    core->flags[thread][FLAG_NC] = true;
                }
                if ((input1 & 0x8000) != (input2 & 0x8000) && (result & 0x8000) != (input1 & 0x8000)) {
                    core->flags[thread][FLAG_V] = true;
                    core->flags[thread][FLAG_NV] = false;
                } else {
                    core->flags[thread][FLAG_V] = false;
                    core->flags[thread][FLAG_NV] = true;
                }
                core->flags[thread][FLAG_LT] = core->flags[thread][FLAG_N] ^ core->flags[thread][FLAG_V];
                core->flags[thread][FLAG_GE] = !core->flags[thread][FLAG_LT];
                core->registers[thread][op1] = result;
                break;
            }
            case (OP_AND): {
                uint16_t result = core->registers[thread][op2] & core->registers[thread][op3];
                updateFlags(core, thread, result);
                core->registers[thread][op1] = result;
                break;
            }
            case (OP_XOR): {
                uint16_t result = core->registers[thread][op2] ^ core->registers[thread][op3];
                updateFlags(core, thread, result);
                core->registers[thread][op1] = result;
                break;
            }
            case (OP_ADDI): {
                uint16_t input1 = core->registers[thread][op2];
                uint16_t input2 = op3;
                uint16_t result = input1 + input2;
                updateFlags(core, thread, result);
                if (result < core->registers[thread][op2]) {
                    core->flags[thread][FLAG_C] = true;
                    core->flags[thread][FLAG_NC] = false;
                } else {
                    core->flags[thread][FLAG_C] = false;
                    core->flags[thread][FLAG_NC] = true;
                }
                if ((input1 & 0x8000) == (input2 & 0x8000) && (result & 0x8000) != (input1 & 0x8000)) {
                    core->flags[thread][FLAG_V] = true;
                    core->flags[thread][FLAG_NV] = false;
                } else {
                    core->flags[thread][FLAG_V] = false;
                    core->flags[thread][FLAG_NV] = true;
                }
                core->registers[thread][op1] = result;
                break;
            }
            case (OP_SUBI): {
                uint16_t input1 = core->registers[thread][op2];
                uint16_t input2 = op3;
                uint16_t result = input1 - input2;
                updateFlags(core, thread, result);
                if (result < core->registers[thread][op2]) {
                    core->flags[thread][FLAG_C] = true;
                    core->flags[thread][FLAG_NC] = false;
                } else {
                    core->flags[thread][FLAG_C] = false;
                    core->flags[thread][FLAG_NC] = true;
                }
                if ((input1 & 0x8000) != (input2 & 0x8000) && (result & 0x8000) != (input1 & 0x8000)) {
                    core->flags[thread][FLAG_V] = true;
                    core->flags[thread][FLAG_NV] = false;
                } else {
                    core->flags[thread][FLAG_V] = false;
                    core->flags[thread][FLAG_NV] = true;
                }
                core->registers[thread][op1] = result;
                break;
            }
            case (OP_BSL): {
                uint16_t result = core->registers[thread][op2] << op3;
                updateFlags(core, thread, result);
                core->registers[thread][op1] = result;
                break;
            }
            case (OP_BSR): {
                uint16_t result;
                if (op3 & 040) {
                    op3 = op3 & 037;
                    result = (int16_t)core->registers[thread][op2] >> op3;
                } else {
                    result = (uint16_t)core->registers[thread][op2] >> op3;
                }
                updateFlags(core, thread, result);
                core->registers[thread][op1] = result;
                break;
            }
            case (OP_FADD): {
                uint16_t op2Val = core->registers[thread][op2];
                uint16_t op3Val = core->registers[thread][op3];
                bool exception;
                uint16_t result = halfAdd(op2Val, op3Val, &exception);
                core->pipeliningBufferVals[thread][BUFFER_FADD_0] = result;
                core->pipeliningBufferDests[thread][BUFFER_FADD_0] = op1;
                core->flags[thread][FLAG_E] = exception;
                break;
            }
            case (OP_FSUB): {
                uint16_t op2Val = core->registers[thread][op2];
                uint16_t op3Val = core->registers[thread][op3];
                bool exception;
                uint16_t result = halfSub(op2Val, op3Val, &exception);
                core->pipeliningBufferVals[thread][BUFFER_FADD_0] = result;
                core->pipeliningBufferDests[thread][BUFFER_FADD_0] = op1;
                core->flags[thread][FLAG_E] = exception;
                break;
            }
            case (OP_FMLT): {
                uint16_t op2Val = core->registers[thread][op2];
                uint16_t op3Val = core->registers[thread][op3];
                bool exception;
                uint16_t result = halfMlt(op2Val, op3Val, &exception);
                core->pipeliningBufferVals[thread][BUFFER_FMLT_0] = result;
                core->pipeliningBufferDests[thread][BUFFER_FMLT_0] = op1;
                core->flags[thread][FLAG_E] = exception;
                break;
            }
            case (OP_FDIV): {
                uint16_t op2Val = core->registers[thread][op2];
                uint16_t op3Val = core->registers[thread][op3];
                bool exception;
                uint16_t result = halfDiv(op2Val, op3Val, &exception);
                core->pipeliningBufferVals[thread][BUFFER_FDIV_0] = result;
                core->pipeliningBufferDests[thread][BUFFER_FDIV_0] = op1;
                core->flags[thread][FLAG_E] = exception;
                break;
            }
            case (OP_FSQRT): {
                uint16_t op2Val = core->registers[thread][op2];
                bool exception;
                uint16_t result = halfSqrt(op2Val, &exception);
                core->pipeliningBufferVals[thread][BUFFER_FSQRT_0] = result;
                core->pipeliningBufferDests[thread][BUFFER_FSQRT_0] = op1;
                core->flags[thread][FLAG_E] = exception;
                break;
            }
            case (OP_FTOI): {
                uint16_t op2Val = core->registers[thread][op2];
                core->registers[thread][op1] = halfToInt(op2Val);
                break;
            }
            case (OP_ITOF): {
                int16_t op2Val = core->registers[thread][op2];
                bool exception;
                core->registers[thread][op1] = intToHalf(op2Val, &exception);
                core->flags[thread][FLAG_E] = exception;
                break;
            }
            case (OP_IMM): {
                core->registers[thread][op1] = (((uint16_t)op2) << 6) + (uint16_t)op3;
                break;
            }
            case (OP_SIMM): {
                core->registers[thread][op1] = (((uint16_t)op2) << 6) + (uint16_t)op3;
                if (op2 & 040) {
                    core->registers[thread][op1] += 0xF000;
                }
                break;
            }
            case (OP_RNG): {
                core->registers[thread][op1] = rand() & 0xFFFF;
                break;
            }
            case (OP_WLOD): {
                core->registers[thread][op1] = core->mem[core->registers[thread][op2] + op3];
                break;
            }
            case (OP_WSTR): {
                core->mem[core->registers[thread][op2] + op3] = core->registers[thread][op1];
                break;
            }
            case (OP_CLOD): {
                core->registers[thread][op1] = atomic_load(&gpu_constBuffer[core->registers[thread][op2] + op3]);
                break;
            }
            case (OP_ILOD): {
                core->registers[thread][op1] = atomic_load(&core->info[thread][core->registers[thread][op2] + op3]);
                break;
            }
            case (OP_DLODL): {
                uint32_t addr = (core->registers[thread][op2] << 16) + core->registers[thread][op3];
                core->registers[thread][op1] = atomic_load(&gpu_deviceMemory[addr]);
                break;
            }
            case (OP_DSTRL): {
                uint32_t addr = (core->registers[thread][op2] << 16) + core->registers[thread][op3];
                atomic_store(&gpu_deviceMemory[addr], core->registers[thread][op1]);
                break;
            }
            case (OP_DLOD): {
                uint32_t addr = core->registers[thread][op2] + (((uint32_t)op3 & 3) << 16) + (op3 >> 2);
                core->registers[thread][op1] = atomic_load(&gpu_deviceMemory[addr]);
                break;
            }
            case (OP_DSTR): {
                uint32_t addr = core->registers[thread][op2] + (((uint32_t)op3 & 3) << 16) + (op3 >> 2);
                atomic_store(&gpu_deviceMemory[addr], core->registers[thread][op1]);
                break;
            }
            case (OP_OUT): {
                int16_t x = core->registers[thread][62];
                int16_t y = core->registers[thread][63];
                uint16_t colorRed = core->registers[thread][op1];
                uint16_t colorGreen = core->registers[thread][op2];
                uint16_t colorBlue = core->registers[thread][op3];
                if (x<width && y<height) {
                    #ifdef INSPECT
                        if (atomic_load(&gpu_inspectX) == x && atomic_load(&gpu_inspectY) == y) {
                            printf("Output at (%d, %d): (%d, %d, %d), ", x, y, colorRed, colorGreen, colorBlue);
                            printf("Thread ID: (%d, %d), ", core->info[thread][0], core->info[thread][1]);
                            printf("Command ID: %d\n", core->info[thread][2]);
                        }
                    #endif

                    mtx_lock(mtxptr);

                    uint8_t *bwrite;
                    if (buffered) {
                        bwrite = data2;
                    } else {
                        bwrite = data;
                    }
                    
                    uint16_t buffWidth = buffered ? width2 : width;
                    uint16_t buffHeight = buffered ? height2 : height;
                    bwrite[(buffHeight - y - 1) * buffWidth * 4 + x * 4 + 0] = colorRed & 0xFF;
                    bwrite[(buffHeight - y - 1) * buffWidth * 4 + x * 4 + 1] = colorGreen & 0xFF;
                    bwrite[(buffHeight - y - 1) * buffWidth * 4 + x * 4 + 2] = colorBlue & 0xFF;

                    mtx_unlock(mtxptr);
                }
                break;
            }
            case (OP_ASTR): {
                atomic_store(&gpu_atomics[core->registers[thread][op2]], core->registers[thread][op3]);
                break;
            }
            case (OP_AADD): {
                int16_t atomicNum = core->registers[thread][op2];
                int16_t addValue = core->registers[thread][op3];
                core->registers[thread][op1] = atomic_fetch_add(&gpu_atomics[atomicNum], addValue);
                break;
            }
            case (OP_FLGM): {
                core->mask ^= (1l - core->flags[thread][op3]) << thread;
                break;
            }
            case (OP_HLT): {
                core->absoluteMask ^= (1l) << thread;
                break;
            }
            default: {
                break;
            }
        }
    }
}

int runCore(void *arg) {
    uint16_t id = *(uint16_t *)arg;
    free(arg);
    execUnit *core = &gpu[id];
    core->absoluteMask = 0;
    core->decodeInst = 0;

    for (uint8_t i = 0; i < WAVE_SIZE; ++i) {
        for (uint8_t j = 0; j < BUFFER_COUNT; ++j) {
            core->pipeliningBufferVals[i][j] = 0;
            core->pipeliningBufferDests[i][j] = 0;
        }
    }

    core->instructionCount = 0;
    atomic_store(&core->ready, true);

    for (;;) {
        if (core->absoluteMask == 0) {
            core->csp = -1;
            core->msp = -1;
            atomic_store(&core->ready, true);
            atomic_fetch_add(&gpu_instruction_count, core->instructionCount);
            core->instructionCount = 0;
            if (atomic_load(&gpu_stop)) {
                return 0;
            }
            thrd_sleep(&(struct timespec){.tv_nsec = 10}, NULL);
        } else {
            atomic_store(&core->ready, false);
            ++core->instructionCount;
            uint32_t instruction = core->decodeInst;
            core->decodeInst = gpu_instructionMem[core->pc];
            gpuOp op = (instruction >> 18) & 077;
            uint32_t op1 = (instruction >> 12) & 077;
            uint32_t op2 = (instruction >> 6) & 077;
            uint32_t op3 = instruction & 077;

            for (uint8_t thread = 0; thread < WAVE_SIZE; ++thread) {

                core->registers[thread][core->pipeliningBufferDests[thread][BUFFER_FADD_3]] = core->pipeliningBufferVals[thread][BUFFER_FADD_3];
                core->pipeliningBufferDests[thread][BUFFER_FADD_3] = core->pipeliningBufferDests[thread][BUFFER_FADD_2];
                core->pipeliningBufferVals[thread][BUFFER_FADD_3] = core->pipeliningBufferVals[thread][BUFFER_FADD_2];
                core->pipeliningBufferDests[thread][BUFFER_FADD_2] = core->pipeliningBufferDests[thread][BUFFER_FADD_1];
                core->pipeliningBufferVals[thread][BUFFER_FADD_2] = core->pipeliningBufferVals[thread][BUFFER_FADD_1];
                core->pipeliningBufferDests[thread][BUFFER_FADD_1] = core->pipeliningBufferDests[thread][BUFFER_FADD_0];
                core->pipeliningBufferVals[thread][BUFFER_FADD_1] = core->pipeliningBufferVals[thread][BUFFER_FADD_0];
                core->pipeliningBufferDests[thread][BUFFER_FADD_0] = 0;
                core->pipeliningBufferVals[thread][BUFFER_FADD_0] = 0;

                core->registers[thread][core->pipeliningBufferDests[thread][BUFFER_FMLT_1]] = core->pipeliningBufferVals[thread][BUFFER_FMLT_1];
                core->pipeliningBufferDests[thread][BUFFER_FMLT_1] = core->pipeliningBufferDests[thread][BUFFER_FMLT_0];
                core->pipeliningBufferVals[thread][BUFFER_FMLT_1] = core->pipeliningBufferVals[thread][BUFFER_FMLT_0];
                core->pipeliningBufferDests[thread][BUFFER_FMLT_0] = 0;
                core->pipeliningBufferVals[thread][BUFFER_FMLT_0] = 0;

                core->registers[thread][core->pipeliningBufferDests[thread][BUFFER_FDIV_4]] = core->pipeliningBufferVals[thread][BUFFER_FDIV_4];
                core->pipeliningBufferDests[thread][BUFFER_FDIV_4] = core->pipeliningBufferDests[thread][BUFFER_FDIV_3];
                core->pipeliningBufferVals[thread][BUFFER_FDIV_4] = core->pipeliningBufferVals[thread][BUFFER_FDIV_3];
                core->pipeliningBufferDests[thread][BUFFER_FDIV_3] = core->pipeliningBufferDests[thread][BUFFER_FDIV_2];
                core->pipeliningBufferVals[thread][BUFFER_FDIV_3] = core->pipeliningBufferVals[thread][BUFFER_FDIV_2];
                core->pipeliningBufferDests[thread][BUFFER_FDIV_2] = core->pipeliningBufferDests[thread][BUFFER_FDIV_1];
                core->pipeliningBufferVals[thread][BUFFER_FDIV_2] = core->pipeliningBufferVals[thread][BUFFER_FDIV_1];
                core->pipeliningBufferDests[thread][BUFFER_FDIV_1] = core->pipeliningBufferDests[thread][BUFFER_FDIV_0];
                core->pipeliningBufferVals[thread][BUFFER_FDIV_1] = core->pipeliningBufferVals[thread][BUFFER_FDIV_0];
                core->pipeliningBufferDests[thread][BUFFER_FDIV_0] = 0;
                core->pipeliningBufferVals[thread][BUFFER_FDIV_0] = 0;

                core->registers[thread][core->pipeliningBufferDests[thread][BUFFER_FSQRT_3]] = core->pipeliningBufferVals[thread][BUFFER_FSQRT_3];
                core->pipeliningBufferDests[thread][BUFFER_FSQRT_3] = core->pipeliningBufferDests[thread][BUFFER_FSQRT_2];
                core->pipeliningBufferVals[thread][BUFFER_FSQRT_3] = core->pipeliningBufferVals[thread][BUFFER_FSQRT_2];
                core->pipeliningBufferDests[thread][BUFFER_FSQRT_2] = core->pipeliningBufferDests[thread][BUFFER_FSQRT_1];
                core->pipeliningBufferVals[thread][BUFFER_FSQRT_2] = core->pipeliningBufferVals[thread][BUFFER_FSQRT_1];
                core->pipeliningBufferDests[thread][BUFFER_FSQRT_1] = core->pipeliningBufferDests[thread][BUFFER_FSQRT_0];
                core->pipeliningBufferVals[thread][BUFFER_FSQRT_1] = core->pipeliningBufferVals[thread][BUFFER_FSQRT_0];
                core->pipeliningBufferDests[thread][BUFFER_FSQRT_0] = 0;
                core->pipeliningBufferVals[thread][BUFFER_FSQRT_0] = 0;

                if (instruction >= (1 << 24)) {
                    #ifdef DEBUG
                        uint32_t prevInstruction = atomic_load(&gpu_instructionMem[core->pc-2]);
                        uint32_t prevOp1 = (prevInstruction >> 12) & 0b111111;
                        uint32_t prevOp2 = (prevInstruction >> 6) & 0b111111;
                        uint32_t prevOp3 = prevInstruction & 0b111111;
                        printf("Hit thread %d breakpoint %d: %x, %x, %x\n", thread, instruction >> 24, core->registers[thread][prevOp1],core->registers[thread][prevOp2],core->registers[thread][prevOp3]);
                    #endif
                }
            }

            switch (op) {
                case (OP_JMPMNZ): {
                    if (core->mask != 0) {
                        core->pc = (op2 << 16) + op3;
                    }
                    continue;
                }
                case (OP_JMPMZ): {
                    if (core->mask == 0) {
                        core->pc = (op2 << 16) + op3;
                    }
                    continue;
                }
                case (OP_CAL): {
                    ++core->csp;
                    core->cstack[core->csp] = core->pc + 1;
                    core->pc = (op2 << 16) + op3;
                    continue;
                }
                case (OP_RET): {
                    core->pc = core->cstack[core->csp];
                    --core->csp;
                    continue;
                }
                case (OP_SETM): {
                    core->mask = (op1 << 12) + (op2 << 6) + op3;
                    break;
                }
                case (OP_SETAM): {
                    core->absoluteMask = (op1 << 12) + (op2 << 6) + op3;
                    break;
                }
                case (OP_PSHM): {
                    ++core->msp;
                    core->mstack[core->msp] = core->mask;
                    break;
                }
                case (OP_POPM): {
                    core->mask = core->mstack[core->msp];
                    --core->msp;
                    break;
                }
                case (OP_XORM): {
                    core->mask ^= core->mstack[core->msp];
                    break;
                }
                default: {
                    execThreadedInstruction(core, op, op1, op2, op3);
                }
            }

            
            
            ++core->pc;
        }
    }
}

void executeCommandBuffer(void) {
    uint16_t queue[WAVE_SIZE][3];
    bool queueMask[WAVE_SIZE];
    uint16_t queuePtr = 0;
    for (uint8_t i = 0; i < WAVE_SIZE; ++i) {
        queue[i][0] = 0;
        queue[i][1] = 0;
        queueMask[i] = false;
    }

    for (uint16_t commandId = 0; commandId < atomic_load(&gpu_atomics[gpu_atomicNumber]); ++commandId) {
        uint16_t commandPtr = commandId * gpu_commandStride + gpu_commandBufferPtr;
        uint16_t countx = gpu_deviceMemory[commandPtr]; //potentially should be atomic but whatevs
        uint16_t county = gpu_deviceMemory[commandPtr + 1];
        #ifdef DEBUG_COMMANDS
            for (int i = 0; i < 5; ++i) {
                printf("%5d ", gpu_deviceMemory[commandPtr + i]);
            }
            for (int i = 5; i <= 11; ++i) {
                printf("%10.7f ", halfToFloat(gpu_deviceMemory[commandPtr + i]));
            }
            printf("\n");
        #endif
        #ifdef DEBUG
            printf("count x: %d, count y: %d\n", countx, county);
        #endif
        if ((int16_t)countx == -1) {
            for (;;) {
                bool exit = true;
                for (int core = 0; core < CORE_COUNT; ++core) {
                    if (!atomic_load(&gpu[core].ready)) {
                        exit = false;
                    }
                }
                if (exit) break;
            }
            continue;
        }
        uint16_t shaderAddr = gpu_deviceMemory[commandPtr + 2];
        
        for (uint16_t threadIdy = 0; threadIdy < county; ++threadIdy) {
            for (uint16_t threadIdx = 0; threadIdx < countx; ++threadIdx) {
                //add to queue
                queue[queuePtr][0] = threadIdx;
                queue[queuePtr][1] = threadIdy;
                queue[queuePtr][2] = commandPtr;
                queueMask[queuePtr] = true;
                ++queuePtr;
                #ifdef DEBUG
                    printf("scheduling command %d thread %d %d\n", commandPtr, threadIdx, threadIdy);
                #endif

                if (queuePtr == WAVE_SIZE || (threadIdy * countx + threadIdx) == (countx * county) - 1) {
                    //find free core
                    uint16_t core = 0;
                    for (;; core = (core + 1) % CORE_COUNT) {
                        if (atomic_load(&gpu[core].ready)) {
                            break;
                        }

                        if (core == CORE_COUNT - 1) {
                            thrd_sleep(&(struct timespec){.tv_nsec = 10}, NULL);
                        }
                    }
                    #ifdef DEBUG
                        printf("core %d chosen\n", core);
                    #endif

                    atomic_store(&gpu[core].pc, shaderAddr);
                    //this has to be mem copied because of stack allocation
                    uint64_t absoluteMask = 0;
                    for (uint8_t i = 0; i < WAVE_SIZE; ++i) {
                        atomic_store(&gpu[core].info[i][0], queue[i][0]);
                        atomic_store(&gpu[core].info[i][1], queue[i][1]);
                        atomic_store(&gpu[core].info[i][2], queue[i][2]);
                        atomic_store(&gpu[core].info[i][3], i);
                        absoluteMask += queueMask[i] << i;
                    }
                    gpu[core].mask = ((uint64_t)1 << WAVE_SIZE) - 1;
                    atomic_store(&gpu[core].absoluteMask, absoluteMask);

                    //reset queue
                    for (uint8_t i = 0; i < WAVE_SIZE; ++i) {
                        queue[i][0] = 0;
                        queue[i][1] = 0;
                        queue[i][2] = 0;
                        queueMask[i] = false;
                    }
                    queuePtr = 0;

                    while (atomic_load(&gpu[core].ready) && atomic_load(&gpu[core].absoluteMask)) {
                        thrd_sleep(&(struct timespec){.tv_nsec = 10}, NULL);
                    }
                }
            }
        }
    }
}

int startGpu(void *args) {
    UNUSED(args);
    srand(time(NULL));
    atomic_store(&gpu_start, 0);
    atomic_store(&gpu_stop, 0);
    atomic_store(&gpu_instruction_count, 0);

    //init execution threads
    for (uint16_t i = 0; i < CORE_COUNT; ++i) {
        atomic_store(&gpu[i].absoluteMask, 0);
        atomic_store(&gpu[i].ready, false);
        int *id = malloc(sizeof(int));
        *id = i;
        thrd_create(&gpu[i].thread, runCore, id);
    }

    atomic_store(&gpu_busy, 0);

    while (!atomic_load(&gpu_stop)) {
        if (atomic_load(&gpu_start)) {
            atomic_store(&gpu_busy, 1);
            executeCommandBuffer();
            for (;;) {
                bool exit = true;
                for (uint16_t core = 0; core < CORE_COUNT; ++core) {
                    if (!atomic_load(&gpu[core].ready)) {
                        exit = false;
                    }
                }
                if (exit) break;
            }
            atomic_store(&gpu_busy, 0);
        }
        thrd_sleep(&(struct timespec){.tv_nsec = 10}, NULL);
    }

    for (uint16_t i = 0; i < CORE_COUNT; ++i) {
        thrd_join(gpu[i].thread, NULL);
    }
    
    return 0;
}
