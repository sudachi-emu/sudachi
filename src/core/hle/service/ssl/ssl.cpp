// SPDX-FileCopyrightText: Copyright 2018 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/string_util.h"

#include "core/core.h"
#include "core/hle/result.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/sockets/bsd.h"
#include "core/hle/service/ssl/cert_store.h"
#include "core/hle/service/ssl/ssl.h"
#include "core/hle/service/ssl/ssl_backend.h"
#include "core/internal_network/network.h"
#include "core/internal_network/sockets.h"

namespace Service::SSL {

// This is nn::ssl::sf::CertificateFormat
enum class CertificateFormat : u32 {
    Pem = 1,
    Der = 2,
};

// This is nn::ssl::sf::ContextOption
enum class ContextOption : u32 {
    None = 0,
    CrlImportDateCheckEnable = 1,
};

// This is nn::ssl::Connection::IoMode
enum class IoMode : u32 {
    Blocking = 1,
    NonBlocking = 2,
};

// This is nn::ssl::sf::OptionType
enum class OptionType : u32 { DoNotCloseSocket = 0, GetServerCertChain = 1, EnableAlpn = 3 };

// This is nn::ssl::sf::SslVersion
struct SslVersion {
    union {
        u32 raw{};

        BitField<0, 1, u32> tls_auto;
        BitField<3, 1, u32> tls_v10;
        BitField<4, 1, u32> tls_v11;
        BitField<5, 1, u32> tls_v12;
        BitField<6, 1, u32> tls_v13;
        BitField<24, 7, u32> api_version;
    };
};

struct SslContextSharedData {
    u32 connection_count = 0;
};

class ISslConnection final : public ServiceFramework<ISslConnection> {
public:
    explicit ISslConnection(Core::System& system_in, SslVersion ssl_version_in,
                            std::shared_ptr<SslContextSharedData>& shared_data_in,
                            std::unique_ptr<SSLConnectionBackend>&& backend_in)
        : ServiceFramework{system_in, "ISslConnection"}, ssl_version{ssl_version_in},
          shared_data{shared_data_in}, backend{std::move(backend_in)} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &ISslConnection::SetSocketDescriptor, "SetSocketDescriptor"},
            {1, &ISslConnection::SetHostName, "SetHostName"},
            {2, &ISslConnection::SetVerifyOption, "SetVerifyOption"},
            {3, &ISslConnection::SetIoMode, "SetIoMode"},
            {4, nullptr, "GetSocketDescriptor"},
            {5, nullptr, "GetHostName"},
            {6, nullptr, "GetVerifyOption"},
            {7, nullptr, "GetIoMode"},
            {8, &ISslConnection::DoHandshake, "DoHandshake"},
            {9, &ISslConnection::DoHandshakeGetServerCert, "DoHandshakeGetServerCert"},
            {10, &ISslConnection::Read, "Read"},
            {11, &ISslConnection::Write, "Write"},
            {12, &ISslConnection::Pending, "Pending"},
            {13, nullptr, "Peek"},
            {14, nullptr, "Poll"},
            {15, nullptr, "GetVerifyCertError"},
            {16, nullptr, "GetNeededServerCertBufferSize"},
            {17, &ISslConnection::SetSessionCacheMode, "SetSessionCacheMode"},
            {18, nullptr, "GetSessionCacheMode"},
            {19, nullptr, "FlushSessionCache"},
            {20, nullptr, "SetRenegotiationMode"},
            {21, nullptr, "GetRenegotiationMode"},
            {22, &ISslConnection::SetOption, "SetOption"},
            {23, nullptr, "GetOption"},
            {24, nullptr, "GetVerifyCertErrors"},
            {25, nullptr, "GetCipherInfo"},
            {26, &ISslConnection::SetNextAlpnProto, "SetNextAlpnProto"},
            {27, &ISslConnection::GetNextAlpnProto, "GetNextAlpnProto"},
            {28, nullptr, "SetDtlsSocketDescriptor"},
            {29, nullptr, "GetDtlsHandshakeTimeout"},
            {30, nullptr, "SetPrivateOption"},
            {31, nullptr, "SetSrtpCiphers"},
            {32, nullptr, "GetSrtpCipher"},
            {33, nullptr, "ExportKeyingMaterial"},
            {34, nullptr, "SetIoTimeout"},
            {35, nullptr, "GetIoTimeout"},
        };
        // clang-format on

        RegisterHandlers(functions);

        shared_data->connection_count++;
    }

    ~ISslConnection() {
        shared_data->connection_count--;
        if (fd_to_close.has_value()) {
            const s32 fd = *fd_to_close;
            if (!do_not_close_socket) {
                LOG_ERROR(Service_SSL,
                          "do_not_close_socket was changed after setting socket; is this right?");
            } else {
                auto bsd = system.ServiceManager().GetService<Service::Sockets::BSD>("bsd:u");
                if (bsd) {
                    auto err = bsd->CloseImpl(fd);
                    if (err != Service::Sockets::Errno::SUCCESS) {
                        LOG_ERROR(Service_SSL, "Failed to close duplicated socket: {}", err);
                    }
                }
            }
        }
    }

