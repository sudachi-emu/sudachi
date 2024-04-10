// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <mutex>

#include "common/error.h"
#include "common/fs/file.h"
#include "common/hex_util.h"
#include "common/string_util.h"

#include "core/hle/service/ssl/ssl_backend.h"
#include "core/internal_network/network.h"
#include "core/internal_network/sockets.h"

namespace {

// These includes are inside the namespace to avoid a conflict on MinGW where
// the headers define an enum containing Network and Service as enumerators
// (which clash with the correspondingly named namespaces).
#define SECURITY_WIN32
#include <schnlsp.h>
#include <security.h>
#include <wincrypt.h>

std::once_flag one_time_init_flag;
bool one_time_init_success = false;

SCHANNEL_CRED schannel_cred{};
CredHandle cred_handle;

static void OneTimeInit() {
    schannel_cred.dwVersion = SCHANNEL_CRED_VERSION;
    schannel_cred.dwFlags =
        SCH_USE_STRONG_CRYPTO |        // don't allow insecure protocols
        SCH_CRED_NO_SERVERNAME_CHECK | // don't validate server names
        SCH_CRED_NO_DEFAULT_CREDS;     // don't automatically present a client certificate
    // ^ I'm assuming that nobody would want to connect Sudachi to a
    // service that requires some OS-provided corporate client
    // certificate, and presenting one to some arbitrary server
    // might be a privacy concern?  Who knows, though.

    const SECURITY_STATUS ret =
        AcquireCredentialsHandle(nullptr, const_cast<LPTSTR>(UNISP_NAME), SECPKG_CRED_OUTBOUND,
                                 nullptr, &schannel_cred, nullptr, nullptr, &cred_handle, nullptr);
    if (ret != SEC_E_OK) {
        // SECURITY_STATUS codes are a type of HRESULT and can be used with NativeErrorToString.
        LOG_ERROR(Service_SSL, "AcquireCredentialsHandle failed: {}",
                  Common::NativeErrorToString(ret));
        return;
    }

    if (getenv("SSLKEYLOGFILE")) {
        LOG_CRITICAL(Service_SSL, "SSLKEYLOGFILE was set but Schannel does not support exporting "
                                  "keys; not logging keys!");
        // Not fatal.
    }

    one_time_init_success = true;
}

} // namespace

namespace Service::SSL {

class SSLConnectionBackendSchannel final : public SSLConnectionBackend {
public:
    Result Init() {
        std::call_once(one_time_init_flag, OneTimeInit);

        if (!one_time_init_success) {
            LOG_ERROR(
                Service_SSL,
                "Can't create SSL connection because Schannel one-time initialization failed");
            return ResultInternalError;
        }

        return ResultSuccess;
    }

    void SetSocket(std::shared_ptr<Network::SocketBase> socket_in) override {
        socket = std::move(socket_in);
    }

    Result SetHostName(const std::string& hostname_in) override {
        hostname = hostname_in;
        return ResultSuccess;
    }

    Result DoHandshake() override {
        while (1) {
            Result r;
            switch (handshake_state) {
            case HandshakeState::Initial:
                if ((r = FlushCiphertextWriteBuf()) != ResultSuccess ||
                    (r = CallInitializeSecurityContext()) != ResultSuccess) {
                    return r;
                }
                // CallInitializeSecurityContext updated `handshake_state`.
                continue;
            case HandshakeState::ContinueNeeded:
            case HandshakeState::IncompleteMessage:
                if ((r = FlushCiphertextWriteBuf()) != ResultSuccess ||
                    (r = FillCiphertextReadBuf()) != ResultSuccess) {
                    return r;
                }
                if (ciphertext_read_buf.empty()) {
                    LOG_ERROR(Service_SSL, "SSL handshake failed because server hung up");
                    return ResultInternalError;
                }
                if ((r = CallInitializeSecurityContext()) != ResultSuccess) {
                    return r;
                }
                // CallInitializeSecurityContext updated `handshake_state`.
                continue;
            case HandshakeState::DoneAfterFlush:
                if ((r = FlushCiphertextWriteBuf()) != ResultSuccess) {
                    return r;
                }
                handshake_state = HandshakeState::Connected;
                return ResultSuccess;
            case HandshakeState::Connected:
                LOG_ERROR(Service_SSL, "Called DoHandshake but we already handshook");
                return ResultInternalError;
            case HandshakeState::Error:
                return ResultInternalError;
            }
        }
    }

