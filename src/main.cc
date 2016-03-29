#include <iostream>
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

using namespace std;

#define err_quit(...) do { \
    fprintf(stderr, __VA_ARGS__); \
    exit(-1); \
} while (0)

static struct context {
    GLFWwindow* window;
    int width, height;

    GLuint program;
    GLuint ts, vs;
    GLuint vao, vbo;

} ctx = {
    nullptr,
    800, 600,
    0,
};

const GLchar* ts_code = R"(
#version 150
in vec2 position;
in vec3 vertexColor;

out vec3 fragColor;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    fragColor = vertexColor;
}
)";

const GLchar* vs_code = R"(
#version 150
in vec3 fragColor;
out vec4 outColor;

void main() {
    outColor = vec4(fragColor.rgb, 1.0);
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

static void gl_init()
{
    //glfwGetFramebufferSize(ctx.window, &ctx.width, &ctx.height);
    glViewport(0, 0, ctx.width, ctx.height);

    glGenVertexArrays(1, &ctx.vao);
    glBindVertexArray(ctx.vao);

    glGenBuffers(1, &ctx.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ctx.vbo);

    static GLfloat data[] = {
        0.0f, 0.5f,   1.0f, 0.0f, 0.0f,
        0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), &data, GL_STATIC_DRAW);

    ctx.program = glCreateProgram();

    ctx.ts = build_shader(ts_code, GL_VERTEX_SHADER);
    glAttachShader(ctx.program, ctx.ts);
    ctx.vs = build_shader(vs_code, GL_FRAGMENT_SHADER);
    glAttachShader(ctx.program, ctx.vs);

    glLinkProgram(ctx.program);
    GLint result = GL_TRUE;
    glGetProgramiv(ctx.program, GL_LINK_STATUS, &result);
    if (result == GL_FALSE) {
        GLchar log[1024];
        glGetProgramInfoLog(ctx.program, sizeof log - 1, NULL, log);
        cerr << log << endl;
    }

    GLint pos_attrib = glGetAttribLocation(ctx.program, "position");
    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), 0);

    GLint clr_attrib = glGetAttribLocation(ctx.program, "vertexColor");
    glEnableVertexAttribArray(clr_attrib);
    glVertexAttribPointer(clr_attrib, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat),
            (const GLvoid*)(2*sizeof(GLfloat)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void render()
{
    //cerr << __PRETTY_FUNCTION__ << endl;
    glValidateProgram(ctx.program);

    GLint validate = GL_TRUE;
    glGetProgramiv(ctx.program, GL_VALIDATE_STATUS, &validate);
    if (validate == GL_FALSE) {
        err_quit("program is invalid\n");
    }

    //glClearColor(0.3, 1.0, 0.4, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(ctx.program);
    glBindVertexArray(ctx.vao);

    glDrawArrays(GL_TRIANGLES, 0, 3);

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
        glfwPollEvents();

        if (glfwGetTime() - time >= 1.0/30.0) {
            time = glfwGetTime();
            render();
        }
    }

    glfwTerminate();
    return 0;
}
