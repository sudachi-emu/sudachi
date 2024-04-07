// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <glad/glad.h>

#include "video_core/engines/fermi_2d.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/texture_cache/types.h"

namespace OpenGL {

using VideoCommon::Extent3D;
using VideoCommon::Offset2D;
using VideoCommon::Region2D;

class ProgramManager;
class Framebuffer;
class ImageView;

class BlitImageHelper {
public:
    explicit BlitImageHelper(ProgramManager& program_manager);
    ~BlitImageHelper();

    void BlitColor(GLuint dst_framebuffer, GLuint src_image_view, GLuint src_sampler,
                   const Region2D& dst_region, const Region2D& src_region,
                   const Extent3D& src_size);

private:
    ProgramManager& program_manager;

    OGLProgram full_screen_vert;
    OGLProgram blit_color_to_color_frag;
};

} // namespace OpenGL
