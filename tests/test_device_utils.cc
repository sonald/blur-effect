#include <gtest/gtest.h>
#include "../src/blur_utils.h"
#include <vector>
#include <string>

class DeviceUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test data
    }
    
    void TearDown() override {
        // Clean up test data
    }
};

TEST_F(DeviceUtilsTest, ChooseBestCardEmpty) {
    std::vector<std::string> empty_cards;
    std::string result = choose_best_card(empty_cards);
    EXPECT_EQ(result, "");
}

TEST_F(DeviceUtilsTest, ChooseBestCardSingle) {
    std::vector<std::string> cards = {"/dev/dri/card0"};
    std::string result = choose_best_card(cards);
    EXPECT_EQ(result, "/dev/dri/card0");
}

TEST_F(DeviceUtilsTest, ChooseBestCardMultiple) {
    std::vector<std::string> cards = {"/dev/dri/card0", "/dev/dri/card1", "/dev/dri/card2"};
    std::string result = choose_best_card(cards);
    // Should return the first card if no Intel card is found
    EXPECT_EQ(result, "/dev/dri/card0");
}

// Parameter validation tests
TEST_F(DeviceUtilsTest, ClampRadiusValid) {
    EXPECT_EQ(clamp_radius(5), 5);
    EXPECT_EQ(clamp_radius(7), 7);
    EXPECT_EQ(clamp_radius(15), 15);
}

TEST_F(DeviceUtilsTest, ClampRadiusTooSmall) {
    EXPECT_EQ(clamp_radius(1), 3);
    EXPECT_EQ(clamp_radius(2), 3);
    EXPECT_EQ(clamp_radius(0), 3);
    EXPECT_EQ(clamp_radius(-5), 3);
}

TEST_F(DeviceUtilsTest, ClampRadiusTooLarge) {
    EXPECT_EQ(clamp_radius(50), 49);
    EXPECT_EQ(clamp_radius(100), 49);
    EXPECT_EQ(clamp_radius(1000), 49);
}

TEST_F(DeviceUtilsTest, ClampRadiusEvenToOdd) {
    EXPECT_EQ(clamp_radius(4), 5);
    EXPECT_EQ(clamp_radius(6), 7);
    EXPECT_EQ(clamp_radius(8), 9);
    EXPECT_EQ(clamp_radius(20), 21);
    EXPECT_EQ(clamp_radius(48), 49);
}

