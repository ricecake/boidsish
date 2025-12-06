# Makefile for 3D Visualization Program
# Cross-platform build for Linux and macOS

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
INCLUDES = -Iinclude

# Detect the operating system
UNAME_S := $(shell uname -s)

# Source files
SRCDIR = src
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:.cpp=.o)

# Target executable
TARGET = boidsish

# Platform-specific settings
ifeq ($(UNAME_S), Linux)
    # Linux-specific flags
    LIBS = -lGL -lGLU -lglfw -lGLEW
    PKG_CONFIG = $(shell pkg-config --cflags --libs glfw3 glew)
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
all: $(TARGET)

# Build the main executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS) $(LIBS)

# Compile source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Build just the library object (for use by examples)
boidsish.o: src/boidsish.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c src/boidsish.cpp -o src/boidsish.o

# Build examples
examples: $(TARGET)
	$(MAKE) -C examples

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)
	$(MAKE) -C examples clean

# Install dependencies (helper targets)
install-deps-linux:
	@echo "Installing dependencies on Linux..."
	@echo "Run: sudo apt-get install libglfw3-dev libglew-dev libgl1-mesa-dev"

install-deps-macos:
	@echo "Installing dependencies on macOS..."
	@echo "Run: brew install glfw glew"

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build the main program"
	@echo "  clean        - Remove build artifacts"
	@echo "  examples     - Build example programs"
	@echo "  install-deps-linux  - Show Linux dependency installation command"
	@echo "  install-deps-macos  - Show macOS dependency installation command"

.PHONY: all clean examples install-deps-linux install-deps-macos help