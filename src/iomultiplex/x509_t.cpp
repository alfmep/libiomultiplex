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
#include <iomultiplex/x509_t.hpp>
#include <sstream>
#include <iomanip>
#include <regex>
#include <cstring>
#include <ctime>
#include <openssl/ts.h>
#include <openssl/asn1.h>


namespace iomultiplex {


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static std::string X509_NAME_to_string (X509_NAME* name)
    {
        std::string retval;
        constexpr unsigned long flags = (XN_FLAG_SEP_CPLUS_SPC | XN_FLAG_DUMP_UNKNOWN_FIELDS) & ~ASN1_STRFLGS_ESC_MSB;
        if (name) {
            BIO* bio = BIO_new (BIO_s_mem());
            if (bio) {
                int result = X509_NAME_print_ex (bio, name, 0, flags);
                if (result != -1) {
                    char* buf;
                    auto size = BIO_get_mem_data (bio, &buf);
                    retval = std::string (buf, size);
                }
                BIO_free (bio);
            }
        }
        return retval;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    x509_t::x509_t ()
        : x (nullptr),
          free_x (false)
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    x509_t::x509_t (X509* x509, bool dont_copy, bool free_buffer)
    {
        if (dont_copy) {
            x = x509;
            free_x = free_buffer;
        }else{
            x = x509 ? X509_dup(x509) : nullptr;
            free_x = true;
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    x509_t::x509_t (const x509_t& x509)
    {
        x = x509.x ? X509_dup(x509.x) : nullptr;
        free_x = true;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    x509_t::x509_t (x509_t&& x509)
    {
        x = x509.x;
        free_x = x509.free_x;

        x509.x = nullptr;
        x509.free_x = false;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    x509_t::~x509_t ()
    {
        if (x && free_x)
            X509_free (x);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    x509_t& x509_t::operator= (const x509_t& x509)
    {
        if (&x509 != this) {
            X509* tmp = x;
            x = x509.x ? X509_dup(x509.x) : nullptr;
            if (tmp && free_x)
                X509_free (tmp);
            free_x = true;
        }
        return *this;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    x509_t& x509_t::operator= (x509_t&& x509)
    {
        if (&x509 != this) {
            x = x509.x;
            free_x = x509.free_x;

            x509.x = nullptr;
            x509.free_x = false;
        }
        return *this;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    x509_t::operator bool() const
    {
        return x != nullptr;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int x509_t::version () const
    {
        return x ? X509_get_version(x)+1 : 0;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    long x509_t::serial () const
    {
        int s = 0;
        if (x) {
            ASN1_INTEGER* ai = X509_get_serialNumber (x);
            if (ai)
                s = (int) ASN1_INTEGER_get (ai);
        }
        return s;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::string x509_t::issuer () const
    {
        return x ? X509_NAME_to_string (X509_get_issuer_name(x)) : "";
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::string x509_t::subject () const
    {
        return x ? X509_NAME_to_string (X509_get_subject_name(x)) : "";
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::string x509_t::common_name () const
    {
        static const std::regex cn_regex (".*CN=(.*),?.*");
        auto s = subject ();
        std::smatch match;
        if (std::regex_match(s, match, cn_regex) && match.size()==2)
            return match[1].str ();
        else
            return "";
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::chrono::time_point<std::chrono::system_clock> x509_t::not_before () const
    {
        auto retval = std::chrono::system_clock::now ();
        if (x) {
            const ASN1_TIME* asn1_time = X509_get0_notBefore (x);
            if (asn1_time) {
                struct tm tm_time;
                if (ASN1_TIME_to_tm(asn1_time, &tm_time) == 1) {
                    time_t t = mktime (&tm_time);
                    retval = std::chrono::system_clock::from_time_t (t);
                }
            }
        }
        return retval;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::chrono::time_point<std::chrono::system_clock> x509_t::not_after () const
    {
        auto retval = std::chrono::time_point<std::chrono::system_clock> ();
        if (x) {
            const ASN1_TIME* asn1_time = X509_get0_notAfter (x);
            if (asn1_time) {
                struct tm tm_time;
                if (ASN1_TIME_to_tm(asn1_time, &tm_time) == 1) {
                    time_t t = mktime (&tm_time);
                    retval = std::chrono::system_clock::from_time_t (t);
                }
            }
        }
        return retval;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::string x509_t::pubkey_algo () const
    {
        std::string retval;
        if (x) {
            EVP_PKEY* key = X509_get0_pubkey (x);
            if (key)
                retval = std::string (OBJ_nid2ln(EVP_PKEY_base_id(key)));
        }
        return retval;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    unsigned x509_t::pubkey_size () const
    {
        unsigned retval = 0;
        if (x) {
            EVP_PKEY* key = X509_get0_pubkey (x);
            if (key)
                retval = EVP_PKEY_bits (key);
        }
        return retval;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::string x509_t::pubkey () const
    {
        std::string retval;
        EVP_PKEY* key = nullptr;
        if (x && (key = X509_get0_pubkey(x))) {
            BIO* bio = BIO_new (BIO_s_mem());
            if (bio) {
                int result = EVP_PKEY_print_public (bio, key, 0, nullptr);
                if (result == 1) {
                    char* buf;
                    auto size = BIO_get_mem_data (bio, &buf);
                    retval = std::string (buf, size);
                }
                BIO_free (bio);
            }
        }
        return retval;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::string x509_t::sig_algo () const
    {
        std::string retval;
        if (x) {
            const X509_ALGOR* algo = X509_get0_tbs_sigalg (x);
            if (algo) {
                auto i = OBJ_obj2nid (algo->algorithm);
                retval = std::string (i==NID_undef ? "UNKNOWN" : OBJ_nid2ln(i));
            }
        }
        return retval;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::string x509_t::sig () const
    {
        std::string retval;
        if (x) {
            const ASN1_BIT_STRING* psig = nullptr;
            const X509_ALGOR* palg = nullptr;
            X509_get0_signature (&psig, &palg, x);
            if (psig) {
                std::stringstream ss;
                int len = ASN1_STRING_length (psig);
                ss << std::setbase(16);
                for (int i=0; i<len;) {
                    ss << (unsigned int)psig->data[i];
                    ++i;
                    if (i < len)
                        ss << ':';
                }
                retval = ss.str ();
            }
        }
        return retval;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::string x509_t::to_string () const
    {
        std::string retval;
        if (x) {
            BIO* bio = BIO_new (BIO_s_mem());
            if (bio) {
                X509_print (bio, x);
                char* buf;
                auto size = BIO_get_mem_data (bio, &buf);
                retval = std::string (buf, size);
                BIO_free (bio);
            }
        }
        return retval;
    }


}
