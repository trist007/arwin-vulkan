# Compiler Flags
#
# -MMD - Generate dependency files
# -w - suppress all warnings
# -Wno<warning-name> - suppress warning
# -Werror=>warning-name> - treat as error
# -g - debugging symbols
CXX := clang++
CXXFLAGS :=-std=c++20 -MMD -w -g
CC := clang
VULKAN_SDK_PATH := /home/trist007/dev/1.4.341.1/x86_64
INCLUDES := -I$(VULKAN_SDK_PATH)/include
LIBDIR := -L$(VULKAN_SDK_PATH)/lib \
	      -L/usr/local/lib64\
		  -L.
LDFLAGS := -lvulkan -lSDL3 -lm -lslang -lslang-compiler -lSDL3_ttf -lGL -lktx \
	-Wl,-rpath,$(VULKAN_SDK_PATH)/lib \
	-Wl,-rpath,/usr/local/lib64
#DEFINES :=-DSIMDJSON_DISABLE_PRETTY_PRINTING=1

# Directories
BINDIR := ../bin
BUILDDIR := ../build
OBJDIR := $(BUILDDIR)

# Final binary
TARGET := $(BINDIR)/arwin

# Source files
#SRCS := $(wildcard *.cpp)
SRCS := $(filter-out text_renderer.cpp, $(wildcard *.cpp))
OBJS := $(patsubst %.cpp,$(OBJDIR)/%.o, $(SRCS))

#GLAD_OBJ := $(BUILDDIR)/glad.o

all: $(TARGET)

$(TARGET): $(OBJS) | $(BINDIR) $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LIBDIR) $(LDFLAGS) -o $@

$(BUILDDIR):
	mkdir -p $@

$(BINDIR):
	mkdir -p $@

# compile .cpp to bin/%.o
$(OBJDIR)/%.o: %.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# compile .c to bin/%.o
#$(GLAD_OBJ): glad.c | $(BUILDDIR)
#	$(CC) $(INCLUDES) -c $< -o $@

# Include auto-generated dependency files (incremental builds)
-include $(OBJS:.o=.d)

clean:
	rm -rf $(BINDIR)/*
	rm -rf $(BUILDDIR)/*