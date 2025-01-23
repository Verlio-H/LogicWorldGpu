#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <GLFW/glfw3.h>
#include <stdbool.h>

#include "include/c11threads.h"

extern uint8_t* data;
extern uint8_t* data2;
extern bool buffered;
extern uint16_t width, height;
extern uint16_t width2, height2;
extern mtx_t* mtxptr;

void initRender(int (*renderFunction)(void *));

struct renderArgs {
    bool stop;
    double mousePosX;
    double mousePosY;
};

#endif
