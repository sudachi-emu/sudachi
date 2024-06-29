// SPDX-FileCopyrightText: Copyright 2020 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>

#include "video_core/renderer_vulkan/vk_texture_cache.h"

#include "common/settings.h"
#include "video_core/host_shaders/blit_color_float_frag_spv.h"
#include "video_core/host_shaders/convert_abgr8_srgb_to_d24s8_frag_spv.h"
#include "video_core/host_shaders/convert_abgr8_to_d24s8_frag_spv.h"
#include "video_core/host_shaders/convert_abgr8_to_d32f_frag_spv.h"
#include "video_core/host_shaders/convert_d24s8_to_abgr8_frag_spv.h"
#include "video_core/host_shaders/convert_d32f_to_abgr8_frag_spv.h"
#include "video_core/host_shaders/convert_depth_to_float_frag_spv.h"
#include "video_core/host_shaders/convert_float_to_depth_frag_spv.h"
#include "video_core/host_shaders/convert_s8d24_to_abgr8_frag_spv.h"
#include "video_core/host_shaders/full_screen_triangle_vert_spv.h"
#include "video_core/host_shaders/vulkan_blit_depth_stencil_frag_spv.h"
#include "video_core/host_shaders/vulkan_color_clear_frag_spv.h"
#include "video_core/host_shaders/vulkan_color_clear_vert_spv.h"
#include "video_core/host_shaders/vulkan_depthstencil_clear_frag_spv.h"
#include "video_core/renderer_vulkan/blit_image.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/renderer_vulkan/vk_state_tracker.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/surface.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

using VideoCommon::ImageViewType;

namespace {
struct PushConstants {
    std::array<float, 2> tex_scale;
    std::array<float, 2> tex_offset;
};

template <u32 binding>
inline constexpr VkDescriptorSetLayoutBinding TEXTURE_DESCRIPTOR_SET_LAYOUT_BINDING{
    .binding = binding,
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    .pImmutableSamplers = nullptr,
};
constexpr std::array TWO_TEXTURES_DESCRIPTOR_SET_LAYOUT_BINDINGS{
    TEXTURE_DESCRIPTOR_SET_LAYOUT_BINDING<0>,
    TEXTURE_DESCRIPTOR_SET_LAYOUT_BINDING<1>,
};
constexpr VkDescriptorSetLayoutCreateInfo ONE_TEXTURE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .bindingCount = 1,
    .pBindings = &TEXTURE_DESCRIPTOR_SET_LAYOUT_BINDING<0>,
};
template <u32 num_textures>
inline constexpr DescriptorBankInfo TEXTURE_DESCRIPTOR_BANK_INFO{
    .uniform_buffers = 0,
    .storage_buffers = 0,
    .texture_buffers = 0,
    .image_buffers = 0,
    .textures = num_textures,
    .images = 0,
    .score = 2,
};
constexpr VkDescriptorSetLayoutCreateInfo TWO_TEXTURES_DESCRIPTOR_SET_LAYOUT_CREATE_INFO{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .bindingCount = static_cast<u32>(TWO_TEXTURES_DESCRIPTOR_SET_LAYOUT_BINDINGS.size()),
    .pBindings = TWO_TEXTURES_DESCRIPTOR_SET_LAYOUT_BINDINGS.data(),
};
template <VkShaderStageFlags stageFlags, size_t size>
inline constexpr VkPushConstantRange PUSH_CONSTANT_RANGE{
    .stageFlags = stageFlags,
    .offset = 0,
    .size = static_cast<u32>(size),
};
constexpr VkPipelineVertexInputStateCreateInfo PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .vertexBindingDescriptionCount = 0,
    .pVertexBindingDescriptions = nullptr,
    .vertexAttributeDescriptionCount = 0,
    .pVertexAttributeDescriptions = nullptr,
};
constexpr VkPipelineInputAssemblyStateCreateInfo PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE,
};
constexpr VkPipelineViewportStateCreateInfo PIPELINE_VIEWPORT_STATE_CREATE_INFO{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .viewportCount = 1,
    .pViewports = nullptr,
    .scissorCount = 1,
    .pScissors = nullptr,
};
constexpr VkPipelineRasterizationStateCreateInfo PIPELINE_RASTERIZATION_STATE_CREATE_INFO{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,
    .depthBiasEnable = VK_FALSE,
    .depthBiasConstantFactor = 0.0f,
    .depthBiasClamp = 0.0f,
    .depthBiasSlopeFactor = 0.0f,
    .lineWidth = 1.0f,
};
constexpr VkPipelineMultisampleStateCreateInfo PIPELINE_MULTISAMPLE_STATE_CREATE_INFO{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .sampleShadingEnable = VK_FALSE,
    .minSampleShading = 0.0f,
    .pSampleMask = nullptr,
    .alphaToCoverageEnable = VK_FALSE,
    .alphaToOneEnable = VK_FALSE,
};
constexpr std::array DYNAMIC_STATES{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
                                    VK_DYNAMIC_STATE_BLEND_CONSTANTS};
