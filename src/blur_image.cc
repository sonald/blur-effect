#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>

#include <iostream>
#include <algorithm>
#include <vector>

#include <gbm.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

using namespace std;

#define err_quit(fmt, ...) do { \
    fprintf(stderr, fmt, ## __VA_ARGS__); \
    exit(-1); \
} while (0)

static struct context {
    EGLDisplay display;
    EGLContext gl_context;

    EGLSurface surface;

	int fd; // fd of drm device

    struct gbm_device *gbm;
    struct gbm_surface *gbm_surface;

    char* img_path;
    int width, height, ncomp;
    unsigned char* img_data;

    GLuint program, programH, programDirect, programSaveBrt, programSetBrt;
    GLuint vbo;
    GLuint tex;

    GLuint fbTex[2]; // texture attached to offscreen fb
    GLuint fb[2];

    GLuint brtFb;
    GLuint brtTex; // texture for brightness info
    bool brightnessAdjusted;

    int tex_width, tex_height;
} ctx = {
    0,
};

static int rounds = 1;
static char* infile = NULL, *outfile = NULL, *drmdev = NULL;
bool adjustBrightness = false;

/** shaders work on OpenGL ES 3.0 */
const GLchar* ts_code = R"(
#version 300 es
precision highp float;

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

#ifdef __mips__

const GLchar* vs_code = R"(
#version 300 es
#define texpick textureLod
precision highp float;

in vec3 fragColor;
in vec2 texCoord;

out vec4 outColor;

float kernel[9] = float[](0.176204,0.160186,0.120139,0.0739318,0.0369659,0.0147864,0.00462074,0.00108723,0.000181205);

uniform vec2 resolution;
uniform sampler2D sampler;

void main() {
    float lod = 0.0;
    outColor = texpick(sampler, texCoord, lod) * kernel[0];
    int limit = 9;
    for (int i = 1; i < limit; i++) {
        outColor += texpick(sampler, texCoord.st - vec2(0.0, float(i)/resolution.y), lod) * kernel[i];
        outColor += texpick(sampler, texCoord.st + vec2(0.0, float(i)/resolution.y), lod) * kernel[i];
    }
}
)";


const GLchar* vs_code_h = R"(
#version 300 es
#define texpick textureLod
precision highp float;

in vec3 fragColor;
in vec2 texCoord;

out vec4 outColor;
float kernel[9] = float[](0.176204,0.160186,0.120139,0.0739318,0.0369659,0.0147864,0.00462074,0.00108723,0.000181205);

uniform vec2 resolution;
uniform sampler2D sampler;

void main() {
    float lod = 0.0;
    vec2 tc = vec2(texCoord.s, texCoord.t);
    outColor = texpick(sampler, tc, lod) * kernel[0];
    int limit = 9;
    for (int i = 1; i < limit; i++) {
        outColor += texpick(sampler, tc + vec2(float(i)/resolution.x, 0.0), lod) * kernel[i];
        outColor += texpick(sampler, tc - vec2(float(i)/resolution.x, 0.0), lod) * kernel[i];
    }
}
)";

#else
const GLchar* vs_code = R"(
#version 300 es
#define texpick textureLod
precision highp float;

in vec3 fragColor;
in vec2 texCoord;

out vec4 outColor;

uniform float kernel[41];

uniform vec2 resolution;
uniform sampler2D sampler;

void main() {   
    float lod = 0.0;
    outColor = texpick(sampler, texCoord, lod) * kernel[21];
    int limit = int(kernel[0]);
    for (int i = 1; i < limit; i++) {
        outColor += texpick(sampler, texCoord.st - vec2(0.0, kernel[1+i]/resolution.y), lod) * kernel[21+i];
        outColor += texpick(sampler, texCoord.st + vec2(0.0, kernel[1+i]/resolution.y), lod) * kernel[21+i];
    }
}
)";

const GLchar* vs_code_h = R"(
#version 300 es
#define texpick textureLod
precision highp float;

in vec3 fragColor;
in vec2 texCoord;

out vec4 outColor;
uniform float kernel[41];

uniform vec2 resolution;
uniform sampler2D sampler;

void main() {
    float lod = 0.0;
    vec2 tc = vec2(texCoord.s, texCoord.t);
    outColor = texpick(sampler, tc, lod) * kernel[21];
    int limit = int(kernel[0]);
    for (int i = 1; i < limit; i++) {
        outColor += texpick(sampler, tc + vec2(kernel[1+i]/resolution.x, 0.0), lod) * kernel[21+i];
        outColor += texpick(sampler, tc - vec2(kernel[1+i]/resolution.x, 0.0), lod) * kernel[21+i];
    }
}
)";