TEST_F(DeviceUtilsTest, ClampLightness) {
    EXPECT_FLOAT_EQ(clamp_lightness(0.5f), 0.5f);
    EXPECT_FLOAT_EQ(clamp_lightness(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(clamp_lightness(255.0f), 255.0f);
    
    // Test clamping
    EXPECT_FLOAT_EQ(clamp_lightness(-1.0f), 0.0f);
    EXPECT_FLOAT_EQ(clamp_lightness(300.0f), 255.0f);
}

TEST_F(DeviceUtilsTest, ClampSaturation) {
    EXPECT_FLOAT_EQ(clamp_saturation(0.5f), 0.5f);
    EXPECT_FLOAT_EQ(clamp_saturation(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(clamp_saturation(255.0f), 255.0f);
    
    // Test clamping
    EXPECT_FLOAT_EQ(clamp_saturation(-1.0f), 0.0f);
    EXPECT_FLOAT_EQ(clamp_saturation(300.0f), 255.0f);
}

TEST_F(DeviceUtilsTest, ClampCustomRange) {
    EXPECT_EQ(clamp_radius(10, 5, 20), 11); // 10 -> 11 (make odd)
    EXPECT_EQ(clamp_radius(25, 5, 20), 19); // 25 -> 19 (clamp and make odd)
    EXPECT_EQ(clamp_radius(2, 5, 20), 5);   // 2 -> 5 (clamp and make odd)
    
    EXPECT_FLOAT_EQ(clamp_lightness(0.5f, 0.2f, 0.8f), 0.5f);
    EXPECT_FLOAT_EQ(clamp_lightness(0.1f, 0.2f, 0.8f), 0.2f);
    EXPECT_FLOAT_EQ(clamp_lightness(0.9f, 0.2f, 0.8f), 0.8f);
}

// HSL/RGB conversion tests
TEST_F(DeviceUtilsTest, RGBToHSLPureColors) {
    // Pure red
    Color3f red(1.0f, 0.0f, 0.0f);
    Color3f hsl_red = rgb_to_hsl(red);
    EXPECT_NEAR(hsl_red.r, 0.0f, 0.001f); // Hue
    EXPECT_NEAR(hsl_red.g, 1.0f, 0.001f); // Saturation
    EXPECT_NEAR(hsl_red.b, 0.5f, 0.001f); // Lightness
    
    // Pure green
    Color3f green(0.0f, 1.0f, 0.0f);
    Color3f hsl_green = rgb_to_hsl(green);
    EXPECT_NEAR(hsl_green.r, 1.0f/3.0f, 0.001f); // Hue ~120 degrees
    EXPECT_NEAR(hsl_green.g, 1.0f, 0.001f);       // Saturation
    EXPECT_NEAR(hsl_green.b, 0.5f, 0.001f);       // Lightness
    
    // Pure blue
    Color3f blue(0.0f, 0.0f, 1.0f);
    Color3f hsl_blue = rgb_to_hsl(blue);
    EXPECT_NEAR(hsl_blue.r, 2.0f/3.0f, 0.001f); // Hue ~240 degrees
    EXPECT_NEAR(hsl_blue.g, 1.0f, 0.001f);       // Saturation
    EXPECT_NEAR(hsl_blue.b, 0.5f, 0.001f);       // Lightness
}

TEST_F(DeviceUtilsTest, RGBToHSLGrayscale) {
    // Black
    Color3f black(0.0f, 0.0f, 0.0f);
    Color3f hsl_black = rgb_to_hsl(black);
    EXPECT_EQ(hsl_black.r, -1.0f); // Undefined hue for achromatic
    EXPECT_NEAR(hsl_black.g, 0.0f, 0.001f); // No saturation
    EXPECT_NEAR(hsl_black.b, 0.0f, 0.001f); // Lightness
    
    // White
    Color3f white(1.0f, 1.0f, 1.0f);
    Color3f hsl_white = rgb_to_hsl(white);
    EXPECT_EQ(hsl_white.r, -1.0f); // Undefined hue for achromatic
    EXPECT_NEAR(hsl_white.g, 0.0f, 0.001f); // No saturation
    EXPECT_NEAR(hsl_white.b, 1.0f, 0.001f); // Lightness
    
    // Gray
    Color3f gray(0.5f, 0.5f, 0.5f);
    Color3f hsl_gray = rgb_to_hsl(gray);
    EXPECT_EQ(hsl_gray.r, -1.0f); // Undefined hue for achromatic
    EXPECT_NEAR(hsl_gray.g, 0.0f, 0.001f); // No saturation
    EXPECT_NEAR(hsl_gray.b, 0.5f, 0.001f); // Lightness
}

TEST_F(DeviceUtilsTest, HSLToRGBPureColors) {
    // Pure red (H=0, S=1, L=0.5)
    Color3f hsl_red(0.0f, 1.0f, 0.5f);
    Color3f rgb_red = hsl_to_rgb(hsl_red);
    EXPECT_NEAR(rgb_red.r, 1.0f, 0.001f);
    EXPECT_NEAR(rgb_red.g, 0.0f, 0.001f);
    EXPECT_NEAR(rgb_red.b, 0.0f, 0.001f);
    
    // Pure green (H=1/3, S=1, L=0.5)
    Color3f hsl_green(1.0f/3.0f, 1.0f, 0.5f);
    Color3f rgb_green = hsl_to_rgb(hsl_green);
    EXPECT_NEAR(rgb_green.r, 0.0f, 0.001f);
    EXPECT_NEAR(rgb_green.g, 1.0f, 0.001f);
    EXPECT_NEAR(rgb_green.b, 0.0f, 0.001f);
    
    // Pure blue (H=2/3, S=1, L=0.5)
    Color3f hsl_blue(2.0f/3.0f, 1.0f, 0.5f);
    Color3f rgb_blue = hsl_to_rgb(hsl_blue);
    EXPECT_NEAR(rgb_blue.r, 0.0f, 0.001f);
    EXPECT_NEAR(rgb_blue.g, 0.0f, 0.001f);
    EXPECT_NEAR(rgb_blue.b, 1.0f, 0.001f);
}

TEST_F(DeviceUtilsTest, HSLToRGBGrayscale) {
    // Black (any H, S=0, L=0)
    Color3f hsl_black(0.5f, 0.0f, 0.0f);
    Color3f rgb_black = hsl_to_rgb(hsl_black);
    EXPECT_NEAR(rgb_black.r, 0.0f, 0.001f);
    EXPECT_NEAR(rgb_black.g, 0.0f, 0.001f);
    EXPECT_NEAR(rgb_black.b, 0.0f, 0.001f);
    
    // White (any H, S=0, L=1)
    Color3f hsl_white(0.5f, 0.0f, 1.0f);
    Color3f rgb_white = hsl_to_rgb(hsl_white);
    EXPECT_NEAR(rgb_white.r, 1.0f, 0.001f);
    EXPECT_NEAR(rgb_white.g, 1.0f, 0.001f);
    EXPECT_NEAR(rgb_white.b, 1.0f, 0.001f);
    
    // Gray (any H, S=0, L=0.5)
    Color3f hsl_gray(0.3f, 0.0f, 0.5f);
    Color3f rgb_gray = hsl_to_rgb(hsl_gray);
    EXPECT_NEAR(rgb_gray.r, 0.5f, 0.001f);
    EXPECT_NEAR(rgb_gray.g, 0.5f, 0.001f);
    EXPECT_NEAR(rgb_gray.b, 0.5f, 0.001f);
}

TEST_F(DeviceUtilsTest, RGBHSLRoundTrip) {
    // Test round-trip conversion for various colors
    std::vector<Color3f> test_colors = {
        Color3f(0.8f, 0.3f, 0.1f),  // Orange-ish
        Color3f(0.2f, 0.7f, 0.9f),  // Blue-ish
        Color3f(0.6f, 0.6f, 0.2f),  // Yellow-ish
        Color3f(0.9f, 0.1f, 0.8f),  // Magenta-ish
    };
    
    for (const auto& original : test_colors) {
        Color3f hsl = rgb_to_hsl(original);
        Color3f converted = hsl_to_rgb(hsl);
        
        EXPECT_NEAR(original.r, converted.r, 0.01f);
        EXPECT_NEAR(original.g, converted.g, 0.01f);
        EXPECT_NEAR(original.b, converted.b, 0.01f);
    }
}