constexpr VkPipelineDynamicStateCreateInfo PIPELINE_DYNAMIC_STATE_CREATE_INFO{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .dynamicStateCount = static_cast<u32>(DYNAMIC_STATES.size()),
    .pDynamicStates = DYNAMIC_STATES.data(),
};
constexpr VkPipelineColorBlendStateCreateInfo PIPELINE_COLOR_BLEND_STATE_EMPTY_CREATE_INFO{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .logicOpEnable = VK_FALSE,
    .logicOp = VK_LOGIC_OP_CLEAR,
    .attachmentCount = 0,
    .pAttachments = nullptr,
    .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
};
constexpr VkPipelineColorBlendAttachmentState PIPELINE_COLOR_BLEND_ATTACHMENT_STATE{
    .blendEnable = VK_FALSE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
};
constexpr VkPipelineColorBlendStateCreateInfo PIPELINE_COLOR_BLEND_STATE_GENERIC_CREATE_INFO{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .logicOpEnable = VK_FALSE,
    .logicOp = VK_LOGIC_OP_CLEAR,
    .attachmentCount = 1,
    .pAttachments = &PIPELINE_COLOR_BLEND_ATTACHMENT_STATE,
    .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
};
constexpr VkPipelineDepthStencilStateCreateInfo PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .depthTestEnable = VK_TRUE,
    .depthWriteEnable = VK_TRUE,
    .depthCompareOp = VK_COMPARE_OP_ALWAYS,
    .depthBoundsTestEnable = VK_FALSE,
    .stencilTestEnable = VK_FALSE,
    .front = VkStencilOpState{},
    .back = VkStencilOpState{},
    .minDepthBounds = 0.0f,
    .maxDepthBounds = 0.0f,
};

template <VkFilter filter>
inline constexpr VkSamplerCreateInfo SAMPLER_CREATE_INFO{
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .magFilter = filter,
    .minFilter = filter,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    .mipLodBias = 0.0f,
    .anisotropyEnable = VK_FALSE,
    .maxAnisotropy = 0.0f,
    .compareEnable = VK_FALSE,
    .compareOp = VK_COMPARE_OP_NEVER,
    .minLod = 0.0f,
    .maxLod = 0.0f,
    .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    .unnormalizedCoordinates = VK_TRUE,
};

constexpr VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo(
    const VkDescriptorSetLayout* set_layout, vk::Span<VkPushConstantRange> push_constants) {
    return VkPipelineLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = (set_layout != nullptr ? 1u : 0u),
        .pSetLayouts = set_layout,
        .pushConstantRangeCount = push_constants.size(),
        .pPushConstantRanges = push_constants.data(),
    };
}

constexpr VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
                                                                        VkShaderModule shader) {
    return VkPipelineShaderStageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = stage,
        .module = shader,
        .pName = "main",
        .pSpecializationInfo = nullptr,
    };
}

constexpr std::array<VkPipelineShaderStageCreateInfo, 2> MakeStages(
    VkShaderModule vertex_shader, VkShaderModule fragment_shader) {
    return std::array{
        PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertex_shader),
        PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader),
    };
}

