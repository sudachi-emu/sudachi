// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <span>
#include <string>
#include <vector>

#include "common/common_types.h"

#include "core/hle/result.h"

namespace Network {
class SocketBase;
}

namespace Service::SSL {

constexpr Result ResultNoSocket{ErrorModule::SSLSrv, 103};
constexpr Result ResultInvalidSocket{ErrorModule::SSLSrv, 106};
constexpr Result ResultTimeout{ErrorModule::SSLSrv, 205};
constexpr Result ResultInternalError{ErrorModule::SSLSrv, 999}; // made up

// ResultWouldBlock is returned from Read and Write, and oddly, DoHandshake,
// with no way in the latter case to distinguish whether the client should poll
// for read or write.  The one official client I've seen handles this by always
// polling for read (with a timeout).
constexpr Result ResultWouldBlock{ErrorModule::SSLSrv, 204};

class SSLConnectionBackend {
public:
    virtual ~SSLConnectionBackend() {}
    virtual void SetSocket(std::shared_ptr<Network::SocketBase> socket) = 0;
    virtual Result SetHostName(const std::string& hostname) = 0;
    virtual Result DoHandshake() = 0;
    virtual Result Read(size_t* out_size, std::span<u8> data) = 0;
    virtual Result Write(size_t* out_size, std::span<const u8> data) = 0;
    virtual Result GetServerCerts(std::vector<std::vector<u8>>* out_certs) = 0;
};

Result CreateSSLConnectionBackend(std::unique_ptr<SSLConnectionBackend>* out_backend);

} // namespace Service::SSL
