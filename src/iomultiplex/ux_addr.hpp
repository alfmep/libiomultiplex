/*
 * Copyright (C) 2021-2023,2025 Dan Arrhenius <dan@ultramarin.se>
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
#ifndef IOMULTIPLEX_UX_ADDR_HPP
#define IOMULTIPLEX_UX_ADDR_HPP

#include <iomultiplex/sock_addr.hpp>
#include <string>


namespace iomultiplex {

    /**
     * A Unix Domain Socket Address.
     */
    class ux_addr : public sock_addr {
    public:
        /**
         * Default constructor.
         */
        ux_addr ();

        /**
         * Constructor.
         * @param path The Unix Domain Socket address.
         * @param abstract If <code>true</code>, the path will
         *        have no connection with filesystem pathnames.
         */
        ux_addr (const std::string& path, const bool abstract=false);

        /**
         * Copy constructor.
         * @param addr The ux_addr objet to copy.
         */
        ux_addr (const ux_addr& addr);

        /*
         * Create an ux_addr from a <code>struct sockaddr</code>.
         */
        //explicit ux_addr (const struct sockaddr& saddr, socklen_t size);

        /**
         * Create an ux_addr from a <code>struct sockaddr_un</code>.
         * @param saddr The sockaddr_un object to copy.
         */
        explicit ux_addr (const struct sockaddr_un& saddr);

        /**
         * Destructor.
         */
        virtual ~ux_addr () = default; // Destructor.

        /**
         * Assignment operator.
         * @param addr The ux_addr objet to copy.
         * @return A reference to this object.
         */
        virtual ux_addr& operator= (const ux_addr& addr);

        /**
         * Assignment operator.
         * @param saddr The sockaddr_un objet to copy.
         * @return A reference to this object.
         */
        ux_addr& operator= (const struct sockaddr_un& saddr);

        /*
         * Compare operator.
         */
        //bool operator== (const ux_addr& addr) const;

        /*
         * Not-equal operator.
         */
        //bool operator!= (const ux_addr& addr) const {
        //    return ! operator== (addr);
        //}

        /*
         * Less-than operator.
         */
        //bool operator< (const ux_addr& rhs) const;

        virtual size_t size () const;

        /**
         * Get the path of the Unix Domain Socket.
         * @return The path of the Unix Domain Socket.
         */
        const std::string path () const;

        /**
         * Set the path of the Unix Domain Socket.
         * @param path The path.
         * @param abstract If <code>true</code>, the path will
         *        have no connection with filesystem pathnames.
         */
        void path (const std::string& path, const bool abstract=false);

        /**
         * Check if this is an abstract address.
         * @return <code>true</code> if this is an abstract address.
         */
        bool is_abstract () const;

        virtual std::shared_ptr<sock_addr> clone () const;
        virtual std::string to_string () const;
    };


}
#endif
