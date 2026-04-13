CXX=g++
CXXFLAGS=-std=c++20
INCLUDE=-I.
LDFLAGS=-lvulkan -lSDL3 -lfastgltf
DEFINES=-DGLM_ENABLE_EXPERIMENTAL
all: arwin

arwin: main.o vk_engine.o vk_initializers.o vk_images.o VkBootstrap.o vk_descriptors.o vk_pipelines.o vk_loader.o camera.o imgui.o imgui_demo.o imgui_draw.o imgui_tables.o imgui_widgets.o imgui_impl_sdl3.o imgui_impl_vulkan.o fastgltf.o simdjson.o base64.o
	g++ $(CXXFLAGS) $^ $(LDFLAGS) -g -o arwin

#main.o: main.cpp fastgltf.o simdjson.o base64.o

#fastgltf.o: fastgltf.cpp
#simdjson.o: simdjson.cpp
#base64.o: base64.cpp

#main.o: main.cpp vk_engine.h
#vk_engine.o: vk_engine.cpp vk_engine.h

%.o: %.cpp
	g++ $(CXXFLAGS) $(INCLUDE) $(DEFINES) -c -g $<

clean:
	rm arwin *.o
