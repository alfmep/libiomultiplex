/*
 * Copyright (C) 2022 Dan Arrhenius <dan@ultramarin.se>
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
#ifndef IOMULTIPLEX_X509_T_HPP
#define IOMULTIPLEX_X509_T_HPP

#include <string>
#include <memory>
#include <chrono>
#include <openssl/x509.h>


namespace iomultiplex {


    /**
     * Wrapper class for OpenSSL X509 objects.
     */
    class x509_t {
    public:

        /**
         * Default constructor.
         */
        x509_t ();

        /**
         * Constructor.
         * Encapsulate or copy an x509 object.
         * @param x509 The X509 object to copy. Or <code>nullptr</code> for no copy.
         * @param dont_copy If <code>true</code>, save the pointer
         *                  to the X509 object and dont't copy it.
         *                  If <code>false</code>, make a copy of
         *                  the X509 object.
         * @param free_buffer If <code>true</code>, free the encapsulated
         *                    X509 object when this object is destroyed.
         *                    Only relevant is parameter <code>dont_copy</code>
         *                    is <code>true</code>.
         */
        x509_t (X509* x509, bool dont_copy=true, bool free_buffer=false);

        /**
         * Copy constructor.
         * @param x509 The object to copy.
         */
        x509_t (const x509_t& x509);

        /**
         * Move constructor.
         * @param x509 The object to move.
         */
        x509_t (x509_t&& x509);

        /**
         * Destructor.
         */
        ~x509_t ();

        /**
         * Assignment operator.
         * @param x509 The object to copy.
         * @return A reference to this object.
         */
        x509_t& operator= (const x509_t& x509);

        /**
         * Move operator.
         * @param x509 The object to move.
         * @return A reference to this object.
         */
        x509_t& operator= (x509_t&& x509);

        /**
         * Return <code>true</code> if this instance
         * has a pointer to an OpenSSL X509 object.
         * @return <code>true</code> if this instance
         *         has a pointer to an OpenSSL X509 object.
         */
        explicit operator bool() const;

        /**
         * Get the certificate version.
         * @return The certificate version.
         */
        int version () const;

        /**
         * Get the certificate serial number.
         * @return The certificate serial number.
         */
        long serial () const;

        std::string subject () const;  /**< Get the name of the issuer (if any).
                                        *   @return The name of the issuer (if any). */
        std::string issuer () const;   /**< Get the name of the issuer (if any).
                                        *   @return The name of the issuer (if any). */
        std::string common_name () const; /**< Get the value of Common Name (if any).
                                           *   @return The value of Common Name (if any). */

        std::chrono::time_point<std::chrono::system_clock> not_before () const; /**< Get the "Not before" time point.
                                                                                 *   @return The "Not before" time point. */
        std::chrono::time_point<std::chrono::system_clock> not_after () const;  /**< Get the "Not after" time point.
                                                                                 *   @return The "Not after" time point. */

        std::string pubkey_algo () const;  /**< Get the public key algorithm (if any).
                                            *   @return The public key algorithm (if any). */
        unsigned    pubkey_size () const;  /**< Get the bit size of the public key (if any).
                                            *   @return The bit size of the public key (if any). */
        std::string pubkey () const;       /**< Get the public key (if any).
                                            *   @return The public key (if any). */

        std::string sig_algo () const; /**< Get the signature algorithm (if any).
                                        *   @return The signature algorithm (if any). */
        std::string sig () const;      /**< Get the signature (if any).
                                        *   @return The signature (if any). */

        std::string to_string () const; /**< Return the certificate as a readable string.
                                         *   @return The certificate as a readable string. */

        const X509* ptr () const { /**< Return a pointer to the OpenSSL X509 object.
                                    *   @return A pointer to the OpenSSL X509 object. */
            return x;
        }


    private:
        X509* x;
        bool free_x;
    };


}
#endif
