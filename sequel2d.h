// sequel2d.h
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif




/* Defines
You can put them before
#include "sequel2d.h"
*/


/* SEQUEL_NO_AUTOMATIC_FLUSH
By default, when needed, sequel_flush() is called
#define SEQUEL_NO_AUTOMATIC_FLUSH to disable it
*/



/* SEQUEL_NO_AUTOMATIC_EXPAND
Same as SEQUEL_NO_AUTOMATIC_FLUSH, when needed, sequel_expand_max_vertices() is called
#define SEQUEL_NO_AUTOMATIC_FLUSH to disable it
*/





/*
Version
*/
#define SEQUEL_VERSION_MAJOR 1
#define SEQUEL_VERSION_MINOR 0

/* SEQUEL_MAX_VERTICES
Max vertices before crash
*/
#define SEQUEL_MAX_VERTICES 8192



/* SEQUEL_DEFAULT_VERTICES
Default size of buffer
*/
#define SEQUEL_DEFAULT_VERTICES 512


/* SEQUEL_EXPAND_STEP
How much vertices to expand the buffer when needed
*/
#define SEQUEL_EXPAND_STEP 512

static void sequel_rotate(float cx, float cy, float angle, float* x, float* y);

void sequel_init(const char* fragmentEffect);
void sequel_flush();
void sequel_expand_max_vertices(int addVertices);
void sequel_triangle(float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float u3, float v3);
void sequel_rectangle(float x, float y, float width, float height, float u0, float v0, float u1, float v1, float angle);
void sequel_uninit();
void sequel_color(float r, float g, float b, float a);
GLuint sequel_generate_texture(unsigned char* image, int w, int h, bool linearInterpolation);
void sequel_set_texture(GLuint texture);
void sequel_free_texture(GLuint texture);
void sequel_set_blend(GLenum sfactor, GLenum dfactor);
void sequel_transform(float x, float y, float scaleX, float scaleY, float angle);
void sequel_clear(float r, float g, float b, float a);
GLint sequel_location(const char* location);
void sequel_uniform_float(GLint location, float value);
void sequel_uniform_vec4(GLint location, float x, float y, float z, float w);
void sequel_uniform_integer(GLint location, int value);
GLenum sequel_error(bool cleanLater);


#ifdef __cplusplus
}
#endif



#ifdef SEQUEL2D_IMPLEMENTATION


static const char* vertexShaderSource[] = {
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec2 aTexCoord;\n"
    "uniform mat4 uTransform;\n"
    "out vec2 TexCoord;\n"
    "void main(){\n"
    "   gl_Position = uTransform * vec4(aPos, 0.0, 1.0);\n"
    "   TexCoord = aTexCoord;\n"
    "}\0"};

static const char* fragmentShaderSource =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "in vec2 TexCoord;\n"
    "uniform vec4 uColor;\n"
    "uniform sampler2D texture1;\n"
    "uniform int uUseTexture;\n"
    "vec4 effect(vec4 color, vec2 uv);\n"
    "void main(){\n"
    "   if(uUseTexture == 1){\n"
    "       FragColor = effect(texture(texture1, TexCoord) * uColor, TexCoord);\n"
    "   } else {\n"
    "       FragColor = effect(uColor, TexCoord);\n"
    "   }\n"
    "}";

static unsigned int shaderProgram = 0;
static int uColor = -1, uTransform = -1;

static GLenum currentSFactor = GL_SRC_ALPHA, currentDFactor = GL_ONE_MINUS_SRC_ALPHA;
static unsigned int currentTexture = 0;
static float currentR = 1, currentG = 1, currentB = 1, currentA = 1;
static float *vertices = NULL;
static unsigned int VBO = 0, VAO = 0;
static int verticesCount = 0;
static int verticesSize = SEQUEL_DEFAULT_VERTICES;

static void sequel_rotate(float cx, float cy, float angle,
                   float* x, float* y)
{
    float s = sinf(angle);
    float c = cosf(angle);

    float dx = *x - cx;
    float dy = *y - cy;

    *x = cx + dx * c - dy * s;
    *y = cy + dx * s + dy * c;
}


/* Initializes SEQUEL
    Сreates shaders for colors, positions, textures etc. and compiles them
    Then it creates Buffers so you can storage vertices
    and lastly it setups default color, blends, and texture.

    About fragmentEffect
    you can write GLSL function called
    
    vec4 effect(EXPECTS vec4 color and vec2 uv)
    {
        return your formula
    }
*/
void sequel_init(const char* fragmentEffect)
{
    vertices = (float*)malloc(verticesSize * sizeof(float));
 
    // Shaders
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    if (!fragmentEffect || strlen(fragmentEffect) == 0) {
        fragmentEffect =
            "vec4 effect(vec4 color, vec2 uv)\n"
            "{\n"
            "   return color;\n"
            "}";
    }

    size_t len = strlen(fragmentShaderSource) + strlen(fragmentEffect) + 2;
    char* fragmentShaderEffects = (char*)malloc(len);
    strcpy(fragmentShaderEffects, fragmentShaderSource);
    strcat(fragmentShaderEffects, fragmentEffect);

    const char* fragmentSource[] = {fragmentShaderEffects};

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, fragmentSource, NULL);
    glCompileShader(fragmentShader);

    free(fragmentShaderEffects);

    // Program
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Uniform
    uColor = glGetUniformLocation(shaderProgram, "uColor");
    uTransform = glGetUniformLocation(shaderProgram, "uTransform");

    // Buffers
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, verticesSize * sizeof(float), NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    float identity[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(uTransform, 1, GL_FALSE, identity);

    sequel_color(currentR, currentG, currentB, currentA);
    sequel_set_texture(currentTexture);

    glEnable(GL_BLEND);
    sequel_set_blend(currentSFactor, currentDFactor);
}



