# Boidsish - Simple 3D Visualization Framework

A lightweight, cross-platform 3D visualization framework designed for creating interactive particle and dot-based visualizations in C++. Perfect for simulating flocking behaviors, particle systems, mathematical visualizations, and other dynamic 3D scenes.

## Features

- **Simple Interface**: Define your visualization with a single function that returns dot positions, colors, and trails
- **Cross-Platform**: Works on Linux and macOS with simple make commands
- **Real-time Rendering**: Smooth OpenGL-based 3D rendering with depth testing and transparency
- **Trail System**: Automatic trail rendering with customizable length and fade effects
- **Camera Controls**: WASD movement and mouse look controls
- **Lightweight**: Minimal dependencies - just OpenGL, GLFW, and GLEW

## Dependencies

### Linux (Ubuntu/Debian)
```bash
sudo apt-get install libglfw3-dev libglew-dev libgl1-mesa-dev
```

### macOS
```bash
brew install glfw glew
```

## Quick Start

1. **Install dependencies** (see above)

2. **Clone and build**:
```bash
cd boidsish
make
```

3. **Run the example**:
```bash
./boidsish
```

## Building

- `make` - Build the main example program
- `make examples` - Build additional example programs
- `make clean` - Clean build artifacts
- `make help` - Show all available targets

## Usage

### Basic Example

```cpp
#include "boidsish.h"
#include <cmath>

using namespace Boidsish;

// Define a function that returns dots for each frame
std::vector<Dot> MyVisualization(float time) {
    std::vector<Dot> dots;

    // Create a rotating dot
    float x = cos(time) * 3.0f;
    float y = sin(time * 0.5f);
    float z = sin(time) * 3.0f;

    // Dot parameters: position, size, color (RGBA), trail_length
    dots.emplace_back(x, y, z, 8.0f, 1.0f, 0.5f, 0.0f, 1.0f, 15);

    return dots;
}

int main() {
    Visualizer viz(800, 600, "My Visualization");
    viz.SetDotFunction(MyVisualization);
    viz.Run();
    return 0;
}
```

### Dot Structure

```cpp
struct Dot {
    float x, y, z;           // Position in 3D space
    float size;              // Size of the dot (in pixels)
    float r, g, b, a;        // Color (red, green, blue, alpha)
    int trail_length;        // Number of trail segments
};
```

### Camera Controls

The visualization includes built-in first-person camera controls:

- **WASD** - Move horizontally (forward, left, back, right)
- **Space** - Move up
- **Shift** - Move down
- **Mouse** - Look around
- **ESC** - Exit

### Advanced Usage

```cpp
// Set custom camera position
Camera camera(x, y, z, pitch, yaw, fov);
viz.SetCamera(camera);

// Update loop for custom control
while (!viz.ShouldClose()) {
    viz.Update();  // Handle input and timing
    viz.Render();  // Render the frame
}
```

## Examples

The `examples/` directory contains several demonstrations:

### Example 1: Spiral Particles
Multiple colored particles following spiral paths with varying trail lengths.

### Example 2: Random Walk
Particles performing random walks with boundary constraints and distance-based coloring.

### Example 3: Wave Function
Grid-based wave simulation showing mathematical function visualization.

**Run examples:**
```bash
cd examples
make
./advanced_examples 1  # Spiral particles
./advanced_examples 2  # Random walk
./advanced_examples 3  # Wave function
```

## Architecture

### Core Components

- **Visualizer**: Main class handling window, rendering, and input
- **Dot**: Structure representing a single particle/point
- **Camera**: 3D camera with position and orientation
- **DotFunction**: User-defined function type for generating dots

### Rendering Pipeline

1. User function generates dots for current time
2. Trail system updates position history
3. OpenGL renders trails (lines) and dots (points)
4. Camera transformation applied
5. Grid overlay for spatial reference

### Platform Abstraction

The Makefile automatically detects the platform and configures:
- OpenGL linking (Linux vs macOS frameworks)
- Library paths (Homebrew on macOS)
- Compiler flags for compatibility

## Customization

### Grid and Environment

Modify `RenderGrid()` in `src/boidsish.cpp` to change the spatial reference grid.

### Rendering Style

- Point sizes, line widths, and colors can be customized per dot
- Alpha blending is enabled for transparency effects
- Depth testing provides proper 3D occlusion

### Performance

- Dots are rendered as OpenGL points for efficiency
- Trails use line strips for smooth curves
- No complex geometry or textures for maximum performance

## Troubleshooting

### Build Issues

1. **Missing headers**: Ensure development packages are installed
2. **Library not found**: Check library paths in Makefile
3. **OpenGL version**: Requires OpenGL 3.3+ compatible drivers

### Runtime Issues

1. **Black screen**: Check OpenGL drivers and version
2. **No response**: Verify GLFW initialization
3. **Crashes**: Enable debug builds with `-g -DDEBUG`

### Platform-Specific

**macOS**: If Homebrew libraries aren't found, manually set paths:
```bash
export CPPFLAGS=-I$(brew --prefix)/include
export LDFLAGS=-L$(brew --prefix)/lib
make
```

**Linux**: For different distributions, package names may vary:
- Fedora: `mesa-libGL-devel glfw-devel glew-devel`
- Arch: `mesa glfw-x11 glew`

## License

This project is provided as example code for educational and development purposes.

## Contributing

Feel free to submit issues and enhancement requests. This framework is designed to be simple and focused - complex features should be implemented in user code rather than the core library.