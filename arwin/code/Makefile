# Compiler Flags
#
# -MMD - Generate dependency files
# -w - suppress all warnings
# -Wno<warning-name> - suppress warning
# -Werror=>warning-name> - treat as error
# -g - debugging symbols
CC := clang++
CCFLAGS :=-std=c++20 -MMD -w -g
VULKAN_SDK_PATH := /home/trist007/dev/1.4.341.1/x86_64
INCLUDES := -I$(VULKAN_SDK_PATH)/include
LIBDIR := -L$(VULKAN_SDK_PATH)/lib \
	-L.
LDFLAGS := -lvulkan -lSDL3 -lm -lslang -lslang-compiler -lktx \
	-Wl,-rpath,$(VULKAN_SDK_PATH)/lib
DEFINES :=-DSIMDJSON_DISABLE_PRETTY_PRINTING=1

# Directories
BINDIR := ../bin
BUILDDIR := ../build
OBJDIR := $(BUILDDIR)

# Final binary
TARGET := $(BINDIR)/arwin

# Source files
SRCS := $(wildcard *.cpp)
OBJS := $(patsubst %.cpp,$(OBJDIR)/%.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS) | $(BINDIR) $(BUILDDIR)
	$(CC) $(CCFLAGS) $(OBJS) $(LIBDIR) $(LDFLAGS) -o $@

$(BUILDDIR):
	mkdir -p $@

$(BINDIR):
	mkdir -p $@

# compile .cpp to bin/%.o
$(OBJDIR)/%.o: %.cpp | $(BUILDDIR)
	$(CC) $(CCFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

# Include auto-generated dependency files (incremental builds)
-include $(OBJS:.o=.d)

clean:
	rm -rf $(BINDIR)/*
	rm -rf $(BUILDDIR)/*