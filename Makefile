# Makefile for 3D Visualization Program
# Cross-platform build for Linux and macOS

CXX = g++
CXXFLAGS = -std=gnu++23 -Wall -Wextra -O3 -MMD -MP -Werror -Wpedantic -Wcast-align -Wcast-qual -Wconversion -Wshadow -Wduplicated-branches -Wduplicated-cond -Wlogical-op -Wnull-dereference -Wuseless-cast
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


TARGET = $(BUILDDIR)/libboidsish.a

# Platform-specific settings
ifeq ($(UNAME_S), Linux)
    # Linux-specific flags
    LIBS = -lGL -lGLU -lglfw -lGLEW
    PKG_CONFIG = $(shell pkg-config --cflags --libs glfw3 glew glm)
    ifneq ($(PKG_CONFIG),)
        LIBS = $(PKG_CONFIG) -lGL -lGLU
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
$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

library: $(TARGET)

$(TARGET): $(BUILDDIR) $(OBJDIR) $(OBJECTS)
	ar rcs $(TARGET) $(OBJECTS)

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Build examples
examples: $(OBJECTS) $(EXAMPLE_TARGETS)

$(BUILDDIR)/%: $(EXAMPLE_SRCDIR)/%.cpp $(OBJECTS) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(OBJECTS) -o $@ $(LDFLAGS) $(LIBS)


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

-include $(OBJECTS:.o=.d)