void UpdateOneTextureDescriptorSet(const Device& device, VkDescriptorSet descriptor_set,
                                   VkSampler sampler, VkImageView image_view) {
    const VkDescriptorImageInfo image_info{
        .sampler = sampler,
        .imageView = image_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    const VkWriteDescriptorSet write_descriptor_set{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &image_info,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };
    device.GetLogical().UpdateDescriptorSets(write_descriptor_set, nullptr);
}

void UpdateTwoTexturesDescriptorSet(const Device& device, VkDescriptorSet descriptor_set,
                                    VkSampler sampler, VkImageView image_view_0,
                                    VkImageView image_view_1) {
    const VkDescriptorImageInfo image_info_0{
        .sampler = sampler,
        .imageView = image_view_0,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    const VkDescriptorImageInfo image_info_1{
        .sampler = sampler,
        .imageView = image_view_1,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    const std::array write_descriptor_sets{
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptor_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &image_info_0,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptor_set,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &image_info_1,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        },
    };
    device.GetLogical().UpdateDescriptorSets(write_descriptor_sets, nullptr);
}

void BindBlitState(vk::CommandBuffer cmdbuf, const Region2D& dst_region) {
    const VkOffset2D offset{
        .x = std::min(dst_region.start.x, dst_region.end.x),
        .y = std::min(dst_region.start.y, dst_region.end.y),
    };
    const VkExtent2D extent{
        .width = static_cast<u32>(std::abs(dst_region.end.x - dst_region.start.x)),
        .height = static_cast<u32>(std::abs(dst_region.end.y - dst_region.start.y)),
    };
    const VkViewport viewport{
        .x = static_cast<float>(offset.x),
        .y = static_cast<float>(offset.y),
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    // TODO: Support scissored blits
    const VkRect2D scissor{
        .offset = offset,
        .extent = extent,
    };
    cmdbuf.SetViewport(0, viewport);
    cmdbuf.SetScissor(0, scissor);
}

void BindBlitState(vk::CommandBuffer cmdbuf, VkPipelineLayout layout, const Region2D& dst_region,
                   const Region2D& src_region, const Extent3D& src_size = {1, 1, 1}) {
    BindBlitState(cmdbuf, dst_region);
    const float scale_x = static_cast<float>(src_region.end.x - src_region.start.x) /
                          static_cast<float>(src_size.width);
    const float scale_y = static_cast<float>(src_region.end.y - src_region.start.y) /
                          static_cast<float>(src_size.height);
    const PushConstants push_constants{
        .tex_scale = {scale_x, scale_y},
        .tex_offset = {static_cast<float>(src_region.start.x) / static_cast<float>(src_size.width),
                       static_cast<float>(src_region.start.y) /
                           static_cast<float>(src_size.height)},
    };
    cmdbuf.PushConstants(layout, VK_SHADER_STAGE_VERTEX_BIT, push_constants);
}

VkExtent2D GetConversionExtent(const ImageView& src_image_view) {
    const auto& resolution = Settings::values.resolution_info;
    const bool is_rescaled = src_image_view.IsRescaled();
    u32 width = src_image_view.size.width;
    u32 height = src_image_view.size.height;
    return VkExtent2D{
        .width = is_rescaled ? resolution.ScaleUp(width) : width,
        .height = is_rescaled ? resolution.ScaleUp(height) : height,
    };
}

void TransitionImageLayout(vk::CommandBuffer& cmdbuf, VkImage image, VkImageLayout target_layout,
                           VkImageLayout source_layout = VK_IMAGE_LAYOUT_GENERAL) {
    constexpr VkFlags flags{VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT};
    const VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = flags,
        .dstAccessMask = flags,
        .oldLayout = source_layout,
        .newLayout = target_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                           0, barrier);
}

void BeginRenderPass(vk::CommandBuffer& cmdbuf, const Framebuffer* framebuffer) {
    const VkRenderPass render_pass = framebuffer->RenderPass();
    const VkFramebuffer framebuffer_handle = framebuffer->Handle();
    const VkExtent2D render_area = framebuffer->RenderArea();
    const VkRenderPassBeginInfo renderpass_bi{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = render_pass,
        .framebuffer = framebuffer_handle,
        .renderArea{
            .offset{},
            .extent = render_area,
        },
        .clearValueCount = 0,
        .pClearValues = nullptr,
    };
    cmdbuf.BeginRenderPass(renderpass_bi, VK_SUBPASS_CONTENTS_INLINE);
}
} // Anonymous namespace

BlitImageHelper::BlitImageHelper(const Device& device_, Scheduler& scheduler_,
                                 StateTracker& state_tracker_, DescriptorPool& descriptor_pool)
    : device{device_}, scheduler{scheduler_}, state_tracker{state_tracker_},
      one_texture_set_layout(device.GetLogical().CreateDescriptorSetLayout(
          ONE_TEXTURE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO)),
      two_textures_set_layout(device.GetLogical().CreateDescriptorSetLayout(
          TWO_TEXTURES_DESCRIPTOR_SET_LAYOUT_CREATE_INFO)),
      one_texture_descriptor_allocator{
          descriptor_pool.Allocator(*one_texture_set_layout, TEXTURE_DESCRIPTOR_BANK_INFO<1>)},
      two_textures_descriptor_allocator{
          descriptor_pool.Allocator(*two_textures_set_layout, TEXTURE_DESCRIPTOR_BANK_INFO<2>)},
      one_texture_pipeline_layout(device.GetLogical().CreatePipelineLayout(PipelineLayoutCreateInfo(
          one_texture_set_layout.address(),
          PUSH_CONSTANT_RANGE<VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstants)>))),
      two_textures_pipeline_layout(
          device.GetLogical().CreatePipelineLayout(PipelineLayoutCreateInfo(
              two_textures_set_layout.address(),
              PUSH_CONSTANT_RANGE<VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstants)>))),
      clear_color_pipeline_layout(device.GetLogical().CreatePipelineLayout(PipelineLayoutCreateInfo(
          nullptr, PUSH_CONSTANT_RANGE<VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(float) * 4>))),
      full_screen_vert(BuildShader(device, FULL_SCREEN_TRIANGLE_VERT_SPV)),
      blit_color_to_color_frag(BuildShader(device, BLIT_COLOR_FLOAT_FRAG_SPV)),
      blit_depth_stencil_frag(BuildShader(device, VULKAN_BLIT_DEPTH_STENCIL_FRAG_SPV)),
      clear_color_vert(BuildShader(device, VULKAN_COLOR_CLEAR_VERT_SPV)),
      clear_color_frag(BuildShader(device, VULKAN_COLOR_CLEAR_FRAG_SPV)),
      clear_stencil_frag(BuildShader(device, VULKAN_DEPTHSTENCIL_CLEAR_FRAG_SPV)),
      convert_depth_to_float_frag(BuildShader(device, CONVERT_DEPTH_TO_FLOAT_FRAG_SPV)),
      convert_float_to_depth_frag(BuildShader(device, CONVERT_FLOAT_TO_DEPTH_FRAG_SPV)),
      convert_abgr8_srgb_to_d24s8_frag(BuildShader(device, CONVERT_ABGR8_SRGB_TO_D24S8_FRAG_SPV)),
      convert_abgr8_to_d24s8_frag(BuildShader(device, CONVERT_ABGR8_TO_D24S8_FRAG_SPV)),
      convert_abgr8_to_d32f_frag(BuildShader(device, CONVERT_ABGR8_TO_D32F_FRAG_SPV)),
      convert_d32f_to_abgr8_frag(BuildShader(device, CONVERT_D32F_TO_ABGR8_FRAG_SPV)),
      convert_d24s8_to_abgr8_frag(BuildShader(device, CONVERT_D24S8_TO_ABGR8_FRAG_SPV)),
      convert_s8d24_to_abgr8_frag(BuildShader(device, CONVERT_S8D24_TO_ABGR8_FRAG_SPV)),
      linear_sampler(device.GetLogical().CreateSampler(SAMPLER_CREATE_INFO<VK_FILTER_LINEAR>)),
      nearest_sampler(device.GetLogical().CreateSampler(SAMPLER_CREATE_INFO<VK_FILTER_NEAREST>)) {}

