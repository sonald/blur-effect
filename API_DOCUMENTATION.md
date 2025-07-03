# Image Blurring Tool - Comprehensive API Documentation

## Table of Contents
1. [Project Overview](#project-overview)
2. [Installation and Building](#installation-and-building)
3. [Command Line Interface](#command-line-interface)
4. [Core Components](#core-components)
5. [API Reference](#api-reference)
6. [Usage Examples](#usage-examples)
7. [Advanced Features](#advanced-features)
8. [Error Handling](#error-handling)
9. [Performance Considerations](#performance-considerations)
10. [Troubleshooting](#troubleshooting)

## Project Overview

This is an offscreen image blurring tool that implements efficient Gaussian blur using linear sampling technique on GPU. The project is based on the [efficient Gaussian blur with linear sampling](http://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/) algorithm and provides both command-line and demonstration interfaces.

### Key Features
- **GPU-accelerated blurring** using OpenGL ES 3.0
- **Efficient Gaussian blur** with linear sampling optimization
- **Brightness adjustment** capabilities
- **HSL (Hue, Saturation, Lightness) manipulation**
- **Multiple rendering passes** for enhanced blur effects
- **Offscreen rendering** without requiring X Server privileges (with render nodes)
- **Multiple image formats** support via GDK-PixBuf

### Project Structure
```
.
├── src/
│   ├── blur_image.cc    # Main CLI application
│   └── main.cc          # Demo application with GUI
├── CMakeLists.txt       # Build configuration
├── README.md           # Basic usage instructions
└── LICENSE             # License information
```

## Installation and Building

### Dependencies

#### Required Dependencies
- **libglm-dev** - OpenGL Mathematics library
- **libgbm-dev** - Generic Buffer Management library
- **libegl1-mesa-dev** - EGL library
- **libgles2-mesa-dev** - OpenGL ES 2.0 library
- **libgdk-pixbuf2.0-dev** - Image loading/saving library

#### Optional Dependencies (for demo)
- **libglfw3-dev** - Windowing library for demo application
- **libglew-dev** - OpenGL Extension Wrangler

### Build Instructions

#### Standard Build
```bash
mkdir build && cd build
cmake ..
make
```

#### Build with Demo
```bash
mkdir build && cd build
cmake -DBUILD_DEMO=on ..
make
```

#### Installation
```bash
sudo make install
```

This installs the binaries to `/usr/local/bin/` by default.

## Command Line Interface

### blur_image

The main command-line tool for image blurring.

#### Syntax
```bash
blur_image [OPTIONS] input_file -o output_file
```

#### Required Arguments
- **input_file** - Path to the input image file
- **-o output_file** - Path for the output blurred image

#### Optional Parameters

| Option | Type | Range | Default | Description |
|--------|------|-------|---------|-------------|
| `-r radius` | integer | 3-49 (odd only) | 19 | Blur radius in pixels |
| `-S sigma` | float | > 0.0 | 1.0 | Sample distance multiplier |
| `-p passes` | integer | 1-∞ | 1 | Number of rendering passes |
| `-d device` | string | - | auto | DRM device path (e.g., /dev/dri/renderD128) |
| `-b` | flag | - | false | Enable brightness adjustment |
| `-l lightness` | float | 0.0-255.0 | 1.0 | Lightness multiplier |
| `-s saturation` | float | 0.0-255.0 | 1.0 | Saturation multiplier |
| `-h` | flag | - | - | Show help message |

#### Usage Examples

##### Basic Blur
```bash
./blur_image input.jpg -o output.jpg
```

##### Advanced Blur with Custom Parameters
```bash
./blur_image -r 25 -p 3 -S 1.5 input.png -o blurred_output.png
```

##### Using Render Node (No sudo required)
```bash
./blur_image -d /dev/dri/renderD128 -r 15 input.jpg -o output.jpg
```

##### Brightness and Color Adjustment
```bash
./blur_image -b -l 0.8 -s 1.2 input.jpg -o adjusted_output.jpg
```

### blur-exp (Demo Application)

A windowing demonstration application that shows real-time blur effects.

#### Usage
```bash
./blur-exp
```

**Note:** Requires `texture.jpg` in the current directory.

#### Controls
- **ESC** - Exit the application
- **Automatic cycling** through different blur parameters

## Core Components

### Rendering Context Structure

The application uses a global context structure to manage OpenGL state:

```cpp
struct context {
    EGLDisplay display;          // EGL display connection
    EGLContext gl_context;       // OpenGL ES context
    EGLSurface surface;          // Rendering surface
    int fd;                      // DRM device file descriptor
    struct gbm_device *gbm;      // GBM device
    struct gbm_surface *gbm_surface; // GBM surface
    
    // Image data
    char* img_path;              // Input image path
    int width, height, ncomp;    // Image dimensions and components
    unsigned char* img_data;     // Raw image data
    
    // OpenGL objects
    GLuint program, programH;    // Shader programs (vertical/horizontal)
    GLuint programDirect;        // Direct copy shader
    GLuint programSaveBrt;       // Brightness calculation shader
    GLuint programSetBrt;        // Brightness adjustment shader
    GLuint programSetLgt;        // Lightness adjustment shader
    
    GLuint vbo;                  // Vertex buffer object
    GLuint tex;                  // Main texture
    GLuint ubo;                  // Uniform buffer object
    GLuint fbTex[2];            // Framebuffer textures
    GLuint fb[2];               // Framebuffers
    
    // Additional features
    GLuint brtFb, brtTex;       // Brightness adjustment
    GLuint lgtFb, lgtTex;       // Lightness adjustment
    bool brightnessAdjusted;     // Brightness adjustment status
    bool lightnessAdjusted;      // Lightness adjustment status
    
    int tex_width, tex_height;   // Texture dimensions
};
```

### Shader Programs

The application uses multiple OpenGL ES 3.0 shaders:

#### 1. Vertex Shader (ts_code)
- **Purpose**: Transform vertices and pass texture coordinates
- **Inputs**: Position, vertex color, texture coordinates
- **Outputs**: Transformed position, fragment color, texture coordinates

#### 2. Vertical Blur Fragment Shader (vs_code)
- **Purpose**: Apply Gaussian blur in vertical direction
- **Features**: Uses uniform buffer for kernel data
- **Optimization**: Linear sampling for improved performance

#### 3. Horizontal Blur Fragment Shader (vs_code_h)
- **Purpose**: Apply Gaussian blur in horizontal direction
- **Features**: Completes two-pass blur algorithm

#### 4. Direct Copy Shader (vs_direct)
- **Purpose**: Direct texture copy without processing
- **Use**: Final output rendering

#### 5. Brightness Shaders
- **vs_save_brightness**: Calculate brightness values
- **vs_set_brightness**: Apply brightness adjustments

#### 6. HSL Adjustment Shader (vs_set_lightness)
- **Purpose**: Modify hue, saturation, and lightness
- **Features**: RGB↔HSV conversion functions

## API Reference

### Core Functions

#### Image Processing Functions

##### `build_gaussian_blur_kernel(GLint* pradius, GLfloat* offset, GLfloat* weight)`
**Purpose**: Constructs Gaussian blur kernel for given radius.

**Parameters**:
- `pradius`: Pointer to blur radius (modified to ensure odd value)
- `offset`: Array to store sample offsets
- `weight`: Array to store kernel weights

**Details**:
- Automatically adjusts radius to nearest odd number
- Implements binomial coefficient calculation
- Applies sigma multiplier for sample distance
- Optimizes using linear sampling technique

```cpp
// Example usage
GLint radius = 15;
GLfloat offsets[50], weights[50];
build_gaussian_blur_kernel(&radius, offsets, weights);
```

##### `render()`
**Purpose**: Main rendering function that applies blur and effects.

**Process**:
1. Validate shader programs
2. Setup uniform buffer objects
3. Apply HSL adjustments (if enabled)
4. Execute multi-pass blur rendering
5. Apply brightness adjustments (if enabled)
6. Output final result
7. Save to image file

**Features**:
- Multi-pass rendering support
- Automatic brightness detection and adjustment
- HSL color space manipulation
- Optimized uniform buffer usage

#### Graphics Context Functions

##### `setup_context()`
**Purpose**: Initialize EGL context and GBM surface.

**Process**:
1. Open DRM device
2. Create GBM device and surface
3. Initialize EGL display
4. Create OpenGL ES 3.0 context
5. Make context current

**Error Handling**:
- Automatic device selection if specified device fails
- Extension requirement validation
- Context creation verification

##### `gl_init()`
**Purpose**: Initialize OpenGL resources and state.

**Setup**:
- Vertex buffer objects
- Texture objects and parameters
- Framebuffer objects
- Shader program compilation and linking
- Uniform buffer objects

##### `cleanup()`
**Purpose**: Release all allocated resources.

**Cleanup**:
- EGL context and surface destruction
- GBM resource cleanup
- File descriptor closure

#### Device Management Functions

##### `open_drm_device()`
**Purpose**: Open and configure DRM device for rendering.

**Logic**:
1. Try user-specified device first
2. Fall back to automatic device selection
3. Open device with appropriate flags

##### `is_device_viable(int id)`
**Purpose**: Check if a DRM device is available and enabled.

**Returns**: `true` if device is viable, `false` otherwise

**Checks**:
- Device file existence
- Enable status in sysfs
- Driver compatibility

##### `choose_best_card(const vector<string>& vs)`
**Purpose**: Select optimal graphics device from available options.

**Priority**:
1. Intel i915 driver (preferred)
2. First available device (fallback)

**Returns**: Path to selected device

#### Shader Management Functions

##### `build_shader(const GLchar* code, GLint type)`
**Purpose**: Compile individual shader from source code.

**Parameters**:
- `code`: Shader source code
- `type`: Shader type (GL_VERTEX_SHADER or GL_FRAGMENT_SHADER)

**Returns**: Compiled shader object ID

**Error Handling**: Automatic compilation error detection and reporting

##### `build_shader_template(const char* shader_tmpl, ...)`
**Purpose**: Build shader source from template with parameters.

**Features**:
- Template parameter substitution
- Dynamic memory allocation
- Variable argument support

##### `build_program(int stage)`
**Purpose**: Create complete shader program for specific rendering stage.

**Stages**:
- Stage 1: Vertical blur
- Stage 2: Horizontal blur  
- Stage 3: Direct copy
- Stage 4: Brightness calculation
- Stage 5: Brightness adjustment
- Stage 6: HSL adjustment

**Returns**: Linked shader program ID

#### Image Adjustment Functions

##### `adjust_brightness(GLuint targetTex)`
**Purpose**: Analyze and adjust image brightness automatically.

**Process**:
1. Create brightness calculation framebuffer
2. Calculate average brightness
3. Apply brightness reduction if needed (threshold: 100)
4. Update context state

##### `adjust_hsl(GLuint targetTex)`
**Purpose**: Apply HSL (Hue, Saturation, Lightness) adjustments.

**Features**:
- RGB to HSV color space conversion
- Separate lightness and saturation controls
- Hardware-accelerated processing

### Configuration Variables

#### Global Parameters

```cpp
// Blur parameters
static int rounds = 1;              // Number of rendering passes
static GLint radius = 19;           // Blur radius (must be odd)
static GLfloat sigma = 1.0;         // Sample distance multiplier

// File paths
static char* infile = NULL;         // Input image path
static char* outfile = NULL;        // Output image path
static char* drmdev = NULL;         // DRM device override

// Color adjustments
static bool adjustBrightness = false;    // Enable brightness adjustment
static bool adjustHSL = false;           // Enable HSL adjustment
static GLfloat lightness = 1.0f;         // Lightness multiplier
static GLfloat saturation = 1.0f;        // Saturation multiplier

// Kernel storage
static GLfloat kernel[101];         // Blur kernel (max radius 49)
```

## Usage Examples

### Basic Image Blurring

#### Simple Blur
```bash
# Basic blur with default settings
./blur_image photo.jpg -o blurred_photo.jpg
```

#### Custom Radius
```bash
# Stronger blur effect
./blur_image -r 31 landscape.png -o soft_landscape.png
```

### Multi-Pass Rendering

#### Multiple Passes for Enhanced Effect
```bash
# 5 passes for very smooth blur
./blur_image -p 5 -r 15 portrait.jpg -o dreamy_portrait.jpg
```

#### Performance vs Quality Trade-off
```bash
# Fewer passes with larger radius (faster)
./blur_image -p 2 -r 25 image.jpg -o fast_blur.jpg

# More passes with smaller radius (higher quality)
./blur_image -p 4 -r 15 image.jpg -o quality_blur.jpg
```

### Advanced Color Processing

#### Brightness and Saturation Adjustment
```bash
# Darken and desaturate for mood effect
./blur_image -b -l 0.7 -s 0.8 -r 21 sunset.jpg -o moody_sunset.jpg
```

#### Enhance Colors
```bash
# Brighten and saturate for vibrant look
./blur_image -l 1.3 -s 1.4 -r 11 flower.jpg -o vibrant_flower.jpg
```

### Render Node Usage (Recommended)

#### Find Available Render Nodes
```bash
ls /dev/dri/renderD*
```

#### Use Specific Render Node
```bash
# No sudo required with render node
./blur_image -d /dev/dri/renderD128 -r 19 input.jpg -o output.jpg
```

### Batch Processing Examples

#### Process Multiple Images
```bash
#!/bin/bash
for img in *.jpg; do
    ./blur_image -r 15 -p 2 "$img" -o "blurred_${img}"
done
```

#### Different Effects for Different Image Types
```bash
#!/bin/bash
# Portraits - soft blur with brightness adjustment
for portrait in portraits/*.jpg; do
    ./blur_image -r 11 -b -l 0.9 "$portrait" -o "soft_${portrait##*/}"
done

# Landscapes - stronger blur with color enhancement
for landscape in landscapes/*.jpg; do
    ./blur_image -r 21 -p 3 -s 1.2 "$landscape" -o "dreamy_${landscape##*/}"
done
```

### Performance Optimization Examples

#### High Performance Setup
```bash
# Use dedicated GPU with render node
./blur_image -d /dev/dri/renderD128 -r 19 -S 1.5 input.jpg -o output.jpg
```

#### Memory-Efficient Processing
```bash
# Single pass with larger sigma for similar effect
./blur_image -p 1 -r 25 -S 2.0 large_image.jpg -o blurred_large.jpg
```

## Advanced Features

### Multi-Pass Rendering

The tool supports multiple rendering passes for enhanced blur quality:

- **Single Pass (-p 1)**: Fastest, basic blur
- **Multiple Passes (-p 2-5)**: Progressive blur enhancement
- **High Pass Count (>5)**: Diminishing returns, slower processing

**Algorithm**: Each pass applies the full two-stage (vertical + horizontal) Gaussian blur to the result of the previous pass.

### Automatic Brightness Adjustment

When `-b` flag is enabled:

1. **Analysis Phase**: Calculate average brightness across the image
2. **Decision Phase**: If average brightness > 100, apply adjustment
3. **Adjustment Phase**: Apply 20% brightness reduction (multiply by 0.8)

**Use Cases**:
- Preventing overexposed blur effects
- Maintaining image contrast
- Artistic mood enhancement

### HSL Color Space Manipulation

#### Lightness Control (`-l` parameter)
- **Range**: 0.0 (black) to 255.0 (extreme bright)
- **Default**: 1.0 (no change)
- **Effect**: Multiplies the lightness component

#### Saturation Control (`-s` parameter)
- **Range**: 0.0 (grayscale) to 255.0 (extreme saturation)
- **Default**: 1.0 (no change)  
- **Effect**: Multiplies the saturation component

#### Color Space Conversion
The tool implements efficient RGB↔HSV conversion in shaders:
- **Hardware acceleration** for real-time processing
- **Precision maintained** through careful algorithm implementation
- **Clamping applied** to prevent overflow artifacts

### Gaussian Blur Implementation

#### Linear Sampling Optimization
The implementation uses the linear sampling technique for improved performance:

1. **Traditional Approach**: N texture samples per pixel
2. **Optimized Approach**: ~N/2 texture samples with linear interpolation
3. **Performance Gain**: Approximately 2x improvement

#### Kernel Generation
```cpp
// Binomial coefficient-based weights
weight[radius+1] = 1.0;
for (int i = 1; i < radius+2; i++) {
    weight[radius-i+1] = weight[radius-i+2] * (N-i+1) / i;
}
```

#### Two-Pass Algorithm
1. **Vertical Pass**: Apply blur in Y direction
2. **Horizontal Pass**: Apply blur in X direction  
3. **Separable Filter**: O(N) complexity instead of O(N²)

### Platform-Specific Optimizations

#### Architecture Support
```cpp
#if defined(__alpha__) || defined(__sw_64__) || defined(__mips__)
adjustHSL = false; // Disable HSL on specific architectures
#endif
```

#### GPU Driver Preferences
The tool automatically prefers Intel i915 driver when available for optimal compatibility.

## Error Handling

### Common Error Scenarios

#### File Access Errors
```bash
# Error: Input file not found
./blur_image nonexistent.jpg -o output.jpg
# Output: load nonexistent.jpg failed

# Error: Permission denied on output
./blur_image input.jpg -o /root/output.jpg
# Output: Permission error message
```

#### Graphics Context Errors
```bash
# Error: No suitable GPU found
./blur_image input.jpg -o output.jpg
# Output: no card found

# Error: EGL context creation failed
./blur_image input.jpg -o output.jpg
# Output: no context created.
```

#### Parameter Validation
```bash
# Invalid radius (automatically corrected)
./blur_image -r 20 input.jpg -o output.jpg
# Radius adjusted to 21 (nearest odd number)

# Radius out of range (clamped)
./blur_image -r 100 input.jpg -o output.jpg
# Radius clamped to 49 (maximum)
```

### Error Recovery Mechanisms

#### Automatic Device Selection
1. Try user-specified device
2. Scan for available devices
3. Check device viability
4. Select best available option
5. Fall back to first viable device

#### Graceful Degradation
- **HSL disabled** on unsupported architectures
- **Automatic parameter clamping** for invalid ranges
- **Fallback device selection** when preferred device unavailable

### Debug Information

#### Verbose Output Examples
```bash
# Device selection information
try to open /dev/dri/card0
backend name: i915

# Blur kernel information  
N = 37, sum = 274877906944
sizes[0] = 104, strides[0] = 4
total ubo size = 424

# Brightness analysis
brightness: 87

# Final processing information
new_path: blurred_output.jpg
```

## Performance Considerations

### Hardware Requirements

#### Minimum Requirements
- **GPU**: OpenGL ES 3.0 compatible
- **RAM**: 512MB (for typical images)
- **Storage**: Input + output image size

#### Recommended Specifications
- **GPU**: Dedicated graphics card with >= 1GB VRAM
- **RAM**: 2GB+ for large images
- **Storage**: SSD for faster I/O

### Performance Factors

#### Image Size Impact
- **Linear relationship**: Processing time scales with pixel count
- **Memory usage**: Approximately 4x image size (RGBA buffers)
- **GPU memory**: Multiple framebuffers for multi-pass rendering

#### Parameter Impact on Performance

| Parameter | Performance Impact | Quality Impact |
|-----------|-------------------|----------------|
| Radius | High (O(n)) | High |
| Passes | Very High (linear) | Medium |
| Sigma | Low | Medium |
| HSL | Medium | Variable |
| Brightness | Low | Low |

#### Optimization Strategies

##### For Speed
```bash
# Single pass with larger radius
./blur_image -p 1 -r 31 -S 2.0 input.jpg -o fast_output.jpg
```

##### For Quality
```bash
# Multiple passes with smaller radius
./blur_image -p 4 -r 15 -S 1.0 input.jpg -o quality_output.jpg
```

##### For Large Images
```bash
# Use render node to avoid privilege requirements
./blur_image -d /dev/dri/renderD128 -p 2 -r 19 large_image.jpg -o output.jpg
```

### Memory Management

#### Automatic Scaling
The tool automatically scales texture size to 25% of original image dimensions for processing:

```cpp
ctx.tex_width = ctx.width * 0.25f;
ctx.tex_height = ctx.height * 0.25f;
```

This provides a good balance between performance and quality for typical blur applications.

#### Buffer Management
- **Ping-pong buffers**: Two framebuffers for multi-pass rendering
- **Automatic cleanup**: All resources freed on exit
- **Error handling**: Graceful cleanup on failures

## Troubleshooting

### Common Issues and Solutions

#### 1. "no card found" Error

**Symptoms**: Application exits with "no card found" message.

**Causes**:
- No graphics hardware detected
- DRM devices not accessible
- Incorrect permissions

**Solutions**:
```bash
# Check available devices
ls -la /dev/dri/

# Verify user permissions
groups $USER

# Add user to video group if needed
sudo usermod -a -G video $USER

# Use render node instead of card node
./blur_image -d /dev/dri/renderD128 input.jpg -o output.jpg
```

#### 2. "Permission denied" Errors

**Symptoms**: Cannot access device files or output directories.

**Solutions**:
```bash
# Use render node (no sudo required)
./blur_image -d /dev/dri/renderD128 input.jpg -o output.jpg

# Or run with sudo for card nodes
sudo ./blur_image input.jpg -o output.jpg

# Ensure output directory is writable
chmod 755 output_directory/
```

#### 3. Image Loading Failures

**Symptoms**: "load [filename] failed" error.

**Causes**:
- Unsupported image format
- Corrupted image file
- File path issues

**Solutions**:
```bash
# Verify file exists and is readable
file input_image.jpg
ls -la input_image.jpg

# Convert to supported format if needed
convert input.webp input.jpg

# Use absolute paths to avoid path issues
./blur_image /full/path/to/input.jpg -o /full/path/to/output.jpg
```

#### 4. OpenGL Context Errors

**Symptoms**: "no context created" or similar OpenGL errors.

**Causes**:
- Insufficient OpenGL ES 3.0 support
- Driver compatibility issues
- Resource exhaustion

**Solutions**:
```bash
# Check OpenGL ES support
glxinfo | grep "OpenGL ES"

# Update graphics drivers
sudo apt update && sudo apt upgrade

# Try different device
./blur_image -d /dev/dri/renderD129 input.jpg -o output.jpg
```

#### 5. Poor Performance

**Symptoms**: Very slow processing times.

**Causes**:
- Large image sizes
- Software rendering fallback
- Suboptimal parameters

**Solutions**:
```bash
# Reduce processing load
./blur_image -p 1 -r 15 input.jpg -o output.jpg

# Verify hardware acceleration
glxinfo | grep "direct rendering"

# Use optimal device
./blur_image -d /dev/dri/renderD128 input.jpg -o output.jpg
```

#### 6. Build Issues

**Common build problems and solutions**:

```bash
# Missing dependencies
sudo apt install libglm-dev libgbm-dev libegl1-mesa-dev libgles2-mesa-dev libgdk-pixbuf2.0-dev

# For demo build
sudo apt install libglfw3-dev libglew-dev

# Clean build
rm -rf build/
mkdir build && cd build
cmake .. && make
```

### Debug Mode

To enable detailed debugging information:

```bash
# Add debug output to see detailed processing steps
./blur_image -r 15 input.jpg -o output.jpg 2>&1 | tee debug.log
```

This will capture all error messages and processing information for analysis.

### Getting Help

#### Command Line Help
```bash
./blur_image -h
```

#### Verbose Information
Most operations provide verbose output to stderr, including:
- Device selection process
- Blur kernel parameters
- Brightness analysis results
- Processing completion status

#### Verifying Installation
```bash
# Test basic functionality
./blur_image -r 5 test_image.jpg -o test_output.jpg

# Verify output
file test_output.jpg
```

---

*This documentation covers all public APIs, functions, and components of the image blurring tool. For additional support or advanced usage scenarios, refer to the source code comments and error messages provided by the application.*