    Result FillCiphertextReadBuf() {
        const size_t fill_size = read_buf_fill_size ? read_buf_fill_size : 4096;
        read_buf_fill_size = 0;
        // This unnecessarily zeroes the buffer; oh well.
        const size_t offset = ciphertext_read_buf.size();
        ASSERT_OR_EXECUTE(offset + fill_size >= offset, { return ResultInternalError; });
        ciphertext_read_buf.resize(offset + fill_size, 0);
        const auto read_span = std::span(ciphertext_read_buf).subspan(offset, fill_size);
        const auto [actual, err] = socket->Recv(0, read_span);
        switch (err) {
        case Network::Errno::SUCCESS:
            ASSERT(static_cast<size_t>(actual) <= fill_size);
            ciphertext_read_buf.resize(offset + actual);
            return ResultSuccess;
        case Network::Errno::AGAIN:
            ciphertext_read_buf.resize(offset);
            return ResultWouldBlock;
        default:
            ciphertext_read_buf.resize(offset);
            LOG_ERROR(Service_SSL, "Socket recv returned Network::Errno {}", err);
            return ResultInternalError;
        }
    }

    // Returns success if the write buffer has been completely emptied.
    Result FlushCiphertextWriteBuf() {
        while (!ciphertext_write_buf.empty()) {
            const auto [actual, err] = socket->Send(ciphertext_write_buf, 0);
            switch (err) {
            case Network::Errno::SUCCESS:
                ASSERT(static_cast<size_t>(actual) <= ciphertext_write_buf.size());
                ciphertext_write_buf.erase(ciphertext_write_buf.begin(),
                                           ciphertext_write_buf.begin() + actual);
                break;
            case Network::Errno::AGAIN:
                return ResultWouldBlock;
            default:
                LOG_ERROR(Service_SSL, "Socket send returned Network::Errno {}", err);
                return ResultInternalError;
            }
        }
        return ResultSuccess;
    }

    Result CallInitializeSecurityContext() {
        const unsigned long req = ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY |
                                  ISC_REQ_INTEGRITY | ISC_REQ_REPLAY_DETECT |
                                  ISC_REQ_SEQUENCE_DETECT | ISC_REQ_STREAM |
                                  ISC_REQ_USE_SUPPLIED_CREDS;
        unsigned long attr;
        // https://learn.microsoft.com/en-us/windows/win32/secauthn/initializesecuritycontext--schannel
        std::array<SecBuffer, 2> input_buffers{{
            // only used if `initial_call_done`
            {
                // [0]
                .cbBuffer = static_cast<unsigned long>(ciphertext_read_buf.size()),
                .BufferType = SECBUFFER_TOKEN,
                .pvBuffer = ciphertext_read_buf.data(),
            },
            {
                // [1] (will be replaced by SECBUFFER_MISSING when SEC_E_INCOMPLETE_MESSAGE is
                //     returned, or SECBUFFER_EXTRA when SEC_E_CONTINUE_NEEDED is returned if the
                //     whole buffer wasn't used)
                .cbBuffer = 0,
                .BufferType = SECBUFFER_EMPTY,
                .pvBuffer = nullptr,
            },
        }};
        std::array<SecBuffer, 2> output_buffers{{
            {
                .cbBuffer = 0,
                .BufferType = SECBUFFER_TOKEN,
                .pvBuffer = nullptr,
            }, // [0]
            {
                .cbBuffer = 0,
                .BufferType = SECBUFFER_ALERT,
                .pvBuffer = nullptr,
            }, // [1]
        }};
        SecBufferDesc input_desc{
            .ulVersion = SECBUFFER_VERSION,
            .cBuffers = static_cast<unsigned long>(input_buffers.size()),
            .pBuffers = input_buffers.data(),
        };
        SecBufferDesc output_desc{
            .ulVersion = SECBUFFER_VERSION,
            .cBuffers = static_cast<unsigned long>(output_buffers.size()),
            .pBuffers = output_buffers.data(),
        };
        ASSERT_OR_EXECUTE_MSG(
            input_buffers[0].cbBuffer == ciphertext_read_buf.size(),
            { return ResultInternalError; }, "read buffer too large");

        bool initial_call_done = handshake_state != HandshakeState::Initial;
        if (initial_call_done) {
            LOG_DEBUG(Service_SSL, "Passing {} bytes into InitializeSecurityContext",
                      ciphertext_read_buf.size());
        }

        char* hostname_ptr = hostname ? const_cast<char*>(hostname->c_str()) : nullptr;
        const SECURITY_STATUS ret = InitializeSecurityContextA(
            &cred_handle, initial_call_done ? &ctxt : nullptr, hostname_ptr, req,
            0, // Reserved1
            0, // TargetDataRep not used with Schannel
            initial_call_done ? &input_desc : nullptr,
            0, // Reserved2
            initial_call_done ? nullptr : &ctxt, &output_desc, &attr,
            nullptr); // ptsExpiry

        if (output_buffers[0].pvBuffer) {
            const std::span span(static_cast<u8*>(output_buffers[0].pvBuffer),
                                 output_buffers[0].cbBuffer);
            ciphertext_write_buf.insert(ciphertext_write_buf.end(), span.begin(), span.end());
            FreeContextBuffer(output_buffers[0].pvBuffer);
        }

        if (output_buffers[1].pvBuffer) {
            const std::span span(static_cast<u8*>(output_buffers[1].pvBuffer),
                                 output_buffers[1].cbBuffer);
            // The documentation doesn't explain what format this data is in.
            LOG_DEBUG(Service_SSL, "Got a {}-byte alert buffer: {}", span.size(),
                      Common::HexToString(span));
        }

        switch (ret) {
        case SEC_I_CONTINUE_NEEDED:
            LOG_DEBUG(Service_SSL, "InitializeSecurityContext => SEC_I_CONTINUE_NEEDED");
            if (input_buffers[1].BufferType == SECBUFFER_EXTRA) {
                LOG_DEBUG(Service_SSL, "EXTRA of size {}", input_buffers[1].cbBuffer);
                ASSERT(input_buffers[1].cbBuffer <= ciphertext_read_buf.size());
                ciphertext_read_buf.erase(ciphertext_read_buf.begin(),
                                          ciphertext_read_buf.end() - input_buffers[1].cbBuffer);
            } else {
                ASSERT(input_buffers[1].BufferType == SECBUFFER_EMPTY);
                ciphertext_read_buf.clear();
            }
            handshake_state = HandshakeState::ContinueNeeded;
            return ResultSuccess;
        case SEC_E_INCOMPLETE_MESSAGE:
            LOG_DEBUG(Service_SSL, "InitializeSecurityContext => SEC_E_INCOMPLETE_MESSAGE");
            ASSERT(input_buffers[1].BufferType == SECBUFFER_MISSING);
            read_buf_fill_size = input_buffers[1].cbBuffer;
            handshake_state = HandshakeState::IncompleteMessage;
            return ResultSuccess;
        case SEC_E_OK:
            LOG_DEBUG(Service_SSL, "InitializeSecurityContext => SEC_E_OK");
            ciphertext_read_buf.clear();
            handshake_state = HandshakeState::DoneAfterFlush;
            return GrabStreamSizes();
        default:
            LOG_ERROR(Service_SSL,
                      "InitializeSecurityContext failed (probably certificate/protocol issue): {}",
                      Common::NativeErrorToString(ret));
            handshake_state = HandshakeState::Error;
            return ResultInternalError;
        }
    }