BlitImageHelper::~BlitImageHelper() = default;

void BlitImageHelper::BlitColor(const Framebuffer* dst_framebuffer, VkImageView src_view,
                                const Region2D& dst_region, const Region2D& src_region,
                                Tegra::Engines::Fermi2D::Filter filter,
                                Tegra::Engines::Fermi2D::Operation operation) {
    const bool is_linear = filter == Tegra::Engines::Fermi2D::Filter::Bilinear;
    const BlitImagePipelineKey key{
        .renderpass = dst_framebuffer->RenderPass(),
        .operation = operation,
    };
    const VkPipelineLayout layout = *one_texture_pipeline_layout;
    const VkSampler sampler = is_linear ? *linear_sampler : *nearest_sampler;
    const VkPipeline pipeline = FindOrEmplaceColorPipeline(key);
    scheduler.RequestRenderpass(dst_framebuffer);
    scheduler.Record([this, dst_region, src_region, pipeline, layout, sampler,
                      src_view](vk::CommandBuffer cmdbuf) {
        // TODO: Barriers
        const VkDescriptorSet descriptor_set = one_texture_descriptor_allocator.Commit();
        UpdateOneTextureDescriptorSet(device, descriptor_set, sampler, src_view);
        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, descriptor_set,
                                  nullptr);
        BindBlitState(cmdbuf, layout, dst_region, src_region);
        cmdbuf.Draw(3, 1, 0, 0);
    });
    scheduler.InvalidateState();
}

void BlitImageHelper::BlitColor(const Framebuffer* dst_framebuffer, VkImageView src_image_view,
                                VkImage src_image, VkSampler src_sampler,
                                const Region2D& dst_region, const Region2D& src_region,
                                const Extent3D& src_size) {
    const BlitImagePipelineKey key{
        .renderpass = dst_framebuffer->RenderPass(),
        .operation = Tegra::Engines::Fermi2D::Operation::SrcCopy,
    };
    const VkPipelineLayout layout = *one_texture_pipeline_layout;
    const VkPipeline pipeline = FindOrEmplaceColorPipeline(key);
    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([this, dst_framebuffer, src_image_view, src_image, src_sampler, dst_region,
                      src_region, src_size, pipeline, layout](vk::CommandBuffer cmdbuf) {
        TransitionImageLayout(cmdbuf, src_image, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
        BeginRenderPass(cmdbuf, dst_framebuffer);
        const VkDescriptorSet descriptor_set = one_texture_descriptor_allocator.Commit();
        UpdateOneTextureDescriptorSet(device, descriptor_set, src_sampler, src_image_view);
        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, descriptor_set,
                                  nullptr);
        BindBlitState(cmdbuf, layout, dst_region, src_region, src_size);
        cmdbuf.Draw(3, 1, 0, 0);
        cmdbuf.EndRenderPass();
    });
}

