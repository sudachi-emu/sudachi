// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/settings.h"
#include "video_core/fsr.h"
#include "video_core/renderer_opengl/gl_fsr.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"
#include "video_core/renderer_opengl/gl_shader_util.h"

namespace OpenGL {
using namespace FSR;

using FsrConstants = std::array<u32, 4 * 4>;

FSR::FSR(std::string_view fsr_vertex_source, std::string_view fsr_easu_source,
         std::string_view fsr_rcas_source)
    : fsr_vertex{CreateProgram(fsr_vertex_source, GL_VERTEX_SHADER)},
      fsr_easu_frag{CreateProgram(fsr_easu_source, GL_FRAGMENT_SHADER)},
      fsr_rcas_frag{CreateProgram(fsr_rcas_source, GL_FRAGMENT_SHADER)} {
    glProgramUniform2f(fsr_vertex.handle, 0, 1.0f, 1.0f);
    glProgramUniform2f(fsr_vertex.handle, 1, 0.0f, 0.0f);
}

FSR::~FSR() = default;

void FSR::Draw(ProgramManager& program_manager, const Common::Rectangle<u32>& screen,
               u32 input_image_width, u32 input_image_height,
               const Common::Rectangle<int>& crop_rect) {

    const auto output_image_width = screen.GetWidth();
    const auto output_image_height = screen.GetHeight();

    if (fsr_intermediate_tex.handle) {
        GLint fsr_tex_width, fsr_tex_height;
        glGetTextureLevelParameteriv(fsr_intermediate_tex.handle, 0, GL_TEXTURE_WIDTH,
                                     &fsr_tex_width);
        glGetTextureLevelParameteriv(fsr_intermediate_tex.handle, 0, GL_TEXTURE_HEIGHT,
                                     &fsr_tex_height);
        if (static_cast<u32>(fsr_tex_width) != output_image_width ||
            static_cast<u32>(fsr_tex_height) != output_image_height) {
            fsr_intermediate_tex.Release();
        }
    }
    if (!fsr_intermediate_tex.handle) {
        fsr_intermediate_tex.Create(GL_TEXTURE_2D);
        glTextureStorage2D(fsr_intermediate_tex.handle, 1, GL_RGB16F, output_image_width,
                           output_image_height);
        glNamedFramebufferTexture(fsr_framebuffer.handle, GL_COLOR_ATTACHMENT0,
                                  fsr_intermediate_tex.handle, 0);
    }

    GLint old_draw_fb;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw_fb);

    glFrontFace(GL_CW);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fsr_framebuffer.handle);
    glViewportIndexedf(0, 0.0f, 0.0f, static_cast<GLfloat>(output_image_width),
                       static_cast<GLfloat>(output_image_height));

    FsrConstants constants;
    FsrEasuConOffset(
        constants.data() + 0, constants.data() + 4, constants.data() + 8, constants.data() + 12,

        static_cast<f32>(crop_rect.GetWidth()), static_cast<f32>(crop_rect.GetHeight()),
        static_cast<f32>(input_image_width), static_cast<f32>(input_image_height),
        static_cast<f32>(output_image_width), static_cast<f32>(output_image_height),
        static_cast<f32>(crop_rect.left), static_cast<f32>(crop_rect.top));

    glProgramUniform4uiv(fsr_easu_frag.handle, 0, sizeof(constants), std::data(constants));

    program_manager.BindPresentPrograms(fsr_vertex.handle, fsr_easu_frag.handle);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, old_draw_fb);
    glBindTextureUnit(0, fsr_intermediate_tex.handle);

    const float sharpening =
        static_cast<float>(Settings::values.fsr_sharpening_slider.GetValue()) / 100.0f;

    FsrRcasCon(constants.data(), sharpening);
    glProgramUniform4uiv(fsr_rcas_frag.handle, 0, sizeof(constants), std::data(constants));
}

void FSR::InitBuffers() {
    fsr_framebuffer.Create();
}

void FSR::ReleaseBuffers() {
    fsr_framebuffer.Release();
    fsr_intermediate_tex.Release();
}

const OGLProgram& FSR::GetPresentFragmentProgram() const noexcept {
    return fsr_rcas_frag;
}

bool FSR::AreBuffersInitialized() const noexcept {
    return fsr_framebuffer.handle;
}

} // namespace OpenGL
