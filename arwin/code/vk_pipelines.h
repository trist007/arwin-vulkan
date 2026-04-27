#pragma once

#include "vk_types.h"
#include "vk_engine.h"

/*
namespace vkutil
{
    bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule *outShaderModule);
}

struct PipelineBuilder
{
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssembly;
    VkPipelineRasterizationStateCreateInfo rasterizer;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo multisampling;
    VkPipelineLayout pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo depthStencil;
    VkPipelineRenderingCreateInfo renderInfo;
    VkFormat colorAttachmentFormat = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
};

VkPipeline build_pipeline(PipelineBuilder *pipe, VkDevice device);

void clear(PipelineBuilder *pipe);
void set_shaders(PipelineBuilder *pipe, VkShaderModule vertexShader, VkShaderModule fragmentShader);
void set_input_topology(PipelineBuilder *pipe, VkPrimitiveTopology topology);
void set_polygon_mode(PipelineBuilder *pipe, VkPolygonMode mode);
void set_cull_mode(PipelineBuilder *pipe, VkCullModeFlags cullMode, VkFrontFace frontFace);
void set_multisampling_none(PipelineBuilder *pipe);
void disable_blending(PipelineBuilder *pipe);
void set_color_attachment_format(PipelineBuilder *pipe, VkFormat format);
void set_depth_format(PipelineBuilder *pipe, VkFormat format);
void disable_depthtest(PipelineBuilder *pipe);
void enable_depthtest(PipelineBuilder *pipe, bool depthWriteEnable, VkCompareOp op);
void enable_blending_additive(PipelineBuilder *pipe);
void enable_blending_alphablend(PipelineBuilder *pipe);
*/
bool create_text_descriptor_layout(struct VulkanEngine *engine);
bool create_text_pipeline(struct VulkanEngine *engine);
bool update_text_descriptors(struct VulkanEngine* engine);
void transition_font_atlas(struct VulkanEngine* engine);
static VkCommandBuffer begin_single_time_commands(struct VulkanEngine* engine);
static void end_single_time_commands(struct VulkanEngine* engine, VkCommandBuffer commandBuffer);