/*
 * Copyright (C) 2021 Dan Arrhenius <dan@ultramarin.se>
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
#ifndef IOMULTIPLEX_SOCKADDR_HPP
#define IOMULTIPLEX_SOCKADDR_HPP

#include <functional>
#include <stdexcept>
#include <string>
#include <memory>
#include <array>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>


namespace iomultiplex {



    /**
     * A generic socket address.
     */
    class SockAddr {
    public:
        /**
         * Default constructor.
         */
        SockAddr ();

        /**
         * Destructor.
         */
        virtual ~SockAddr () = default;

        /**
         * Compare operator.
         */
        bool operator== (const SockAddr& addr) const;

        /**
         * Not-equal operator.
         */
        bool operator!= (const SockAddr& addr) const {
            return ! operator== (addr);
        }

        /**
         * Less-than operator.
         */
        bool operator< (const SockAddr& rhs) const;

        /**
         * Return the size of the address data.
         */
        virtual size_t size () const = 0;

        /**
         * Return the address data.
         */
        const struct sockaddr* data () const;

        /**
         * Return address family: AF_xxx.
         * @return The address family.
         */
        sa_family_t family () const;

        /**
         * Reset the address data.
         */
        void clear ();

        /**
         * Make a clone of this address object.
         */
        virtual std::shared_ptr<SockAddr> clone () const = 0;

        /**
         * Return a string representation of the socket address.
         */
        virtual std::string to_string () const = 0;


    protected:
        struct sockaddr_storage sa;
    };
}


/**
 * Classes injected in namespace <code>std</code>.
 */
namespace std {
    /**
     * Functor to get the hash code of a iomultiplex::SockAddr object.
     */
    template<> struct hash<iomultiplex::SockAddr> {
        using argument_type = iomultiplex::SockAddr;
        using result_type   = std::size_t;

        /**
         * Return the hash code of an SockAddr object.
         */
        result_type operator () (argument_type const& addr) const noexcept {
            result_type h = 0;
            const uint8_t* buf = (const uint8_t*) addr.data ();
            for (unsigned i=0; i<sizeof(addr.size()); ++i)
                h = h ^ (std::hash<uint8_t>{}(buf[i]) << 1);
            return h;
        }
    };
}


#endif