// Draws ALL the vertices buffer had
void sequel_flush()
{
    if(verticesCount < 1) return;

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, verticesCount * sizeof(float), vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, verticesCount / 2);
    verticesCount = 0;
}



void sequel_expand_max_vertices(int addVertices)
{
    if(verticesCount + addVertices > SEQUEL_MAX_VERTICES)
    {
        printf("Vertices count went beyond maxVertices\n");
        sequel_uninit();
        abort();
    }

    verticesSize += addVertices;
    vertices = (float*)realloc(vertices, verticesSize * sizeof(float));

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, verticesSize * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}



// Deletes all the buffers and compiled shaders
void sequel_uninit()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    free(vertices);
}



// Sets color by using uniform
void sequel_color(float r, float g, float b, float a)
{   
    #ifndef SEQUEL_NO_AUTOMATIC_FLUSH
    if (r!=currentR || g!=currentG || b!=currentB || a!=currentA) {
        sequel_flush();
        currentG = r; currentG = g; currentB = b; currentA = a;
    }
    #endif
    glUseProgram(shaderProgram);
    glUniform4f(uColor, r,g,b,a);
}



// Loads texture by using stb_image
GLuint sequel_generate_texture(unsigned char* image, int w, int h, bool linearInterpolation)
{
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    if(!linearInterpolation)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

    return tex;
}



void sequel_set_texture(GLuint texture)
{
    #ifndef SEQUEL_NO_AUTOMATIC_FLUSH
    if (currentTexture != texture) {
        sequel_flush();
        currentTexture = texture;
    }
    #endif
    glUseProgram(shaderProgram);

    if (texture == 0)
    {
        glUniform1i(glGetUniformLocation(shaderProgram, "uUseTexture"), 0);
        return;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "uUseTexture"), 1);
}



void sequel_rectangle(float x, float y, float width, float height,
                       float u0, float v0, float u1, float v1,
                       float angle)
{
    float cx = x + width  * 0.5f;
    float cy = y + height * 0.5f;

    float x0 = x;
    float y0 = y;

    float x1 = x + width;
    float y1 = y;

    float x2 = x + width;
    float y2 = y + height;

    float x3 = x;
    float y3 = y + height;

    sequel_rotate(cx, cy, angle, &x0, &y0);
    sequel_rotate(cx, cy, angle, &x1, &y1);
    sequel_rotate(cx, cy, angle, &x2, &y2);
    sequel_rotate(cx, cy, angle, &x3, &y3);

    #ifndef SEQUEL_NO_AUTOMATIC_EXPAND
        if(verticesCount + 24 > verticesSize)
            sequel_expand_max_vertices(SEQUEL_EXPAND_STEP);
    #endif

    vertices[verticesCount + 0]  = x0;
    vertices[verticesCount + 1]  = y0;
    vertices[verticesCount + 2]  = u0;
    vertices[verticesCount + 3]  = v0;

    vertices[verticesCount + 4]  = x1;
    vertices[verticesCount + 5]  = y1;
    vertices[verticesCount + 6]  = u1;
    vertices[verticesCount + 7]  = v0;

    vertices[verticesCount + 8]  = x2;
    vertices[verticesCount + 9]  = y2;
    vertices[verticesCount + 10] = u1;
    vertices[verticesCount + 11] = v1;

    vertices[verticesCount + 12] = x2;
    vertices[verticesCount + 13] = y2;
    vertices[verticesCount + 14] = u1;
    vertices[verticesCount + 15] = v1;

    vertices[verticesCount + 16] = x3;
    vertices[verticesCount + 17] = y3;
    vertices[verticesCount + 18] = u0;
    vertices[verticesCount + 19] = v1;

    vertices[verticesCount + 20] = x0;
    vertices[verticesCount + 21] = y0;
    vertices[verticesCount + 22] = u0;
    vertices[verticesCount + 23] = v0;

    verticesCount += 24;
}



void sequel_free_texture(GLuint texture)
{
    if (glIsTexture(texture)) {
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &texture);
    }
}


void sequel_set_blend(GLenum sfactor, GLenum dfactor)
{
    #ifndef SEQUEL_NO_AUTOMATIC_FLUSH
        if(currentSFactor != sfactor || currentDFactor != dfactor) sequel_flush();
        currentSFactor = sfactor;
        currentDFactor = dfactor;
    #endif
    glBlendFunc(sfactor, dfactor);
}


void sequel_transform(float x, float y, float scaleX, float scaleY, float angle)
{

    float rad = angle * 3.14159265f / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);

    float mat[16] = {
        scaleX *  c, scaleY *  s, 0.0f, 0.0f,
        -scaleX * s, scaleY *  c, 0.0f, 0.0f,
        0.0f,       0.0f,        1.0f, 0.0f,
        -x,         -y,          0.0f, 1.0f
    };

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(uTransform, 1, GL_FALSE, mat);
}


void sequel_clear(float r, float g, float b, float a)
{
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(r, g, b, a);
}


GLint sequel_location(const char* location)
{
    return glGetUniformLocation(shaderProgram, location);
}


void sequel_uniform_float(GLint location, float value)
{
    glUniform1f(location, value);
}


void sequel_uniform_vec4(GLint location, float x, float y, float z, float w)
{
    glUniform4f(location, x, y, z, w);
}


void sequel_uniform_integer(GLint location, int value)
{
    glUniform1i(location, value);
}

GLenum sequel_error(bool cleanLater)
{
    GLenum error = glGetError();
    if (cleanLater) while(glGetError() != GL_NO_ERROR);
    return error;
}

#endif