# Makefile for Boidsish Examples

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
INCLUDES = -I../include

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
LIB_OBJ = ../src/boidsish.o

# Targets
EXAMPLES = advanced_examples

all: $(EXAMPLES)

# Build the library object if it doesn't exist
$(LIB_OBJ):
	$(MAKE) -C .. boidsish.o

# Build examples
advanced_examples: advanced_examples.cpp $(LIB_OBJ)
	$(CXX) $(CXXFLAGS) $(INCLUDES) advanced_examples.cpp $(LIB_OBJ) -o advanced_examples $(LDFLAGS) $(LIBS)

# Clean
clean:
	rm -f $(EXAMPLES)

.PHONY: all clean