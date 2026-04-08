#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

//#include <fmt/core.h> //<format> already included in c++20
//#include <format>     // I'm doing to use SDL_Log()

#include "HandmadeMath.h"
//#include <glm/mat4x4.hpp>
//#include <glm/vec4.hpp>

#ifdef DEBUG
#define assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define assert(Expression)
#endif

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
             SDL_Log("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)