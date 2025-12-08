# Makefile for Boidsish Examples

CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -O3
INCLUDES = -isystem ../external/include -I../include

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

# Library objects
LIB_OBJS = $(BUILDDIR)/obj/boidsish.o $(BUILDDIR)/obj/graph.o

# Targets
EXAMPLES = $(BUILDDIR)/advanced_examples $(BUILDDIR)/entity_example $(BUILDDIR)/vector_demo $(BUILDDIR)/collision_example $(BUILDDIR)/raycast_example $(BUILDDIR)/graph_avoidance_example

all: $(BUILDDIR) $(EXAMPLES)

# Create build directory
$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

# Build the library objects if they don't exist
$(LIB_OBJS):
	$(MAKE) -C .. boidsish.o graph.o

# Build examples
$(BUILDDIR)/advanced_examples: advanced_examples.cpp $(LIB_OBJS) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) advanced_examples.cpp $(LIB_OBJS) -o $@ $(LDFLAGS) $(LIBS)

$(BUILDDIR)/entity_example: entity_example.cpp $(LIB_OBJS) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) entity_example.cpp $(LIB_OBJS) -o $@ $(LDFLAGS) $(LIBS)

$(BUILDDIR)/vector_demo: vector_demo.cpp $(LIB_OBJS) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) vector_demo.cpp $(LIB_OBJS) -o $@ $(LDFLAGS) $(LIBS)

$(BUILDDIR)/collision_example: collision_example.cpp $(LIB_OBJS) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) collision_example.cpp $(LIB_OBJS) -o $@ $(LDFLAGS) $(LIBS)

$(BUILDDIR)/raycast_example: raycast_example.cpp $(LIB_OBJS) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) raycast_example.cpp $(LIB_OBJS) -o $@ $(LDFLAGS) $(LIBS)

$(BUILDDIR)/graph_avoidance_example: graph_avoidance_example.cpp $(LIB_OBJS) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) graph_avoidance_example.cpp $(LIB_OBJS) -o $@ $(LDFLAGS) $(LIBS)

# Clean
clean:
	rm -f $(BUILDDIR)/advanced_examples $(BUILDDIR)/entity_example $(BUILDDIR)/vector_demo $(BUILDDIR)/collision_example $(BUILDDIR)/raycast_example $(BUILDDIR)/graph_avoidance_example

.PHONY: all clean
