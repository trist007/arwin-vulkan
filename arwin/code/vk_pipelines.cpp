#include <vk_pipelines.h>
#include <fstream>
#include <vk_initializers.h>

bool vkutil::load_shader_module(const char* filePath,
    VkDevice device,
    VkShaderModule* outShaderModule)
{
    // open the file. With cursor at the end
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return false;
    }

    // find what the size of the file is by looking up the location of the cursor
    // because the cursor is at the end, it gives the size directly in bytes
    size_t fileSize = (size_t)file.tellg();

    // spirv expects the buffer to be on uint32, so make sure to reserve a int
    // vector big enough for the entire file
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    // put file cursor at beginning
    file.seekg(0);

    // load the entire file into the buffer
    file.read((char*)buffer.data(), fileSize);

    // now that the file is loaded into the buffer, we can close it
    file.close();

    // create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;

    // codeSize has to be in bytes, so multply the ints in the buffer by size of
    // int to know the real size of the buffer
    createInfo.codeSize = buffer.size() * sizeof(uint32_t);
    createInfo.pCode = buffer.data();

    // check that the creation goes well.
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return false;
    }
    *outShaderModule = shaderModule;
    return true;
}

void clear(PipelineBuilder *pipe)
{
    // clear all of the structs we need back to 0 with their correct stype

    pipe->inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };

    pipe->rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };

    pipe->colorBlendAttachment = {};

    pipe->multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };

    pipe->pipelineLayout = {};

    pipe->depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

    pipe->renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

    pipe->shaderStages.clear();
}

VkPipeline build_pipeline(PipelineBuilder *pipe, VkDevice device)
{
    // make viewport state from our stored viewport and scissor.
    // at the moment we wont support multiple viewports or scissors
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;

    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // setup dummy color blending. We arent using transparent objects yet
    // the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;

    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &pipe->colorBlendAttachment;

    // completely clear VertexInputStateCreateInfo, as we have no need for it
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    // build the actual pipeline
    // we now use all of the info structs we have been writing into into this one
    // to create the pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    // connect the renderInfo to the pNext extension mechanism
    pipelineInfo.pNext = &pipe->renderInfo;

    pipelineInfo.stageCount = (uint32_t)pipe->shaderStages.size();
    pipelineInfo.pStages = pipe->shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &pipe->inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &pipe->rasterizer;
    pipelineInfo.pMultisampleState = &pipe->multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &pipe->depthStencil;
    pipelineInfo.layout = pipe->pipelineLayout;

    // ! NOTE: trist007: this allows the pipeline to change viewport and scissors instead of requiring a whole new
    // ! pipeline because we are using VkDynamicState
    VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamicInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicInfo.pDynamicStates = &state[0];
    dynamicInfo.dynamicStateCount = 2;

    pipelineInfo.pDynamicState = &dynamicInfo;

    // its easy to error out on create graphics pipeline, so we handle it a bit
    // better than the common VK_CHECK case
    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
            nullptr, &newPipeline)
        != VK_SUCCESS) {
        SDL_Log("failed to create pipeline");
        return VK_NULL_HANDLE; // failed to create graphics pipeline
    } else {
        return newPipeline;
    }
}

void set_shaders(PipelineBuilder *pipe, VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
    pipe->shaderStages.clear();

    pipe->shaderStages.push_back(
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));

    pipe->shaderStages.push_back(
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
}

void set_input_topology(PipelineBuilder *pipe, VkPrimitiveTopology topology)
{
    pipe->inputAssembly.topology = topology;
    // we are not going to use primitive restart on the entire tutorial so leave
    // it on false
    pipe->inputAssembly.primitiveRestartEnable = VK_FALSE;
}

void set_polygon_mode(PipelineBuilder *pipe, VkPolygonMode mode)
{
    pipe->rasterizer.polygonMode = mode;
    pipe->rasterizer.lineWidth = 1.f;
}

void set_cull_mode(PipelineBuilder *pipe, VkCullModeFlags cullMode, VkFrontFace frontFace)
{
    pipe->rasterizer.cullMode = cullMode;
    pipe->rasterizer.frontFace = frontFace;
}

void set_multisampling_none(PipelineBuilder *pipe)
{
    pipe->multisampling.sampleShadingEnable = VK_FALSE;
    // multisampling defaulted to no multisampling (1 sample per pixel)
    pipe->multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipe->multisampling.minSampleShading = 1.0f;
    pipe->multisampling.pSampleMask = nullptr;
    // no alpha to coverage either
    pipe->multisampling.alphaToCoverageEnable = VK_FALSE;
    pipe->multisampling.alphaToOneEnable = VK_FALSE; 
}

void disable_blending(PipelineBuilder *pipe)
{
    // default write mask
    pipe->colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    // no blending
    pipe->colorBlendAttachment.blendEnable = VK_FALSE;
}

void set_color_attachment_format(PipelineBuilder *pipe, VkFormat format)
{
    pipe->colorAttachmentFormat = format;

    // connect the format to the renderInfo  structure
    pipe->renderInfo.colorAttachmentCount = 1;
    pipe->renderInfo.pColorAttachmentFormats = &pipe->colorAttachmentFormat;
}

void set_depth_format(PipelineBuilder *pipe, VkFormat format)
{
    pipe->renderInfo.depthAttachmentFormat = format;
}

void disable_depthtest(PipelineBuilder *pipe)
{
    pipe->depthStencil.depthTestEnable = VK_FALSE;
    pipe->depthStencil.depthWriteEnable = VK_FALSE;
    pipe->depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    pipe->depthStencil.depthBoundsTestEnable = VK_FALSE;
    pipe->depthStencil.stencilTestEnable = VK_FALSE;
    pipe->depthStencil.front = {};
    pipe->depthStencil.back = {};
    pipe->depthStencil.minDepthBounds = 0.f;
    pipe->depthStencil.maxDepthBounds = 1.f;   
}

void enable_depthtest(PipelineBuilder *pipe, bool depthWriteEnable, VkCompareOp op)
{
    pipe->depthStencil.depthTestEnable = VK_TRUE;
    pipe->depthStencil.depthWriteEnable = depthWriteEnable;
    pipe->depthStencil.depthCompareOp = op;
    pipe->depthStencil.depthBoundsTestEnable = VK_FALSE;
    pipe->depthStencil.stencilTestEnable = VK_FALSE;
    pipe->depthStencil.front = {};
    pipe->depthStencil.back = {};
    pipe->depthStencil.minDepthBounds = 0.f;
    pipe->depthStencil.maxDepthBounds = 1.f;
}

void enable_blending_additive(PipelineBuilder *pipe)
{
    pipe->colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    pipe->colorBlendAttachment.blendEnable = VK_TRUE;
    pipe->colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    pipe->colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    pipe->colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    pipe->colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    pipe->colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    pipe->colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void enable_blending_alphablend(PipelineBuilder *pipe)
{
    pipe->colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    pipe->colorBlendAttachment.blendEnable = VK_TRUE;
    pipe->colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    pipe->colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    pipe->colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    pipe->colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    pipe->colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    pipe->colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}