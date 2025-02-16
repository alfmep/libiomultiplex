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
#ifndef IOMULTIPLEX_SOCK_ADDR_HPP
#define IOMULTIPLEX_SOCK_ADDR_HPP

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
    class sock_addr {
    public:
        /**
         * Default constructor.
         */
        sock_addr ();

        /**
         * Destructor.
         */
        virtual ~sock_addr () = default;

        /**
         * Compare operator.
         * @param addr The sock_addr object to compare.
         * @return <code>true</code> if the addresses are equal.
         */
        bool operator== (const sock_addr& addr) const;

        /**
         * Not-equal operator.
         * @param addr The sock_addr object to compare.
         * @return <code>true</code> if the addresses are <em>not</em> equal.
         */
        bool operator!= (const sock_addr& addr) const {
            return ! operator== (addr);
        }

        /**
         * Less-than operator.
         * @param rhs The right hand size of the comparison.
         * @return <code>true</code> if this object
         *         is less than <code>rhs</code>.
         */
        bool operator< (const sock_addr& rhs) const;

        /**
         * Return the size of the address data.
         * @return The size of the address data.
         */
        virtual size_t size () const = 0;

        /**
         * Return the address data.
         * @return A pointer to the address data.
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
         * @return A copy of this object.
         */
        virtual std::shared_ptr<sock_addr> clone () const = 0;

        /**
         * Return a string representation of the socket address.
         * @return A string representation of the socket address.
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
     * Functor to get the hash code of a iomultiplex::sock_addr object.
     */
    template<> struct hash<iomultiplex::sock_addr> {
        using argument_type = iomultiplex::sock_addr;
        using result_type   = std::size_t;

        /**
         * Return the hash code of an sock_addr object.
         * @param addr A sock_addr object.
         * @return The hash code of an sock_addr object.
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
