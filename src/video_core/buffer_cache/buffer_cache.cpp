// SPDX-FileCopyrightText: Copyright 2022 sudachi Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_core/buffer_cache/buffer_cache_base.h"
#include "video_core/control/channel_state_cache.inc"

namespace VideoCommon {

template class VideoCommon::ChannelSetupCaches<VideoCommon::BufferCacheChannelInfo>;

} // namespace VideoCommon