    Result GrabStreamSizes() {
        const SECURITY_STATUS ret =
            QueryContextAttributes(&ctxt, SECPKG_ATTR_STREAM_SIZES, &stream_sizes);
        if (ret != SEC_E_OK) {
            LOG_ERROR(Service_SSL, "QueryContextAttributes(SECPKG_ATTR_STREAM_SIZES) failed: {}",
                      Common::NativeErrorToString(ret));
            handshake_state = HandshakeState::Error;
            return ResultInternalError;
        }
        return ResultSuccess;
    }

    Result Read(size_t* out_size, std::span<u8> data) override {
        *out_size = 0;
        if (handshake_state != HandshakeState::Connected) {
            LOG_ERROR(Service_SSL, "Called Read but we did not successfully handshake");
            return ResultInternalError;
        }
        if (data.size() == 0 || got_read_eof) {
            return ResultSuccess;
        }
        while (1) {
            if (!cleartext_read_buf.empty()) {
                *out_size = std::min(cleartext_read_buf.size(), data.size());
                std::memcpy(data.data(), cleartext_read_buf.data(), *out_size);
                cleartext_read_buf.erase(cleartext_read_buf.begin(),
                                         cleartext_read_buf.begin() + *out_size);
                return ResultSuccess;
            }
            if (!ciphertext_read_buf.empty()) {
                SecBuffer empty{
                    .cbBuffer = 0,
                    .BufferType = SECBUFFER_EMPTY,
                    .pvBuffer = nullptr,
                };
                std::array<SecBuffer, 5> buffers{{
                    {
                        .cbBuffer = static_cast<unsigned long>(ciphertext_read_buf.size()),
                        .BufferType = SECBUFFER_DATA,
                        .pvBuffer = ciphertext_read_buf.data(),
                    },
                    empty,
                    empty,
                    empty,
                }};
                ASSERT_OR_EXECUTE_MSG(
                    buffers[0].cbBuffer == ciphertext_read_buf.size(),
                    { return ResultInternalError; }, "read buffer too large");
                SecBufferDesc desc{
                    .ulVersion = SECBUFFER_VERSION,
                    .cBuffers = static_cast<unsigned long>(buffers.size()),
                    .pBuffers = buffers.data(),
                };
                SECURITY_STATUS ret =
                    DecryptMessage(&ctxt, &desc, /*MessageSeqNo*/ 0, /*pfQOP*/ nullptr);
                switch (ret) {
                case SEC_E_OK:
                    ASSERT_OR_EXECUTE(buffers[0].BufferType == SECBUFFER_STREAM_HEADER,
                                      { return ResultInternalError; });
                    ASSERT_OR_EXECUTE(buffers[1].BufferType == SECBUFFER_DATA,
                                      { return ResultInternalError; });
                    ASSERT_OR_EXECUTE(buffers[2].BufferType == SECBUFFER_STREAM_TRAILER,
                                      { return ResultInternalError; });
                    cleartext_read_buf.assign(static_cast<u8*>(buffers[1].pvBuffer),
                                              static_cast<u8*>(buffers[1].pvBuffer) +
                                                  buffers[1].cbBuffer);
                    if (buffers[3].BufferType == SECBUFFER_EXTRA) {
                        ASSERT(buffers[3].cbBuffer <= ciphertext_read_buf.size());
                        ciphertext_read_buf.erase(ciphertext_read_buf.begin(),
                                                  ciphertext_read_buf.end() - buffers[3].cbBuffer);
                    } else {
                        ASSERT(buffers[3].BufferType == SECBUFFER_EMPTY);
                        ciphertext_read_buf.clear();
                    }
                    continue;
                case SEC_E_INCOMPLETE_MESSAGE:
                    break;
                case SEC_I_CONTEXT_EXPIRED:
                    // Server hung up by sending close_notify.
                    got_read_eof = true;
                    *out_size = 0;
                    return ResultSuccess;
                default:
                    LOG_ERROR(Service_SSL, "DecryptMessage failed: {}",
                              Common::NativeErrorToString(ret));
                    return ResultInternalError;
                }
            }
            const Result r = FillCiphertextReadBuf();
            if (r != ResultSuccess) {
                return r;
            }
            if (ciphertext_read_buf.empty()) {
                got_read_eof = true;
                *out_size = 0;
                return ResultSuccess;
            }
        }
    }

