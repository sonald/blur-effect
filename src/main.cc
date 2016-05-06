#include <iostream>
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

using namespace std;

#define err_quit(...) do { \
    fprintf(stderr, __VA_ARGS__); \
    exit(-1); \
} while (0)

static struct context {
    GLFWwindow* window;
    int width, height;

    GLuint program, programH;
    GLuint vbo;
    GLuint tex;

    GLuint fbTex[2]; // texture attached to offscreen fb
    GLuint fb[2];
} ctx = {
    nullptr,
    0,
};

static float vps = 1.0f;

/** shaders work on OpenGL 2.1 */
const GLchar* ts_code = R"(
#version 120
attribute vec2 position;
attribute vec3 vertexColor;
attribute vec2 vTexCoord;

varying vec3 fragColor;
varying vec2 texCoord;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    fragColor = vertexColor;
    texCoord = vTexCoord;
}
)";

const GLchar* vs_code = R"(
#version 120
#extension GL_ARB_shader_texture_lod: enable
#ifdef GL_ARB_shader_texture_lod
#define texpick texture2DLod
#else
#define texpick texture2D
#endif

varying vec3 fragColor;
varying vec2 texCoord;

// kernel[0] = radius, kernel[1-20] = offset, kernel[21-40] = weight
uniform float kernel[41];

uniform vec2 resolution;
uniform sampler2D sampler;

void main() {   
    float lod = 4.2;
    gl_FragColor = texpick(sampler, texCoord, lod) * kernel[21];
    for (int i = 1; i < kernel[0]; i++) {
        gl_FragColor += texpick(sampler, texCoord.st - vec2(0.0, kernel[1+i]/resolution.y), lod) * kernel[21+i];
        gl_FragColor += texpick(sampler, texCoord.st + vec2(0.0, kernel[1+i]/resolution.y), lod) * kernel[21+i];
    }
}
)";

//FIXME: reverse texture outside?
const GLchar* vs_code_h = R"(
#version 120
#extension GL_ARB_shader_texture_lod: enable
#ifdef GL_ARB_shader_texture_lod
#define texpick texture2DLod
#else
#define texpick texture2D
#endif

varying vec3 fragColor;
varying vec2 texCoord;

uniform float kernel[41];

uniform vec2 resolution;
uniform sampler2D sampler;

