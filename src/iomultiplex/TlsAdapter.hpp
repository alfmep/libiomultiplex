/*
 * Copyright (C) 2021-2023 Dan Arrhenius <dan@ultramarin.se>
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
#ifndef IOMULTIPLEX_TLSADAPTER_HPP
#define IOMULTIPLEX_TLSADAPTER_HPP

#include <iomultiplex/Adapter.hpp>
#include <iomultiplex/Connection.hpp>
#include <iomultiplex/TlsConfig.hpp>
#include <iomultiplex/x509_t.hpp>
#include <functional>
#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <openssl/ssl.h>


namespace iomultiplex {


    /**
     * I/O Adapter implementing secure connections using TLS.
     */
    class TlsAdapter : public Adapter {
    public:
        /**
         * Callback that is called when a TLS handshake is finished (successful or not).
         * @param tls The TlsAdapter instance that attempted to establish a secure connection.
         */
        using tls_handshake_cb_t = std::function<void (TlsAdapter& tls)>;


        /**
         * Default constructor.
         * Constructs an adapter without a slave connection.
         */
        TlsAdapter () = default;

        /**
         * Constructor.
         * @param conn The slave connection used by this adapter.
         * @param close_on_destruct If <code>true</code>, the
         *                          slave connection used by this adapter
         *                          will be closed in the descructor of
         *                          this object. if <code>false</code>,
         *                          the slave connection will be kept
         *                          open.
         */
        TlsAdapter (Connection& conn, bool close_on_destruct=false);

        /**
         * Constructor.
         * @param conn_ptr A shared ponter to the slave connection
         *                 used by this adapter. The slave connection
         *                 isn't closed by the adapter in the destructor,
         *                 it will be closed by the slave connection itself
         *                 when the shared pointer calls its destructor.
         */
        TlsAdapter (std::shared_ptr<Connection> conn_ptr);

        /**
         * Destructor.
         * The slave connection is only closed by this
         * destructor if the constructor was called with
         * parameter <code>close_on_destruct</code> set
         * to <code>true</code>.
         * <br/>In any case the slave connection will
         * be closed when itself is destroyed.
         */
        virtual ~TlsAdapter ();

        virtual void close ();

        /**
         * Is TLS active or not?
         * @return <code>true</code> if a TLS handshake has been
         *         successully performed, or is under negotiation.
         */
        bool is_tls_active () const;

        /**
         * Return the last error code generated by the OpenSSL library.
         * @return The last error code generated by the OpenSSL library.
         */
        unsigned long last_error () const;

        /**
         * Return a description of the last error generated by the OpenSSL library.
         * @return A description of the last error generated by the OpenSSL library.
         */
        std::string last_error_msg () const;

        /**
         * Get the certificate of the peer (if any).
         * @return The certificate of the peer (if any).
         */
        x509_t peer_cert () const;

        /**
         * Return the name of the currently used cipher, if any.
         * @return The name of the currently used cipher, if any.
         */
        std::string cipher_name () const;

        /**
         * Return the version of the currently used (D)TLS protocol, if any.
         * @return The version of the currently used (D)TLS protocol, if any.
         */
        std::string proto_ver () const;

        /**
         * Verify the peer certificate.
         * @return <code>true</code> If the certificate
         *         of the peer is successfully verified.
         */
        inline bool verify_peer () const {
            long error_code;
            return verify_peer (error_code);
        }

        /**
         * Verify the peer certificate.
         * @param error_code This will contain an OpenSSL error code.
         *                   On success, the value is X509_V_OK.
         *                   If a TLS connection isn't established, or
         *                   the peer doesn't have a certificate, the
         *                   error code will be X509_V_ERR_UNSPECIFIED.
         * @return <code>true</code> If the certificate
         *         of the peer is successfully verified.
         */
        bool verify_peer (long& error_code) const;

        /**
         * Verify the peer certificate.
         * @param error_message This will contain an error message
         *                      upon verification error.
         * @return <code>true</code> If the certificate
         *         of the peer is successfully verified.
         */
        inline bool verify_peer (std::string& error_message) const {
            long error_code;
            return verify_peer (error_code, error_message);
        }

        /**
         * Verify the peer certificate.
         * @param error_code This will contain an OpenSSL error code.
         *                   On success, the value is X509_V_OK.
         *                   If a TLS connection isn't established, or
         *                   the peer doesn't have a certificate, the
         *                   error code will be X509_V_ERR_UNSPECIFIED.
         * @param error_message This will contain an error message
         *                      upon verification error.
         * @return <code>true</code> If the certificate
         *         of the peer is successfully verified.
         */
        bool verify_peer (long& error_code, std::string& error_message) const;

        /**
         * Start a TLS handshake.
         * @param tls_config TLS configuration.
         * @param is_server Set to <code>true</code> to initiate a server handshake,
         *                  <code>false</code> to initiate a client handshake.
         * @param use_dtls Set to <code>true</code> to use DTLS instead of TLS.
         * @param buf If a server handshake is started, use data already read from
         *            the client. <code>buf</code> contains a buffer that is pre-read from the client.
         *            If <code>nullptr</code>, data is read using the slave connection.
         * @param buf_len The size in bytes of the (optional) pre-read buffer when starting
         *                a server handshake.
         *                If <code>0</code>, data is read using the slave connection.
         * @param callback Callback to be called when the TLS handshake is finished.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        int start_tls (const TlsConfig& tls_config,
                       bool is_server,
                       bool use_dtls,
                       void* buf,
                       size_t buf_len,
                       tls_handshake_cb_t callback,
                       unsigned timeout=-1);

        /**
         * Synchronized call to start a TLS handshake.
         * @param tls_config TLS configuration.
         * @param is_server Set to <code>true</code> to initiate a server handshake,
         *                  <code>false</code> to initiate a client handshake.
         * @param use_dtls Set to <code>true</code> to use DTLS instead of TLS.
         * @param buf If a server handshake is started, use data already read from
         *            the client. <code>buf</code> contains a buffer that is pre-read from the client.
         *            If <code>nullptr</code>, data is read using the slave connection.
         * @param buf_len The size in bytes of the (optional) pre-read buffer when starting
         *                a server handshake.
         *                If <code>0</code>, data is read using the slave connection.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        int start_tls (const TlsConfig& tls_config,
                       bool is_server,
                       bool use_dtls,
                       void* buf,
                       size_t buf_len,
                       unsigned timeout=-1);

        /**
         * Start a TLS handshake.
         * @param tls_config TLS configuration.
         * @param is_server Set to <code>true</code> to initiate a server handshake,
         *                  <code>false</code> to initiate a client handshake.
         * @param use_dtls Set to <code>true</code> to use DTLS instead of TLS.
         * @param callback Callback to be called when the TLS handshake is finished.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        inline int start_tls (const TlsConfig& tls_config,
                              bool is_server,
                              bool use_dtls,
                              tls_handshake_cb_t callback,
                              unsigned timeout=-1)
        {
            return start_tls (tls_config, is_server, use_dtls, nullptr, 0, callback, timeout);
        }

        /**
         * Synchronized call to start a TLS handshake.
         * @param tls_config TLS configuration.
         * @param is_server Set to <code>true</code> to initiate a server handshake,
         *                  <code>false</code> to initiate a client handshake.
         * @param use_dtls Set to <code>true</code> to use DTLS instead of TLS.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        inline int start_tls (const TlsConfig& tls_config,
                              bool is_server,
                              bool use_dtls,
                              unsigned timeout=-1)
        {
            return start_tls (tls_config, is_server, use_dtls, nullptr, 0, timeout);
        }

        /**
         * Start a TLS server handshake.
         * @param tls_config TLS configuration.
         * @param callback Callback to be called when the TLS handshake is finished.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        inline int start_server_tls (const TlsConfig& tls_config,
                                     tls_handshake_cb_t callback,
                                     unsigned timeout=-1)
        {
            return start_tls (tls_config, true, false, nullptr, 0, callback, timeout);
        }

        /**
         * Start a TLS server handshake using pre-read data.
         * @param tls_config TLS configuration.
         * @param buf A buffer that is pre-read from the client.
         *            If <code>nullptr</code>, data is read using the slave connection.
         * @param buf_len The size in bytes of the (optional) pre-read buffer.
         *                If <code>0</code>, data is read using the slave connection.
         * @param callback Callback to be called when the TLS handshake is finished.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        inline int start_server_tls (const TlsConfig& tls_config,
                                     void* buf,
                                     size_t buf_len,
                                     tls_handshake_cb_t callback,
                                     unsigned timeout=-1)
        {
            return start_tls (tls_config, true, false, buf, buf_len, callback, timeout);
        }

        /**
         * Synchronized call to start a TLS server handshake.
         * @param tls_config TLS configuration.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        inline int start_server_tls (const TlsConfig& tls_config, unsigned timeout=-1)
        {
            return start_tls (tls_config, true, false, nullptr, 0, timeout);
        }

        /**
         * Synchronized call to start a TLS server handshake using pre-read data.
         * @param tls_config TLS configuration.
         * @param buf A buffer that is pre-read from the client.
         *            If <code>nullptr</code>, data is read using the slave connection.
         * @param buf_len The size in bytes of the (optional) pre-read buffer.
         *                If <code>0</code>, data is read using the slave connection.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        inline int start_server_tls (const TlsConfig& tls_config,
                                     void* buf,
                                     size_t buf_len,
                                     unsigned timeout=-1)
        {
            return start_tls (tls_config, true, false, buf, buf_len, timeout);
        }

        /**
         * Start a TLS client handshake.
         * @param tls_config TLS configuration.
         * @param callback Callback to be called when the TLS handshake is finished.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        inline int start_client_tls (const TlsConfig& tls_config,
                                     tls_handshake_cb_t callback,
                                     unsigned timeout=-1)
        {
            return start_tls (tls_config, false, false, nullptr, 0, callback, timeout);
        }

        /**
         * Synchronized call to start a TLS client handshake.
         * @param tls_config TLS configuration.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        inline int start_client_tls (const TlsConfig& tls_config, unsigned timeout=-1)
        {
            return start_tls (tls_config, false, false, nullptr, 0, timeout);
        }

        /**
         * Start a DTLS server handshake.
         * @param tls_config TLS configuration.
         * @param callback Callback to be called when the DTLS handshake is finished.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        inline int start_server_dtls (const TlsConfig& tls_config,
                                      tls_handshake_cb_t callback,
                                      unsigned timeout=-1)
        {
            return start_tls (tls_config, true, true, nullptr, 0, callback, timeout);
        }

        /**
         * Start a DTLS server handshake using pre-read data.
         * @param tls_config TLS configuration.
         * @param buf A buffer that is pre-read from the client.
         *            If <code>nullptr</code>, data is read using the slave connection.
         * @param buf_len The size in bytes of the (optional) pre-read buffer.
         *                If <code>0</code>, data is read using the slave connection.
         * @param callback Callback to be called when the DTLS handshake is finished.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        inline int start_server_dtls (const TlsConfig& tls_config,
                                      void* buf,
                                      size_t buf_len,
                                      tls_handshake_cb_t callback,
                                      unsigned timeout=-1)
        {
            return start_tls (tls_config, true, true, buf, buf_len, callback, timeout);
        }

        /**
         * Synchronized call to start a DTLS server handshake.
         * @param tls_config TLS configuration.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        inline int start_server_dtls (const TlsConfig& tls_config, unsigned timeout=-1)
        {
            return start_tls (tls_config, true, true, nullptr, 0, timeout);
        }

        /**
         * Synchronized call to start a DTLS server handshake.
         * @param tls_config TLS configuration.
         * @param buf A buffer that is pre-read from the client.
         *            If <code>nullptr</code>, data is read using the slave connection.
         * @param buf_len The size in bytes of the (optional) pre-read buffer.
         *                If <code>0</code>, data is read using the slave connection.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        inline int start_server_dtls (const TlsConfig& tls_config,
                                      void* buf,
                                      size_t buf_len,
                                      unsigned timeout=-1)
        {
            return start_tls (tls_config, true, true, buf, buf_len, timeout);
        }

        /**
         * Start a DTLS client handshake.
         * @param tls_config TLS configuration.
         * @param callback Callback to be called when the DTLS handshake is finished.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        inline int start_client_dtls (const TlsConfig& tls_config,
                                      tls_handshake_cb_t callback,
                                      unsigned timeout=-1)
        {
            return start_tls (tls_config, false, true, nullptr, 0, callback, timeout);
        }

        /**
         * Synchronized call to start a DTLS client handshake.
         * @param tls_config TLS configuration.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        inline int start_client_dtls (const TlsConfig& tls_config, unsigned timeout=-1)
        {
            return start_tls (tls_config, false, true, nullptr, 0, timeout);
        }


        /**
         * Read encrypted data from the slave connection.
         *
         * @param buf A pointer to the memory area where data should be stored.
         * @param size The number of bytes to read.
         * @param errnum The value of <code>errno</code> after
         *               the read operation. Always 0 if no error occurred.
         *
         * @return The number of bytes the adapter stored in the supplied
         *         buffer. Note that this can differ from the actual number
         *         of bytes read by the slave connection if, for example,
         *         the adapter implements decompression of data.
         *         <br/>
         *         On error, -1 is returned and parameter <code>errnum</code>
         *         is set to some appropriate value.
         */
        virtual ssize_t do_read (void* buf, size_t size, int& errnum);

        /**
         * Write encrypted data to the slave connection.
         *
         * @param buf A pointer to the memory that should be written.
         * @param size The number of bytes to write.
         * @param errnum The value of <code>errno</code> after
         *               the read operation. Always 0 if no error occurred.
         *
         * @return The number of bytes the adapter consumed from the supplied
         *         buffer. Note that this can differ from the actual number
         *         of bytes written on the slave connection if, for example,
         *         the adapter implements compression of data.
         *         <br/>
         *         On error, -1 is returned and parameter <code>errnum</code>
         *         is set to some appropriate value.
         */
        virtual ssize_t do_write (const void* buf, size_t size, int& errnum);


    private:
        int initiate_tls_server_handshake (void* buf,
                                           size_t buf_len,
                                           tls_handshake_cb_t cb,
                                           unsigned timeout);
        bool handle_tls_handshake (bool is_client,
                                   tls_handshake_cb_t cb,
                                   unsigned timeout,
                                   int errnum);
        void clear_error ();
        void update_error (const char* msg=nullptr);

        SSL_CTX* tls_ctx; // OpenSSL context object
        SSL* tls;         // OpenSSL SSL(TLS) object
        BIO* mem_bio;
        BIO* fd_bio;
        unsigned long last_err;
        std::string last_err_msg;
        std::unique_ptr<char> mem_bio_buf;
        std::atomic_bool tls_started; // TLS handshake started
        std::atomic_bool tls_active;  // TLS handshake done and TLS active
    };


}


#endif
