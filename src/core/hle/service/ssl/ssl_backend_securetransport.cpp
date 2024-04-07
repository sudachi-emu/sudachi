// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <mutex>

// SecureTransport has been deprecated in its entirety in favor of
// Network.framework, but that does not allow layering TLS on top of an
// arbitrary socket.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <Security/SecureTransport.h>
#pragma GCC diagnostic pop
#endif

#include "core/hle/service/ssl/ssl_backend.h"
#include "core/internal_network/network.h"
#include "core/internal_network/sockets.h"

namespace {

template <typename T>
struct CFReleaser {
    T ptr;

    SUDACHI_NON_COPYABLE(CFReleaser);
    constexpr CFReleaser() : ptr(nullptr) {}
    constexpr CFReleaser(T ptr) : ptr(ptr) {}
    constexpr operator T() {
        return ptr;
    }
    ~CFReleaser() {
        if (ptr) {
            CFRelease(ptr);
        }
    }
};

std::string CFStringToString(CFStringRef cfstr) {
    CFReleaser<CFDataRef> cfdata(
        CFStringCreateExternalRepresentation(nullptr, cfstr, kCFStringEncodingUTF8, 0));
    ASSERT_OR_EXECUTE(cfdata, { return "???"; });
    return std::string(reinterpret_cast<const char*>(CFDataGetBytePtr(cfdata)),
                       CFDataGetLength(cfdata));
}

std::string OSStatusToString(OSStatus status) {
    CFReleaser<CFStringRef> cfstr(SecCopyErrorMessageString(status, nullptr));
    if (!cfstr) {
        return "[unknown error]";
    }
    return CFStringToString(cfstr);
}

} // namespace

namespace Service::SSL {

class SSLConnectionBackendSecureTransport final : public SSLConnectionBackend {
public:
    Result Init() {
        static std::once_flag once_flag;
        std::call_once(once_flag, []() {
            if (getenv("SSLKEYLOGFILE")) {
                LOG_CRITICAL(Service_SSL, "SSLKEYLOGFILE was set but SecureTransport does not "
                                          "support exporting keys; not logging keys!");
                // Not fatal.
            }
        });

        context.ptr = SSLCreateContext(nullptr, kSSLClientSide, kSSLStreamType);
        if (!context) {
            LOG_ERROR(Service_SSL, "SSLCreateContext failed");
            return ResultInternalError;
        }

        OSStatus status;
        if ((status = SSLSetIOFuncs(context, ReadCallback, WriteCallback)) ||
            (status = SSLSetConnection(context, this))) {
            LOG_ERROR(Service_SSL, "SSLContext initialization failed: {}",
                      OSStatusToString(status));
            return ResultInternalError;
        }

        return ResultSuccess;
    }

    void SetSocket(std::shared_ptr<Network::SocketBase> in_socket) override {
        socket = std::move(in_socket);
    }

    Result SetHostName(const std::string& hostname) override {
        OSStatus status = SSLSetPeerDomainName(context, hostname.c_str(), hostname.size());
        if (status) {
            LOG_ERROR(Service_SSL, "SSLSetPeerDomainName failed: {}", OSStatusToString(status));
            return ResultInternalError;
        }
        return ResultSuccess;
    }

    Result DoHandshake() override {
        OSStatus status = SSLHandshake(context);
        return HandleReturn("SSLHandshake", 0, status);
    }

    Result Read(size_t* out_size, std::span<u8> data) override {
        OSStatus status = SSLRead(context, data.data(), data.size(), out_size);
        return HandleReturn("SSLRead", out_size, status);
    }

    Result Write(size_t* out_size, std::span<const u8> data) override {
        OSStatus status = SSLWrite(context, data.data(), data.size(), out_size);
        return HandleReturn("SSLWrite", out_size, status);
    }