void BlitImageHelper::BlitDepthStencil(const Framebuffer* dst_framebuffer,
                                       VkImageView src_depth_view, VkImageView src_stencil_view,
                                       const Region2D& dst_region, const Region2D& src_region,
                                       Tegra::Engines::Fermi2D::Filter filter,
                                       Tegra::Engines::Fermi2D::Operation operation) {
    if (!device.IsExtShaderStencilExportSupported()) {
        return;
    }
    ASSERT(filter == Tegra::Engines::Fermi2D::Filter::Point);
    ASSERT(operation == Tegra::Engines::Fermi2D::Operation::SrcCopy);
    const BlitImagePipelineKey key{
        .renderpass = dst_framebuffer->RenderPass(),
        .operation = operation,
    };
    const VkPipelineLayout layout = *two_textures_pipeline_layout;
    const VkSampler sampler = *nearest_sampler;
    const VkPipeline pipeline = FindOrEmplaceDepthStencilPipeline(key);
    scheduler.RequestRenderpass(dst_framebuffer);
    scheduler.Record([dst_region, src_region, pipeline, layout, sampler, src_depth_view,
                      src_stencil_view, this](vk::CommandBuffer cmdbuf) {
        // TODO: Barriers
        const VkDescriptorSet descriptor_set = two_textures_descriptor_allocator.Commit();
        UpdateTwoTexturesDescriptorSet(device, descriptor_set, sampler, src_depth_view,
                                       src_stencil_view);
        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, descriptor_set,
                                  nullptr);
        BindBlitState(cmdbuf, layout, dst_region, src_region);
        cmdbuf.Draw(3, 1, 0, 0);
    });
    scheduler.InvalidateState();
}

void BlitImageHelper::ConvertD32ToR32(const Framebuffer* dst_framebuffer,
                                      const ImageView& src_image_view) {
    ConvertDepthToColorPipeline(convert_d32_to_r32_pipeline, dst_framebuffer->RenderPass());
    Convert(*convert_d32_to_r32_pipeline, dst_framebuffer, src_image_view);
}

void BlitImageHelper::ConvertR32ToD32(const Framebuffer* dst_framebuffer,
                                      const ImageView& src_image_view) {
    ConvertColorToDepthPipeline(convert_r32_to_d32_pipeline, dst_framebuffer->RenderPass());
    Convert(*convert_r32_to_d32_pipeline, dst_framebuffer, src_image_view);
}

void BlitImageHelper::ConvertD16ToR16(const Framebuffer* dst_framebuffer,
                                      const ImageView& src_image_view) {
    ConvertDepthToColorPipeline(convert_d16_to_r16_pipeline, dst_framebuffer->RenderPass());
    Convert(*convert_d16_to_r16_pipeline, dst_framebuffer, src_image_view);
}

void BlitImageHelper::ConvertR16ToD16(const Framebuffer* dst_framebuffer,
                                      const ImageView& src_image_view) {
    ConvertColorToDepthPipeline(convert_r16_to_d16_pipeline, dst_framebuffer->RenderPass());
    Convert(*convert_r16_to_d16_pipeline, dst_framebuffer, src_image_view);
}

void BlitImageHelper::ConvertABGR8SRGBToD24S8(const Framebuffer* dst_framebuffer,
                                              const ImageView& src_image_view) {
    ConvertPipelineDepthTargetEx(convert_abgr8_srgb_to_d24s8_pipeline,
                                 dst_framebuffer->RenderPass(), convert_abgr8_srgb_to_d24s8_frag);
    Convert(*convert_abgr8_srgb_to_d24s8_pipeline, dst_framebuffer, src_image_view);
}

void BlitImageHelper::ConvertABGR8ToD24S8(const Framebuffer* dst_framebuffer,
                                          const ImageView& src_image_view) {
    ConvertPipelineDepthTargetEx(convert_abgr8_to_d24s8_pipeline, dst_framebuffer->RenderPass(),
                                 convert_abgr8_to_d24s8_frag);
    Convert(*convert_abgr8_to_d24s8_pipeline, dst_framebuffer, src_image_view);
}

void BlitImageHelper::ConvertABGR8ToD32F(const Framebuffer* dst_framebuffer,
                                         const ImageView& src_image_view) {
    ConvertPipelineDepthTargetEx(convert_abgr8_to_d32f_pipeline, dst_framebuffer->RenderPass(),
                                 convert_abgr8_to_d32f_frag);
    Convert(*convert_abgr8_to_d32f_pipeline, dst_framebuffer, src_image_view);
}

void BlitImageHelper::ConvertD32FToABGR8(const Framebuffer* dst_framebuffer,
                                         ImageView& src_image_view) {
    ConvertPipelineColorTargetEx(convert_d32f_to_abgr8_pipeline, dst_framebuffer->RenderPass(),
                                 convert_d32f_to_abgr8_frag);
    ConvertDepthStencil(*convert_d32f_to_abgr8_pipeline, dst_framebuffer, src_image_view);
}

void BlitImageHelper::ConvertD24S8ToABGR8(const Framebuffer* dst_framebuffer,
                                          ImageView& src_image_view) {
    ConvertPipelineColorTargetEx(convert_d24s8_to_abgr8_pipeline, dst_framebuffer->RenderPass(),
                                 convert_d24s8_to_abgr8_frag);
    ConvertDepthStencil(*convert_d24s8_to_abgr8_pipeline, dst_framebuffer, src_image_view);
}

