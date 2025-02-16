/*
 * Copyright (C) 2021,2022,2025 Dan Arrhenius <dan@ultramarin.se>
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

#define THIS_FILE "TlsAdapter.cpp"
#ifdef TRACE_DEBUG
#  include <sys/types.h>
#  define TRACE(format, ...) do{ \
      auto save_errno=errno; \
      Log::debug("[%u] %s:%s:%d: " format, gettid(), THIS_FILE, __FUNCTION__, __LINE__, ## __VA_ARGS__); \
      errno=save_errno; \
   }while(false)
#else
#  define TRACE(format, ...)
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
          last_err (0),
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
          last_err (0),
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
        if (is_tls_active()) {
            cancel (true, true, false);
            SSL_shutdown (tls);
            clear_resources ();
        }

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
    void TlsAdapter::clear_error ()
    {
        ERR_clear_error ();
        last_err = 0;
        last_err_msg = "";
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void TlsAdapter::update_error (const char* msg)
    {
        last_err = ERR_peek_last_error ();
        if (!msg) {
            const char* err_str = ERR_reason_error_string (last_err);
            last_err_msg = err_str ? err_str : "";
        }else{
            last_err_msg = msg;
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    unsigned long TlsAdapter::last_error () const
    {
        return last_err;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::string TlsAdapter::last_error_msg () const
    {
        return last_err_msg;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    x509_t TlsAdapter::peer_cert () const
    {
        x509_t x509;
        if (tls_active)
            x509 = x509_t (SSL_get_peer_certificate(tls), true, true);
        return x509;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::string TlsAdapter::cipher_name () const
    {
        const char* name = tls_active ? SSL_get_cipher_name(tls) : "";
        return std::string (name ? name : "");
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::string TlsAdapter::proto_ver () const
    {
        const char* name = tls_active ? SSL_get_cipher_version(tls) : "";
        return std::string (name ? name : "");
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool TlsAdapter::verify_peer (long& error_code) const
    {
        error_code = X509_V_ERR_UNSPECIFIED;
        X509* x509 = nullptr;
        if (tls_active && (x509=SSL_get_peer_certificate(tls)) != nullptr) {
            error_code = SSL_get_verify_result (tls);
            X509_free (x509);
        }
        return error_code == X509_V_OK;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool TlsAdapter::verify_peer (long& error_code, std::string& error_message) const
    {
        auto retval = verify_peer (error_code);
        error_message = std::string (X509_verify_cert_error_string(error_code));
        return retval;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static int set_tls_cert_files (SSL_CTX* ctx, const TlsConfig& cfg)
    {
        if (cfg.ca_path.empty() && cfg.ca_file.empty()) {
            // Default CA file and dir
            TRACE ("Setting default TLS CA dir/file");
            SSL_CTX_set_default_verify_paths (ctx);
        }else{
            TRACE ("Setting TLS CA dir and/or file");
            if (0 == SSL_CTX_load_verify_locations(ctx,
                                                   (cfg.ca_file.empty() ? nullptr : cfg.ca_file.c_str()),
                                                   (cfg.ca_path.empty() ? nullptr : cfg.ca_path.c_str())))
            {
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
        if (!cfg.cipher_suites.empty()) {
            if (SSL_CTX_set_ciphersuites(ctx, cfg.cipher_suites.c_str()) == 0) { // TLSv1.3
                TRACE ("Error setting TLS cipher suites");
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

        // Disable renegotiation in TLSv1.2 and earlier
        auto opts = SSL_CTX_get_options (tls_ctx);
        opts |= SSL_OP_NO_RENEGOTIATION;
        SSL_CTX_set_options (tls_ctx, opts);

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

        clear_error ();

        TRACE ("Starting TLS %s handshake on fd %d",
               (is_server ? "server" : "client"), handle());

        // Configure TLS context
        tls_ctx = configure_tls (tls_config, is_server, use_dtls);
        if (!tls_ctx) {
            tls_ctx     = nullptr;
            tls_active  = false;
            tls_started = false;
            TRACE ("Failed to configure TLS on fd %d", handle());
            errno = EINVAL;
            return -1;
        }

        // Create and initialize an SSL structure
        tls = SSL_new (tls_ctx);
        if (!tls || SSL_set_fd(tls, handle())==0) {
            TRACE ("Failed to initialize an SSL structure on fd %d", handle());
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

        // Configure SNI sent by client
        if (!is_server && !tls_config.sni.empty())
            SSL_set_tlsext_host_name (tls, tls_config.sni.c_str());

        // Initiate the TLS handshake
        //
        int result;
        errno = 0;
        if (is_server) {
            if (buf && buf_len) {
                // We have an initial RX buffer and is starting a server handshake
                TRACE ("Start TLS server handshake on fd %d with pre-read buffer, size: %lu",
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
            TRACE ("TLS handshake failed for fd %d: %s", handle(), strerror(errnum));
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
        long tls_error = 0;
        int errnum = 0;
        int retval = -1;
        // Initiate TLS handshake
        int start_result;
        start_result = start_tls (tls_config,
                                  is_server,
                                  use_dtls,
                                  buf,
                                  buf_len,
                                  [this, &tls_error, &errnum, &io_done](TlsAdapter& ta, int e){
                                      std::unique_lock<std::mutex> lock (sync_mutex);
                                      io_done = true;
                                      tls_error = last_error ();
                                      errnum = e;
                                      sync_cond.notify_one ();
                                  },
                                  timeout);
        if (start_result == 0) {
            // Wait for TLS handshake to finish, fail, or timeout
            std::unique_lock<std::mutex> lock (sync_mutex);
            sync_cond.wait (lock, [&io_done]{return io_done;});
            if (tls_error || errnum) {
                cancel (true, true, false);
                if (tls)
                    SSL_free (tls);
                if (tls_ctx)
                    SSL_CTX_free (tls_ctx);
                tls         = nullptr;
                tls_ctx     = nullptr;
                tls_active  = false;
                tls_started = false;
                if (errnum == 0)
                    errnum = EIO;
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

        clear_error ();

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
            TRACE ("TLS handshake success for fd %d after only initial RX buffer data", handle());
            tls_active = true;
            if (cb) {
                retval = wait_for_tx (
                        [this, cb](io_result_t& ior)->bool{
                            cb (*this, ior.errnum);
                            return false;
                        },
                        timeout);
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
            update_error ();
            retval = -1;
            TRACE ("SSL_ERROR_SYSCALL: %s", last_err_msg.c_str());
            break;

        case SSL_ERROR_SSL:
        default:
            update_error ();
            errno = ECONNREFUSED;
            retval = -1;
            TRACE ("SSL_ERROR_SSL: %s", last_err_msg.c_str());
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
        int tls_error {0};
        int ret {0};

        if (!errnum) {
            clear_error ();
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
                if (!errnum) {
                    ERR_put_error (ERR_LIB_USER, SYS_F_CONNECT, ERR_R_SYS_LIB, THIS_FILE, __LINE__);
                    errnum = EIO;
                }
                update_error (strerror(errnum));
                break;

            case SSL_ERROR_SSL:
            default:
                update_error ();
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
            TRACE ("TLS handshake failed for fd %d: %s, errno: %d - %s",
                   handle(), last_err_msg.c_str(), errnum, strerror(errnum));
        }else{
            TRACE ("TLS handshake success for fd %d", handle());
            tls_active = true;
        }

        if (cb)
            cb (*this, errnum);

        return false;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void TlsAdapter::clear_resources ()
    {
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

        clear_error ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int TlsAdapter::shutdown (tls_handshake_cb_t callback, unsigned timeout)
    {
        // Sanity check
        if (handle() < 0) {
            errno = EBADF;
            return -1;
        }
        if (!is_tls_active()) {
            errno = 0;
            return -1;
        }

        TRACE ("Perform (D)TLS shutdown on fd %d", handle());
        clear_error ();
        errno = 0;
        auto result = SSL_shutdown (tls);

        if (result == 1) {
            // Shutdown successfully completed !
            if (io_handler().same_context()) {
                TRACE ("(D)TLS shutdown complete on fd %d", handle());
                clear_resources ();
                if (callback)
                    callback (*this, 0);
                return 0;
            }else{
                return wait_for_rx ([this, callback](io_result_t& ior)->bool
                    {
                        TRACE ("(D)TLS shutdown complete on fd %d", handle());
                        clear_resources ();
                        if (callback)
                            callback (*this, 0);
                        return false;
                    }, 0);
            }
        }
        if (result == 0) {
            // Wait for the peer's close_notify alert.
            TRACE ("Waiting for peer (D)TLS shutdown on fd %d", handle());
            return wait_for_rx (
                    [this, callback](io_result_t& ior)->bool{
                        handle_tls_shutdown (callback, 0, ior.errnum);
                        return false;
                    },
                    timeout);
        }

        int retval = 0;
        auto tls_error = SSL_get_error (tls, result);
        switch (tls_error) {
        // case SSL_ERROR_NONE:
        //     break;

        case SSL_ERROR_WANT_READ:
            TRACE ("RX needed for (D)TLS shutdown on fd %d", handle());
            retval = wait_for_rx (
                    [this, callback, timeout](io_result_t& ior)->bool{
                        handle_tls_shutdown (callback, timeout, ior.errnum);
                        return false;
                    },
                    timeout);
            break;

        case SSL_ERROR_WANT_WRITE:
            TRACE ("TX needed for (D)TLS shutdown on fd %d", handle());
            retval = wait_for_tx (
                    [this, callback, timeout](io_result_t& ior)->bool{
                        handle_tls_shutdown (callback, timeout, ior.errnum);
                        return false;
                    },
                    timeout);
            break;

        case SSL_ERROR_SYSCALL:
            update_error ();
            retval = -1;
            TRACE ("SSL_ERROR_SYSCALL: %s", last_err_msg.c_str());
            break;

        case SSL_ERROR_SSL:
        default:
            update_error ();
            retval = -1;
            TRACE ("SSL_ERROR_SSL: %s", last_err_msg.c_str());
            break;
        }

        return retval;
    }


    //--------------------------------------------------------------------------
    // I/O handler context
    //--------------------------------------------------------------------------
    void TlsAdapter::handle_tls_shutdown (tls_handshake_cb_t callback,
                                          unsigned timeout,
                                          int errnum)
    {
        if (errnum) {
            TRACE ("(D)TLS shutdown error on fd %d: %s", handle(), strerror(errnum));
            clear_resources ();
            if (callback)
                callback (*this, errnum);
            return;
        }

        clear_error ();
        errno = 0;
        int result = SSL_shutdown (tls);

        if (result == 1) {
            // Shutdown successfully completed !
            TRACE ("(D)TLS shutdown complete on fd %d", handle());
            clear_resources ();
            if (callback)
                callback (*this, 0);
            return;
        }
        if (result == 0) {
            // Wait for the peer's close_notify alert.
            TRACE ("Waiting for peer (D)TLS shutdown on fd %d", handle());
            if (wait_for_rx (
                        [this, callback](io_result_t& ior)->bool{
                            clear_resources ();
                            if (callback)
                                callback (*this, ior.errnum);
                            return false;
                        }, timeout))
            {
                if (callback)
                    callback (*this, errno);
            }
            return;
        }

        auto tls_error = SSL_get_error (tls, result);
        switch (tls_error) {
        case SSL_ERROR_WANT_READ:
            TRACE ("RX needed for (D)TLS shutdown on fd %d", handle());
            result = wait_for_rx (
                    [this, callback, timeout](io_result_t& ior)->bool{
                        handle_tls_shutdown (callback, timeout, ior.errnum);
                        return false;
                    },
                    timeout);
            break;

        case SSL_ERROR_WANT_WRITE:
            TRACE ("TX needed for (D)TLS shutdown on fd %d", handle());
            result = wait_for_tx (
                    [this, callback, timeout](io_result_t& ior)->bool{
                        handle_tls_shutdown (callback, timeout, ior.errnum);
                        return false;
                    },
                    timeout);
            break;

        case SSL_ERROR_SYSCALL:
            update_error ();
            result = -1;
            errno = EIO;
            TRACE ("SSL_ERROR_SYSCALL: %s", last_err_msg.c_str());
            break;

        case SSL_ERROR_SSL:
        default:
            update_error ();
            result = -1;
            TRACE ("SSL_ERROR_SSL: %s", last_err_msg.c_str());
            break;
        }

        if (result) {
            if (callback)
                callback (*this, errno);
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t TlsAdapter::do_read (void* buf, size_t size, int& errnum)
    {
        if (!is_tls_active())
            return Adapter::do_read (buf, size, errnum);

        ssize_t bytes_read {0};
        clear_error ();
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
                    update_error ();
                    bytes_read = -1;
                    Log::debug ("TLS read error: %s", last_err_msg.c_str());
                }
                break;

            default:
                update_error ();
                bytes_read = -1;
                errnum = errno ? errno : EIO;
                Log::debug ("TLS read error: %s", last_err_msg.c_str());
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
        clear_error ();
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
                    update_error ();
                    bytes_written = -1;
                    Log::debug ("TLS write error: %s", last_err_msg.c_str());
                }
                break;

            default:
                update_error ();
                bytes_written = -1;
                errnum = EIO;
                Log::debug ("TLS write error: %s", last_err_msg.c_str());
            }
        }
        return bytes_written;
    }


}
