// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <mutex>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "common/fs/file.h"
#include "common/hex_util.h"
#include "common/string_util.h"

#include "core/hle/service/ssl/ssl_backend.h"
#include "core/internal_network/network.h"
#include "core/internal_network/sockets.h"

using namespace Common::FS;

namespace Service::SSL {

// Import OpenSSL's `SSL` type into the namespace.  This is needed because the
// namespace is also named `SSL`.
using ::SSL;

namespace {

std::once_flag one_time_init_flag;
bool one_time_init_success = false;

SSL_CTX* ssl_ctx;
IOFile key_log_file; // only open if SSLKEYLOGFILE set in environment
BIO_METHOD* bio_meth;

Result CheckOpenSSLErrors();
void OneTimeInit();
void OneTimeInitLogFile();
bool OneTimeInitBIO();

} // namespace

class SSLConnectionBackendOpenSSL final : public SSLConnectionBackend {
public:
    Result Init() {
        std::call_once(one_time_init_flag, OneTimeInit);

        if (!one_time_init_success) {
            LOG_ERROR(Service_SSL,
                      "Can't create SSL connection because OpenSSL one-time initialization failed");
            return ResultInternalError;
        }

        ssl = SSL_new(ssl_ctx);
        if (!ssl) {
            LOG_ERROR(Service_SSL, "SSL_new failed");
            return CheckOpenSSLErrors();
        }

        SSL_set_connect_state(ssl);

        bio = BIO_new(bio_meth);
        if (!bio) {
            LOG_ERROR(Service_SSL, "BIO_new failed");
            return CheckOpenSSLErrors();
        }

        BIO_set_data(bio, this);
        BIO_set_init(bio, 1);
        SSL_set_bio(ssl, bio, bio);

        return ResultSuccess;
    }

    void SetSocket(std::shared_ptr<Network::SocketBase> socket_in) override {
        socket = std::move(socket_in);
    }

    Result SetHostName(const std::string& hostname) override {
        if (!SSL_set1_host(ssl, hostname.c_str())) { // hostname for verification
            LOG_ERROR(Service_SSL, "SSL_set1_host({}) failed", hostname);
            return CheckOpenSSLErrors();
        }
        if (!SSL_set_tlsext_host_name(ssl, hostname.c_str())) { // hostname for SNI
            LOG_ERROR(Service_SSL, "SSL_set_tlsext_host_name({}) failed", hostname);
            return CheckOpenSSLErrors();
        }
        return ResultSuccess;
    }

    Result DoHandshake() override {
        SSL_set_verify_result(ssl, X509_V_OK);
        const int ret = SSL_do_handshake(ssl);
        const long verify_result = SSL_get_verify_result(ssl);
        if (verify_result != X509_V_OK) {
            LOG_ERROR(Service_SSL, "SSL cert verification failed because: {}",
                      X509_verify_cert_error_string(verify_result));
            return CheckOpenSSLErrors();
        }
        if (ret <= 0) {
            const int ssl_err = SSL_get_error(ssl, ret);
            if (ssl_err == SSL_ERROR_ZERO_RETURN ||
                (ssl_err == SSL_ERROR_SYSCALL && got_read_eof)) {
                LOG_ERROR(Service_SSL, "SSL handshake failed because server hung up");
                return ResultInternalError;
            }
        }
        return HandleReturn("SSL_do_handshake", 0, ret);
    }

    Result Read(size_t* out_size, std::span<u8> data) override {
        const int ret = SSL_read_ex(ssl, data.data(), data.size(), out_size);
        return HandleReturn("SSL_read_ex", out_size, ret);
    }

    Result Write(size_t* out_size, std::span<const u8> data) override {
        const int ret = SSL_write_ex(ssl, data.data(), data.size(), out_size);
        return HandleReturn("SSL_write_ex", out_size, ret);
    }

