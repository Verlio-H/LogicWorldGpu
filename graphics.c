#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/c11threads.h"
#include "include/glad.h"
#include "graphics.h"
#include <GLFW/glfw3.h>

#define WIDTH 256
#define HEIGHT 256

#define UNUSED(x) ((void)x)

//public (locked by window mutex)
uint8_t* data = NULL;
uint8_t* data2;
bool buffered = false;
uint16_t width, height;
uint16_t width2, height2;
mtx_t* mtxptr;

//private
int rwidth,rheight;

float vertices[] = {
    1.0, -1.0, 0.0,
    1.0,  1.0, 0.0,
   -1.0, -1.0, 0.0,
   -1.0,  1.0, 0.0
};

void framebuffer_size_callback(GLFWwindow* window, int wwidth, int wheight) {
    UNUSED(window);

    rwidth = wwidth;
    rheight = wheight;
}

uint32_t makeShader(const char * const vertSrc, const char * const fragSrc) {
    uint32_t vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertSrc, NULL);
    glCompileShader(vertexShader);

    // check for shader compile errors
    int32_t success;
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

    GLFWwindow* window;
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

    width = WIDTH;
    height = HEIGHT;

    glfwGetFramebufferSize(window, &rwidth, &rheight);
    glfwSetWindowAspectRatio(window, rwidth, rheight);
    double scale = (double)rwidth/width;


    data = malloc(width * height * 4);

    char *title = malloc(64);
    snprintf(title, 64, "%dx%d", width, height);
    glfwSetWindowTitle(window, title);
    free(title);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    uint32_t VBO, VAO;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), NULL);
    glEnableVertexAttribArray(0);

    const char * const vertexShaderSource = 
                                    "#version 330 core\n"
                                    "layout (location = 0) in vec3 aPos;\n"
                                    "void main() {\n"
                                    "   gl_Position = vec4(aPos, 1.0);\n"
                                    "}";
    const char * const fragmentShaderSource =
                                    "#version 330 core\n"
                                    "out vec4 FragColor;\n"
                                    "uniform int sizex;\n"
                                    "uniform int sizey;\n"
                                    "uniform sampler2D pixels;\n"
                                    "void main() {\n"
                                    "    FragColor = texture(pixels,vec2(float(gl_FragCoord.x)/sizex,float(gl_FragCoord.y)/sizey),0);\n"
                                    "}";
    uint32_t shaderProgram = makeShader(vertexShaderSource, fragmentShaderSource);

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

    uint32_t texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    mtx_t windowmtx;
    mtx_init(&windowmtx,mtx_plain);
    mtxptr = &windowmtx;

    struct renderArgs args = (struct renderArgs){.stop = false, .mousePosX = 0, .mousePosY = 0};
    
    thrd_t renderthread;
    thrd_create(&renderthread, renderFunction, &args);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glfwSwapInterval(0);
    while(!glfwWindowShouldClose(window)) {
        glfwGetCursorPos(window, &args.mousePosX, &args.mousePosY);
        args.mousePosX *= (double)width/rwidth*scale;
        args.mousePosY *= (double)height/rheight*scale;
        glUseProgram(shaderProgram);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(glGetUniformLocation(shaderProgram, "sizex"), rwidth);
        glUniform1i(glGetUniformLocation(shaderProgram, "sizey"), rheight);
        glUniform1i(glGetUniformLocation(shaderProgram, "pixels"), 0);
        glViewport(0, 0, rwidth, rheight);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    args.stop = true;
    thrd_join(renderthread, NULL);

    free(data);
}
