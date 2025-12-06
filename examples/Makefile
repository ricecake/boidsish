# Makefile for Boidsish Examples

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
INCLUDES = -I../include

# Build directory (use parent's build directory)
BUILDDIR = ../build

# Detect the operating system
UNAME_S := $(shell uname -s)

# Platform-specific settings
ifeq ($(UNAME_S), Linux)
    LIBS = -lGL -lGLU -lglfw -lGLEW
    PKG_CONFIG = $(shell pkg-config --cflags --libs glfw3 glew)
    ifneq ($(PKG_CONFIG),)
        LIBS = $(PKG_CONFIG) -lGL -lGLU
    endif
endif

ifeq ($(UNAME_S), Darwin)
    LIBS = -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
    HOMEBREW_PREFIX = $(shell brew --prefix 2>/dev/null)
    ifneq ($(HOMEBREW_PREFIX),)
        INCLUDES += -I$(HOMEBREW_PREFIX)/include
        LDFLAGS += -L$(HOMEBREW_PREFIX)/lib
    endif
    LIBS += -lglfw -lGLEW
endif

# Library object
LIB_OBJ = $(BUILDDIR)/obj/boidsish.o

# Targets
EXAMPLES = $(BUILDDIR)/advanced_examples $(BUILDDIR)/entity_example $(BUILDDIR)/vector_demo

all: $(BUILDDIR) $(EXAMPLES)

# Create build directory
$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

# Build the library object if it doesn't exist
$(LIB_OBJ):
	$(MAKE) -C .. boidsish.o

# Build examples
$(BUILDDIR)/advanced_examples: advanced_examples.cpp $(LIB_OBJ) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) advanced_examples.cpp $(LIB_OBJ) -o $@ $(LDFLAGS) $(LIBS)

$(BUILDDIR)/entity_example: entity_example.cpp $(LIB_OBJ) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) entity_example.cpp $(LIB_OBJ) -o $@ $(LDFLAGS) $(LIBS)

$(BUILDDIR)/vector_demo: vector_demo.cpp $(LIB_OBJ) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) vector_demo.cpp $(LIB_OBJ) -o $@ $(LDFLAGS) $(LIBS)

# Clean
clean:
	rm -f $(BUILDDIR)/advanced_examples $(BUILDDIR)/entity_example $(BUILDDIR)/vector_demo

.PHONY: all clean