#endif

const GLchar* vs_direct = R"(
#version 300 es
precision highp float;

in vec3 fragColor;
in vec2 texCoord;

out vec4 outColor;
uniform sampler2D sampler;

void main() {
    vec2 tc = vec2(texCoord.s, texCoord.t);
    outColor = texture(sampler, tc);
}
)";

const GLchar* vs_save_brightness = R"(
#version 300 es
precision highp float;

in vec3 fragColor;
in vec2 texCoord;

out vec4 outColor;
uniform sampler2D sampler;

float brightness(vec4 c) {
    //return dot(c.rgb, vec3(0.2126, 0.7152, 0.0722));
    return sqrt(dot(c.rgb * c.rgb, vec3(0.241, 0.691, 0.068)));
}

void main() {
    outColor.a = brightness(texture(sampler, texCoord.st));
}
)";

const GLchar* vs_set_brightness = R"(
#version 300 es
precision highp float;

in vec3 fragColor;
in vec2 texCoord;

out vec4 outColor;
uniform sampler2D sampler;

vec4 darken(vec4 c) {
    return vec4(c.rgb * 0.8, c.a);
}

void main() {
    outColor = darken(texture(sampler, texCoord.st));
}
)";

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
    const GLchar* vs_src = stage == 1 ? vs_code :
        stage == 2 ? vs_code_h : 
        stage == 3 ? vs_direct : 
        stage == 4 ? vs_save_brightness : vs_set_brightness;
    GLuint vs = build_shader(vs_src, GL_FRAGMENT_SHADER);
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
static GLint radius = 19;
static GLfloat kernel[41];

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

    GLfloat bias = 1.0;
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
}

static void gl_init()
{
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

    GLenum pixel_fmt = ctx.ncomp == 4 ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, pixel_fmt, ctx.width, ctx.height, 0,
            pixel_fmt, GL_UNSIGNED_BYTE, ctx.img_data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(2, ctx.fbTex);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, ctx.fbTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, pixel_fmt, ctx.tex_width, ctx.tex_height,
                0, pixel_fmt, GL_UNSIGNED_BYTE, NULL);
        GLenum err;
        if ((err = glGetError()) != GL_NO_ERROR) {
            fprintf(stderr, "texture error %x\n", err);
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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
    ctx.programDirect = build_program(3);
    if (adjustBrightness) {
        ctx.programSaveBrt = build_program(4);
        ctx.programSetBrt = build_program(5);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);

#ifndef __mips__
    build_gaussian_blur_kernel(&radius, &kernel[1], &kernel[21]);
    kernel[0] = radius;
#endif
}

static void adjust_brightness()
{
    glGenTextures(1, &ctx.brtTex);
    glBindTexture(GL_TEXTURE_2D, ctx.brtTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ctx.tex_width, ctx.tex_height,
            0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    GLenum err;
    if ((err = glGetError()) != GL_NO_ERROR) {
        fprintf(stderr, "texture error %x\n", err);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &ctx.brtFb);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx.brtFb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx.brtTex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            err_quit("framebuffer create failed\n");
        }

    // calculate brightness
    glBindTexture(GL_TEXTURE_2D, ctx.fbTex[1]);
    glUseProgram(ctx.programSaveBrt);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    char* data = (char*)malloc(ctx.width * ctx.height * 4);
    glReadPixels(0, 0, ctx.width, ctx.height, GL_RGBA, GL_UNSIGNED_BYTE, data);

    int count = ctx.width * ctx.height;
    int total = 0;
    unsigned int* clr = (unsigned int *)data;
    for (int i = 0; i < count; i++) {
        total += (clr[0] >>24) & 0xff;
    }

    cerr << "brightness: " << total / count << endl;
    if (total / count > 100) {
        // update brightness
        glBindTexture(GL_TEXTURE_2D, ctx.fbTex[1]);
        glUseProgram(ctx.programSetBrt);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        ctx.brightnessAdjusted = true;
    }
}