    Result Write(size_t* out_size, std::span<const u8> data) override {
        *out_size = 0;

        if (handshake_state != HandshakeState::Connected) {
            LOG_ERROR(Service_SSL, "Called Write but we did not successfully handshake");
            return ResultInternalError;
        }
        if (data.size() == 0) {
            return ResultSuccess;
        }
        data = data.subspan(0, std::min<size_t>(data.size(), stream_sizes.cbMaximumMessage));
        if (!cleartext_write_buf.empty()) {
            // Already in the middle of a write.  It wouldn't make sense to not
            // finish sending the entire buffer since TLS has
            // header/MAC/padding/etc.
            if (data.size() != cleartext_write_buf.size() ||
                std::memcmp(data.data(), cleartext_write_buf.data(), data.size())) {
                LOG_ERROR(Service_SSL, "Called Write but buffer does not match previous buffer");
                return ResultInternalError;
            }
            return WriteAlreadyEncryptedData(out_size);
        } else {
            cleartext_write_buf.assign(data.begin(), data.end());
        }

        std::vector<u8> header_buf(stream_sizes.cbHeader, 0);
        std::vector<u8> tmp_data_buf = cleartext_write_buf;
        std::vector<u8> trailer_buf(stream_sizes.cbTrailer, 0);

        std::array<SecBuffer, 3> buffers{{
            {
                .cbBuffer = stream_sizes.cbHeader,
                .BufferType = SECBUFFER_STREAM_HEADER,
                .pvBuffer = header_buf.data(),
            },
            {
                .cbBuffer = static_cast<unsigned long>(tmp_data_buf.size()),
                .BufferType = SECBUFFER_DATA,
                .pvBuffer = tmp_data_buf.data(),
            },
            {
                .cbBuffer = stream_sizes.cbTrailer,
                .BufferType = SECBUFFER_STREAM_TRAILER,
                .pvBuffer = trailer_buf.data(),
            },
        }};
        ASSERT_OR_EXECUTE_MSG(
            buffers[1].cbBuffer == tmp_data_buf.size(), { return ResultInternalError; },
            "temp buffer too large");
        SecBufferDesc desc{
            .ulVersion = SECBUFFER_VERSION,
            .cBuffers = static_cast<unsigned long>(buffers.size()),
            .pBuffers = buffers.data(),
        };

        const SECURITY_STATUS ret = EncryptMessage(&ctxt, /*fQOP*/ 0, &desc, /*MessageSeqNo*/ 0);
        if (ret != SEC_E_OK) {
            LOG_ERROR(Service_SSL, "EncryptMessage failed: {}", Common::NativeErrorToString(ret));
            return ResultInternalError;
        }
        ciphertext_write_buf.insert(ciphertext_write_buf.end(), header_buf.begin(),
                                    header_buf.end());
        ciphertext_write_buf.insert(ciphertext_write_buf.end(), tmp_data_buf.begin(),
                                    tmp_data_buf.end());
        ciphertext_write_buf.insert(ciphertext_write_buf.end(), trailer_buf.begin(),
                                    trailer_buf.end());
        return WriteAlreadyEncryptedData(out_size);
    }