void main() {
    float lod = 4.2;
    vec2 tc = vec2(texCoord.s, 1.0 - texCoord.t);
    gl_FragColor = texpick(sampler, tc, lod) * kernel[21];
    for (int i = 1; i < kernel[0]; i++) {
        gl_FragColor += texpick(sampler, tc + vec2(kernel[1+i]/resolution.x, 0.0), lod) * kernel[21+i];
        gl_FragColor += texpick(sampler, tc - vec2(kernel[1+i]/resolution.x, 0.0), lod) * kernel[21+i];
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

// must be odd
static GLint radius = 9;
static GLfloat kernel[41];
static GLfloat kernel2[41];
static GLfloat kernel3[41];

static void build_gaussian_blur_kernel(GLint* pradius, GLfloat* offset, GLfloat* weight)
{
    GLint radius = *pradius;
    radius += (radius + 1) % 2;
    GLint sz = (radius+2)*2-1;
    GLint N = sz-1;

    GLfloat sum = powf(2, N);
    weight[radius+1] = 1.0;
    for (int i = 1; i < radius+2; i++) {
        weight[radius-i+1] = weight[radius-i+2] * (N-i+1) / i;
    }
    sum -= (weight[radius+1] + weight[radius]) * 2.0;

    cerr << "N = " << N << ", sum = " << sum << endl;

    GLfloat bias = radius <= 5 ? 1.0 : 2.0;
    for (int i = 0; i < radius; i++) {
        offset[i] = (GLfloat)i*bias;
        weight[i] /= sum;
    }

    *pradius = radius;

    //step2: interpolate,
    //FIXME: this introduce some artifacts
#if 0
    radius = (radius+1)/2;
    for (int i = 1; i < radius; i++) {
        float w = weight[i*2] + weight[i*2-1];
        float off = (offset[i*2] * weight[i*2] + offset[i*2-1] * weight[i*2-1]) / w;
        offset[i] = off;
        weight[i] = w;
    }
    *pradius = radius;
#endif

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

    if (x >= ctx.width && y >= ctx.height) {
        cerr << "down scale texture\n";
        float scale = fminf(ctx.width / (float)x, ctx.height / (float)y);
        int ox = x * scale, oy = y * scale;
        unsigned char* output = (unsigned char*)malloc(n*ox*oy);
        stbir_resize_uint8(pixbuf, x, y, 0, output, ox, oy, 0, n);
        stbi_image_free(pixbuf);
        x = ox;
        y = oy;
        pixbuf = output;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, n == 4 ? GL_RGBA : GL_RGB, x, y, 0,
            n == 4 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, pixbuf);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 4);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixbuf);


    glGenTextures(2, ctx.fbTex);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, ctx.fbTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ctx.width * vps, ctx.height * vps,
                0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        //glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glGenFramebuffers(2, ctx.fb);
    for (int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, ctx.fb[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx.fbTex[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            err_quit("framebuffer create failed\n");
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    ctx.program = build_program(1);
    ctx.programH = build_program(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    build_gaussian_blur_kernel(&radius, &kernel[1], &kernel[21]);
    kernel[0] = radius;

    radius = 7;
    build_gaussian_blur_kernel(&radius, &kernel2[1], &kernel2[21]);
    kernel2[0] = radius;

    radius = 9;
    build_gaussian_blur_kernel(&radius, &kernel3[1], &kernel3[21]);
    kernel3[0] = radius;
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

    glBindBuffer(GL_ARRAY_BUFFER, ctx.vbo);

    glDisable(GL_DEPTH_TEST);

    int rounds = 1;
    for (int i = 0; i < rounds; i++) {
        GLuint tex1 = i == 0 ? ctx.tex : ctx.fbTex[1];
        GLuint fb2 = i == rounds-1 ? 0: ctx.fb[1];
        GLfloat* kernels = i == 0 ? &kernel[0] : (i == 1 ? &kernel2[0] : &kernel3[0]);

        glBindFramebuffer(GL_FRAMEBUFFER, ctx.fb[0]);
        glBindTexture(GL_TEXTURE_2D, tex1);
        glUseProgram(ctx.program);
        glUniform1fv(glGetUniformLocation(ctx.program, "kernel"), 41, kernels);
        glUniform2f(glGetUniformLocation(ctx.program, "resolution"),
                (GLfloat)ctx.width * vps, (GLfloat)ctx.height * vps);
        glDrawArrays(GL_TRIANGLES, 0, 6);


        glBindFramebuffer(GL_FRAMEBUFFER, fb2);
        glBindTexture(GL_TEXTURE_2D, ctx.fbTex[0]);
        glGenerateMipmap(GL_TEXTURE_2D);

        glUseProgram(ctx.programH);
        glUniform1fv(glGetUniformLocation(ctx.programH, "kernel"), 41, kernels);
        glUniform2f(glGetUniformLocation(ctx.programH, "resolution"),
                (GLfloat)ctx.width, (GLfloat)ctx.height);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glfwSwapBuffers(ctx.window);
}

int main(int argc, char *argv[])
{
    if (!glfwInit()) {
        err_quit("glfwInit failed\n");
    }

    auto* monitor = glfwGetPrimaryMonitor();
    auto* mode = glfwGetVideoMode(monitor);

    ctx.width = mode->width;
    ctx.height = mode->height;
    int wx, wy;
    glfwGetMonitorPos(monitor, &wx, &wy);

    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2); 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    ctx.window = glfwCreateWindow(ctx.width, ctx.height, "Blur Demo", NULL, NULL);
    if (!ctx.window) {
        glfwTerminate();
        err_quit("glfwCreateWindow failed\n");
    }
    glfwSetWindowPos(ctx.window, wx, wy);
    glfwSetKeyCallback(ctx.window, key_callback);
    glfwMakeContextCurrent(ctx.window);

    // do it after context is current
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        err_quit("glewInit failed\n");
    }
    if(!GLEW_VERSION_3_0)
        throw std::runtime_error("OpenGL 3.x API is not available.");

    if (GLEW_ARB_timer_query) {
        cerr << "ARB_timer_query exists\n";
    }
    glfwSwapInterval(1);

    GLuint query;
    GLint available = 0;
    GLuint64 timeElapsed = 0; // nanoseconds

    if (GLEW_ARB_timer_query) {
        glGenQueries(1, &query);
    }

#define query_timer() do { \
    if (GLEW_ARB_timer_query) glBeginQuery(GL_TIME_ELAPSED, query); \
} while (0)

#define end_timer() do { \
        if (GLEW_ARB_timer_query) {     \
            glEndQuery(GL_TIME_ELAPSED);    \
            while (!available) {    \
                glGetQueryObjectiv(query, GL_QUERY_RESULT_AVAILABLE, &available);   \
            }   \
    \
            glGetQueryObjectui64v(query, GL_QUERY_RESULT, &timeElapsed);    \
            cerr << "cost = " << (GLdouble)timeElapsed / 1000000.0 << endl;     \
        }   \
} while (0)

    query_timer();
    gl_init();
    end_timer();

    glfwShowWindow(ctx.window);
    auto time = glfwGetTime();
    while (!glfwWindowShouldClose(ctx.window)) {
        query_timer();

        render();

        end_timer();

        while (glfwGetTime() - time < 1.0/10.0) {
            glfwWaitEvents();
        }
        time = glfwGetTime();
    }

    glfwTerminate();
    return 0;
}
