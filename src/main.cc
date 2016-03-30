#include <iostream>
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;

#define err_quit(...) do { \
    fprintf(stderr, __VA_ARGS__); \
    exit(-1); \
} while (0)

static struct context {
    GLFWwindow* window;
    int width, height;

    GLuint program, programH;
    GLuint vao, vbo;
    GLuint tex;

    GLuint fbTex; // texture attached to offscreen fb
    GLuint fb;
} ctx = {
    nullptr,
    1024, 576,
    0,
};

const GLchar* ts_code = R"(
#version 150
in vec2 position;
in vec3 vertexColor;
in vec2 vTexCoord;

out vec3 fragColor;
out vec2 texCoord;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    fragColor = vertexColor;
    texCoord = vTexCoord;
}
)";

const GLchar* vs_code = R"(
#version 150
in vec3 fragColor;
in vec2 texCoord;
out vec4 outColor;

uniform int radius;
uniform float offset[100];
uniform float weight[100];
uniform vec2 resolution;
uniform sampler2D sampler;

void main() {
    outColor = texture2D(sampler, texCoord) * weight[0];
    for (int i = 1; i < radius; i++) {
        outColor += texture2D(sampler, texCoord.st - vec2(0.0, offset[i]/resolution.y)) * weight[i];
        outColor += texture2D(sampler, texCoord.st + vec2(0.0, offset[i]/resolution.y)) * weight[i];
    }
}
)";

//FIXME: reverse texture outside?
const GLchar* vs_code_h = R"(
#version 150
in vec3 fragColor;
in vec2 texCoord;
out vec4 outColor;

uniform int radius;
uniform float offset[100];
uniform float weight[100];
uniform vec2 resolution;
uniform sampler2D sampler;

void main() {
    vec2 tc = vec2(texCoord.s, 1.0 - texCoord.t);
    outColor = texture2D(sampler, tc) * weight[0];
    for (int i = 1; i < radius; i++) {
        outColor += texture2D(sampler, tc + vec2(offset[i]/resolution.x, 0.0)) * weight[i];
        outColor += texture2D(sampler, tc - vec2(offset[i]/resolution.x, 0.0)) * weight[i];
    }
}
)";

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(ctx.window, GL_TRUE);
}

static GLuint build_shader(const GLchar* code, GLint type)
{
    GLuint shader = glCreateShader(type);
    if (shader) {
        glShaderSource(shader, 1, &code, NULL);
        glCompileShader(shader);

        GLint result = GL_TRUE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &result);

        if (result == GL_FALSE) {
            GLchar log[1024];
            glGetShaderInfoLog(shader, sizeof log - 1, NULL, log);
            cerr << log << endl;
        }
    }

    return shader;
}

static GLuint build_program(int stage)
{
    GLuint program = glCreateProgram();

    GLuint ts = build_shader(ts_code, GL_VERTEX_SHADER);
    glAttachShader(program, ts);
    GLuint vs = build_shader(stage == 1 ? vs_code : vs_code_h, GL_FRAGMENT_SHADER);
    glAttachShader(program, vs);

    glLinkProgram(program);
    GLint result = GL_TRUE;
    glGetProgramiv(program, GL_LINK_STATUS, &result);
    if (result == GL_FALSE) {
        GLchar log[1024];
        glGetProgramInfoLog(program, sizeof log - 1, NULL, log);
        cerr << log << endl;
    }

    GLint pos_attrib = glGetAttribLocation(program, "position");
    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), 0);

    GLint clr_attrib = glGetAttribLocation(program, "vertexColor");
    if (clr_attrib >= 0) {
        glEnableVertexAttribArray(clr_attrib);
        glVertexAttribPointer(clr_attrib, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat),
                (const GLvoid*)(2*sizeof(GLfloat)));
    }

    GLint tex_attrib = glGetAttribLocation(program, "vTexCoord");
    assert(tex_attrib != 0);
    glEnableVertexAttribArray(tex_attrib);
    glVertexAttribPointer(tex_attrib, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat),
            (const GLvoid*)(5*sizeof(GLfloat)));

    return program;
}

static GLint radius = 10;
static GLfloat offset[100], weight[100];

static void build_gaussian_blur_kernel(GLint radius, GLfloat* offset, GLfloat* weight)
{
    int sz = (radius-1)*2+5;
    int tbl1[sz], tbl2[sz];

    tbl1[0] = 1;
    for (int i = 1; i < sz; i++) {
        int* ref = i % 2 == 1 ? tbl1 : tbl2;
        int* tbl = i % 2 == 0 ? tbl1 : tbl2;
        tbl[0] = 1;
        for (int k = 1; k < i; k++) {
            tbl[k] = ref[k-1] + ref[k];
        }
        tbl[i] = 1;
    }

    int* tbl = sz % 2 == 1 ? tbl1 : tbl2;
    GLfloat sum = powf(2, sz-1) - tbl[0] - tbl[1] - tbl[sz-1] - tbl[sz-2];
    cerr << "sum = " << sum << " ";
    for (int i = 0; i < sz; i++) {
        cerr << tbl[i] << " ";
    }
    cerr << endl;

    for (int i = 0; i < radius; i++) {
        offset[i] = (GLfloat)i;
        weight[radius-i-1] = (GLfloat)tbl[i+2] / sum;
    }

    for (int i = 0; i < radius; i++) {
        cerr << offset[i] << " ";
    }
    cerr << endl;
    for (int i = 0; i < radius; i++) {
        cerr << weight[i] << " ";
    }
    cerr << endl;
}

