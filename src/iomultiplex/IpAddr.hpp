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
#ifndef IOMULTIPLEX_IPADDR_HPP
#define IOMULTIPLEX_IPADDR_HPP

#include <iomultiplex/SockAddr.hpp>
#include <array>
#include <string>
#include <stdexcept>
#include <functional>
#include <cstdint>
#include <netinet/in.h>


namespace iomultiplex {


    /**
     * An IPv4 or IPv6 address.
     */
    class IpAddr : public SockAddr {
    public:
        /**
         * Default constructor.
         * Constructs an IPv4 address with all values set to zero.
         */
        IpAddr ();

        /**
         * Copy constructor.
         * @param addr The IpAddr object to copy.
         */
        IpAddr (const IpAddr& addr);

        /**
         * Create an IpAddr from a <code>struct sockaddr_in</code>.
         * @param saddr A sockaddr_in object to copy.
         */
        explicit IpAddr (const struct sockaddr_in& saddr);

        /**
         * Create an IpAddr from a <code>struct sockaddr_in6</code>.
         * @param saddr A sockaddr_in6 object to copy.
         */
        explicit IpAddr (const struct sockaddr_in6& saddr);

        /**
         * Create an IPv4 address.
         * @param ipv4_addr A 32 bit IPv4 address in host byte order.
         * @param port_num A port number in host byte order.
         */
        explicit IpAddr (uint32_t ipv4_addr, uint16_t port_num=0);

        /**
         * Create an IPv4 address in format <code>a.b.c.d</code>.
         * @param a First byte in the IPv4 address to copy.
         * @param b Second byte in the IPv4 address to copy.
         * @param c Third byte in the IPv4 address to copy.
         * @param d Fourth byte in the IPv4 address to copy.
         * @param port_num A port number in host byte order.
         */
        IpAddr (uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port_num=0);

        /**
         * Create an IPv6 address in format <code>[a0:a1:a2:a3:a4:a5:a6:a7]</code>.
         * \note All 16 bit parameters are in host byte order.
         * @param a0 The highlighted 16 bits in the IPv6 address:
                     <code>[</code><b><code>a0</code></b><code>:a1:a2:a3:a4:a5:a6:a7]</code>
         * @param a1 The highlighted 16 bits in the IPv6 address:
                     <code>[a0:</code><b><code>a1</code></b><code>:a2:a3:a4:a5:a6:a7]</code>
         * @param a2 The highlighted 16 bits in the IPv6 address:
                     <code>[a0:a1:</code><b><code>a2</code></b><code>:a3:a4:a5:a6:a7]</code>
         * @param a3 The highlighted 16 bits in the IPv6 address:
                     <code>[a0:a1:a2:</code><b><code>a3</code></b><code>:a4:a5:a6:a7]</code>
         * @param a4 The highlighted 16 bits in the IPv6 address:
                     <code>[a0:a1:a2:a3:</code><b><code>a4</code></b><code>:a5:a6:a7]</code>
         * @param a5 The highlighted 16 bits in the IPv6 address:
                     <code>[a0:a1:a2:a3:a4:</code><b><code>a5</code></b><code>:a6:a7]</code>
         * @param a6 The highlighted 16 bits in the IPv6 address:
                     <code>[a0:a1:a2:a3:a4:a5:</code><b><code>a6</code></b><code>:a7]</code>
         * @param a7 The highlighted 16 bits in the IPv6 address:
                     <code>[a0:a1:a2:a3:a4:a5:a6:</code><b><code>a7</code></b><code>]</code>
         * @param port_num A port number in host byte order.
         */
        IpAddr (uint16_t a0,
                uint16_t a1,
                uint16_t a2,
                uint16_t a3,
                uint16_t a4,
                uint16_t a5,
                uint16_t a6,
                uint16_t a7,
                uint16_t port_num=0);

        /**
         * Parse a string and Create an IP address:port.
         * @param address A string containing an IPv[4|6] address and optionally a port number.
         * @throw std::invalid_argument on parse error.
         */
        explicit IpAddr (const std::string& address);

        /**
         * Parse a string and Create an IP address.
         * @param address A string containing an IPv[4|6] address.
         * @param port_num A port number.
         * @throw std::invalid_argument on parse error.
         */
        explicit IpAddr (const std::string& address, uint16_t port_num);

        /**
         * Destructor.
         */
        virtual ~IpAddr () = default; // Destructor.

        /**
         * Assignment operator.
         * @param addr The address to copy.
         * @return A reference to this object.
         */
        IpAddr& operator= (const IpAddr& addr);

        /**
         * Assignment operator.
         * @param saddr The address to copy.
         * @return A reference to this object.
         */
        IpAddr& operator= (const struct sockaddr_in& saddr);

        /**
         * Assignment operator.
         * @param saddr The address to copy.
         * @return A reference to this object.
         */
        IpAddr& operator= (const struct sockaddr_in6& saddr);

        /**
         * Parse an address from a string.
         * @param address The string to parse.
         * @param parse_port If <code>true</code>,
         *                   also check for a port number in the string.
         * @return <code>true</code> on success, <code>false</code> on failure.
         */
        bool parse (const std::string& address, const bool parse_port=true);

        virtual size_t size () const;

        /**
         * Return the port number in host byte order.
         * @return The port number in host byte order.
         */
        uint16_t port () const;

        /**
         * Set the port number in host byte order.
         * @param port_num A port number in host byte order.
         */
        void port (const uint16_t port_num);

        /**
         * Return a 32 bit IPv4 address in host byte order.
         * @return The IPv4 address as a 32 bit integer in host byte order.
         */
        uint32_t ipv4 () const;

        /**
         * Set a 32 bit IPv4 address.
         * This will also set the address type to <code>AddrType::ipv4</code>.
         * @param addr A 32 bit IPv4 address in host byte order.
         */
        void ipv4 (const uint32_t addr);

        /**
         * Return a 128 bit IPv6 address in host byte order.
         * @return A 128 bit IPv6 address in host byte order.
         */
        std::array<uint16_t, 8> ipv6 () const;

        /**
         * Set a 128 bit IPv6 address.
         * This will also set the address type to AddrType::ipv6.
         * @param addr A 128 bit IPv6 address in host byte order.
         */
        void ipv6 (const std::array<uint16_t, 8>& addr);

        virtual std::shared_ptr<SockAddr> clone () const;
        virtual std::string to_string () const;

        /**
         * Return a string representation of the IP address (optionally) including port number.
         * @param include_port If <code>true</code>, include the port number.
         * @return A string representation of the IP address.
         */
        std::string to_string (bool include_port) const;
    };


    /**
     * Empty IP address corresponting to any IPv4 address and port.
     */
    extern const IpAddr IPv4Addr_any;

    /**
     * Empty IP address corresponting to any IPv6 address and port.
     */
    extern const IpAddr IPv6Addr_any;

}


#endif
