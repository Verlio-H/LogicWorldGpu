#ifndef GPU_H
#define GPU_H

#include <stdbool.h>
#include <stdatomic.h>

#define WAVE_SIZE 31
#define CORE_COUNT 4
#define THRD_CNT (WAVE_SIZE * CORE_COUNT)

extern atomic_short gpu_deviceMemory[262144];
extern atomic_int gpu_instructionMem[65536];
extern atomic_short gpu_constBuffer[65536];
extern atomic_short gpu_atomics[16];

extern atomic_llong gpu_instruction_count;

extern atomic_bool gpu_busy;
extern atomic_bool gpu_start;
extern atomic_bool gpu_stop; //exists for threading purposes

extern atomic_short gpu_commandBufferPtr;
extern atomic_short gpu_commandStride;
extern atomic_short gpu_atomicNumber;

int startGpu(void *args);

#endif
