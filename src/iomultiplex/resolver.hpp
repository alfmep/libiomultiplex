/*
 * Copyright (C) 2021,2025 Dan Arrhenius <dan@ultramarin.se>
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
#ifndef IOMULTIPLEX_RESOLVER_HPP
#define IOMULTIPLEX_RESOLVER_HPP

#include <iomultiplex/ip_addr.hpp>
#include <string>
#include <vector>


namespace iomultiplex {

    /**
     * A DNS resolver.
     */
    class resolver {
    public:

        /**
         * Default constructor.
         */
        resolver () = default;

        /**
         * Destructor.
         */
        virtual ~resolver () = default;

        /**
         * Lookup a domain using a DNS SRV query.
         * @param domain The domain to look up.
         * @param proto The protocol to use. Default is any protocol.
         * @param service The service to use. Default is any service..
         * @param dns_fallback If the SRV query fails, fallback to a normal DNS host query.
         * @return A list of ip_addr objects.
         */
        virtual std::vector<ip_addr> lookup_srv (const std::string& domain,
                                                 const std::string& proto="any",
                                                 const std::string& service="",
                                                 bool dns_fallback=true);

        /**
         * Lookup a domain using a normal address resolution.
         * @param host The host to look up.
         * @param port An optional port number in host byte order. Default is 0.
         * @return A list of ip_addr objects.
         */
        virtual std::vector<ip_addr> lookup_host (const std::string& host,
                                                  const uint16_t port=0);
    };


}


#endif
