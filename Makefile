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
all: $(BUILDDIR)/boidsish examples

$(BUILDDIR)/boidsish: $(TARGET) $(OBJDIR)/boidsish.o
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OBJDIR)/boidsish.o -o $@ -L$(BUILDDIR) -lboidsish $(LIBS)

$(OBJDIR)/boidsish.o: src/boidsish.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Create build directories
$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

$(TARGET): $(BUILDDIR) $(OBJDIR) $(OBJECTS)
	ar rcs $(TARGET) $(OBJECTS)

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

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