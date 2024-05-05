// SPDX-FileCopyrightText: 2015 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string_view>
#include <glad/glad.h>
#include "common/assert.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_util.h"

namespace OpenGL {

void OGLRenderbuffer::Create() {
    if (handle != 0)
        return;

    glCreateRenderbuffers(1, &handle);
}

void OGLRenderbuffer::Release() {
    if (handle == 0)
        return;

    glDeleteRenderbuffers(1, &handle);
    handle = 0;
}

void OGLTexture::Create(GLenum target) {
    if (handle != 0)
        return;

    glCreateTextures(target, 1, &handle);
}

void OGLTexture::Release() {
    if (handle == 0)
        return;

    glDeleteTextures(1, &handle);
    handle = 0;
}

void OGLTextureView::Create() {
    if (handle != 0)
        return;

    glGenTextures(1, &handle);
}

void OGLTextureView::Release() {
    if (handle == 0)
        return;

    glDeleteTextures(1, &handle);
    handle = 0;
}

void OGLSampler::Create() {
    if (handle != 0)
        return;

    glCreateSamplers(1, &handle);
}

void OGLSampler::Release() {
    if (handle == 0)
        return;

    glDeleteSamplers(1, &handle);
    handle = 0;
}

void OGLShader::Release() {
    if (handle == 0)
        return;

    glDeleteShader(handle);
    handle = 0;
}

void OGLProgram::Release() {
    if (handle == 0)
        return;

    glDeleteProgram(handle);
    handle = 0;
}

void OGLAssemblyProgram::Release() {
    if (handle == 0) {
        return;
    }
    glDeleteProgramsARB(1, &handle);
    handle = 0;
}

void OGLPipeline::Create() {
    if (handle != 0)
        return;

    glGenProgramPipelines(1, &handle);
}

void OGLPipeline::Release() {
    if (handle == 0)
        return;

    glDeleteProgramPipelines(1, &handle);
    handle = 0;
}

void OGLBuffer::Create() {
    if (handle != 0)
        return;

    glCreateBuffers(1, &handle);
}

void OGLBuffer::Release() {
    if (handle == 0)
        return;

    glDeleteBuffers(1, &handle);
    handle = 0;
}

void OGLSync::Create() {
    if (handle != 0)
        return;

    // Don't profile here, this one is expected to happen ingame.
    handle = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void OGLSync::Release() {
    if (handle == 0)
        return;

    // Don't profile here, this one is expected to happen ingame.
    glDeleteSync(handle);
    handle = 0;
}

bool OGLSync::IsSignaled() const noexcept {
    // At least on Nvidia, glClientWaitSync with a timeout of 0
    // is faster than glGetSynciv of GL_SYNC_STATUS.
    // Timeout of 0 means this check is non-blocking.
    const auto sync_status = glClientWaitSync(handle, 0, 0);
    ASSERT(sync_status != GL_WAIT_FAILED);
    return sync_status != GL_TIMEOUT_EXPIRED;
}

void OGLFramebuffer::Create() {
    if (handle != 0)
        return;

    // Bind to READ_FRAMEBUFFER to stop Nvidia's driver from creating an EXT_framebuffer instead of
    // a core framebuffer. EXT framebuffer attachments have to match in size and can be shared
    // across contexts. sudachi doesn't share framebuffers across contexts and we need attachments with
    // mismatching size, this is why core framebuffers are preferred.
    glGenFramebuffers(1, &handle);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, handle);
}

void OGLFramebuffer::Release() {
    if (handle == 0)
        return;

    glDeleteFramebuffers(1, &handle);
    handle = 0;
}

void OGLQuery::Create(GLenum target) {
    if (handle != 0)
        return;

    glCreateQueries(target, 1, &handle);
}

void OGLQuery::Release() {
    if (handle == 0)
        return;

    glDeleteQueries(1, &handle);
    handle = 0;
}

void OGLTransformFeedback::Create() {
    if (handle != 0)
        return;

    glCreateTransformFeedbacks(1, &handle);
}

void OGLTransformFeedback::Release() {
    if (handle == 0)
        return;

    glDeleteTransformFeedbacks(1, &handle);
    handle = 0;
}

} // namespace OpenGL
