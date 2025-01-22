/* Example usage of emulator
 * Draws 2 triangles
*/


#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "include/c11threads.h" // This can be replaced by threads.h if available

#include "gpu.h"
#include "graphics.h"

//Approximate MacOS compile command:
//clang main.c gpu.c graphics.c include/*.c -lglfw -framework OpenGL -Ofast -march=native -Wall -Wextra
//Approximate Linux compile command:
//clang main.c gpu.c graphics.c include/*.c -lglfw -lGL -lm -lpthread -Ofast -march=native -Wall -Wextra

const uint32_t vertexShader[] = {
    025010000, 025020002, 030020214, 006030101, 000020203, 030040200, 030050204, 031040301, 
    031050305, 047000000, 000000000
};

const uint32_t geometryShader[] = {
    025010000, 025020002, 030020214, 006030102, 000020203, 030040200, 030050204, 030060210,
    017110001, 034110011, 006031103, 006121102, 000030312, 030130401, 030140501, 030150601,
    000551300, 011361315, 011371413, 011401513, 017432500, 006434304, 013413743, 013424043,
    013434343, 043000000, 001001413, 046000004, 000161400, 000141300, 000131600, 044000000,
    043000000, 001001513, 046000004, 000131500, 045000000, 001001514, 046000005, 000141500,
    044000000, 017151600, 006151504, 010141415, 030250405, 030260505, 030270605, 000562500,
    011522725, 011502526, 011512625, 015131300, 012414152, 016611300, 011556155, 015141400,
    012425142, 001141413, 011414142, 004141401, 007141401, 006141402, 013414341, 043000000,
    001002625, 046000004, 000162600, 000262500, 000251600, 044000000, 012525241, 012505041,
    012363641, 012373741, 012575552, 043000000, 001272500, 046000004, 000252700, 045000000,
    001002726, 046000005, 000262700, 044000000, 010262615, 015252500, 012605550, 016612500,
    017011000, 011566156, 031010310, 015262600, 001262625, 012615636, 004262601, 010575761,
    007262601, 012615637, 031140300, 010606061, 031260304, 031130314, 031250320, 031520324,
    031500330, 031360334, 031370340, 031570344, 031600350, 047000000, 000000000
};

const uint32_t fragmentShader[] = {
    025010000, 025020001, 025030002, 030040314, 030050320, 030120324, 030130330, 030140334,
    030150340, 030160344, 030170350, 006020201, 017060001, 002060106, 000020206, 007010101,
    016200100, 016210200, 012121220, 012131320, 012141421, 012151521, 000770502, 010121214,
    017021700, 010131315, 017302500, 010121216, 000760401, 043000000, 010131317, 006303004,
    013121230, 000121200, 046000005, 013131330, 006020204, 011020212, 000131300, 046000005,
    017012677, 011020213, 006010104, 004010110, 012121201, 000020200, 046000005, 012131301,
    012020201, 015121200, 015131300, 015020200, 032121302, 044000000, 047000000, 000000000
};

#define countof(x) (sizeof(x)/sizeof((x)[0]))

