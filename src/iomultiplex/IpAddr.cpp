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
#include <iomultiplex/IpAddr.hpp>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <regex>
#include <arpa/inet.h>
#include <sys/socket.h>

#ifndef s6_addr16
#  define s6_addr16 __in6_u.__u6_addr16
#endif


namespace iomultiplex {


    const IpAddr IPv4Addr_any (0, 0);
    const IpAddr IPv6Addr_any ({0,0,0,0,0,0,0,0}, 0);


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IpAddr::IpAddr ()
    {
        memset (&sa, 0, sizeof(sa));
        ((struct sockaddr_in&)sa).sin_family = AF_INET;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IpAddr::IpAddr (const IpAddr& addr)
    {
        memcpy (&sa, &addr.sa, sizeof(sa));
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IpAddr::IpAddr (const struct sockaddr_in& saddr)
    {
        memset (&sa, 0, sizeof(sa));
        memcpy (&sa, &saddr, sizeof(sockaddr_in));
        ((struct sockaddr_in&)sa).sin_family = AF_INET;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IpAddr::IpAddr (const struct sockaddr_in6& saddr)
    {
        memset (&sa, 0, sizeof(sa));
        memcpy (&sa, &saddr, sizeof(struct sockaddr_in6));
        ((struct sockaddr_in&)sa).sin_family = AF_INET6;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IpAddr::IpAddr (uint32_t ipv4_addr, uint16_t port_num)
    {
        memset (&sa, 0, sizeof(sa));
        ipv4 (ipv4_addr);
        port (port_num);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IpAddr::IpAddr (uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port_num)
    {
        memset (&sa, 0, sizeof(sa));
        uint32_t addr;
        addr  = ((uint32_t)a) << 24;
        addr |= ((uint32_t)b) << 16;
        addr |= ((uint32_t)c) <<  8;
        addr |= ((uint32_t)d);
        ipv4 (addr);
        port (port_num);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IpAddr::IpAddr (const std::array<uint16_t, 8>& ipv6_addr, uint16_t port_num)
    {
        memset (&sa, 0, sizeof(sa));
        ipv6 (ipv6_addr);
        port (port_num);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IpAddr::IpAddr (uint16_t a0,
                    uint16_t a1,
                    uint16_t a2,
                    uint16_t a3,
                    uint16_t a4,
                    uint16_t a5,
                    uint16_t a6,
                    uint16_t a7,
                    uint16_t port_num)
    {
        memset (&sa, 0, sizeof(sa));
        std::array<uint16_t, 8> addr;
        addr[0] = a0;
        addr[1] = a1;
        addr[2] = a2;
        addr[3] = a3;
        addr[4] = a4;
        addr[5] = a5;
        addr[6] = a6;
        addr[7] = a7;
        ipv6 (addr);
        port (port_num);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IpAddr& IpAddr::operator= (const IpAddr& addr)
    {
        memcpy (&sa, &addr.sa, sizeof(sa));
        return *this;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IpAddr& IpAddr::operator= (const struct sockaddr_in& saddr)
    {
        memset (&sa, 0, sizeof(sa));
        memcpy (&sa, &saddr, sizeof(struct sockaddr_in));
        return *this;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IpAddr& IpAddr::operator= (const struct sockaddr_in6& saddr)
    {
        memset (&sa, 0, sizeof(sa));
        memcpy (&sa, &saddr, sizeof(struct sockaddr_in6));
        return *this;
    }


    /*
    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool IpAddr::operator== (const IpAddr& addr) const
    {
        return memcmp(&sa, &addr.sa, size()) == 0;
    }
    */

    /*
    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool IpAddr::operator< (const IpAddr& rhs) const
    {
        switch (sa.sa.sa_family) {
        case AF_INET:
            if (ipv4() < rhs.ipv4()) {
                return true;
            }else{
                if (ipv4() == rhs.ipv4())
                    return port() < rhs.port();
                return false;
            }

        case AF_INET6:
            {
                auto lhs_addr = ipv6 ();
                auto rhs_addr = rhs.ipv6 ();
                for (int i=0; i<8; ++i) {
                    if (lhs_addr[i] < rhs_addr[i])
                        return true;
                    else if (lhs_addr[i] != rhs_addr[i])
                        return false;
                }
                return port() < rhs.port();
            }

        default:
            {
                const char* lhs_addr = (const char*) &sa.sa;
                const char* rhs_addr = (const char*) &rhs.sa.sa;
                for (unsigned i=0; i<sizeof(sa.sa); ++i) {
                    if (lhs_addr[i] < rhs_addr[i])
                        return true;
                    else if (lhs_addr[i] != rhs_addr[i])
                        return false;
                }
                return false;
            }
        }
    }
    */

    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static bool parse_port (const char* str, int& port)
    {
        // Regular expression to validate a port number
        static std::regex regex_port_num ("0*(?:"
                                          "[0-9]|"
                                          "[1-9][0-9]{1,3}|"
                                          "[1-5][0-9]{4}|"
                                          "6[0-4][0-9]{3}|"
                                          "65[0-4]{2}|"
                                          "655[0-2][0-9]|"
                                          "6553[0-5]"
                                          ")");
        std::cmatch m;
        if (!regex_match(str, m, regex_port_num))
            return false;

        port = (uint16_t) atoi (str);
        return true;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool IpAddr::parse (const std::string& address, const bool also_parse_port)
    {
        std::string ip_str;
        bool success = false;
        bool try_ipv4 = true;
        bool try_ipv6 = true;
        int port_num = -1;

        if (address[0] == '[') {
            // This must be an IPv6 address
            try_ipv4 = false;
            auto pos = address.find ("]:");
            if (pos != std::string::npos) {
                // Check for a valid port number
                ip_str = address.substr (1, pos-1);
                if (!parse_port(address.c_str()+pos+2, port_num))
                    try_ipv6 = false; // Invalid port number
            }
            else if (address[address.size()-1] == ']') {
                ip_str = address.substr (1, address.size()-2);
            }
        }else{
            auto pos = address.find (".");
            if (pos != std::string::npos) {
                // This must be an IPv4 address
                try_ipv6 = false;
                pos = address.find (":");
                if (pos != std::string::npos) {
                    // Check for a valid port number
                    ip_str = address.substr (0, pos);
                    if (!parse_port(address.c_str()+pos+1, port_num))
                        try_ipv4 = false; // Invalid port number
                }
            }
        }

        const std::string& addr = ip_str.empty() ? address : ip_str;
        struct in_addr  ipv4addr;
        struct in6_addr ipv6addr;

        if (try_ipv4 && inet_pton(AF_INET, addr.c_str(), &ipv4addr)) {
            ((struct sockaddr_in&)sa).sin_family = AF_INET;
            ((struct sockaddr_in&)sa).sin_addr = ipv4addr;
            success = true;
        }
        else if (try_ipv6 && inet_pton(AF_INET6, addr.c_str(), &ipv6addr)) {
            ((struct sockaddr_in6&)sa).sin6_family = AF_INET6;
            ((struct sockaddr_in6&)sa).sin6_addr = ipv6addr;
            success = true;
        }
        if (success && port_num != -1)
            port ((uint16_t)port_num);

        return success;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    size_t IpAddr::size () const
    {
        switch (((struct sockaddr&)sa).sa_family) {
        case AF_INET:
            return sizeof (struct sockaddr_in);
        case AF_INET6:
            return sizeof (struct sockaddr_in6);
        default:
            return 0;
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    uint16_t IpAddr::port () const
    {
        switch (((struct sockaddr&)sa).sa_family) {
        case AF_INET:
            return ntohs (((struct sockaddr_in&)sa).sin_port);
        case AF_INET6:
            return ntohs (((struct sockaddr_in6&)sa).sin6_port);
        default:
            return 0;
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void IpAddr::port (const uint16_t port_num)
    {
        switch (((struct sockaddr&)sa).sa_family) {
        case AF_INET:
            ((struct sockaddr_in&)sa).sin_port = htons (port_num);
        case AF_INET6:
            ((struct sockaddr_in6&)sa).sin6_port = htons (port_num);
        default:
            ;
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    uint32_t IpAddr::ipv4 () const
    {
        return ntohl (((struct sockaddr_in&)sa).sin_addr.s_addr);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void IpAddr::ipv4 (const uint32_t addr)
    {
        ((struct sockaddr_in&)sa).sin_addr.s_addr = htonl (addr);
        ((struct sockaddr_in&)sa).sin_family = AF_INET;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::array<uint16_t, 8> IpAddr::ipv6 () const
    {
        std::array<uint16_t, 8> addr;
        addr[0] = ntohs (((struct sockaddr_in6&)sa).sin6_addr.s6_addr16[0]);
        addr[1] = ntohs (((struct sockaddr_in6&)sa).sin6_addr.s6_addr16[1]);
        addr[2] = ntohs (((struct sockaddr_in6&)sa).sin6_addr.s6_addr16[2]);
        addr[3] = ntohs (((struct sockaddr_in6&)sa).sin6_addr.s6_addr16[3]);
        addr[4] = ntohs (((struct sockaddr_in6&)sa).sin6_addr.s6_addr16[4]);
        addr[5] = ntohs (((struct sockaddr_in6&)sa).sin6_addr.s6_addr16[5]);
        addr[6] = ntohs (((struct sockaddr_in6&)sa).sin6_addr.s6_addr16[6]);
        addr[7] = ntohs (((struct sockaddr_in6&)sa).sin6_addr.s6_addr16[7]);
        return addr;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void IpAddr::ipv6 (const std::array<uint16_t, 8>& addr)
    {
        ((struct sockaddr_in6&)sa).sin6_addr.s6_addr16[0] = htons (addr[0]);
        ((struct sockaddr_in6&)sa).sin6_addr.s6_addr16[1] = htons (addr[1]);
        ((struct sockaddr_in6&)sa).sin6_addr.s6_addr16[2] = htons (addr[2]);
        ((struct sockaddr_in6&)sa).sin6_addr.s6_addr16[3] = htons (addr[3]);
        ((struct sockaddr_in6&)sa).sin6_addr.s6_addr16[4] = htons (addr[4]);
        ((struct sockaddr_in6&)sa).sin6_addr.s6_addr16[5] = htons (addr[5]);
        ((struct sockaddr_in6&)sa).sin6_addr.s6_addr16[6] = htons (addr[6]);
        ((struct sockaddr_in6&)sa).sin6_addr.s6_addr16[7] = htons (addr[7]);
        ((struct sockaddr_in6&)sa).sin6_family = AF_INET6;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::shared_ptr<SockAddr> IpAddr::clone () const
    {
        return std::make_shared<IpAddr> (*this);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::string IpAddr::to_string () const
    {
        return to_string (true);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::string IpAddr::to_string (bool include_port) const
    {
        std::stringstream ss;

        if (family() == AF_INET) {
            char tmp[INET_ADDRSTRLEN];
            inet_ntop (AF_INET, &(((struct sockaddr_in&)sa).sin_addr), tmp, sizeof(tmp));
            ss << tmp;
            if (include_port)
                ss << ':' << port();
        }
        else if (family() == AF_INET6) {
            char tmp[INET6_ADDRSTRLEN];
            inet_ntop (AF_INET6, &(((struct sockaddr_in6&)sa).sin6_addr), tmp, sizeof(tmp));
            if (include_port)
                ss << '[';
            ss << tmp;
            if (include_port)
                ss << "]:" << std::dec << port();
        }
        else {
            ss << "[n/a]";
        }
        return ss.str ();
    }


}
