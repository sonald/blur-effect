#include <gtest/gtest.h>
#include "../src/blur_utils.h"
#include <cmath>

class GaussianKernelTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test data
    }
    
    void TearDown() override {
        // Clean up test data
    }
};

TEST_F(GaussianKernelTest, BasicKernelGeneration) {
    GLint radius = 3;
    GLfloat offset[50] = {0};
    GLfloat weight[50] = {0};
    GLfloat sigma = 1.0f;
    
    build_gaussian_blur_kernel(&radius, offset, weight, sigma);
    
    // Check that radius is odd
    EXPECT_TRUE(radius % 2 == 1);
    EXPECT_GE(radius, 3);
    
    // Check that weights sum to approximately 1.0
    float weight_sum = weight[radius + 1]; // center weight
    for (int i = 1; i <= radius; i++) {
        weight_sum += 2 * weight[radius + 1 + i]; // symmetric weights
    }
    EXPECT_NEAR(weight_sum, 1.0f, 0.1f);
    
    // Check that center weight is the largest
    for (int i = 1; i <= radius; i++) {
        EXPECT_GE(weight[radius + 1], weight[radius + 1 + i]);
    }
}

TEST_F(GaussianKernelTest, DifferentSigmaValues) {
    GLint radius = 5;
    GLfloat offset1[50] = {0}, offset2[50] = {0};
    GLfloat weight1[50] = {0}, weight2[50] = {0};
    
    GLint r1 = radius, r2 = radius;
    build_gaussian_blur_kernel(&r1, offset1, weight1, 1.0f);
    build_gaussian_blur_kernel(&r2, offset2, weight2, 2.0f);
    
    // Higher sigma should result in larger offsets
    for (int i = 1; i < r1; i++) {
        EXPECT_LT(offset1[i], offset2[i]);
    }
}

TEST_F(GaussianKernelTest, SmallRadius) {
    GLint radius = 1;
    GLfloat offset[50] = {0};
    GLfloat weight[50] = {0};
    
    build_gaussian_blur_kernel(&radius, offset, weight, 1.0f);
    
    // Minimum radius should be 3
    EXPECT_GE(radius, 3);
    EXPECT_TRUE(radius % 2 == 1);
}

TEST_F(GaussianKernelTest, LargeRadius) {
    GLint radius = 21;
    GLfloat offset[50] = {0};
    GLfloat weight[50] = {0};
    
    build_gaussian_blur_kernel(&radius, offset, weight, 1.0f);
    
    EXPECT_TRUE(radius % 2 == 1);
    EXPECT_EQ(radius, 21);
    
    // Check weights are positive and decreasing
    for (int i = 1; i <= radius; i++) {
        EXPECT_GT(weight[radius + 1 + i], 0.0f);
        if (i > 1) {
            EXPECT_GE(weight[radius + i], weight[radius + 1 + i]);
        }
    }
}

TEST_F(GaussianKernelTest, EvenRadiusConversion) {
    GLint radius = 4; // Even radius
    GLfloat offset[50] = {0};
    GLfloat weight[50] = {0};
    
    build_gaussian_blur_kernel(&radius, offset, weight, 1.0f);
    
    // Should be converted to odd (5)
    EXPECT_EQ(radius, 5);
    EXPECT_TRUE(radius % 2 == 1);
}