void BlitImageHelper::ConvertS8D24ToABGR8(const Framebuffer* dst_framebuffer,
                                          ImageView& src_image_view) {
    ConvertPipelineColorTargetEx(convert_s8d24_to_abgr8_pipeline, dst_framebuffer->RenderPass(),
                                 convert_s8d24_to_abgr8_frag);
    ConvertDepthStencil(*convert_s8d24_to_abgr8_pipeline, dst_framebuffer, src_image_view);
}

void BlitImageHelper::ClearColor(const Framebuffer* dst_framebuffer, u8 color_mask,
                                 const std::array<f32, 4>& clear_color,
                                 const Region2D& dst_region) {
    const BlitImagePipelineKey key{
        .renderpass = dst_framebuffer->RenderPass(),
        .operation = Tegra::Engines::Fermi2D::Operation::BlendPremult,
    };
    const VkPipeline pipeline = FindOrEmplaceClearColorPipeline(key);
    const VkPipelineLayout layout = *clear_color_pipeline_layout;
    scheduler.RequestRenderpass(dst_framebuffer);
    scheduler.Record(
        [pipeline, layout, color_mask, clear_color, dst_region](vk::CommandBuffer cmdbuf) {
            cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            const std::array blend_color = {
                (color_mask & 0x1) ? 1.0f : 0.0f, (color_mask & 0x2) ? 1.0f : 0.0f,
                (color_mask & 0x4) ? 1.0f : 0.0f, (color_mask & 0x8) ? 1.0f : 0.0f};
            cmdbuf.SetBlendConstants(blend_color.data());
            BindBlitState(cmdbuf, dst_region);
            cmdbuf.PushConstants(layout, VK_SHADER_STAGE_FRAGMENT_BIT, clear_color);
            cmdbuf.Draw(3, 1, 0, 0);
        });
    scheduler.InvalidateState();
}

void BlitImageHelper::ClearDepthStencil(const Framebuffer* dst_framebuffer, bool depth_clear,
                                        f32 clear_depth, u8 stencil_mask, u32 stencil_ref,
                                        u32 stencil_compare_mask, const Region2D& dst_region) {
    const BlitDepthStencilPipelineKey key{
        .renderpass = dst_framebuffer->RenderPass(),
        .depth_clear = depth_clear,
        .stencil_mask = stencil_mask,
        .stencil_compare_mask = stencil_compare_mask,
        .stencil_ref = stencil_ref,
    };
    const VkPipeline pipeline = FindOrEmplaceClearStencilPipeline(key);
    const VkPipelineLayout layout = *clear_color_pipeline_layout;
    scheduler.RequestRenderpass(dst_framebuffer);
    scheduler.Record([pipeline, layout, clear_depth, dst_region](vk::CommandBuffer cmdbuf) {
        constexpr std::array blend_constants{0.0f, 0.0f, 0.0f, 0.0f};
        cmdbuf.SetBlendConstants(blend_constants.data());
        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        BindBlitState(cmdbuf, dst_region);
        cmdbuf.PushConstants(layout, VK_SHADER_STAGE_FRAGMENT_BIT, clear_depth);
        cmdbuf.Draw(3, 1, 0, 0);
    });
    scheduler.InvalidateState();
}

void BlitImageHelper::Convert(VkPipeline pipeline, const Framebuffer* dst_framebuffer,
                              const ImageView& src_image_view) {
    const VkPipelineLayout layout = *one_texture_pipeline_layout;
    const VkImageView src_view = src_image_view.Handle(Shader::TextureType::Color2D);
    const VkSampler sampler = *nearest_sampler;
    const VkExtent2D extent = GetConversionExtent(src_image_view);

    scheduler.RequestRenderpass(dst_framebuffer);
    scheduler.Record([pipeline, layout, sampler, src_view, extent, this](vk::CommandBuffer cmdbuf) {
        const VkOffset2D offset{
            .x = 0,
            .y = 0,
        };
        const VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(extent.width),
            .height = static_cast<float>(extent.height),
            .minDepth = 0.0f,
            .maxDepth = 0.0f,
        };
        const VkRect2D scissor{
            .offset = offset,
            .extent = extent,
        };
        const PushConstants push_constants{
            .tex_scale = {viewport.width, viewport.height},
            .tex_offset = {0.0f, 0.0f},
        };
        const VkDescriptorSet descriptor_set = one_texture_descriptor_allocator.Commit();
        UpdateOneTextureDescriptorSet(device, descriptor_set, sampler, src_view);

        // TODO: Barriers
        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, descriptor_set,
                                  nullptr);
        cmdbuf.SetViewport(0, viewport);
        cmdbuf.SetScissor(0, scissor);
        cmdbuf.PushConstants(layout, VK_SHADER_STAGE_VERTEX_BIT, push_constants);
        cmdbuf.Draw(3, 1, 0, 0);
    });
    scheduler.InvalidateState();
}

