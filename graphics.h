#include <GLFW/glfw3.h>

extern unsigned char* data;
extern unsigned char* data2;
extern unsigned char* bwrite;
extern int buffered;
extern int width, height;
extern int width2, height2;
extern void* mtxptr;
extern GLFWwindow* window;

void initRender(int (*renderFunction)(void *));

