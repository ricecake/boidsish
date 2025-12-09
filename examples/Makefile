# Makefile for Boidsish Examples

# Build directory (use parent's build directory)
BUILDDIR = ../build

CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -O3
INCLUDES = -isystem ../external/include -I../include
LIBS = -L$(BUILDDIR) -lboidsish


# Detect the operating system
UNAME_S := $(shell uname -s)

# Platform-specific settings
ifeq ($(UNAME_S), Linux)
    LIBS += -lGL -lGLU -lglfw -lGLEW
    PKG_CONFIG = $(shell pkg-config --cflags --libs glfw3 glew)
    ifneq ($(PKG_CONFIG),)
        LIBS += $(PKG_CONFIG) -lGL -lGLU
    endif
endif

ifeq ($(UNAME_S), Darwin)
    LIBS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
    HOMEBREW_PREFIX = $(shell brew --prefix 2>/dev/null)
    ifneq ($(HOMEBREW_PREFIX),)
        INCLUDES += -I$(HOMEBREW_PREFIX)/include
        LDFLAGS += -L$(HOMEBREW_PREFIX)/lib
    endif
    LIBS += -lglfw -lGLEW
endif


# Targets
EXAMPLES = $(BUILDDIR)/advanced_examples $(BUILDDIR)/entity_example $(BUILDDIR)/vector_demo $(BUILDDIR)/basic_demo

all: $(BUILDDIR) $(EXAMPLES)

# Create build directory
$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

# Build examples
$(BUILDDIR)/%: %.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS) $(LIBS)

.PHONY: all clean