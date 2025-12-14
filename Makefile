# Makefile for 3D Visualization Program
# Cross-platform build for Linux and macOS

CXX = g++
CXXFLAGS = -std=gnu++23 -Wall -Wextra -O3 -MMD -MP
INCLUDES = -isystem external/include -Iinclude

# Build directory
BUILDDIR = build
OBJDIR = $(BUILDDIR)/obj

# Detect the operating system
UNAME_S := $(shell uname -s)

# Source files
SRCDIR = src
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# Example files
EXAMPLE_SRCDIR = examples
EXAMPLE_SOURCES = $(wildcard $(EXAMPLE_SRCDIR)/*.cpp)
EXAMPLE_TARGETS = $(patsubst $(EXAMPLE_SRCDIR)/%.cpp,$(BUILDDIR)/%,$(EXAMPLE_SOURCES))

# Shader files
SHADER_SRCDIR = shaders
SHADER_FILES := $(shell find $(SHADER_SRCDIR) -type f)

TARGET = $(BUILDDIR)/libboidsish.a

# Platform-specific settings
ifeq ($(UNAME_S), Linux)
	LIBS = -lGL -lGLU -lglfw -lGLEW
	PKG_CONFIG_CHECK = $(shell pkg-config --cflags --libs glfw3 glew glm 2>/dev/null)
    ifeq ($(strip $(PKG_CONFIG_CHECK)),)
        # Fallback if pkg-config fails or libraries are not in standard paths
        $(warning "pkg-config failed to find glfw3, glew, or glm. Using default flags.")
    else
        # Use pkg-config for library flags
        LIBS = $(PKG_CONFIG_CHECK) -lGL -lGLU
    endif
endif

ifeq ($(UNAME_S), Darwin)
    # macOS-specific flags
    LIBS = -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
    # Try to find libraries via Homebrew
    HOMEBREW_PREFIX = $(shell brew --prefix 2>/dev/null)
    ifneq ($(HOMEBREW_PREFIX),)
        INCLUDES += -I$(HOMEBREW_PREFIX)/include
        LDFLAGS += -L$(HOMEBREW_PREFIX)/lib
    endif
    LIBS += -lglfw -lGLEW
endif

# Default target
all: library examples

# Create build directories
$(OBJDIR) $(BUILDDIR):
	@mkdir -p $@

library: $(TARGET)

$(TARGET): $(BUILDDIR) $(OBJDIR) $(OBJECTS)
	ar rcs $(TARGET) $(OBJECTS)

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Build examples
examples: $(EXAMPLE_TARGETS)

# Link examples. The target depends on all shader files.
$(BUILDDIR)/%: $(EXAMPLE_SRCDIR)/%.cpp $(TARGET) $(SHADER_FILES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(TARGET) -o $@ $(LDFLAGS) $(LIBS)

# Clean build artifacts
clean:
	rm -rf $(BUILDDIR)

# Install dependencies (helper targets)
install-deps-linux:
	@echo "Installing dependencies on Linux..."
	@echo "Run: sudo apt-get install libglfw3-dev libglew-dev libgl1-mesa-dev"

install-deps-macos:
	@echo "Installing dependencies on macOS..."
	@echo "Run: brew install glfw glew"

format:
	@find  include src examples \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) ! -path './.*/*' -exec clang-format --Wno-error=unknown -i '{}' \;

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build the library and examples"
	@echo "  library      - Build the libboidsish.a library"
	@echo "  clean        - Remove build artifacts"
	@echo "  examples     - Build example programs"
	@echo "  install-deps-linux  - Show Linux dependency installation command"
	@echo "  install-deps-macos  - Show macOS dependency installation command"

.PHONY: all clean examples install-deps-linux install-deps-macos help format library

# Include all dependency files
-include $(OBJECTS:.o=.d)