    Result HandleReturn(const char* what, size_t* actual, OSStatus status) {
        switch (status) {
        case 0:
            return ResultSuccess;
        case errSSLWouldBlock:
            return ResultWouldBlock;
        default: {
            std::string reason;
            if (got_read_eof) {
                reason = "server hung up";
            } else {
                reason = OSStatusToString(status);
            }
            LOG_ERROR(Service_SSL, "{} failed: {}", what, reason);
            return ResultInternalError;
        }
        }
    }

    Result GetServerCerts(std::vector<std::vector<u8>>* out_certs) override {
        CFReleaser<SecTrustRef> trust;
        OSStatus status = SSLCopyPeerTrust(context, &trust.ptr);
        if (status) {
            LOG_ERROR(Service_SSL, "SSLCopyPeerTrust failed: {}", OSStatusToString(status));
            return ResultInternalError;
        }
        for (CFIndex i = 0, count = SecTrustGetCertificateCount(trust); i < count; i++) {
            SecCertificateRef cert = SecTrustGetCertificateAtIndex(trust, i);
            CFReleaser<CFDataRef> data(SecCertificateCopyData(cert));
            ASSERT_OR_EXECUTE(data, { return ResultInternalError; });
            const u8* ptr = CFDataGetBytePtr(data);
            out_certs->emplace_back(ptr, ptr + CFDataGetLength(data));
        }
        return ResultSuccess;
    }

    static OSStatus ReadCallback(SSLConnectionRef connection, void* data, size_t* dataLength) {
        return ReadOrWriteCallback(connection, data, dataLength, true);
    }

    static OSStatus WriteCallback(SSLConnectionRef connection, const void* data,
                                  size_t* dataLength) {
        return ReadOrWriteCallback(connection, const_cast<void*>(data), dataLength, false);
    }

    static OSStatus ReadOrWriteCallback(SSLConnectionRef connection, void* data, size_t* dataLength,
                                        bool is_read) {
        auto self =
            static_cast<SSLConnectionBackendSecureTransport*>(const_cast<void*>(connection));
        ASSERT_OR_EXECUTE_MSG(
            self->socket, { return 0; }, "SecureTransport asked to {} but we have no socket",
            is_read ? "read" : "write");

        // SecureTransport callbacks (unlike OpenSSL BIO callbacks) are
        // expected to read/write the full requested dataLength or return an
        // error, so we have to add a loop ourselves.
        size_t requested_len = *dataLength;
        size_t offset = 0;
        while (offset < requested_len) {
            std::span cur(reinterpret_cast<u8*>(data) + offset, requested_len - offset);
            auto [actual, err] = is_read ? self->socket->Recv(0, cur) : self->socket->Send(cur, 0);
            LOG_CRITICAL(Service_SSL, "op={}, offset={} actual={}/{} err={}", is_read, offset,
                         actual, cur.size(), static_cast<s32>(err));
            switch (err) {
            case Network::Errno::SUCCESS:
                offset += actual;
                if (actual == 0) {
                    ASSERT(is_read);
                    self->got_read_eof = true;
                    return errSecEndOfData;
                }
                break;
            case Network::Errno::AGAIN:
                *dataLength = offset;
                return errSSLWouldBlock;
            default:
                LOG_ERROR(Service_SSL, "Socket {} returned Network::Errno {}",
                          is_read ? "recv" : "send", err);
                return errSecIO;
            }
        }
        ASSERT(offset == requested_len);
        return 0;
    }

private:
    CFReleaser<SSLContextRef> context = nullptr;
    bool got_read_eof = false;

    std::shared_ptr<Network::SocketBase> socket;
};

Result CreateSSLConnectionBackend(std::unique_ptr<SSLConnectionBackend>* out_backend) {
    auto conn = std::make_unique<SSLConnectionBackendSecureTransport>();

    R_TRY(conn->Init());

    *out_backend = std::move(conn);
    return ResultSuccess;
}

} // namespace Service::SSL
