# Compiler Flags
#
# -MMD - Generate dependency files
# -w - suppress all warnings
# -Wno<warning-name> - suppress warning
# -Werror=>warning-name> - treat as error
# -g - debugging symbols
CC=clang++
CCFLAGS=-std=c++20 -MMD -w -g
INCLUDE=-I.
LDFLAGS=-lvulkan -lSDL3 -lfastgltf -lm
DEFINES=-DGLM_ENABLE_EXPERIMENTAL -DSIMDJSON_DISABLE_PRETTY_PRINTING=1

# Directories
BINDIR := ../bin
OBJDIR := $(BINDIR)

# Final binary
TARGET := $(BINDIR)/arwin

# Source files
SRCS := $(wildcard *.cpp)
OBJS := $(patsubst %.cpp,$(OBJDIR)/%.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS) | $(BINDIR)
	$(CC) $(CCFLAGS) $(OBJS) $(LDFLAGS) -o $@

$(BINDIR):
	mkdir -p $@

# compile .cpp to bin/%.o
$(OBJDIR)/%.o: %.cpp | $(BINDIR)
	$(CC) $(CCFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

# Include auto-generated dependency files (incremental builds)
-include $(OBJS:.o=.d)


clean:
	rm  -rf $(BINDIR)/*
#arwin: main.o vk_engine.o vk_initializers.o vk_images.o VkBootstrap.o vk_descriptors.o vk_pipelines.o vk_loader.o camera.o imgui.o imgui_demo.o imgui_draw.o imgui_tables.o imgui_widgets.o imgui_impl_sdl3.o imgui_impl_vulkan.o fastgltf.o simdjson.o base64.o

#main.o: main.cpp fastgltf.o simdjson.o base64.o

#fastgltf.o: fastgltf.cpp
#simdjson.o: simdjson.cpp
#base64.o: base64.cpp

#main.o: main.cpp vk_engine.h
#vk_engine.o: vk_engine.cpp vk_engine.h

#%.o: %.cpp
#	$(CC) $(CCFLAGS) $(INCLUDE) $(DEFINES) -c -g $<