    Result WriteAlreadyEncryptedData(size_t* out_size) {
        const Result r = FlushCiphertextWriteBuf();
        if (r != ResultSuccess) {
            return r;
        }
        // write buf is empty
        *out_size = cleartext_write_buf.size();
        cleartext_write_buf.clear();
        return ResultSuccess;
    }

    Result GetServerCerts(std::vector<std::vector<u8>>* out_certs) override {
        PCCERT_CONTEXT returned_cert = nullptr;
        const SECURITY_STATUS ret =
            QueryContextAttributes(&ctxt, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &returned_cert);
        if (ret != SEC_E_OK) {
            LOG_ERROR(Service_SSL,
                      "QueryContextAttributes(SECPKG_ATTR_REMOTE_CERT_CONTEXT) failed: {}",
                      Common::NativeErrorToString(ret));
            return ResultInternalError;
        }
        PCCERT_CONTEXT some_cert = nullptr;
        while ((some_cert = CertEnumCertificatesInStore(returned_cert->hCertStore, some_cert)) !=
               nullptr) {
            out_certs->emplace_back(static_cast<u8*>(some_cert->pbCertEncoded),
                                    static_cast<u8*>(some_cert->pbCertEncoded) +
                                        some_cert->cbCertEncoded);
        }
        std::reverse(out_certs->begin(),
                     out_certs->end()); // Windows returns certs in reverse order from what we want
        CertFreeCertificateContext(returned_cert);
        return ResultSuccess;
    }

    ~SSLConnectionBackendSchannel() {
        if (handshake_state != HandshakeState::Initial) {
            DeleteSecurityContext(&ctxt);
        }
    }

    enum class HandshakeState {
        // Haven't called anything yet.
        Initial,
        // `SEC_I_CONTINUE_NEEDED` was returned by
        // `InitializeSecurityContext`; must finish sending data (if any) in
        // the write buffer, then read at least one byte before calling
        // `InitializeSecurityContext` again.
        ContinueNeeded,
        // `SEC_E_INCOMPLETE_MESSAGE` was returned by
        // `InitializeSecurityContext`; hopefully the write buffer is empty;
        // must read at least one byte before calling
        // `InitializeSecurityContext` again.
        IncompleteMessage,
        // `SEC_E_OK` was returned by `InitializeSecurityContext`; must
        // finish sending data in the write buffer before having `DoHandshake`
        // report success.
        DoneAfterFlush,
        // We finished the above and are now connected.  At this point, writing
        // and reading are separate 'state machines' represented by the
        // nonemptiness of the ciphertext and cleartext read and write buffers.
        Connected,
        // Another error was returned and we shouldn't allow initialization
        // to continue.
        Error,
    } handshake_state = HandshakeState::Initial;

    CtxtHandle ctxt;
    SecPkgContext_StreamSizes stream_sizes;

    std::shared_ptr<Network::SocketBase> socket;
    std::optional<std::string> hostname;

    std::vector<u8> ciphertext_read_buf;
    std::vector<u8> ciphertext_write_buf;
    std::vector<u8> cleartext_read_buf;
    std::vector<u8> cleartext_write_buf;

    bool got_read_eof = false;
    size_t read_buf_fill_size = 0;
};

Result CreateSSLConnectionBackend(std::unique_ptr<SSLConnectionBackend>* out_backend) {
    auto conn = std::make_unique<SSLConnectionBackendSchannel>();

    R_TRY(conn->Init());

    *out_backend = std::move(conn);
    return ResultSuccess;
}

} // namespace Service::SSL
