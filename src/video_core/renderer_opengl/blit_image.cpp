// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>

#include "video_core/host_shaders/blit_color_float_frag.h"
#include "video_core/host_shaders/full_screen_triangle_vert.h"
#include "video_core/renderer_opengl/blit_image.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"
#include "video_core/renderer_opengl/gl_shader_util.h"

namespace OpenGL {

BlitImageHelper::BlitImageHelper(ProgramManager& program_manager_)
    : program_manager(program_manager_),
      full_screen_vert(CreateProgram(HostShaders::FULL_SCREEN_TRIANGLE_VERT, GL_VERTEX_SHADER)),
      blit_color_to_color_frag(
          CreateProgram(HostShaders::BLIT_COLOR_FLOAT_FRAG, GL_FRAGMENT_SHADER)) {}

BlitImageHelper::~BlitImageHelper() = default;

void BlitImageHelper::BlitColor(GLuint dst_framebuffer, GLuint src_image_view, GLuint src_sampler,
                                const Region2D& dst_region, const Region2D& src_region,
                                const Extent3D& src_size) {
    glDisable(GL_CULL_FACE);
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_RASTERIZER_DISCARD);
    glDisable(GL_ALPHA_TEST);
    glDisablei(GL_BLEND, 0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glFrontFace(GL_CW);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthRangeIndexed(0, 0.0, 0.0);

    program_manager.BindPresentPrograms(full_screen_vert.handle, blit_color_to_color_frag.handle);
    glProgramUniform2f(full_screen_vert.handle, 0,
                       static_cast<float>(src_region.end.x - src_region.start.x) /
                           static_cast<float>(src_size.width),
                       static_cast<float>(src_region.end.y - src_region.start.y) /
                           static_cast<float>(src_size.height));
    glProgramUniform2f(full_screen_vert.handle, 1,
                       static_cast<float>(src_region.start.x) / static_cast<float>(src_size.width),
                       static_cast<float>(src_region.start.y) /
                           static_cast<float>(src_size.height));
    glViewport(std::min(dst_region.start.x, dst_region.end.x),
               std::min(dst_region.start.y, dst_region.end.y),
               std::abs(dst_region.end.x - dst_region.start.x),
               std::abs(dst_region.end.y - dst_region.start.y));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_framebuffer);
    glBindSampler(0, src_sampler);
    glBindTextureUnit(0, src_image_view);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
} // namespace OpenGL
