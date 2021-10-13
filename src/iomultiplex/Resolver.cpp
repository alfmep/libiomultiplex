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
#include <iomultiplex/Resolver.hpp>
#include <iomultiplex/SockAddr.hpp>
#include <iomultiplex/SocketConnection.hpp>
#include <iomultiplex/Log.hpp>
#include <algorithm>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>


namespace iomultiplex {


    struct srv_record {
        uint16_t prio;
        uint16_t weight;
        uint16_t port;
        std::string target;
    };


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static std::vector<srv_record> do_srv_query (const std::string& query)
    {
        std::vector<srv_record> srv_records;
        union {
            HEADER hdr;
            u_char buf[4096];
        } response;

        Log::debug ("DNS SRV query: %s", query.c_str());

        // Make the SRV DNS query
        //
        int len = res_query (query.c_str(),
                             C_IN,  // ns_c_in
                             T_SRV, // ns_t_srv
                             response.buf,
                             sizeof(response.buf));

        // Check if we got an answer
        //
        if (len <= static_cast<int>(sizeof(HEADER)) || ntohs(response.hdr.ancount)==0) {
            Log::debug ("No response for DNS SRV query: %s", query.c_str());
            return srv_records;
        }

        // Set a pointer that points to the answer beyond the answer header.
        //
        u_char* ptr = response.buf;
        ptr += sizeof (HEADER);

        // Skip the query record(s)
        //
        for (short i=0; i<ntohs(response.hdr.qdcount); ++i) {
            char tmpbuf[256];
            int c = dn_expand (response.buf, response.buf+len, ptr, tmpbuf, sizeof(tmpbuf));
            if (c < 0) {
                Log::debug ("Error reading query record from DNS SRV answer");
                return srv_records;
            }
            ptr += c + QFIXEDSZ;
        }

        // Check the answer records for SRV records
        //
        for (short i=0; i<ntohs(response.hdr.ancount); ++i) {
            srv_record record;
            char tmpbuf[256];
            int c = dn_expand (response.buf, response.buf+len, ptr, tmpbuf, sizeof(tmpbuf));
            if (c < 0) {
                Log::debug ("Error reading answer record from DNS SRV answer");
                return srv_records;
            }
            ptr += c;

            // Get the record type
            uint16_t type = ntohs (*reinterpret_cast<uint16_t*>(ptr));
            ptr += sizeof (type);

            // Get the record class
            //uint16_t the_class = ntohs (*reinterpret_cast<uint16_t*>(ptr));
            ptr += sizeof (uint16_t);

            // Get TTL
            //uint32_t ttl = ntohl (*reinterpret_cast<uint32_t*>(ptr));
            ptr += sizeof (uint32_t);

            // Get data length
            uint16_t dlen = ntohs (*reinterpret_cast<uint16_t*>(ptr));
            ptr += sizeof (dlen);

            // Check if this is a SRV record
            if (type != T_SRV/*ns_t_srv*/) {
                ptr += dlen;
                continue;
            }

            // Get priority
            record.prio = ntohs (*reinterpret_cast<uint16_t*>(ptr));
            ptr += sizeof (record.prio);

            // Get weight
            record.weight = ntohs (*reinterpret_cast<uint16_t*>(ptr));
            ptr += sizeof (record.weight);

            // Get port
            record.port = ntohs (*reinterpret_cast<uint16_t*>(ptr));
            ptr += sizeof (record.port);

            // Get target
            c = dn_expand (response.buf, response.buf+len, ptr, tmpbuf, sizeof(tmpbuf));
            if (c < 0) {
                Log::debug ("Error reading target from DNS SRV answer");
                return srv_records;
            }
            ptr += c;

            record.target = std::string (tmpbuf);
            srv_records.push_back (record);
        }

        return srv_records;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::vector<IpAddr> Resolver::lookup_srv (const std::string& domain,
                                              const std::string& proto,
                                              const std::string& service,
                                              bool dns_fallback)
    {
        std::vector<IpAddr> addr_list;

        // Make the DNS SRV query string
        //
        std::string query = std::string("_") + service +
            std::string("._") + proto + std::string(".") + domain;

        // Do the DNS SRV query
        //
        std::vector<srv_record> srv_records = do_srv_query (query);

        // Sort to get the highest prio first
        //
        std::sort (srv_records.begin(),
                   srv_records.end(),
                   [](const srv_record& r1, const srv_record& r2)->bool {
                       return r1.prio!=r2.prio ? (r1.prio < r2.prio) : (r1.weight > r2.weight);
                   });

        // Check if the answer is a single record with "." as target.
        // If so, clear the record list.
        //
        if (srv_records.size()==1 && srv_records[0].target==".") {
            srv_records.clear ();
        }

        Log::debug ("DNS SRV query gave %u response(s) for domain: %s",
                    srv_records.size(), domain.c_str());

        // If no SRV records are found, fall back to a normal address lookup.
        //
        if (srv_records.empty() && dns_fallback) {
            Log::debug ("DNS SRV query gave no response, "
                        "using normal address resolution for %s", domain.c_str());
            struct servent* se = getservbyname (service.c_str(), proto.c_str());
            uint16_t port {0};
            if (se)
                port = ntohs (se->s_port);
            else
                Log::debug ("Unknown service '%s', using port 0", service.c_str());

            return lookup_host (domain, port);
        }

        for (auto srv_rec : srv_records) {
            Log::debug ("Get IP addresses for %s", srv_rec.target.c_str());
            std::vector<IpAddr> addresses = lookup_host (srv_rec.target, srv_rec.port);
            for (auto& addr : addresses)
                addr_list.push_back (addr);
        }

        return addr_list;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::vector<IpAddr> Resolver::lookup_host (const std::string& hostname,
                                               const uint16_t port)
    {
        std::vector<IpAddr> addr_list;

        int result;
        struct addrinfo* ai_list;
        struct addrinfo* ai;
        struct addrinfo  hints;

        std::memset (&hints, 0, sizeof(hints));
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = 0;
        hints.ai_flags    = /*AI_V4MAPPED |*/ /*AI_CANONNAME |*/ AI_ADDRCONFIG;

        result = getaddrinfo (hostname.c_str(), nullptr, &hints, &ai_list);
        if (result) {
            Log::info ("Unable to resolve host %s: %s",
                       hostname.c_str(), gai_strerror(result));
            return addr_list;
        }

        for (ai=ai_list; ai!=nullptr; ai=ai->ai_next) {
            IpAddr addr;
            if (ai->ai_family==AF_INET)
                addr = (struct sockaddr_in&) *ai->ai_addr;
            else if (ai->ai_family==AF_INET6)
                addr = (struct sockaddr_in6&) *ai->ai_addr;
            else
                continue;
            addr.port (port);
            addr_list.emplace_back (addr);
        }
        if (ai_list)
            freeaddrinfo (ai_list);

        return addr_list;
    }


}