static void gl_init()
{
    glfwGetFramebufferSize(ctx.window, &ctx.width, &ctx.height);
    glViewport(0, 0, ctx.width, ctx.height);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glGenVertexArrays(1, &ctx.vao);
    glBindVertexArray(ctx.vao);

    glGenBuffers(1, &ctx.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ctx.vbo);

    static GLfloat vdata[] = {
        -1.0f, 1.0f,   1.0f, 0.0f, 0.0f,  0.0f, 1.0f,
        1.0f, 1.0f,    0.0f, 1.0f, 0.0f,  1.0f, 1.0f,
        1.0f, -1.0f,   1.0f, 0.0f, 0.0f,  1.0f, 0.0f,

        -1.0f, 1.0f,   1.0f, 0.0f, 0.0f,  0.0f, 1.0f,
        1.0f, -1.0f,   1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
        -1.0f, -1.0f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vdata), &vdata, GL_STATIC_DRAW);


    glGenTextures(1, &ctx.tex);
    glBindTexture(GL_TEXTURE_2D, ctx.tex);

    int x,y,n;
    unsigned char *pixbuf = stbi_load("./texture.jpg", &x, &y, &n, 0);
    if (!pixbuf) {
        err_quit("load texture.jpg failed\n");
    }

    glTexImage2D(GL_TEXTURE_2D, 0, n == 4 ? GL_RGBA : GL_RGB, x, y, 0,
            n == 4 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, pixbuf);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);  
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixbuf);


    glGenTextures(1, &ctx.fbTex);
    glBindTexture(GL_TEXTURE_2D, ctx.fbTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ctx.width, ctx.height,
            0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &ctx.fb);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx.fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx.fbTex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        err_quit("framebuffer create failed\n");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    ctx.program = build_program(1);
    ctx.programH = build_program(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    build_gaussian_blur_kernel(radius, offset, weight);
}


static void render()
{
    //cerr << __PRETTY_FUNCTION__ << endl;
    GLint validate = GL_TRUE;
    glValidateProgram(ctx.program);
    glGetProgramiv(ctx.program, GL_VALIDATE_STATUS, &validate);
    if (validate == GL_FALSE) {
        err_quit("program is invalid\n");
    }

    glValidateProgram(ctx.programH);
    glGetProgramiv(ctx.programH, GL_VALIDATE_STATUS, &validate);
    if (validate == GL_FALSE) {
        err_quit("program is invalid\n");
    }

    glDisable(GL_DEPTH_TEST);

    glBindVertexArray(ctx.vao);

    glBindFramebuffer(GL_FRAMEBUFFER, ctx.fb);
    glBindTexture(GL_TEXTURE_2D, ctx.tex);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(ctx.program);
    glUniform1fv(glGetUniformLocation(ctx.program, "offset"), radius, offset);
    glUniform1fv(glGetUniformLocation(ctx.program, "weight"), radius, weight);
    glUniform1i(glGetUniformLocation(ctx.program, "radius"), radius);
    glUniform2f(glGetUniformLocation(ctx.program, "resolution"),
            (GLfloat)ctx.width, (GLfloat)ctx.height);
    glDrawArrays(GL_TRIANGLES, 0, 6);


    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, ctx.fbTex);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(ctx.programH);
    glUniform1fv(glGetUniformLocation(ctx.program, "offset"), radius, offset);
    glUniform1fv(glGetUniformLocation(ctx.program, "weight"), radius, weight);
    glUniform1i(glGetUniformLocation(ctx.program, "radius"), radius);
    glUniform2f(glGetUniformLocation(ctx.program, "resolution"),
            (GLfloat)ctx.width, (GLfloat)ctx.height);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glfwSwapBuffers(ctx.window);
}

int main(int argc, char *argv[])
{
    if (!glfwInit()) {
        err_quit("glfwInit failed\n");
    }

    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    ctx.window = glfwCreateWindow(ctx.width, ctx.height, "Blur Demo", NULL, NULL);
    if (!ctx.window) {
        glfwTerminate();
        err_quit("glfwCreateWindow failed\n");
    }
    glfwSetKeyCallback(ctx.window, key_callback);
    glfwMakeContextCurrent(ctx.window);

    // do it after context is current
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        err_quit("glewInit failed\n");
    }
    if(!GLEW_VERSION_3_2)
        throw std::runtime_error("OpenGL 3.2 API is not available.");

    glfwSwapInterval(1);

    gl_init();

    glfwShowWindow(ctx.window);
    auto time = glfwGetTime();
    while (!glfwWindowShouldClose(ctx.window)) {
        render();

        while (glfwGetTime() - time < 1.0/30.0) {
            glfwWaitEvents();
        }
        time = glfwGetTime();
        //glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