void BlitImageHelper::ConvertDepthStencil(VkPipeline pipeline, const Framebuffer* dst_framebuffer,
                                          ImageView& src_image_view) {
    const VkPipelineLayout layout = *two_textures_pipeline_layout;
    const VkImageView src_depth_view = src_image_view.DepthView();
    const VkImageView src_stencil_view = src_image_view.StencilView();
    const VkSampler sampler = *nearest_sampler;
    const VkExtent2D extent = GetConversionExtent(src_image_view);

    scheduler.RequestRenderpass(dst_framebuffer);
    scheduler.Record([pipeline, layout, sampler, src_depth_view, src_stencil_view, extent,
                      this](vk::CommandBuffer cmdbuf) {
        const VkOffset2D offset{
            .x = 0,
            .y = 0,
        };
        const VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(extent.width),
            .height = static_cast<float>(extent.height),
            .minDepth = 0.0f,
            .maxDepth = 0.0f,
        };
        const VkRect2D scissor{
            .offset = offset,
            .extent = extent,
        };
        const PushConstants push_constants{
            .tex_scale = {viewport.width, viewport.height},
            .tex_offset = {0.0f, 0.0f},
        };
        const VkDescriptorSet descriptor_set = two_textures_descriptor_allocator.Commit();
        UpdateTwoTexturesDescriptorSet(device, descriptor_set, sampler, src_depth_view,
                                       src_stencil_view);
        // TODO: Barriers
        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, descriptor_set,
                                  nullptr);
        cmdbuf.SetViewport(0, viewport);
        cmdbuf.SetScissor(0, scissor);
        cmdbuf.PushConstants(layout, VK_SHADER_STAGE_VERTEX_BIT, push_constants);
        cmdbuf.Draw(3, 1, 0, 0);
    });
    scheduler.InvalidateState();
}

VkPipeline BlitImageHelper::FindOrEmplaceColorPipeline(const BlitImagePipelineKey& key) {
    const auto it = std::ranges::find(blit_color_keys, key);
    if (it != blit_color_keys.end()) {
        return *blit_color_pipelines[std::distance(blit_color_keys.begin(), it)];
    }
    blit_color_keys.push_back(key);

    const std::array stages = MakeStages(*full_screen_vert, *blit_color_to_color_frag);
    const VkPipelineColorBlendAttachmentState blend_attachment{
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    // TODO: programmable blending
    const VkPipelineColorBlendStateCreateInfo color_blend_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_CLEAR,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
    };
    blit_color_pipelines.push_back(device.GetLogical().CreateGraphicsPipeline({
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<u32>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pInputAssemblyState = &PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pTessellationState = nullptr,
        .pViewportState = &PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pRasterizationState = &PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pMultisampleState = &PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pDepthStencilState = nullptr,
        .pColorBlendState = &color_blend_create_info,
        .pDynamicState = &PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .layout = *one_texture_pipeline_layout,
        .renderPass = key.renderpass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    }));
    return *blit_color_pipelines.back();
}

VkPipeline BlitImageHelper::FindOrEmplaceDepthStencilPipeline(const BlitImagePipelineKey& key) {
    const auto it = std::ranges::find(blit_depth_stencil_keys, key);
    if (it != blit_depth_stencil_keys.end()) {
        return *blit_depth_stencil_pipelines[std::distance(blit_depth_stencil_keys.begin(), it)];
    }
    blit_depth_stencil_keys.push_back(key);
    const std::array stages = MakeStages(*full_screen_vert, *blit_depth_stencil_frag);
    blit_depth_stencil_pipelines.push_back(device.GetLogical().CreateGraphicsPipeline({
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<u32>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pInputAssemblyState = &PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pTessellationState = nullptr,
        .pViewportState = &PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pRasterizationState = &PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pMultisampleState = &PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pDepthStencilState = &PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pColorBlendState = &PIPELINE_COLOR_BLEND_STATE_GENERIC_CREATE_INFO,
        .pDynamicState = &PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .layout = *two_textures_pipeline_layout,
        .renderPass = key.renderpass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    }));
    return *blit_depth_stencil_pipelines.back();
}

VkPipeline BlitImageHelper::FindOrEmplaceClearColorPipeline(const BlitImagePipelineKey& key) {
    const auto it = std::ranges::find(clear_color_keys, key);
    if (it != clear_color_keys.end()) {
        return *clear_color_pipelines[std::distance(clear_color_keys.begin(), it)];
    }
    clear_color_keys.push_back(key);
    const std::array stages = MakeStages(*clear_color_vert, *clear_color_frag);
    const VkPipelineColorBlendAttachmentState color_blend_attachment_state{
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_COLOR,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    const VkPipelineColorBlendStateCreateInfo color_blend_state_generic_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_CLEAR,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment_state,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
    };
    clear_color_pipelines.push_back(device.GetLogical().CreateGraphicsPipeline({
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<u32>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pInputAssemblyState = &PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pTessellationState = nullptr,
        .pViewportState = &PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pRasterizationState = &PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pMultisampleState = &PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pDepthStencilState = &PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pColorBlendState = &color_blend_state_generic_create_info,
        .pDynamicState = &PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .layout = *clear_color_pipeline_layout,
        .renderPass = key.renderpass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    }));
    return *clear_color_pipelines.back();
}