int run(void *args) {
    bool *stop = *(bool **)args;

    //copy shaders
    for (size_t i = 0; i < countof(vertexShader); ++i) {
        atomic_store(gpu_instructionMem + 0 + i, vertexShader[i]);
    }

    for (size_t i = 0; i < countof(geometryShader); ++i) {
        atomic_store(gpu_instructionMem + 256 + i, geometryShader[i]);
    }

    for (size_t i = 0; i < countof(fragmentShader); ++i) {
        atomic_store(gpu_instructionMem + 512 + i, fragmentShader[i]);
    }

    //create VBO
    _Float16 tmpFloat;
    tmpFloat = 0.1 * width;
    atomic_store(gpu_deviceMemory + 1024 + 0*2 + 0, *(uint16_t *)&tmpFloat);
    tmpFloat = 0.1 * width;
    atomic_store(gpu_deviceMemory + 1024 + 0*2 + 1, *(uint16_t *)&tmpFloat);
    
    tmpFloat = 0.9 * width;
    atomic_store(gpu_deviceMemory + 1024 + 1*2 + 0, *(uint16_t *)&tmpFloat);
    tmpFloat = 0.1 * width;
    atomic_store(gpu_deviceMemory + 1024 + 1*2 + 1, *(uint16_t *)&tmpFloat);

    tmpFloat = 0.1 * width;
    atomic_store(gpu_deviceMemory + 1024 + 2*2 + 0, *(uint16_t *)&tmpFloat);
    tmpFloat = 0.9 * width;
    atomic_store(gpu_deviceMemory + 1024 + 2*2 + 1, *(uint16_t *)&tmpFloat);

    tmpFloat = 0.9 * width;
    atomic_store(gpu_deviceMemory + 1024 + 3*2 + 0, *(uint16_t *)&tmpFloat);
    tmpFloat = 0.9 * width;
    atomic_store(gpu_deviceMemory + 1024 + 3*2 + 1, *(uint16_t *)&tmpFloat);

    //create EBO
    atomic_store(gpu_deviceMemory + 2048 + 0*4 + 0, 0*2);
    atomic_store(gpu_deviceMemory + 2048 + 0*4 + 1, 2*2);
    atomic_store(gpu_deviceMemory + 2048 + 0*4 + 2, 1*2);

    atomic_store(gpu_deviceMemory + 2048 + 1*4 + 0, 1*2);
    atomic_store(gpu_deviceMemory + 2048 + 1*4 + 1, 2*2);
    atomic_store(gpu_deviceMemory + 2048 + 1*4 + 2, 3*2);

    //spawn gpu thread
    atomic_store(&gpu_busy, true);
    thrd_t gpu_thread;
    thrd_create(&gpu_thread, startGpu, NULL);

    //wait for gpu to initialize
    while (atomic_load(&gpu_busy)) { 
        thrd_sleep(&(struct timespec){.tv_nsec = 10000}, NULL);
    };

    struct timespec prevTime;
    timespec_get(&prevTime, TIME_UTC);
    for (uint32_t frame = 1; !*stop; ++frame) {
        if (frame % 32 == 0) {
            struct timespec time;
            timespec_get(&time, TIME_UTC);
            printf("frametime: %fms\n", (double)(time.tv_nsec - prevTime.tv_nsec)/1000000/32 + (time.tv_sec - prevTime.tv_sec)*1000/32);
            printf("instruction count: %lld\n", atomic_load(&gpu_instruction_count));
            prevTime = time;
        }

        //encode command buffer

        //stride of 12 to contain triangle information after generation by geometry shader

        //first command (vertex shader)
        atomic_store(gpu_deviceMemory + 0*12 + 0, 4  ); //vertex count
        atomic_store(gpu_deviceMemory + 0*12 + 1, 1  ); //thread count y
        atomic_store(gpu_deviceMemory + 0*12 + 2, 0  ); //shader addr
        atomic_store(gpu_deviceMemory + 0*12 + 3, 1024); //VBO start address in shared buffer
        
        //second command (barrier)
        atomic_store(gpu_deviceMemory + 1*12 + 0, -1);

        //third command (input assembly and geometry)
        atomic_store(gpu_deviceMemory + 2*12 + 0, 2  ); //triangle count
        atomic_store(gpu_deviceMemory + 2*12 + 1, 1  ); //thread count y
        atomic_store(gpu_deviceMemory + 2*12 + 2, 256); //shader address
        atomic_store(gpu_deviceMemory + 2*12 + 3, 2048); //EBO start address

        //fourth command (barrier)
        atomic_store(gpu_deviceMemory + 3*12 + 0, -1);

        //fifth command+ is generated by geometry shader

        //check busy state
        while (atomic_load(&gpu_busy)) { 
            thrd_sleep(&(struct timespec){.tv_nsec = 10}, NULL);
        }

        //write display buffer
        mtx_lock(mtxptr);
        memcpy(data, data2, fmin(width, width2) * fmin(height, height2) * 4);
        mtx_unlock(mtxptr);
        free(data2);

        mtx_lock(mtxptr);
        data2 = malloc(width * height * 4);
        width2 = width;
        height2 = height;

        buffered = 1;

        //clear display buffer
        memset(data2, 0, height * width * 4);

        mtx_unlock(mtxptr);

        //store time for vertex shader usage
        _Float16 time = glfwGetTime();
        atomic_store(gpu_deviceMemory + 4096, *(uint16_t *)&time);

        //store command count in atomic variable
        atomic_store(gpu_atomics + 0, 4);

        //setup the call buffer
        atomic_store(&gpu_commandBufferPtr, 0);
        atomic_store(&gpu_commandStride, 12);
        atomic_store(&gpu_atomicNumber, 0);

        //call gpu
        atomic_store(&gpu_start, true);
        while (true) {
            if (atomic_load(&gpu_busy)) {
                break;
            }
            thrd_sleep(&(struct timespec){.tv_nsec = 10}, NULL);
        }

        atomic_store(&gpu_start, false);

        while (atomic_load(&gpu_busy)) { 
            thrd_sleep(&(struct timespec){.tv_nsec = 10}, NULL);
        }
    }
    
    atomic_store(&gpu_stop, true);
    thrd_join(gpu_thread, NULL);
    return 0;
}

int main(void) {
    initRender(run);
}