    Result HandleReturn(const char* what, size_t* actual, int ret) {
        const int ssl_err = SSL_get_error(ssl, ret);
        CheckOpenSSLErrors();
        switch (ssl_err) {
        case SSL_ERROR_NONE:
            return ResultSuccess;
        case SSL_ERROR_ZERO_RETURN:
            LOG_DEBUG(Service_SSL, "{} => SSL_ERROR_ZERO_RETURN", what);
            // DoHandshake special-cases this, but for Read and Write:
            *actual = 0;
            return ResultSuccess;
        case SSL_ERROR_WANT_READ:
            LOG_DEBUG(Service_SSL, "{} => SSL_ERROR_WANT_READ", what);
            return ResultWouldBlock;
        case SSL_ERROR_WANT_WRITE:
            LOG_DEBUG(Service_SSL, "{} => SSL_ERROR_WANT_WRITE", what);
            return ResultWouldBlock;
        default:
            if (ssl_err == SSL_ERROR_SYSCALL && got_read_eof) {
                LOG_DEBUG(Service_SSL, "{} => SSL_ERROR_SYSCALL because server hung up", what);
                *actual = 0;
                return ResultSuccess;
            }
            LOG_ERROR(Service_SSL, "{} => other SSL_get_error return value {}", what, ssl_err);
            return ResultInternalError;
        }
    }

    Result GetServerCerts(std::vector<std::vector<u8>>* out_certs) override {
        STACK_OF(X509)* chain = SSL_get_peer_cert_chain(ssl);
        if (!chain) {
            LOG_ERROR(Service_SSL, "SSL_get_peer_cert_chain returned nullptr");
            return ResultInternalError;
        }
        int count = sk_X509_num(chain);
        ASSERT(count >= 0);
        for (int i = 0; i < count; i++) {
            X509* x509 = sk_X509_value(chain, i);
            ASSERT_OR_EXECUTE(x509 != nullptr, { continue; });
            unsigned char* buf = nullptr;
            int len = i2d_X509(x509, &buf);
            ASSERT_OR_EXECUTE(len >= 0 && buf, { continue; });
            out_certs->emplace_back(buf, buf + len);
            OPENSSL_free(buf);
        }
        return ResultSuccess;
    }

    ~SSLConnectionBackendOpenSSL() {
        // this is null-tolerant:
        SSL_free(ssl);
    }

    static void KeyLogCallback(const SSL* ssl, const char* line) {
        std::string str(line);
        str.push_back('\n');
        // Do this in a single WriteString for atomicity if multiple instances
        // are running on different threads (though that can't currently
        // happen).
        if (key_log_file.WriteString(str) != str.size() || !key_log_file.Flush()) {
            LOG_CRITICAL(Service_SSL, "Failed to write to SSLKEYLOGFILE");
        }
        LOG_DEBUG(Service_SSL, "Wrote to SSLKEYLOGFILE: {}", line);
    }

    static int WriteCallback(BIO* bio, const char* buf, size_t len, size_t* actual_p) {
        auto self = static_cast<SSLConnectionBackendOpenSSL*>(BIO_get_data(bio));
        ASSERT_OR_EXECUTE_MSG(
            self->socket, { return 0; }, "OpenSSL asked to send but we have no socket");
        BIO_clear_retry_flags(bio);
        auto [actual, err] = self->socket->Send({reinterpret_cast<const u8*>(buf), len}, 0);
        switch (err) {
        case Network::Errno::SUCCESS:
            *actual_p = actual;
            return 1;
        case Network::Errno::AGAIN:
            BIO_set_flags(bio, BIO_FLAGS_WRITE | BIO_FLAGS_SHOULD_RETRY);
            return 0;
        default:
            LOG_ERROR(Service_SSL, "Socket send returned Network::Errno {}", err);
            return -1;
        }
    }

    static int ReadCallback(BIO* bio, char* buf, size_t len, size_t* actual_p) {
        auto self = static_cast<SSLConnectionBackendOpenSSL*>(BIO_get_data(bio));
        ASSERT_OR_EXECUTE_MSG(
            self->socket, { return 0; }, "OpenSSL asked to recv but we have no socket");
        BIO_clear_retry_flags(bio);
        auto [actual, err] = self->socket->Recv(0, {reinterpret_cast<u8*>(buf), len});
        switch (err) {
        case Network::Errno::SUCCESS:
            *actual_p = actual;
            if (actual == 0) {
                self->got_read_eof = true;
            }
            return actual ? 1 : 0;
        case Network::Errno::AGAIN:
            BIO_set_flags(bio, BIO_FLAGS_READ | BIO_FLAGS_SHOULD_RETRY);
            return 0;
        default:
            LOG_ERROR(Service_SSL, "Socket recv returned Network::Errno {}", err);
            return -1;
        }
    }

