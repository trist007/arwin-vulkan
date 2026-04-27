#include "vk_pipelines.h"
#include "vk_initializers.h"

/*
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

/*
VkPipeline build_pipeline(PipelineBuilder *pipe, VkDevice device)
{
    // Force correct sType + pNext = nullptr on all state structs
    pipe->inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipe->inputAssembly.pNext = nullptr;

    pipe->rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pipe->rasterizer.pNext = nullptr;

    pipe->multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipe->multisampling.pNext = nullptr;

    pipe->depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    pipe->depthStencil.pNext = nullptr;

    pipe->rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pipe->rasterizer.pNext                   = nullptr;
    pipe->rasterizer.depthClampEnable        = VK_FALSE;
    pipe->rasterizer.rasterizerDiscardEnable = VK_FALSE;
    pipe->rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    pipe->rasterizer.cullMode                = VK_CULL_MODE_NONE;
    pipe->rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
    pipe->rasterizer.depthBiasEnable         = VK_FALSE;               // Important
    pipe->rasterizer.depthBiasConstantFactor = 0.0f;
    pipe->rasterizer.depthBiasClamp          = 0.0f;                   // ← Explicitly 0.0f
    pipe->rasterizer.depthBiasSlopeFactor    = 0.0f;
    pipe->rasterizer.lineWidth               = 1.0f;

    // make viewport state from our stored viewport and scissor.
    // at the moment we wont support multiple viewports or scissors
    // Viewport and Scissor (non-dynamic for text pipeline)
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)engine->windowExtent.width,
        .height = (float)engine->windowExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = {engine->windowExtent.width, engine->windowExtent.height}
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,           // ← Fixed
        .scissorCount = 1,
        .pScissors = &scissor              // ← Fixed
    };

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

    // === CRITICAL: Create a fresh, properly initialized renderInfo ===
    //! I added this
    VkPipelineRenderingCreateInfo renderInfo = {};
    renderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderInfo.pNext = nullptr;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachmentFormats = &pipe->colorAttachmentFormat;
    renderInfo.depthAttachmentFormat = pipe->depthFormat;
    // renderInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    // build the actual pipeline
    // we now use all of the info structs we have been writing into into this one
    // to create the pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    // connect the renderInfo to the pNext extension mechanism
    //pipelineInfo.pNext = &pipe->renderInfo;
    pipelineInfo.pNext = &renderInfo;

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
    VkPipeline newPipeline = VK_NULL_HANDLE;

    VkResult result = vkCreateGraphicsPipelines(device,
        VK_NULL_HANDLE,
        1,
        &pipelineInfo,
        nullptr,
        &newPipeline);

    if(result != VK_SUCCESS)
    {
        SDL_Log("failed to create pipeline");
        return VK_NULL_HANDLE; // failed to create graphics pipeline
    }

    return newPipeline;
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
    pipe->inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipe->inputAssembly.pNext = nullptr;
    pipe->inputAssembly.flags = 0;           // Important
    pipe->inputAssembly.topology = topology;
    pipe->inputAssembly.primitiveRestartEnable = VK_FALSE;
}

void set_polygon_mode(PipelineBuilder *pipe, VkPolygonMode mode)
{
    pipe->rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pipe->rasterizer.pNext = nullptr;
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
    pipe->multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipe->multisampling.pNext = nullptr;
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
*/

bool create_text_descriptor_layout(struct VulkanEngine* engine)
{
    VkDescriptorSetLayoutBinding bindings[1] = {};

    // Binding 0: Font texture (Combined Image + Sampler)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutCI = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = bindings
    };

    VK_CHECK(vkCreateDescriptorSetLayout(engine->device, &layoutCI, NULL, &engine->textDescriptorSetLayout));

        // === IMPORTANT: Create a proper pool for the text descriptor ===
    VkDescriptorPoolSize poolSize[1] = {};
    poolSize[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize[0].descriptorCount = 1;        // only need 1 for text

    VkDescriptorPoolCreateInfo poolCI = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = poolSize
    };

    VK_CHECK(vkCreateDescriptorPool(engine->device, &poolCI, NULL, &engine->textDescriptorPool));

    // Allocate the descriptor set
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = engine->textDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &engine->textDescriptorSetLayout
    };

    VK_CHECK(vkAllocateDescriptorSets(engine->device, &allocInfo, &engine->textDescriptorSet));

    SDL_Log("Text descriptor layout created successfully");
    return true;
}

