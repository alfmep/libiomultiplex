/*
 * Copyright (C) 2021,2022 Dan Arrhenius <dan@ultramarin.se>
 *
 * This file is part of libiomultiplex
 *
 * libiomultiplex is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <iomultiplex/TlsAdapter.hpp>
#include <iomultiplex/iohandler_base.hpp>
#include <iomultiplex/Log.hpp>
#include <cstring>
#include <cerrno>
#include <openssl/err.h>


//#define TRACE_DEBUG

#ifdef TRACE_DEBUG
#include <sys/types.h>
#define TRACE(format, ...) do{ \
    auto save_errno=errno; \
    Log::debug("[%u] %s:%s:%d: " format, gettid(), __FILE__, __FUNCTION__, __LINE__, ## __VA_ARGS__); \
    errno=save_errno; \
}while(false)
#else
#define TRACE(format, ...)
#endif



namespace iomultiplex {


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    TlsAdapter::TlsAdapter (Connection& conn, bool close_on_destruct)
        : Adapter (conn, close_on_destruct),
          tls_ctx (nullptr),
          tls (nullptr),
          mem_bio (nullptr),
          fd_bio (nullptr),
          tls_started (false),
          tls_active (false)
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    TlsAdapter::TlsAdapter (std::shared_ptr<Connection> conn_ptr)
        : Adapter (conn_ptr),
          tls_ctx (nullptr),
          tls (nullptr),
          mem_bio (nullptr),
          fd_bio (nullptr),
          tls_started (false),
          tls_active (false)
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    TlsAdapter::~TlsAdapter ()
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void TlsAdapter::close ()
    {
        if (tls_active) {
            TRACE ("Shutdown TLS");
            cancel ();
            SSL_shutdown (tls);
        }
        // Release TLS resources
        if (mem_bio) {
            if (tls && SSL_get_rbio(tls) == mem_bio)
                SSL_set0_rbio (tls, fd_bio);
            else
                BIO_free (mem_bio);
            mem_bio_buf.reset ();
            mem_bio = nullptr;
            fd_bio = nullptr;
        }
        if (tls)
            SSL_free (tls);
        if (tls_ctx)
            SSL_CTX_free (tls_ctx);
        tls         = nullptr;
        tls_ctx     = nullptr;
        tls_active  = false;
        tls_started = false;

        ERR_clear_error ();
        Adapter::close ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool TlsAdapter::is_tls_active () const
    {
        return tls_started || tls_active;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    unsigned long TlsAdapter::last_error () const
    {
        return ERR_peek_last_error ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::string TlsAdapter::last_error_msg () const
    {
        const char* err_str = ERR_reason_error_string (ERR_peek_last_error());
        return std::string (err_str ? err_str : "");
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static int set_tls_cert_files (SSL_CTX* ctx, const TlsConfig& cfg)
    {
        if (!cfg.ca_file.empty()) {
            if (SSL_CTX_load_verify_locations(ctx, cfg.ca_file.c_str(), nullptr) == 0) {
                TRACE ("Error setting TLS CA file");
                return -1;
            }
        }
        if (!cfg.cert_file.empty()) {
            if (SSL_CTX_use_certificate_file(ctx, cfg.cert_file.c_str(), SSL_FILETYPE_PEM) == 0) {
                TRACE ("Error setting TLS certificate file");
                return -1;
            }
        }
        if (!cfg.privkey_file.empty()) {
            if (SSL_CTX_use_PrivateKey_file(ctx, cfg.privkey_file.c_str(), SSL_FILETYPE_PEM) == 0) {
                TRACE ("Error setting TLS private key file");
                return -1;
            }
        }
        return 0;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static int set_tls_ciphers (SSL_CTX* ctx, const TlsConfig& cfg)
    {
        if (!cfg.cipher_list.empty()) {
            if (SSL_CTX_set_cipher_list(ctx, cfg.cipher_list.c_str()) == 0) {
                TRACE ("Error setting TLS cipher list");
                return -1;
            }
        }
        if (!cfg.cipher_list.empty()) {
            if (SSL_CTX_set_cipher_list(ctx, cfg.cipher_list.c_str()) == 0) { // TLSv1.2 and below
                TRACE ("Error setting TLS cipher list");
                return -1;
            }
        }
        return 0;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static SSL_CTX* configure_tls (const TlsConfig& tls_cfg, bool is_server, bool use_dtls)
    {
        SSL_CTX* tls_ctx {nullptr};

        // Set the TLS method and allowed versions
        //
        if (use_dtls) {
            // DTLS
            tls_ctx = SSL_CTX_new (is_server ? DTLS_server_method() : DTLS_client_method());
            SSL_CTX_set_min_proto_version (tls_ctx, tls_cfg.min_dtls_ver);
            SSL_CTX_set_max_proto_version (tls_ctx, tls_cfg.max_dtls_ver);
        }else{
            // TLS
            tls_ctx = SSL_CTX_new (is_server ? TLS_server_method() : TLS_client_method());
            SSL_CTX_set_min_proto_version (tls_ctx, tls_cfg.min_tls_ver);
            SSL_CTX_set_max_proto_version (tls_ctx, tls_cfg.max_tls_ver);
        }
        if (tls_ctx == nullptr) {
            TRACE ("Error creating the TLS context");
            return nullptr;
        }

        // Configure certificate files
        if (set_tls_cert_files(tls_ctx, tls_cfg)) {
            SSL_CTX_free (tls_ctx);
            return nullptr;
        }

        // Configure ciphers
        if (set_tls_ciphers(tls_ctx, tls_cfg)) {
            SSL_CTX_free (tls_ctx);
            return nullptr;
        }

        // Set peer certificate verification
        if (tls_cfg.verify_peer)
            SSL_CTX_set_verify (tls_ctx,
                                SSL_VERIFY_PEER | (is_server?SSL_VERIFY_FAIL_IF_NO_PEER_CERT:0),
                                nullptr);
        else
            SSL_CTX_set_verify (tls_ctx,
                                SSL_VERIFY_NONE,
                                nullptr);

        return tls_ctx;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int TlsAdapter::start_tls (const TlsConfig& tls_config,
                               bool is_server,
                               bool use_dtls,
                               void* buf,
                               size_t buf_len,
                               tls_handshake_cb_t callback,
                               unsigned timeout)
    {
        // Sanity check
        if (handle() < 0) {
            errno = EBADF;
            return -1;
        }

        // TLS already active or pending ?
        if (tls_started.exchange(true)) {
            TRACE ("Can't start TLS - already in progress");
            errno = EINPROGRESS;
            return -1; // TLS already active or pending.
        }

        ERR_clear_error ();

        TRACE ("Starting TLS %s handshake on file handle %d",
               (is_server ? "server" : "client"), handle());

        // Configure TLS context
        tls_ctx = configure_tls (tls_config, is_server, use_dtls);
        if (!tls_ctx) {
            tls_ctx     = nullptr;
            tls_active  = false;
            tls_started = false;
            TRACE ("File handle %d failed to configure TLS", handle());
            errno = EINVAL;
            return -1;
        }

        // Create and initialize an SSL structure
        tls = SSL_new (tls_ctx);
        if (!tls || SSL_set_fd(tls, handle())==0) {
            TRACE ("File Handle %d failed to initialize an SSL structure", handle());
            if (tls)
                SSL_free (tls);
            SSL_CTX_free (tls_ctx);
            tls         = nullptr;
            tls_ctx     = nullptr;
            tls_active  = false;
            tls_started = false;
            errno       = EINVAL;
            return -1;
        }

        // Initiate the TLS handshake
        //
        int result;
        errno = 0;
        if (is_server) {
            if (buf && buf_len) {
                // We have an initial RX buffer and is starting a server handshake
                TRACE ("Start TLS server handshake on file handle %d with pre-read buffer, size: %lu",
                       handle(), buf_len);
                result = initiate_tls_server_handshake (buf, buf_len, callback, timeout);
            }else{
                result = wait_for_rx ([this, callback, timeout](io_result_t& ior)->bool{
                                          return handle_tls_handshake (true, callback, timeout, ior.errnum);
                                      }, timeout);
            }
        }else{
            result = wait_for_tx ([this, callback, timeout](io_result_t& ior)->bool{
                                      return handle_tls_handshake (false, callback, timeout, ior.errnum);
                                  }, timeout);
        }
        if (result) {
#ifdef TRACE_DEBUG
            auto errnum = errno;
            TRACE ("TLS handshake failed for file handle %d: %s", handle(), strerror(errnum));
            errno = errnum;
#endif
            if (mem_bio) {
                if (SSL_get_rbio(tls) == mem_bio)
                    SSL_set0_rbio (tls, fd_bio);
                else
                    BIO_free (mem_bio);
                mem_bio = nullptr;
            }
            mem_bio_buf.reset ();
            fd_bio = nullptr;
            SSL_free (tls);
            SSL_CTX_free (tls_ctx);
            tls         = nullptr;
            tls_ctx     = nullptr;
            tls_active  = false;
            tls_started = false;
        }

        return result;
    }


    //--------------------------------------------------------------------------
    // Synchronized operation
    // (assumes the I/O handler running in another thread)
    //--------------------------------------------------------------------------
    int TlsAdapter::start_tls (const TlsConfig& tls_config,
                               bool is_server,
                               bool use_dtls,
                               void* buf,
                               size_t buf_len,
                               unsigned timeout)
    {
        if (io_handler().same_context()) {
            errno = EDEADLK;
            return -1;
        }
        bool io_done = false;
        int errnum = 0;
        int retval = -1;
        // Initiate TLS handshake
        if (start_tls(tls_config,
                      is_server,
                      use_dtls,
                      buf,
                      buf_len,
                      [this, &errnum, &io_done](Connection& conn, int err, const std::string& errstr){
                          std::unique_lock<std::mutex> lock (sync_mutex);
                          io_done = true;
                          errnum = err;
                          sync_cond.notify_one ();
                      },
                      timeout) == 0)
        {
            // Wait for TLS handshake to finish, fail, or timeout
            std::unique_lock<std::mutex> lock (sync_mutex);
            sync_cond.wait (lock, [&io_done]{return io_done;});
            if (errnum) {
                cancel ();
                if (tls)
                    SSL_free (tls);
                if (tls_ctx)
                    SSL_CTX_free (tls_ctx);
                tls         = nullptr;
                tls_ctx     = nullptr;
                tls_active  = false;
                tls_started = false;
            }else{
                retval = 0;
            }
            errno = errnum;
        }
        return retval;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int TlsAdapter::initiate_tls_server_handshake (void* buf,
                                                   size_t buf_len,
                                                   tls_handshake_cb_t cb,
                                                   unsigned timeout)
    {
        int retval = 0;

        // Save the original fd BIO
        fd_bio = SSL_get_rbio (tls);
        if (!fd_bio) {
            errno = EINVAL;
            return -1;
        }

        // Create and initialize a memory BIO using the supplied buffer
        mem_bio_buf.reset (new char[buf_len]);
        mem_bio = BIO_new_mem_buf (mem_bio_buf.get(), buf_len);
        if (!mem_bio) {
            mem_bio_buf.reset ();
            errno = ENOMEM;
            return -1;
        }

        // Use the memory BIO instead of the fd BIO
        memcpy (mem_bio_buf.get(), buf, buf_len);
        BIO_up_ref (fd_bio);
        BIO_set_mem_eof_return (mem_bio, -1);
        SSL_set0_rbio (tls, mem_bio);

        ERR_clear_error ();

        // Start TLS server handshake
        auto ret = SSL_accept (tls);
        auto tls_error = SSL_get_error (tls, ret);

        switch (tls_error) {
        case SSL_ERROR_NONE:
            // Successful handshake after only initial RX data ?!?
            // This should never happen when we start a server handshake,
            // but we handle it anyway
            SSL_set0_rbio (tls, fd_bio);
            fd_bio = nullptr;
            mem_bio = nullptr;
            memset (mem_bio_buf.get(), 0xff, buf_len);
            mem_bio_buf.reset ();
            TRACE ("TLS handshake success for file handle %d after only initial RX buffer data", handle());
            tls_active = true;
            if (cb) {
                retval = wait_for_tx ([this, cb](io_result_t& ior)->bool{
                                          cb (*this, 0, "");
                                          return false;
                                      }, timeout);
            }
            break;

        case SSL_ERROR_WANT_READ:
            TRACE ("SSL_ERROR_WANT_READ");
            SSL_set0_rbio (tls, fd_bio);
            fd_bio = nullptr;
            mem_bio = nullptr;
            memset (mem_bio_buf.get(), 0xff, buf_len);
            mem_bio_buf.reset ();
            retval = wait_for_rx ([this, cb, timeout](io_result_t& ior)->bool{
                                      return handle_tls_handshake (true, cb, timeout, ior.errnum);
                                  }, timeout);
            break;

        case SSL_ERROR_WANT_WRITE:
            TRACE ("SSL_ERROR_WANT_WRITE");
            retval = wait_for_tx ([this, cb, timeout](io_result_t& ior)->bool{
                                      return handle_tls_handshake (true, cb, timeout, ior.errnum);
                                  }, timeout);
            break;

        case SSL_ERROR_SYSCALL:
            TRACE ("SSL_ERROR_SYSCALL: %s", ERR_reason_error_string(ERR_get_error()));
            retval = -1;
            break;

        case SSL_ERROR_SSL:
            TRACE ("SSL_ERROR_SSL: %s", ERR_reason_error_string(ERR_get_error()));
        default:
            errno = ECONNREFUSED;
            retval = -1;
        }

        return retval;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool TlsAdapter::handle_tls_handshake (bool is_server,
                                           tls_handshake_cb_t cb,
                                           unsigned timeout,
                                           int errnum)
    {
        const char* err_str {""};
        int tls_error {0};
        int ret {0};

        if (!errnum) {
            ERR_clear_error ();
            if (is_server)
                ret = SSL_accept (tls);
            else
                ret = SSL_connect (tls);
            tls_error = SSL_get_error (tls, ret);

            switch (tls_error) {
            case SSL_ERROR_NONE:
                break;

            case SSL_ERROR_WANT_READ:
                if (mem_bio) {
                    // TLS RX pre-buffer empty, read from file handle
                    SSL_set0_rbio (tls, fd_bio);
                    fd_bio = nullptr;
                    mem_bio = nullptr;
                    mem_bio_buf.reset ();
                }
                wait_for_rx ([this, is_server, cb, timeout](io_result_t& ior)->bool{
                        return handle_tls_handshake (is_server, cb, timeout, ior.errnum);
                    }, timeout);
                return false;

            case SSL_ERROR_WANT_WRITE:
                wait_for_tx ([this, is_server, cb, timeout](io_result_t& ior)->bool{
                        return handle_tls_handshake (is_server, cb, timeout, ior.errnum);
                    }, timeout);
                return false;

            case SSL_ERROR_SYSCALL:
                errnum = errno ? errno : EIO;
                err_str = ERR_reason_error_string (ERR_get_error());
                break;

            case SSL_ERROR_SSL:
            default:
                err_str = ERR_reason_error_string (ERR_get_error());
                errnum = ECONNREFUSED;
            }
        }

        if (mem_bio) {
            SSL_set0_rbio (tls, fd_bio);
            fd_bio = nullptr;
            mem_bio = nullptr;
            mem_bio_buf.reset ();
        }

        if (errnum) {
            if (tls)
                SSL_free (tls);
            if (tls_ctx)
                SSL_CTX_free (tls_ctx);
            tls         = nullptr;
            tls_ctx     = nullptr;
            tls_active  = false;
            tls_started = false;
            if (!err_str)
                err_str = strerror (errnum);
            TRACE ("TLS handshake failed for file handle %d: %s", handle(), err_str);
        }else{
            TRACE ("TLS handshake success for file handle %d", handle());
            tls_active = true;
        }

        if (cb)
            cb (*this, errnum, err_str);

        return false;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t TlsAdapter::do_read (void* buf, size_t size, int& errnum)
    {
        if (!is_tls_active())
            return Adapter::do_read (buf, size, errnum);

        ssize_t bytes_read {0};
        ERR_clear_error ();
        int result = SSL_read_ex (tls, buf, size, (size_t*)&bytes_read);
        if (result > 0) {
            errnum = 0;
        }else{
            switch (SSL_get_error(tls, result)) {
            case SSL_ERROR_ZERO_RETURN:
                TRACE ("SSL_ERROR_ZERO_RETURN");
                errnum = 0;
                bytes_read = 0;
                break;

            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                errnum = EAGAIN;
                bytes_read = -1;
                break;

            case SSL_ERROR_SYSCALL:
                TRACE ("SSL_ERROR_SYSCALL");
                errnum = errno;
                if (errnum == 0  ||  errnum == ECONNRESET) {
                    errnum = 0;
                    bytes_read = 0;
                }else{
                    bytes_read = -1;
                    const char* err_str = ERR_reason_error_string (ERR_get_error());
                    if (!err_str)
                        err_str = strerror (errnum);
                    Log::debug ("TLS read error: %s", err_str);
                }
                break;

            default:
                bytes_read = -1;
                errnum = errno ? errno : EIO;
                const char* err_str = ERR_reason_error_string (ERR_get_error());
                if (!err_str)
                    err_str = strerror (errnum);
                Log::debug ("TLS read error: %s", err_str);
            }
        }
        return bytes_read;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t TlsAdapter::do_write (const void* buf, size_t size, int& errnum)
    {
        if (!is_tls_active())
            return Adapter::do_write (buf, size, errnum);

        ssize_t bytes_written {0};
        ERR_clear_error ();
        int result = SSL_write_ex (tls, buf, size, (size_t*)&bytes_written);
        if (result > 0) {
            errnum = 0;
        }else{
            switch (SSL_get_error(tls, result)) {
            case SSL_ERROR_ZERO_RETURN:
                TRACE ("SSL_ERROR_ZERO_RETURN");
                errnum = 0;
                bytes_written = 0;
                break;

            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                errnum = EAGAIN;
                bytes_written = -1;
                break;

            case SSL_ERROR_SYSCALL:
                TRACE ("SSL_ERROR_SYSCALL");
                errnum = errno;
                if (errnum == 0  ||  errnum == ECONNRESET) {
                    errnum = 0;
                    bytes_written = 0;
                }else{
                    bytes_written = -1;
                    const char* err_str = ERR_reason_error_string (ERR_get_error());
                    if (!err_str)
                        err_str = strerror (errnum);
                    Log::debug ("TLS write error: %s", err_str);
                }
                break;

            default:
                bytes_written = -1;
                errnum = EIO;
                const char* err_str = ERR_reason_error_string (ERR_get_error());
                if (!err_str)
                    err_str = strerror (errnum);
                Log::debug ("TLS write error: %s", err_str);
            }
        }
        return bytes_written;
    }


}