VkPipeline BlitImageHelper::FindOrEmplaceClearStencilPipeline(
    const BlitDepthStencilPipelineKey& key) {
    const auto it = std::ranges::find(clear_stencil_keys, key);
    if (it != clear_stencil_keys.end()) {
        return *clear_stencil_pipelines[std::distance(clear_stencil_keys.begin(), it)];
    }
    clear_stencil_keys.push_back(key);
    const std::array stages = MakeStages(*clear_color_vert, *clear_stencil_frag);
    const auto stencil = VkStencilOpState{
        .failOp = VK_STENCIL_OP_KEEP,
        .passOp = VK_STENCIL_OP_REPLACE,
        .depthFailOp = VK_STENCIL_OP_KEEP,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .compareMask = key.stencil_compare_mask,
        .writeMask = key.stencil_mask,
        .reference = key.stencil_ref,
    };
    const VkPipelineDepthStencilStateCreateInfo depth_stencil_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = key.depth_clear,
        .depthWriteEnable = key.depth_clear,
        .depthCompareOp = VK_COMPARE_OP_ALWAYS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_TRUE,
        .front = stencil,
        .back = stencil,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 0.0f,
    };
    clear_stencil_pipelines.push_back(device.GetLogical().CreateGraphicsPipeline({
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<u32>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pInputAssemblyState = &PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pTessellationState = nullptr,
        .pViewportState = &PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pRasterizationState = &PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pMultisampleState = &PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pDepthStencilState = &depth_stencil_ci,
        .pColorBlendState = &PIPELINE_COLOR_BLEND_STATE_GENERIC_CREATE_INFO,
        .pDynamicState = &PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .layout = *clear_color_pipeline_layout,
        .renderPass = key.renderpass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    }));
    return *clear_stencil_pipelines.back();
}

void BlitImageHelper::ConvertPipeline(vk::Pipeline& pipeline, VkRenderPass renderpass,
                                      bool is_target_depth) {
    if (pipeline) {
        return;
    }
    VkShaderModule frag_shader =
        is_target_depth ? *convert_float_to_depth_frag : *convert_depth_to_float_frag;
    const std::array stages = MakeStages(*full_screen_vert, frag_shader);
    pipeline = device.GetLogical().CreateGraphicsPipeline({
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<u32>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pInputAssemblyState = &PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pTessellationState = nullptr,
        .pViewportState = &PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pRasterizationState = &PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pMultisampleState = &PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pDepthStencilState = is_target_depth ? &PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO : nullptr,
        .pColorBlendState = is_target_depth ? &PIPELINE_COLOR_BLEND_STATE_EMPTY_CREATE_INFO
                                            : &PIPELINE_COLOR_BLEND_STATE_GENERIC_CREATE_INFO,
        .pDynamicState = &PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .layout = *one_texture_pipeline_layout,
        .renderPass = renderpass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    });
}

void BlitImageHelper::ConvertDepthToColorPipeline(vk::Pipeline& pipeline, VkRenderPass renderpass) {
    ConvertPipeline(pipeline, renderpass, false);
}

void BlitImageHelper::ConvertColorToDepthPipeline(vk::Pipeline& pipeline, VkRenderPass renderpass) {
    ConvertPipeline(pipeline, renderpass, true);
}

void BlitImageHelper::ConvertPipelineEx(vk::Pipeline& pipeline, VkRenderPass renderpass,
                                        vk::ShaderModule& module, bool single_texture,
                                        bool is_target_depth) {
    if (pipeline) {
        return;
    }
    const std::array stages = MakeStages(*full_screen_vert, *module);
    pipeline = device.GetLogical().CreateGraphicsPipeline({
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<u32>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pInputAssemblyState = &PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pTessellationState = nullptr,
        .pViewportState = &PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pRasterizationState = &PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pMultisampleState = &PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pDepthStencilState = is_target_depth ? &PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO : nullptr,
        .pColorBlendState = &PIPELINE_COLOR_BLEND_STATE_GENERIC_CREATE_INFO,
        .pDynamicState = &PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .layout = single_texture ? *one_texture_pipeline_layout : *two_textures_pipeline_layout,
        .renderPass = renderpass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    });
}

void BlitImageHelper::ConvertPipelineColorTargetEx(vk::Pipeline& pipeline, VkRenderPass renderpass,
                                                   vk::ShaderModule& module) {
    ConvertPipelineEx(pipeline, renderpass, module, false, false);
}

void BlitImageHelper::ConvertPipelineDepthTargetEx(vk::Pipeline& pipeline, VkRenderPass renderpass,
                                                   vk::ShaderModule& module) {
    ConvertPipelineEx(pipeline, renderpass, module, true, true);
}

} // namespace Vulkan
