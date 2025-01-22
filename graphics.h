#include <GLFW/glfw3.h>
#include "include/c11threads.h"

extern unsigned char* data;
extern unsigned char* data2;
extern unsigned char* bwrite;
extern int buffered;
extern int width, height;
extern int width2, height2;
extern mtx_t* mtxptr;
extern GLFWwindow* window;

void initRender(int (*renderFunction)(void *));

