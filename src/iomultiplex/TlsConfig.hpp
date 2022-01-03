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
#ifndef IOMULTIPLEX_TLSCONFIG_HPP
#define IOMULTIPLEX_TLSCONFIG_HPP

#include <string>
#include <openssl/ssl.h>


namespace iomultiplex {


    /**
     * TLS configuration.
     */
    class TlsConfig {
    public:
        /**
         * Default constructor.
         */
        TlsConfig ()
            : verify_peer  {true},
              min_tls_ver  {TLS1_VERSION},
              max_tls_ver  {TLS_MAX_VERSION},
              min_dtls_ver {DTLS_MIN_VERSION},
              max_dtls_ver {DTLS_MAX_VERSION}
            {
            }

        /**
         * Constructor.
         */
        TlsConfig (bool verify)
            : verify_peer  {verify},
              min_tls_ver  {TLS1_VERSION},
              max_tls_ver  {TLS_MAX_VERSION},
              min_dtls_ver {DTLS_MIN_VERSION},
              max_dtls_ver {DTLS_MAX_VERSION}
            {
            }

        /**
         * Constructor.
         */
        TlsConfig (bool verify,
                   const std::string& certificate_authority_file)
            : verify_peer  {verify},
              min_tls_ver  {TLS1_VERSION},
              max_tls_ver  {TLS_MAX_VERSION},
              min_dtls_ver {DTLS_MIN_VERSION},
              max_dtls_ver {DTLS_MAX_VERSION},
              ca_file      {certificate_authority_file}
            {
            }

        /**
         * Constructor.
         */
        TlsConfig (bool verify,
                   const std::string& certificate_authority_file,
                   const std::string& certificate_file,
                   const std::string& private_key_file)
            : verify_peer  {verify},
              min_tls_ver  {TLS1_VERSION},
              max_tls_ver  {TLS_MAX_VERSION},
              min_dtls_ver {DTLS_MIN_VERSION},
              max_dtls_ver {DTLS_MAX_VERSION},
              ca_file      {certificate_authority_file},
              cert_file    {certificate_file},
              privkey_file {private_key_file}
            {
            }

        bool verify_peer;          /**< Verify peer certificate. Default is <code>true</code>. */
        unsigned min_tls_ver;      /**< Minimum allowed TLS version. Default is TLS1_VERSION, as defined in OpenSSL. */
        unsigned max_tls_ver;      /**< Maximum allowed TLS version. Default is TLS_MAX_VERSION, as defined in OpenSSL. */
        unsigned min_dtls_ver;     /**< Minimum allowed DTLS version. Default is DTLS_MIN_VERSION, defined in OpenSSL. */
        unsigned max_dtls_ver;     /**< Maximum allowed DTLS version. Default is DTLS_MAX_VERSION, defined in OpenSSL. */
        std::string ca_file;       /**< File containing a Certificate Authority. */
        std::string cert_file;     /**< File containing a certificate to use. */
        std::string privkey_file;  /**< File containing the private key of the used certificate. */
        std::string cipher_list;   /**< Optional list of supported ciphers for TLS v1.2 and lower. */
        std::string cipher_suites; /**< Optional list of supported ciphers for TLS v1.3. */
    };


}


#endif