    static long CtrlCallback(BIO* bio, int cmd, long l_arg, void* p_arg) {
        switch (cmd) {
        case BIO_CTRL_FLUSH:
            // Nothing to flush.
            return 1;
        case BIO_CTRL_PUSH:
        case BIO_CTRL_POP:
#ifdef BIO_CTRL_GET_KTLS_SEND
        case BIO_CTRL_GET_KTLS_SEND:
        case BIO_CTRL_GET_KTLS_RECV:
#endif
            // We don't support these operations, but don't bother logging them
            // as they're nothing unusual.
            return 0;
        default:
            LOG_DEBUG(Service_SSL, "OpenSSL BIO got ctrl({}, {}, {})", cmd, l_arg, p_arg);
            return 0;
        }
    }

    SSL* ssl = nullptr;
    BIO* bio = nullptr;
    bool got_read_eof = false;

    std::shared_ptr<Network::SocketBase> socket;
};

Result CreateSSLConnectionBackend(std::unique_ptr<SSLConnectionBackend>* out_backend) {
    auto conn = std::make_unique<SSLConnectionBackendOpenSSL>();

    R_TRY(conn->Init());

    *out_backend = std::move(conn);
    return ResultSuccess;
}

namespace {

Result CheckOpenSSLErrors() {
    unsigned long rc;
    const char* file;
    int line;
    const char* func;
    const char* data;
    int flags;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    while ((rc = ERR_get_error_all(&file, &line, &func, &data, &flags)))
#else
    // Can't get function names from OpenSSL on this version, so use mine:
    func = __func__;
    while ((rc = ERR_get_error_line_data(&file, &line, &data, &flags)))
#endif
    {
        std::string msg;
        msg.resize(1024, '\0');
        ERR_error_string_n(rc, msg.data(), msg.size());
        msg.resize(strlen(msg.data()), '\0');
        if (flags & ERR_TXT_STRING) {
            msg.append(" | ");
            msg.append(data);
        }
        Common::Log::FmtLogMessage(Common::Log::Class::Service_SSL, Common::Log::Level::Error,
                                   Common::Log::TrimSourcePath(file), line, func, "OpenSSL: {}",
                                   msg);
    }
    return ResultInternalError;
}

void OneTimeInit() {
    ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!ssl_ctx) {
        LOG_ERROR(Service_SSL, "SSL_CTX_new failed");
        CheckOpenSSLErrors();
        return;
    }

    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, nullptr);

    if (!SSL_CTX_set_default_verify_paths(ssl_ctx)) {
        LOG_ERROR(Service_SSL, "SSL_CTX_set_default_verify_paths failed");
        CheckOpenSSLErrors();
        return;
    }

    OneTimeInitLogFile();

    if (!OneTimeInitBIO()) {
        return;
    }

    one_time_init_success = true;
}

void OneTimeInitLogFile() {
    const char* logfile = getenv("SSLKEYLOGFILE");
    if (logfile) {
        key_log_file.Open(logfile, FileAccessMode::Append, FileType::TextFile,
                          FileShareFlag::ShareWriteOnly);
        if (key_log_file.IsOpen()) {
            SSL_CTX_set_keylog_callback(ssl_ctx, &SSLConnectionBackendOpenSSL::KeyLogCallback);
        } else {
            LOG_CRITICAL(Service_SSL,
                         "SSLKEYLOGFILE was set but file could not be opened; not logging keys!");
        }
    }
}

bool OneTimeInitBIO() {
    bio_meth =
        BIO_meth_new(BIO_get_new_index() | BIO_TYPE_SOURCE_SINK, "SSLConnectionBackendOpenSSL");
    if (!bio_meth ||
        !BIO_meth_set_write_ex(bio_meth, &SSLConnectionBackendOpenSSL::WriteCallback) ||
        !BIO_meth_set_read_ex(bio_meth, &SSLConnectionBackendOpenSSL::ReadCallback) ||
        !BIO_meth_set_ctrl(bio_meth, &SSLConnectionBackendOpenSSL::CtrlCallback)) {
        LOG_ERROR(Service_SSL, "Failed to create BIO_METHOD");
        return false;
    }
    return true;
}

} // namespace

} // namespace Service::SSL
