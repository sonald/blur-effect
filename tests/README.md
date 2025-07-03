# Unit Tests for Blur Image

This directory contains comprehensive unit tests for the blur image application using Google Test framework.

## Test Coverage

The unit tests cover the following areas:

### 1. Gaussian Kernel Generation (`test_gaussian_kernel.cc`)
- **BasicKernelGeneration**: Tests that Gaussian blur kernels are generated correctly with proper weight distribution
- **DifferentSigmaValues**: Verifies that different sigma values produce appropriate kernel offsets
- **SmallRadius**: Ensures minimum radius constraints are enforced
- **LargeRadius**: Tests kernel generation with large radius values
- **EvenRadiusConversion**: Verifies that even radius values are converted to odd

### 2. Shader Utilities (`test_shader_utils.cc`)
- **ValidVertexShader**: Tests compilation of valid vertex shaders
- **ValidFragmentShader**: Tests compilation of valid fragment shaders (GL 2.1 and ES 3.0)
- **InvalidShaders**: Tests rejection of invalid shader code
- **ShaderTemplate**: Tests shader code generation from templates
- **NullHandling**: Tests proper handling of null inputs

### 3. Device Utilities and Parameter Validation (`test_device_utils.cc`)
- **Graphics Card Selection**: Tests device enumeration and best card selection
- **Parameter Clamping**: Tests radius, lightness, and saturation value validation
- **HSL/RGB Conversion**: Comprehensive tests for color space conversion
  - Pure color conversions (red, green, blue)
  - Grayscale handling (black, white, gray)
  - Round-trip conversion accuracy

## Building and Running Tests

### Prerequisites
- CMake 2.8.11 or later
- C++11 compatible compiler
- pkg-config
- Required system libraries: `gdk-pixbuf-2.0`, `libdrm`, `gbm`, `egl`, `glesv2`

### Build Instructions

1. **Configure with tests enabled (default):**
   ```bash
   mkdir build
   cd build
   cmake ..
   ```

2. **Disable tests (if needed):**
   ```bash
   cmake -DBUILD_TESTS=OFF ..
   ```

3. **Build the tests:**
   ```bash
   make blur_tests
   ```

4. **Run the tests:**
   ```bash
   ./blur_tests
   ```

   Or use CTest:
   ```bash
   make test
   ```

### Test Dependencies

The tests automatically download and build Google Test if not found on the system. If Google Test is already installed, the system version will be used.

The testing library (`blur_lib`) contains the extracted testable functions from the main application, allowing tests to run without requiring a full OpenGL context.

## Test Organization

```
tests/
├── README.md                 # This file
├── test_main.cc             # Test runner entry point
├── test_gaussian_kernel.cc  # Gaussian blur kernel tests
├── test_shader_utils.cc     # Shader compilation and template tests
└── test_device_utils.cc     # Device utils and color conversion tests
```

## Adding New Tests

To add new tests:

1. Create a new test file in the `tests/` directory
2. Include the appropriate headers:
   ```cpp
   #include <gtest/gtest.h>
   #include "../src/blur_utils.h"
   ```
3. Add the test file to the `blur_tests` target in `CMakeLists.txt`
4. Follow the existing test patterns and naming conventions

## Continuous Integration

These tests are designed to run in headless environments and do not require a graphics display or OpenGL context, making them suitable for CI/CD pipelines.

## Test Results

The tests validate:
- Mathematical correctness of Gaussian blur algorithms
- Proper parameter validation and clamping
- Color space conversion accuracy
- Shader template generation
- Device detection logic (where applicable)

All tests should pass on systems with the required dependencies installed.