#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#include "include/c11threads.h"
#include "include/glad.h"
#include <GLFW/glfw3.h>

#define WIDTH 256
#define HEIGHT 256

uint8_t* data = NULL;
uint8_t* data2;
uint8_t* bwrite;
int buffered = 0;
int width, height;
int width2, height2;
void* mtxptr;
int rwidth,rheight;

mtx_t windowmtx;
GLFWwindow *window;

uint32_t texture;

float vertices[] = {
    1.0, -1.0, 0.0,
    1.0,  1.0, 0.0,
   -1.0, -1.0, 0.0,
   -1.0,  1.0, 0.0
};

void framebuffer_size_callback(GLFWwindow* window, int wwidth, int wheight) {
    mtx_lock(&windowmtx);
    rwidth = wwidth;
    rheight = wheight;
    mtx_unlock(&windowmtx);
    char* title = malloc(64);
    snprintf(title,64,"%dx%d",width,height);
    glfwSetWindowTitle(window,title);
}

char *readFile(char *filename) {
    char *buffer = 0;
    int len;
    FILE *f = fopen(filename, "rb");

    if (f) {
        fseek(f, 0, SEEK_END);
        len = ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = malloc(len + 1);
        if (buffer) {
            fread(buffer, 1, len, f);
        }
        fclose(f);
        buffer[len] = '\0';
    }
    return buffer;
}

uint32_t makeShader(const char *vertSrc, char *afragSrc) {
    char *thing = "\0";
    const char *fragSrc = strncat(afragSrc, thing, 16);
    uint32_t vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertSrc, NULL);
    glCompileShader(vertexShader);

    // check for shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        printf("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s\n", infoLog);
    }

    // fragment shader
    uint32_t fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragSrc, NULL);
    glCompileShader(fragmentShader);

    // check for shader compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        printf("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n%s\n", infoLog);
    }

    // link shaders
    uint32_t shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        printf("ERROR::SHADER::PROGRAM::LINKING_FAILED\n%s\n", infoLog);
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

void initRender(int (*renderFunction)(void *)) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window = glfwCreateWindow(WIDTH, HEIGHT, "", NULL, NULL);
    if (!window) {
        glfwTerminate();
        printf("glfw failed to create a window!\n");
        return;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("glad init failed!\n");
        return;
    }

    glfwGetFramebufferSize(window, &width, &height);
    glfwSetWindowAspectRatio(window, width, height);
    rwidth = width;
    rheight = height;
    glViewport(0, 0, width, height);
    width = WIDTH;
    height = HEIGHT;
    data = malloc(width*height*4);
    char* title = malloc(64);
    snprintf(title, 64, "%dx%d", width, height);
    glfwSetWindowTitle(window, title);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    uint32_t VBO, VAO;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), NULL);
    glEnableVertexAttribArray(0);

    const char *vertexShaderSource = "#version 330 core\n"
                                     "layout (location = 0) in vec3 aPos;\n"
                                     "void main()\n"
                                     "{\n"
                                     "   gl_Position = vec4(aPos, 1.0);\n"
                                     "}\0";
    uint32_t shaderProgram = makeShader(vertexShaderSource, readFile("include/shader.fs"));

    uint32_t framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    uint32_t textureColorbuffer;
    glGenTextures(1, &textureColorbuffer);
    glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WIDTH, HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("main framebuffer is not complete\n");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // setup actual render thread
    mtx_init(&windowmtx,mtx_plain);
    thrd_t renderthread;
    mtxptr = &windowmtx;
    thrd_create(&renderthread, renderFunction, NULL);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glfwSwapInterval(0);
    while(!glfwWindowShouldClose(window)) {
        glUseProgram(shaderProgram);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(glGetUniformLocation(shaderProgram, "sizex"), rwidth);
        glUniform1i(glGetUniformLocation(shaderProgram, "sizey"), rheight);
        glUniform1i(glGetUniformLocation(shaderProgram, "pixels"), 0);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    mtx_lock(mtxptr);
    mtx_unlock(mtxptr);
    glfwTerminate();
    thrd_join(renderthread, NULL);
}