bool update_text_descriptors(struct VulkanEngine* engine)
{

    if (engine->fontAtlas.imageView == VK_NULL_HANDLE) {
        SDL_Log("ERROR: Font atlas imageView is null - cannot update text descriptor");
        return false;
    }
    if (engine->fontAtlas.sampler == VK_NULL_HANDLE) {
        SDL_Log("ERROR: Font atlas sampler is null");
        return false;
    }
    if (engine->textDescriptorSet == VK_NULL_HANDLE) {
        SDL_Log("ERROR: textDescriptorSet is null");
        return false;
    }

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.sampler     = engine->fontAtlas.sampler;
    imageInfo.imageView   = engine->fontAtlas.imageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write = {};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = engine->textDescriptorSet;   // ← text set, not the main one
    write.dstBinding      = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo      = &imageInfo;

    vkUpdateDescriptorSets(engine->device, 1, &write, 0, NULL);

    SDL_Log("Text descriptor set updated with font atlas");

    return true;
}

bool create_text_pipeline(struct VulkanEngine* engine)
{
    SDL_Log("=== Starting create_text_pipeline ===");

    // ===================================================================
    // 1. Load and compile text shader
    // ===================================================================
    size_t sourceSize = 0;
    void* sourceData = SDL_LoadFile("../data/assets/text.slang", &sourceSize);
    if (!sourceData) {
        SDL_Log("ERROR: Failed to load ../data/assets/text.slang");
        return false;
    }

    /*
    if (!engine->slangGlobalSession) {
        SDL_Log("ERROR: slangGlobalSession not created yet!");
        SDL_free(sourceData);
        return false;
    }

    Slang::ComPtr<slang::ISession> textSession;
    slang::TargetDesc target = {
        .format = SLANG_SPIRV,
        .profile = engine->slangGlobalSession->findProfile("spirv_1_6")
    };

    slang::CompilerOptionEntry option = {
        .name = slang::CompilerOptionName::EmitSpirvDirectly,
        .value = { .kind = slang::CompilerOptionValueKind::Int, .intValue0 = 1 }
    };

    slang::SessionDesc sessionDesc{
        .targets = &target,
        .targetCount = 1,
        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
        .compilerOptionEntries = &option,
        .compilerOptionEntryCount = 1
    };

    if (engine->slangGlobalSession->createSession(sessionDesc, textSession.writeRef()) != SLANG_OK || !textSession) {
        SDL_Log("ERROR: Failed to create text Slang session");
        SDL_free(sourceData);
        return false;
    }

    Slang::ComPtr<slang::IModule> textModule;
    Slang::ComPtr<ISlangBlob> diagBlob;

    textModule = textSession->loadModuleFromSourceString(
        "text", "../data/assets/text.slang", (const char*)sourceData, diagBlob.writeRef());

    SDL_free(sourceData);

    if (!textModule) {
        const char* diag = diagBlob ? (const char*)diagBlob->getBufferPointer() : "No diagnostics";
        SDL_Log("ERROR: Failed to compile text.slang: %s", diag);
        return false;
    }

    // Get SPIR-V and create shader module
    Slang::ComPtr<ISlangBlob> spirv;
    textModule->getTargetCode(0, spirv.writeRef());
    */

    VkShaderModuleCreateInfo shaderCI = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sourceSize,
        .pCode = sourceData
    };

    VK_CHECK(vkCreateShaderModule(engine->device, &shaderCI, NULL, &engine->textShaderModule));

    SDL_free(sourceData);

    SDL_Log("Text shader compiled successfully");

    // ===================================================================
    // 2. Pipeline Creation
    // ===================================================================

    // Vertex Input for TextVertex { float2 position; float2 uv; }
    VkVertexInputBindingDescription bindingDesc = {
        .binding = 0,
        .stride = sizeof(TextVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkVertexInputAttributeDescription attrDescs[2] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(TextVertex, position) },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(TextVertex, uv) }
    };

    VkPipelineVertexInputStateCreateInfo vertexInput = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDesc,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = attrDescs
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    // Dynamic viewport + scissor (matches your draw code)
    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = ArraySize(dynamicStates),
        .pDynamicStates = dynamicStates
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineDepthStencilStateCreateInfo depthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE
    };

    // Alpha blending
    VkPipelineColorBlendAttachmentState blendAttachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment
    };

    // Push constant
    VkPushConstantRange pushConstant = {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(PushColor)
    };

    // Pipeline Layout
    VkPipelineLayoutCreateInfo layoutCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &engine->textDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstant
    };

    VK_CHECK(vkCreatePipelineLayout(engine->device, &layoutCI, NULL, &engine->textPipelineLayout));

    // Dynamic Rendering
    VkPipelineRenderingCreateInfo renderingCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &engine->swapchainImageFormat,
        .depthAttachmentFormat = engine->depthFormat
    };

    // Shader stages
    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT,   .module = engine->textShaderModule, .pName = "VSMain" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = engine->textShaderModule, .pName = "PSMain" }
    };

    // Final pipeline
    VkGraphicsPipelineCreateInfo pipelineCI = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingCI,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = NULL,           // dynamic viewport/scissor
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = engine->textPipelineLayout
    };

    VK_CHECK(vkCreateGraphicsPipelines(engine->device, VK_NULL_HANDLE, 1, &pipelineCI, NULL, &engine->textPipeline));

    SDL_Log("Text pipeline created successfully with alpha blending");
    return true;
}

