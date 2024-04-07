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
    explicit FSR(std::string_view fsr_vertex_source, std::string_view fsr_easu_source,
                 std::string_view fsr_rcas_source);
    ~FSR();

    void Draw(ProgramManager& program_manager, const Common::Rectangle<u32>& screen,
              u32 input_image_width, u32 input_image_height,
              const Common::Rectangle<int>& crop_rect);

    void InitBuffers();

    void ReleaseBuffers();

    [[nodiscard]] const OGLProgram& GetPresentFragmentProgram() const noexcept;

    [[nodiscard]] bool AreBuffersInitialized() const noexcept;

private:
    OGLFramebuffer fsr_framebuffer;
    OGLProgram fsr_vertex;
    OGLProgram fsr_easu_frag;
    OGLProgram fsr_rcas_frag;
    OGLTexture fsr_intermediate_tex;
};

} // namespace OpenGL
