// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>

#include "common/common_types.h"
#include "common/math_util.h"
#include "video_core/fsr.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace OpenGL {

class ProgramManager;

class FSR {
public:
    explicit FSR(u32 output_width, u32 output_height);
    ~FSR();

    GLuint Draw(ProgramManager& program_manager, GLuint texture, u32 input_image_width,
                u32 input_image_height, const Common::Rectangle<f32>& crop_rect);

    bool NeedsRecreation(const Common::Rectangle<u32>& screen);

private:
    const u32 width;
    const u32 height;
    OGLFramebuffer framebuffer;
    OGLSampler sampler;
    OGLProgram vert;
    OGLProgram easu_frag;
    OGLProgram rcas_frag;
    OGLTexture easu_tex;
    OGLTexture rcas_tex;
};

} // namespace OpenGL