// Simple one-time layout transition helper
void transition_font_atlas(struct VulkanEngine* engine)
{
    if (engine->fontAtlas.image == VK_NULL_HANDLE) {
        SDL_Log("ERROR: fontAtlasImage is null");
        return;
    }

    // Get a command buffer (use your existing immediate / one-time command buffer)
    VkCommandBuffer cmd = begin_single_time_commands(engine);   // ← You probably already have this helper

    VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext               = NULL,
        .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,   // after copy from staging buffer
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,      // or TRANSFER_DST_OPTIMAL if you used it
        .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = engine->fontAtlas.image,
        .subresourceRange    = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,          // source stage
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,   // destination stage
        0,
        0, NULL,
        0, NULL,
        1, &barrier);

    end_single_time_commands(engine, cmd);   // submit and wait (or integrate into your frame cmd buffer)

    SDL_Log("Font atlas transitioned to SHADER_READ_ONLY_OPTIMAL");
}

// Allocate and begin a one-time command buffer
static VkCommandBuffer begin_single_time_commands(struct VulkanEngine* engine)
{
    VkCommandBufferAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = engine->commandPool,          // Use your main command pool
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    VK_CHECK(vkAllocateCommandBuffers(engine->device, &allocInfo, &commandBuffer));

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT   // Important for one-time use
    };

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    return commandBuffer;
}

// End, submit, and wait for the one-time command buffer to finish
static void end_single_time_commands(struct VulkanEngine* engine, VkCommandBuffer commandBuffer)
{
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &commandBuffer
    };

    // Submit to graphics queue and wait for it to finish
    VK_CHECK(vkQueueSubmit(engine->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(engine->graphicsQueue));   // Wait for completion (simple but safe for init)

    // Clean up
    vkFreeCommandBuffers(engine->device, engine->commandPool, 1, &commandBuffer);
}