#include <gtest/gtest.h>
#include "../src/blur_utils.h"
#include <cstring>
#include <cstdlib>

class ShaderUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test data
    }
    
    void TearDown() override {
        // Clean up test data
    }
};

TEST_F(ShaderUtilsTest, ValidVertexShader) {
    const char* vertex_shader = R"(
        #version 120
        attribute vec2 position;
        void main() {
            gl_Position = vec4(position, 0.0, 1.0);
        }
    )";
    
    GLuint shader_id = build_shader(vertex_shader, GL_VERTEX_SHADER);
    EXPECT_NE(shader_id, 0u);
}

TEST_F(ShaderUtilsTest, ValidFragmentShader) {
    const char* fragment_shader = R"(
        #version 120
        varying vec2 texCoord;
        uniform sampler2D sampler;
        void main() {
            gl_FragColor = texture2D(sampler, texCoord);
        }
    )";
    
    GLuint shader_id = build_shader(fragment_shader, GL_FRAGMENT_SHADER);
    EXPECT_NE(shader_id, 0u);
}

TEST_F(ShaderUtilsTest, ValidFragmentShaderES3) {
    const char* fragment_shader = R"(
        #version 300 es
        precision mediump float;
        in vec2 texCoord;
        out vec4 outColor;
        uniform sampler2D sampler;
        void main() {
            outColor = texture(sampler, texCoord);
        }
    )";
    
    GLuint shader_id = build_shader(fragment_shader, GL_FRAGMENT_SHADER);
    EXPECT_NE(shader_id, 0u);
}

TEST_F(ShaderUtilsTest, InvalidVertexShader) {
    const char* invalid_shader = R"(
        #version 120
        attribute vec2 position;
        void main() {
            // Missing gl_Position assignment
        }
    )";
    
    GLuint shader_id = build_shader(invalid_shader, GL_VERTEX_SHADER);
    EXPECT_EQ(shader_id, 0u);
}

TEST_F(ShaderUtilsTest, InvalidFragmentShader) {
    const char* invalid_shader = R"(
        #version 120
        varying vec2 texCoord;
        void main() {
            // Missing gl_FragColor or outColor assignment
        }
    )";
    
    GLuint shader_id = build_shader(invalid_shader, GL_FRAGMENT_SHADER);
    EXPECT_EQ(shader_id, 0u);
}

TEST_F(ShaderUtilsTest, NullShaderCode) {
    GLuint shader_id = build_shader(nullptr, GL_VERTEX_SHADER);
    EXPECT_EQ(shader_id, 0u);
}

TEST_F(ShaderUtilsTest, ShaderTemplate) {
    const char* template_shader = R"(
        #version 120
        uniform float kernel[%d];
        void main() {
            gl_FragColor = vec4(1.0);
        }
    )";
    
    char* result = build_shader_template(template_shader, 21);
    ASSERT_NE(result, nullptr);
    
    // Check that the template was filled correctly
    EXPECT_TRUE(strstr(result, "kernel[21]") != nullptr);
    
    free(result);
}

TEST_F(ShaderUtilsTest, ShaderTemplateMultipleParams) {
    const char* template_shader = R"(
        const float lightness = %f;
        const float saturation = %f;
        void main() {
            gl_FragColor = vec4(%f, %f, 1.0, 1.0);
        }
    )";
    
    char* result = build_shader_template(template_shader, 0.8f, 1.2f, 0.5f, 0.7f);
    ASSERT_NE(result, nullptr);
    
    // Check that values were substituted
    EXPECT_TRUE(strstr(result, "0.8") != nullptr);
    EXPECT_TRUE(strstr(result, "1.2") != nullptr);
    EXPECT_TRUE(strstr(result, "0.5") != nullptr);
    EXPECT_TRUE(strstr(result, "0.7") != nullptr);
    
    free(result);
}

TEST_F(ShaderUtilsTest, NullTemplate) {
    char* result = build_shader_template(nullptr, 5);
    EXPECT_EQ(result, nullptr);
}