static void render()
{
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

    glViewport(0, 0, ctx.tex_width, ctx.tex_height);
    for (int i = 0; i < rounds; i++) {
        GLuint tex1 = i == 0 ? ctx.tex : ctx.fbTex[1];

        glBindFramebuffer(GL_FRAMEBUFFER, ctx.fb[0]);
        glBindTexture(GL_TEXTURE_2D, tex1);
        glUseProgram(ctx.program);
#ifndef __mips__
        glUniform1fv(glGetUniformLocation(ctx.program, "kernel"), 41, kernel);
#endif
        glUniform2f(glGetUniformLocation(ctx.program, "resolution"),
                (GLfloat)ctx.tex_width, (GLfloat)ctx.tex_height);
        glDrawArrays(GL_TRIANGLES, 0, 6);


        glBindFramebuffer(GL_FRAMEBUFFER, ctx.fb[1]);
        glBindTexture(GL_TEXTURE_2D, ctx.fbTex[0]);
        glUseProgram(ctx.programH);
#ifndef __mips__
        glUniform1fv(glGetUniformLocation(ctx.program, "kernel"), 41, kernel);
#endif
        glUniform2f(glGetUniformLocation(ctx.programH, "resolution"),
                (GLfloat)ctx.tex_width, (GLfloat)ctx.tex_height);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    if (adjustBrightness) {
        adjust_brightness();
    }
    
    glViewport(0, 0, ctx.width, ctx.height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (ctx.brightnessAdjusted)
        glBindTexture(GL_TEXTURE_2D, ctx.brtTex);
    else
        glBindTexture(GL_TEXTURE_2D, ctx.fbTex[1]);
    glUseProgram(ctx.programDirect);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    char* data = (char*)malloc(ctx.width * ctx.height * 4);
    glReadPixels(0, 0, ctx.width, ctx.height, GL_RGBA, GL_UNSIGNED_BYTE, data);

    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_data((const guchar*)data, 
            GDK_COLORSPACE_RGB, TRUE, 8, ctx.width,
            ctx.height, ctx.width * 4, NULL, NULL);

    string new_path = string("blurred.") + ctx.img_path;
    if (outfile) new_path = outfile;
    cout << "new_path: " << new_path << endl;
    auto suffix = new_path.substr(new_path.find_last_of('.')+1, new_path.size());
    if (suffix == "jpg" || suffix.empty()) suffix = "jpeg";

    GError* error = NULL;
    if (!gdk_pixbuf_save(pixbuf, new_path.c_str(), suffix.c_str(), &error, NULL)) {
        err_quit("%s\n", error->message);
    }

}

static bool is_device_viable(int id)
{
    char path[128];
    snprintf(path, sizeof path, "/sys/class/drm/card%d", id);
    if (access(path, F_OK) != 0) {
        return false;
    }

    char buf[512];
    snprintf(buf, sizeof buf, "%s/device/enable", path);
    
    FILE* fp = fopen(buf, "r");
    if (!fp) {
        return false;
    }

    int enabled = 0;
    fscanf(fp, "%d", &enabled);
    fclose(fp);

    return enabled == 1;
}

static string choose_best_card(const vector<string>& vs)
{
    for (auto card: vs) {
        char buf[1024] = {0};
        int id = std::stoi(card.substr(card.size()-1));
        snprintf(buf, sizeof buf, "/sys/class/drm/card%d/device/driver", id);

        char buf2[1024] = {0};
        readlink(buf, buf2, sizeof buf2);
        string driver = basename(buf2);
        if (driver == "i915") {
            return card;
        } 
    }

    return vs[0];
}

static void open_best_device()
{
    vector<string> viables;
    string card = "/dev/dri/card0";
    const char* const tmpl = "/dev/dri/card%d";
    for (int i = 0; i < 4; i++) {
        char buf[128];
        snprintf(buf, 127, tmpl, i);
        if (is_device_viable(i)) {
            viables.push_back(buf);
        }
    }

    if (viables.size() == 0) {
        err_quit("no card found\n");
    }

    card = choose_best_card(viables);

    std::cerr << "try to open " << card << endl;
    ctx.fd = open(card.c_str(), O_RDWR|O_CLOEXEC|O_NONBLOCK);
    if (ctx.fd < 0) { 
        err_quit(strerror(errno));
    }
}

static void open_drm_device()
{
    if (drmdev != NULL && access(drmdev, F_OK) == 0) {
        std::cerr << "try to open " << drmdev << endl;
        ctx.fd = open(drmdev, O_RDWR|O_CLOEXEC|O_NONBLOCK);
        if (ctx.fd < 0) {
            open_best_device();
        }
    } else {
        open_best_device();
    }

    //drmSetMaster(ctx.fd);
}

static void setup_context()
{
    open_drm_device();

    ctx.gbm = gbm_create_device(ctx.fd);
    printf("backend name: %s\n", gbm_device_get_backend_name(ctx.gbm));

    ctx.gbm_surface = gbm_surface_create(ctx.gbm, ctx.width,
            ctx.height, GBM_FORMAT_XRGB8888,
            GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!ctx.gbm_surface) {
        printf("cannot create gbm surface (%d): %m", errno);
        exit(-EFAULT);
    }

    EGLint major;
    EGLint minor;
    const char *extensions;
    ctx.display = eglGetDisplay(ctx.gbm);
    eglInitialize(ctx.display, &major, &minor);
    extensions = eglQueryString(ctx.display, EGL_EXTENSIONS);

    if (!strstr(extensions, "EGL_KHR_surfaceless_context")) {
        err_quit("%s\n", "need EGL_KHR_surfaceless_context extension");
    }

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        std::cerr << "bind api failed" << std::endl;
        exit(-1);
    }

    static const EGLint conf_att[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE,
    };
    static const EGLint ctx_att[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    EGLConfig conf;
    int num_conf;
    EGLBoolean ret = eglChooseConfig(ctx.display, conf_att, &conf, 1, &num_conf);
    if (!ret || num_conf != 1) {
        printf("cannot find a proper EGL framebuffer configuration");
        exit(-1);
    }

    ctx.gl_context = eglCreateContext(ctx.display, conf, EGL_NO_CONTEXT, ctx_att);
    if (ctx.gl_context == EGL_NO_CONTEXT) {
        printf("no context created.\n"); exit(0);
    }

    ctx.surface = eglCreateWindowSurface(ctx.display, conf, (EGLNativeWindowType)ctx.gbm_surface, NULL);
    if (ctx.surface == EGL_NO_SURFACE) {
        printf("cannot create EGL window surface");
        exit(-1);
    }

    if (!eglMakeCurrent(ctx.display, ctx.surface, ctx.surface, ctx.gl_context)) {
        printf("cannot activate EGL context");
        exit(-1);
    }
}

static void cleanup()
{
    eglDestroySurface(ctx.display, ctx.surface);
    eglDestroyContext(ctx.display, ctx.gl_context);
    eglTerminate(ctx.display);
}

static void usage()
{
    err_quit("usage: blur_image infile -o outfile \n"
            "\t[-r radius] radius now should be odd number ranging [3-19], disabled for loongson\n"
            "\t[-b] adjust brightness after blurring\n"
            "\t[-d drmdev] use drmdev (/dev/dri/card0 e.g) to render\n"
            "\t[-p rendering passes] iterate passes of rendering, raning [1-INF]\n");
}

int main(int argc, char *argv[])
{
    int ch;
    while ((ch = getopt(argc, argv, "d:o:r:p:bh")) != -1) {
        switch(ch) {
            case 'd': drmdev = strdup(optarg); break;
            case 'o': outfile = strdup(optarg); break;
            case 'r': radius = atoi(optarg); break;
            case 'p': rounds = atoi(optarg); break;
            case 'b': adjustBrightness = true; break;
            case 'h': 
            default: usage(); break;
        }
    }

    if (optind < argc && !infile) {
        infile = strdup(argv[optind]);
    }

    if (!infile || !outfile) {
        usage();
    }

    cout << "outfile: " << outfile << ", infile: " << infile << ", r: " << radius << ", p: " << rounds << endl;
    ctx.img_path = strdup(infile);
    GError *error = NULL;
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(infile, &error);
    ctx.img_data = gdk_pixbuf_get_pixels(pixbuf);
    ctx.ncomp = gdk_pixbuf_get_n_channels(pixbuf);
    ctx.width = gdk_pixbuf_get_width(pixbuf);
    ctx.height = gdk_pixbuf_get_height(pixbuf);
    cout << "image " << (ctx.ncomp == 4? "has": "has no") << " alpha" << endl;
    if (!ctx.img_data) {
        err_quit("load %s failed\n", ctx.img_path);
    }

    ctx.tex_width = ctx.width * 0.25f;
    ctx.tex_height = ctx.height * 0.25f;

    setup_context();
    gl_init();
    render();

    g_object_unref (pixbuf);
    free(infile);
    free(outfile);
    cleanup();
    return 0;
}

