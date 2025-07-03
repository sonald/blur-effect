#ifndef BLUR_UTILS_H
#define BLUR_UTILS_H

#include <vector>
#include <string>

// OpenGL types for testing (avoid full OpenGL dependency in tests)
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#endif

typedef unsigned int GLuint;
typedef int GLint;
typedef float GLfloat;
typedef char GLchar;

// Gaussian kernel generation
void build_gaussian_blur_kernel(GLint* pradius, GLfloat* offset, GLfloat* weight, GLfloat sigma = 1.0);

// Shader utilities
GLuint build_shader(const GLchar* code, GLint type);
char* build_shader_template(const char* shader_tmpl, ...);

// Device utilities  
bool is_device_viable(int id);
std::string choose_best_card(const std::vector<std::string>& cards);

// Parameter validation
GLint clamp_radius(GLint radius, GLint min_val = 3, GLint max_val = 49);
GLfloat clamp_lightness(GLfloat lightness, GLfloat min_val = 0.0f, GLfloat max_val = 255.0f);
GLfloat clamp_saturation(GLfloat saturation, GLfloat min_val = 0.0f, GLfloat max_val = 255.0f);

// HSL/RGB conversion utilities (CPU versions for testing)
struct Color3f {
    float r, g, b;
    Color3f(float r = 0, float g = 0, float b = 0) : r(r), g(g), b(b) {}
};

Color3f rgb_to_hsl(const Color3f& rgb);
Color3f hsl_to_rgb(const Color3f& hsl);

#endif // BLUR_UTILS_H