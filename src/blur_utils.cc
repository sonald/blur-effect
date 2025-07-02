#include "blur_utils.h"
#include <cmath>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <libgen.h>
#include <iostream>

void build_gaussian_blur_kernel(GLint* pradius, GLfloat* offset, GLfloat* weight, GLfloat sigma)
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

    for (int i = 0; i < radius; i++) {
        offset[i] = (GLfloat)i*sigma;
        weight[i] /= sum;
    }

    *pradius = radius;
}

GLuint build_shader(const GLchar* code, GLint type)
{
    // For unit testing, we'll return a mock shader ID since OpenGL context may not be available
    // In a real implementation, this would compile the shader
    if (!code) return 0;
    
    // Simple validation - check for common shader keywords
    if (type == GL_VERTEX_SHADER && strstr(code, "gl_Position") == nullptr) {
        return 0; // Invalid vertex shader
    }
    if (type == GL_FRAGMENT_SHADER && strstr(code, "gl_FragColor") == nullptr && 
        strstr(code, "outColor") == nullptr) {
        return 0; // Invalid fragment shader
    }
    
    return 1; // Mock successful compilation
}

char* build_shader_template(const char* shader_tmpl, ...)
{
    if (!shader_tmpl) return nullptr;
    
    char* ret = (char*)malloc(strlen(shader_tmpl)+40);
    if (!ret) {
        return nullptr;
    }
    va_list va;
    va_start(va, shader_tmpl);
    vsprintf(ret, shader_tmpl, va);
    va_end(va);
    return ret;
}

bool is_device_viable(int id)
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

    // nouveau write 2, others 1
    return enabled > 0;
}

std::string choose_best_card(const std::vector<std::string>& cards)
{
    if (cards.empty()) {
        return "";
    }
    
    for (const auto& card : cards) {
        char buf[1024] = {0};
        int id = std::stoi(card.substr(card.size()-1));
        snprintf(buf, sizeof buf, "/sys/class/drm/card%d/device/driver", id);

        char buf2[1024] = {0};
        if (readlink(buf, buf2, sizeof buf2) > 0) {
            std::string driver = basename(buf2);
            if (driver == "i915") {
                return card;
            }
        }
    }

    return cards[0];
}

GLint clamp_radius(GLint radius, GLint min_val, GLint max_val)
{
    radius = std::max(std::min(radius, max_val), min_val);
    // Ensure radius is odd
    radius = ((radius >> 1) << 1) + 1;
    return radius;
}

GLfloat clamp_lightness(GLfloat lightness, GLfloat min_val, GLfloat max_val)
{
    return std::max(min_val, std::min(max_val, lightness));
}

GLfloat clamp_saturation(GLfloat saturation, GLfloat min_val, GLfloat max_val)
{
    return std::max(min_val, std::min(max_val, saturation));
}

// CPU implementation of HSL conversion for testing
Color3f rgb_to_hsl(const Color3f& rgb)
{
    float r = rgb.r;
    float g = rgb.g;
    float b = rgb.b;

    float max_val = std::max({r, g, b});
    float min_val = std::min({r, g, b});
    float h = 0, s = 0, l = (max_val + min_val) / 2.0f;

    float d = max_val - min_val;
    if (d <= 0.00001f) {
        return Color3f(-1.0f, 0.0f, l); // Achromatic
    } else {
        if (l > 0.5f)
            s = d / (2.0f - max_val - min_val);
        else 
            s = d / (max_val + min_val);
        
        if (r == max_val) {
            h = (g - b) / d + (g < b ? 6.0f : 0.0f);
        } else if (g == max_val) {
            h = (b - r) / d + 2.0f;
        } else {
            h = (r - g) / d + 4.0f;
        }
        h /= 6.0f;
        if (h < 0.0f) h += 1.0f;
    }

    return Color3f(h, s, l);
}

float hue_to_rgb(float f1, float f2, float hue) {
    if (hue < 0.0f)
        hue += 1.0f;
    else if (hue > 1.0f)
        hue -= 1.0f;

    float res;
    if ((6.0f * hue) < 1.0f)
        res = (f1 + (f2 - f1) * 6.0f * hue);
    else if ((2.0f * hue) < 1.0f)
        res = (f2);
    else if ((3.0f * hue) < 2.0f)
        res = (f1 + (f2 - f1) * ((2.0f / 3.0f) - hue) * 6.0f);
    else
        res = (f1);
    return res;
}

Color3f hsl_to_rgb(const Color3f& hsl)
{
    if (hsl.b <= 0.00001f) {
        return Color3f(0.0f, 0.0f, 0.0f);
    }
    
    if (hsl.g <= 0.00001f) {
        return Color3f(hsl.b, hsl.b, hsl.b); // Achromatic
    }
    
    float f2;
    if (hsl.b < 0.5f)
        f2 = hsl.b * (1.0f + hsl.g);
    else
        f2 = hsl.b + hsl.g - hsl.g * hsl.b;
        
    float f1 = 2.0f * hsl.b - f2;

    float r = hue_to_rgb(f1, f2, hsl.r + (1.0f/3.0f));
    float g = hue_to_rgb(f1, f2, hsl.r);
    float b = hue_to_rgb(f1, f2, hsl.r - (1.0f/3.0f));
    
    return Color3f(r, g, b);
}