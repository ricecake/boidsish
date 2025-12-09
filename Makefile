# Makefile for 3D Visualization Program
# Cross-platform build for Linux and macOS

CXX = g++
CXXFLAGS = -std=gnu++23 -Wall -Wextra -O3
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

# Target executable
TARGET = $(BUILDDIR)/boidsish

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
all: $(TARGET) examples

# Create build directories
$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

# Build the main executable
$(TARGET): $(BUILDDIR) $(OBJDIR) $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS) $(LIBS)

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Build just the library object (for use by examples)
$(OBJDIR)/boidsish.o: $(SRCDIR)/boidsish.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

boidsish.o: $(OBJDIR)/boidsish.o
	@cp $(OBJDIR)/boidsish.o src/boidsish.o

# Build examples
examples: $(TARGET)
	$(MAKE) -C examples

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
	@find ./ \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) ! -path './.*/*' -exec clang-format --Wno-error=unknown -i '{}' \;

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build the main program"
	@echo "  clean        - Remove build artifacts"
	@echo "  examples     - Build example programs"
	@echo "  install-deps-linux  - Show Linux dependency installation command"
	@echo "  install-deps-macos  - Show macOS dependency installation command"

.PHONY: all clean examples install-deps-linux install-deps-macos help format