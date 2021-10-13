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
#ifndef IOMULTIPLEX_UXADDR_HPP
#define IOMULTIPLEX_UXADDR_HPP

#include <iomultiplex/SockAddr.hpp>
#include <string>


namespace iomultiplex {

    /**
     * A Unix Domain Socket Address.
     */
    class UxAddr : public SockAddr {
    public:
        /**
         * Default constructor.
         */
        UxAddr ();

        /**
         * Constructor.
         */
        UxAddr (const std::string& path, const bool abstract=false);

        /**
         * Copy constructor.
         */
        UxAddr (const UxAddr& addr);

        /**
         * Create an UxAddr from a <code>struct sockaddr</code>.
         */
        //explicit UxAddr (const struct sockaddr& saddr, socklen_t size);

        /**
         * Create an UxAddr from a <code>struct sockaddr_in</code>.
         */
        explicit UxAddr (const struct sockaddr_un& saddr);

        /**
         * Destructor.
         */
        virtual ~UxAddr () = default; // Destructor.

        /**
         * Assignment operator.
         */
        virtual UxAddr& operator= (const UxAddr& addr);

        /**
         * Assignment operator.
         */
        UxAddr& operator= (const struct sockaddr_un& saddr);

        /**
         * Compare operator.
         */
        //bool operator== (const UxAddr& addr) const;

        /**
         * Not-equal operator.
         */
        //bool operator!= (const UxAddr& addr) const {
        //    return ! operator== (addr);
        //}

        /**
         * Less-than operator.
         */
        //bool operator< (const UxAddr& rhs) const;

        /**
         * Return the size of the address data.
         */
        virtual size_t size () const;

        /**
         * Get the pat to the Unix Domain Socket.
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
         */
        bool is_abstract () const;

        /**
         * Make a clone of this address object.
         */
        virtual std::shared_ptr<SockAddr> clone () const;

        /**
         * Return a string representation of the address.
         */
        virtual std::string to_string () const;
    };


}
#endif