private:
    SslVersion ssl_version;
    std::shared_ptr<SslContextSharedData> shared_data;
    std::unique_ptr<SSLConnectionBackend> backend;
    std::optional<int> fd_to_close;
    bool do_not_close_socket = false;
    bool get_server_cert_chain = false;
    std::shared_ptr<Network::SocketBase> socket;
    bool did_handshake = false;
    bool enable_alpn = false;

    Result SetSocketDescriptorImpl(s32* out_fd, s32 fd) {
        LOG_DEBUG(Service_SSL, "called, fd={}", fd);
        ASSERT(!did_handshake);
        auto bsd = system.ServiceManager().GetService<Service::Sockets::BSD>("bsd:u");
        ASSERT_OR_EXECUTE(bsd, { return ResultInternalError; });

        // Based on https://switchbrew.org/wiki/SSL_services#SetSocketDescriptor
        if (do_not_close_socket) {
            auto res = bsd->DuplicateSocketImpl(fd);
            if (!res.has_value()) {
                LOG_ERROR(Service_SSL, "Failed to duplicate socket with fd {}", fd);
                return ResultInvalidSocket;
            }
            fd = *res;
            fd_to_close = fd;
            *out_fd = fd;
        } else {
            *out_fd = -1;
        }
        std::optional<std::shared_ptr<Network::SocketBase>> sock = bsd->GetSocket(fd);
        if (!sock.has_value()) {
            LOG_ERROR(Service_SSL, "invalid socket fd {}", fd);
            return ResultInvalidSocket;
        }
        socket = std::move(*sock);
        backend->SetSocket(socket);
        return ResultSuccess;
    }

    Result SetHostNameImpl(const std::string& hostname) {
        LOG_DEBUG(Service_SSL, "called. hostname={}", hostname);
        ASSERT(!did_handshake);
        return backend->SetHostName(hostname);
    }

    Result SetVerifyOptionImpl(u32 option) {
        ASSERT(!did_handshake);
        LOG_WARNING(Service_SSL, "(STUBBED) called. option={}", option);
        return ResultSuccess;
    }

    Result SetIoModeImpl(u32 input_mode) {
        auto mode = static_cast<IoMode>(input_mode);
        ASSERT(mode == IoMode::Blocking || mode == IoMode::NonBlocking);
        ASSERT_OR_EXECUTE(socket, { return ResultNoSocket; });

        const bool non_block = mode == IoMode::NonBlocking;
        const Network::Errno error = socket->SetNonBlock(non_block);
        if (error != Network::Errno::SUCCESS) {
            LOG_ERROR(Service_SSL, "Failed to set native socket non-block flag to {}", non_block);
        }
        return ResultSuccess;
    }

    Result SetSessionCacheModeImpl(u32 mode) {
        ASSERT(!did_handshake);
        LOG_WARNING(Service_SSL, "(STUBBED) called. value={}", mode);
        return ResultSuccess;
    }

    Result DoHandshakeImpl() {
        ASSERT_OR_EXECUTE(!did_handshake && socket, { return ResultNoSocket; });
        Result res = backend->DoHandshake();
        did_handshake = res.IsSuccess();
        return res;
    }

    std::vector<u8> SerializeServerCerts(const std::vector<std::vector<u8>>& certs) {
        struct Header {
            u64 magic;
            u32 count;
            u32 pad;
        };
        struct EntryHeader {
            u32 size;
            u32 offset;
        };
        if (!get_server_cert_chain) {
            // Just return the first one, unencoded.
            ASSERT_OR_EXECUTE_MSG(
                !certs.empty(), { return {}; }, "Should be at least one server cert");
            return certs[0];
        }
        std::vector<u8> ret;
        Header header{0x4E4D684374726543, static_cast<u32>(certs.size()), 0};
        ret.insert(ret.end(), reinterpret_cast<u8*>(&header), reinterpret_cast<u8*>(&header + 1));
        size_t data_offset = sizeof(Header) + certs.size() * sizeof(EntryHeader);
        for (auto& cert : certs) {
            EntryHeader entry_header{static_cast<u32>(cert.size()), static_cast<u32>(data_offset)};
            data_offset += cert.size();
            ret.insert(ret.end(), reinterpret_cast<u8*>(&entry_header),
                       reinterpret_cast<u8*>(&entry_header + 1));
        }
        for (auto& cert : certs) {
            ret.insert(ret.end(), cert.begin(), cert.end());
        }
        return ret;
    }

    Result ReadImpl(std::vector<u8>* out_data) {
        ASSERT_OR_EXECUTE(did_handshake, { return ResultInternalError; });
        size_t actual_size{};
        Result res = backend->Read(&actual_size, *out_data);
        if (res != ResultSuccess) {
            return res;
        }
        out_data->resize(actual_size);
        return res;
    }

    Result WriteImpl(size_t* out_size, std::span<const u8> data) {
        ASSERT_OR_EXECUTE(did_handshake, { return ResultInternalError; });
        return backend->Write(out_size, data);
    }

    Result PendingImpl(s32* out_pending) {
        LOG_WARNING(Service_SSL, "(STUBBED) called.");
        *out_pending = 0;
        return ResultSuccess;
    }

    void SetSocketDescriptor(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const s32 in_fd = rp.Pop<s32>();
        s32 out_fd{-1};
        const Result res = SetSocketDescriptorImpl(&out_fd, in_fd);
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(res);
        rb.Push<s32>(out_fd);
    }

    void SetHostName(HLERequestContext& ctx) {
        const std::string hostname = Common::StringFromBuffer(ctx.ReadBuffer());
        const Result res = SetHostNameImpl(hostname);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(res);
    }

    void SetVerifyOption(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u32 option = rp.Pop<u32>();
        const Result res = SetVerifyOptionImpl(option);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(res);
    }

    void SetIoMode(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u32 mode = rp.Pop<u32>();
        const Result res = SetIoModeImpl(mode);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(res);
    }

    void DoHandshake(HLERequestContext& ctx) {
        const Result res = DoHandshakeImpl();
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(res);
    }

    void DoHandshakeGetServerCert(HLERequestContext& ctx) {
        struct OutputParameters {
            u32 certs_size;
            u32 certs_count;
        };
        static_assert(sizeof(OutputParameters) == 0x8);

        Result res = DoHandshakeImpl();
        OutputParameters out{};
        if (res == ResultSuccess) {
            std::vector<std::vector<u8>> certs;
            res = backend->GetServerCerts(&certs);
            if (res == ResultSuccess) {
                const std::vector<u8> certs_buf = SerializeServerCerts(certs);
                ctx.WriteBuffer(certs_buf);
                out.certs_count = static_cast<u32>(certs.size());
                out.certs_size = static_cast<u32>(certs_buf.size());
            }
        }
        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(res);
        rb.PushRaw(out);
    }

    void Read(HLERequestContext& ctx) {
        std::vector<u8> output_bytes(ctx.GetWriteBufferSize());
        const Result res = ReadImpl(&output_bytes);
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(res);
        if (res == ResultSuccess) {
            rb.Push(static_cast<u32>(output_bytes.size()));
            ctx.WriteBuffer(output_bytes);
        } else {
            rb.Push(static_cast<u32>(0));
        }
    }

    void Write(HLERequestContext& ctx) {
        size_t write_size{0};
        const Result res = WriteImpl(&write_size, ctx.ReadBuffer());
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(res);
        rb.Push(static_cast<u32>(write_size));
    }

    void Pending(HLERequestContext& ctx) {
        s32 pending_size{0};
        const Result res = PendingImpl(&pending_size);
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(res);
        rb.Push<s32>(pending_size);
    }

    void SetSessionCacheMode(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u32 mode = rp.Pop<u32>();
        const Result res = SetSessionCacheModeImpl(mode);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(res);
    }

    void SetOption(HLERequestContext& ctx) {
        struct Parameters {
            OptionType option;
            s32 value;
        };
        static_assert(sizeof(Parameters) == 0x8, "Parameters is an invalid size");

        IPC::RequestParser rp{ctx};
        const auto parameters = rp.PopRaw<Parameters>();

        switch (parameters.option) {
        case OptionType::DoNotCloseSocket:
            do_not_close_socket = static_cast<bool>(parameters.value);
            break;
        case OptionType::GetServerCertChain:
            get_server_cert_chain = static_cast<bool>(parameters.value);
            break;
        case OptionType::EnableAlpn:
            LOG_ERROR(Service_SSL, "(STUBBED) called.");
            enable_alpn = static_cast<bool>(parameters.value);
            break;
        default:
            LOG_WARNING(Service_SSL, "Unknown option={}, value={}", parameters.option,
                        parameters.value);
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void SetNextAlpnProto(HLERequestContext& ctx) {
        LOG_ERROR(Service_SSL, "(STUBBED) called.");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void GetNextAlpnProto(HLERequestContext& ctx) {
        LOG_ERROR(Service_SSL, "(STUBBED) called.");

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
    }
};

class ISslContext final : public ServiceFramework<ISslContext> {
public:
    explicit ISslContext(Core::System& system_, SslVersion version)
        : ServiceFramework{system_, "ISslContext"}, ssl_version{version},
          shared_data{std::make_shared<SslContextSharedData>()} {
        static const FunctionInfo functions[] = {
            {0, &ISslContext::SetOption, "SetOption"},
            {1, nullptr, "GetOption"},
            {2, &ISslContext::CreateConnection, "CreateConnection"},
            {3, &ISslContext::GetConnectionCount, "GetConnectionCount"},
            {4, &ISslContext::ImportServerPki, "ImportServerPki"},
            {5, &ISslContext::ImportClientPki, "ImportClientPki"},
            {6, nullptr, "RemoveServerPki"},
            {7, nullptr, "RemoveClientPki"},
            {8, nullptr, "RegisterInternalPki"},
            {9, nullptr, "AddPolicyOid"},
            {10, nullptr, "ImportCrl"},
            {11, nullptr, "RemoveCrl"},
            {12, nullptr, "ImportClientCertKeyPki"},
            {13, nullptr, "GeneratePrivateKeyAndCert"},
        };
        RegisterHandlers(functions);
    }

private:
    SslVersion ssl_version;
    std::shared_ptr<SslContextSharedData> shared_data;

    void SetOption(HLERequestContext& ctx) {
        struct Parameters {
            ContextOption option;
            s32 value;
        };
        static_assert(sizeof(Parameters) == 0x8, "Parameters is an invalid size");

        IPC::RequestParser rp{ctx};
        const auto parameters = rp.PopRaw<Parameters>();

        LOG_WARNING(Service_SSL, "(STUBBED) called. option={}, value={}", parameters.option,
                    parameters.value);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void CreateConnection(HLERequestContext& ctx) {
        LOG_WARNING(Service_SSL, "called");

        std::unique_ptr<SSLConnectionBackend> backend;
        const Result res = CreateSSLConnectionBackend(&backend);

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(res);
        if (res == ResultSuccess) {
            rb.PushIpcInterface<ISslConnection>(system, ssl_version, shared_data,
                                                std::move(backend));
        }
    }

    void GetConnectionCount(HLERequestContext& ctx) {
        LOG_DEBUG(Service_SSL, "connection_count={}", shared_data->connection_count);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(shared_data->connection_count);
    }

    void ImportServerPki(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto certificate_format = rp.PopEnum<CertificateFormat>();
        [[maybe_unused]] const auto pkcs_12_certificates = ctx.ReadBuffer(0);

        constexpr u64 server_id = 0;

        LOG_WARNING(Service_SSL, "(STUBBED) called, certificate_format={}", certificate_format);

        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(ResultSuccess);
        rb.Push(server_id);
    }

    void ImportClientPki(HLERequestContext& ctx) {
        [[maybe_unused]] const auto pkcs_12_certificate = ctx.ReadBuffer(0);
        [[maybe_unused]] const auto ascii_password = [&ctx] {
            if (ctx.CanReadBuffer(1)) {
                return ctx.ReadBuffer(1);
            }

            return std::span<const u8>{};
        }();

        constexpr u64 client_id = 0;

        LOG_WARNING(Service_SSL, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(ResultSuccess);
        rb.Push(client_id);
    }
};

class ISslService final : public ServiceFramework<ISslService> {
public:
    explicit ISslService(Core::System& system_)
        : ServiceFramework{system_, "ssl"}, cert_store{system} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &ISslService::CreateContext, "CreateContext"},
            {1, nullptr, "GetContextCount"},
            {2, D<&ISslService::GetCertificates>, "GetCertificates"},
            {3, D<&ISslService::GetCertificateBufSize>, "GetCertificateBufSize"},
            {4, nullptr, "DebugIoctl"},
            {5, &ISslService::SetInterfaceVersion, "SetInterfaceVersion"},
            {6, nullptr, "FlushSessionCache"},
            {7, nullptr, "SetDebugOption"},
            {8, nullptr, "GetDebugOption"},
            {8, nullptr, "ClearTls12FallbackFlag"},
            {103, nullptr, "VerifySignature"} // 18.0.0+
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void CreateContext(HLERequestContext& ctx) {
        struct Parameters {
            SslVersion ssl_version;
            INSERT_PADDING_BYTES(0x4);
            u64 pid_placeholder;
        };
        static_assert(sizeof(Parameters) == 0x10, "Parameters is an invalid size");

        IPC::RequestParser rp{ctx};
        const auto parameters = rp.PopRaw<Parameters>();

        LOG_WARNING(Service_SSL, "(STUBBED) called, api_version={}, pid_placeholder={}",
                    parameters.ssl_version.api_version, parameters.pid_placeholder);

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<ISslContext>(system, parameters.ssl_version);
    }

    void SetInterfaceVersion(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        u32 ssl_version = rp.Pop<u32>();

        LOG_DEBUG(Service_SSL, "called, ssl_version={}", ssl_version);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    Result GetCertificateBufSize(
        Out<u32> out_size, InArray<CaCertificateId, BufferAttr_HipcMapAlias> certificate_ids) {
        LOG_INFO(Service_SSL, "called");
        u32 num_entries;
        R_RETURN(cert_store.GetCertificateBufSize(out_size, &num_entries, certificate_ids));
    }

    Result GetCertificates(Out<u32> out_num_entries, OutBuffer<BufferAttr_HipcMapAlias> out_buffer,
                           InArray<CaCertificateId, BufferAttr_HipcMapAlias> certificate_ids) {
        LOG_INFO(Service_SSL, "called");
        R_RETURN(cert_store.GetCertificates(out_num_entries, out_buffer, certificate_ids));
    }

private:
    CertStore cert_store;
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("ssl", std::make_shared<ISslService>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::SSL
