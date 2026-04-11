#pragma once

#include <vk_types.h>

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
    VkFormat colorAttachmentFormat;
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

