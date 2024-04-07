// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"

#include "core/hle/service/ssl/ssl_backend.h"

namespace Service::SSL {

Result CreateSSLConnectionBackend(std::unique_ptr<SSLConnectionBackend>* out_backend) {
    LOG_ERROR(Service_SSL,
              "Can't create SSL connection because no SSL backend is available on this platform");
    return ResultInternalError;
}

} // namespace Service::SSL
