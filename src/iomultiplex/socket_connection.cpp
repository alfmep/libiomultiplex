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
#include <iomultiplex/socket_connection.hpp>
#include <iomultiplex/iohandler_base.hpp>
#include <iomultiplex/log.hpp>
#include <atomic>
#include <map>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>


//#define TRACE_DEBUG

#ifdef TRACE_DEBUG
#define THIS_FILE "socket_connection.cpp"
#define TRACE(format, ...) log::debug("[%u] %s:%s:%d: " format, gettid(), THIS_FILE, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#define TRACE(format, ...)
#endif


namespace iomultiplex {


    class sc_invalid_sockaddr : public sock_addr {
    public:
        sc_invalid_sockaddr () = default;
        virtual ~sc_invalid_sockaddr () = default;
        virtual size_t size () const;
        virtual std::shared_ptr<sock_addr> clone () const;
        virtual std::string to_string () const;
        void family (int addr_family);
    };
    size_t sc_invalid_sockaddr::size () const {
        return 0;
    }
    std::shared_ptr<sock_addr> sc_invalid_sockaddr::clone () const {
        return std::make_shared<sc_invalid_sockaddr> ();
    }
    std::string sc_invalid_sockaddr::to_string () const {
        return "";
    }
    void sc_invalid_sockaddr::family (int addr_family) {
        ((struct sockaddr&)sa).sa_family = addr_family;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    const std::string sock_type_to_string (const int type)
    {
        switch (type & ~(SOCK_NONBLOCK|SOCK_CLOEXEC)) {
        case 0:
            return "any";
        case SOCK_STREAM:
            return "SOCK_STREAM";
        case SOCK_DGRAM:
            return "SOCK_DGRAM";
        case SOCK_SEQPACKET:
            return "SOCK_SEQPACKET";
        case SOCK_RAW:
            return "SOCK_RAW";
        case SOCK_RDM:
            return "SOCK_RDM";
        case SOCK_PACKET:
            return "SOCK_PACKET";
        case SOCK_DCCP:
            return "SOCK_DCCP";
        default:
            return "n/a";
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    const std::string sock_family_to_string (const int family)
    {
        switch (family) {
        case AF_UNIX:
            return "AF_UNIX";
        case AF_INET:
            return "AF_INET";
        case AF_INET6:
            return "AF_INET6";
        case AF_IPX:
            return "AF_IPX";
        case AF_NETLINK:
            return "AF_NETLINK";
        case AF_X25:
            return "AF_X25";
        case AF_AX25:
            return "AF_AX25";
        case AF_ATMPVC:
            return "AF_ATMPVC";
        case AF_APPLETALK:
            return "AF_APPLETALK";
        case AF_PACKET:
            return "AF_PACKET";
        case AF_ALG:
            return "AF_ALG";
        default:
            return "n/a";
        };
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    const std::string& sock_proto_to_string (const int protocol)
    {
        static std::map<int, std::string> proto_map;
        static const std::string n_a = "";

        auto i = proto_map.find (protocol);
        if (i != proto_map.end()) {
            errno = 0;
            return i->second;
        }else{
            struct protoent* p = getprotobynumber (protocol);
            if (p) {
                auto ib = proto_map.emplace (protocol, std::string(p->p_name));
                errno = 0;
                return ib.first->second;
            }else{
                errno = ENOPROTOOPT;
                return n_a;
            }
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int sock_proto_by_name (const std::string& protocol)
    {
        static std::map<std::string, int> proto_map = {{"any", 0}};
        int proto_num;

        auto i = proto_map.find (protocol);
        if (i != proto_map.end()) {
            errno = 0;
            proto_num = i->second;
        }else{
            struct protoent* p = getprotobyname (protocol.c_str());
            if (p) {
                proto_map.emplace (protocol, p->p_proto);
                errno = 0;
                proto_num = p->p_proto;
            }else{
                errno = ENOPROTOOPT;
                proto_num = -1;
            }
        }
        return proto_num;
    }



    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    socket_connection::socket_connection (iohandler_base& io_handler)
        : fd_connection  (io_handler),
          connected      {false},
          bound          {false},
          def_sock_rx_cb {nullptr},
          def_sock_tx_cb {nullptr}
    {
        TRACE ("Constructing a socket connection");
        local_addr = std::make_shared<sc_invalid_sockaddr> ();
        peer_addr  = std::make_shared<sc_invalid_sockaddr> ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    socket_connection::socket_connection (socket_connection&& rhs)
        : fd_connection  (std::move(rhs)),
          connected      {rhs.connected.exchange(false)},
          bound          {rhs.bound.exchange(false)},
          local_addr     {std::move(rhs.local_addr)},
          peer_addr      {std::move(rhs.peer_addr)},
          def_sock_rx_cb {std::move(rhs.def_sock_rx_cb)},
          def_sock_tx_cb {std::move(rhs.def_sock_tx_cb)}
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    socket_connection::~socket_connection ()
    {
        close ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    socket_connection& socket_connection::operator= (socket_connection&& rhs)
    {
        if (this != &rhs) {
            fd_connection::operator= (std::move(rhs));
            connected      = rhs.connected.exchange (false);
            bound          = rhs.bound.exchange (false);
            local_addr     = std::move (rhs.local_addr);
            peer_addr      = std::move (rhs.peer_addr);
            def_sock_rx_cb = std::move (rhs.def_sock_rx_cb);
            def_sock_tx_cb = std::move (rhs.def_sock_tx_cb);
        }
        return *this;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int socket_connection::open (int domain, int type, int proto, bool cloexec)
    {
        errno = 0;
        if (handle() == -1) {
            if (proto==-1) {
                errno = ENOPROTOOPT;
                return -1;
            }
            TRACE ("Open socket, domain: %s, type: %s, protocol: %s",
                   sock_family_to_string(domain).c_str(),
                   sock_type_to_string(type).c_str(),
                   sock_proto_to_string(proto).c_str());
            type |= cloexec ? SOCK_NONBLOCK|SOCK_CLOEXEC : SOCK_NONBLOCK;
            fd = socket (domain, type, proto);
            if (fd == -1) {
                auto errnum = errno;
                TRACE ("socket() failed: %s", strerror(errno));
                errno = errnum;
                return -1;
            }
            dynamic_cast<sc_invalid_sockaddr&>(*local_addr).family (domain);
        }
        return 0;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void socket_connection::close ()
    {
        if (handle() < 0)
            return;

        TRACE ("Closing socket %d", handle());

        // Use parent class for closing the socket.
        fd_connection::close ();
        connected = false;
        bound = false;
        local_addr = std::make_shared<sc_invalid_sockaddr> ();
        peer_addr  = std::make_shared<sc_invalid_sockaddr> ();
        TRACE ("Socket is closed");
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int socket_connection::type ()
    {
        return getsockopt (SO_TYPE);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int socket_connection::protocol ()
    {
        return getsockopt (SO_PROTOCOL);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int socket_connection::bind (const sock_addr& addr)
    {
        errno = 0;
        // Sanity checks
        //
        if (handle() < 0) {
            TRACE ("bind() failed: Socket not open");
            errno = EBADF;
            return -1;
        }
        if (addr.family() != local_addr->family()) {
            TRACE ("bind() failed: invalid socket type: %s, expected: %s",
                   sock_family_to_string(addr.family()).c_str(),
                   sock_family_to_string(local_addr->family()).c_str());
            errno = EAFNOSUPPORT;
            return -1;
        }

        local_addr = addr.clone ();

        TRACE ("Bind socket %d to address %s", handle(), addr.to_string().c_str());
        if (::bind(handle(), local_addr->data(), local_addr->size())) {
#ifdef TRACE_DEBUG
            auto errnum = errno; // The log function may change errno
            TRACE ("bind() failed: %s", strerror(errnum));
            errno = errnum; // Restore errno
#endif
            return -1;
        }
        bound = true;
        return 0;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    const sock_addr& socket_connection::addr () const
    {
        return *local_addr;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    const sock_addr& socket_connection::peer () const
    {
        return *peer_addr;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int socket_connection::connect (const sock_addr& addr,
                                    connect_cb_t callback,
                                    unsigned timeout)
    {
        // Sanity checks
        //
        if (handle() < 0) {
            TRACE ("connect() failed: Socket not open");
            errno = EBADF;
            return -1;
        }
        if (addr.family() != local_addr->family()) {
            TRACE ("connect() failed: invalid socket addres type");
            errno = EAFNOSUPPORT;
            return -1;
        }

        TRACE ("Connect socket %d to %s", handle(), addr.to_string().c_str());
        errno = 0;

        // Initiate the connection
        auto result = ::connect (handle(), addr.data(), addr.size());
        if (result==-1 && errno!=EINPROGRESS) {
#ifdef TRACE_DEBUG
            auto errnum = errno; // The log function may change errno
            TRACE ("connect() socket %d failed to connect to %s: %s",
                   handle(), addr.to_string().c_str(), strerror(errnum));
            errno = errnum; // Restore errno
#endif
            return -1;
        }

        // Update the remote and local address
        peer_addr = addr.clone ();
        if (local_addr->size()==0)
            local_addr = peer_addr->clone ();
        socklen_t slen = local_addr->size ();
        if (getsockname(handle(), const_cast<struct sockaddr*>(local_addr->data()), &slen))
            local_addr->clear ();

        if (callback==nullptr && result==0) {
            TRACE ("Socket %d connected to %s", handle(), peer_addr->to_string().c_str());
            connected = true;
        }else{
            // Wait in background for connection to finish
            result = wait_for_tx ([this, callback](io_result_t& ior)->bool{
                    if (ior.errnum == 0) {
                        socklen_t optlen = sizeof (ior.errnum);
                        if (getsockopt(SO_ERROR, &ior.errnum, &optlen))
                            ior.errnum = errno; // getsockopt failed
                    }
                    connected = ior.errnum == 0;
#ifdef TRACE_DEBUG
                    if (connected)
                        TRACE ("Socket %d connected to %s", handle(), peer_addr->to_string().c_str());
                    else
                        TRACE ("Socket %d failed connection to %s: %s",
                               handle(), peer_addr->to_string().c_str(), strerror(ior.errnum));;
#endif
                    if (callback)
                        callback (*this, ior.errnum);
                    return true;
                }, timeout);
        }
        if (result)
            return -1;

        errno = 0;
        return 0;
    }


    //--------------------------------------------------------------------------
    // Synchronized operation using a datagram socket
    //--------------------------------------------------------------------------
    int socket_connection::connect_using_datagram (const sock_addr& addr)
    {
        TRACE ("Connecting a datagram socket");

        // Sanity checks
        if (handle() < 0) {
            TRACE ("connect() failed: Socket not open");
            errno = EBADF;
            return -1;
        }
        if (addr.family() != local_addr->family()) {
            TRACE ("connect() failed: invalid socket type");
            errno = EAFNOSUPPORT;
            return -1;
        }

        errno = 0;
        auto result = ::connect (handle(), addr.data(), addr.size());
        if (result) {
#ifdef TRACE_DEBUG
            auto errnum = errno; // The log function may change errno
            TRACE ("connect() socket %d failed to connect to %s: %s",
                   handle(), addr.to_string().c_str(), strerror(errnum));
            errno = errnum; // Restore errno
#endif
            return -1;
        }

        // Update the remote and local address
        peer_addr = addr.clone ();
        if (local_addr->size()==0)
            local_addr = peer_addr->clone ();
        socklen_t slen = local_addr->size ();
        if (getsockname(handle(), const_cast<struct sockaddr*>(local_addr->data()), &slen))
            local_addr->clear ();

        TRACE ("Socket %d connected to %s", handle(), peer_addr->to_string().c_str());
        connected = true;
        return 0;
    }


    //--------------------------------------------------------------------------
    // Synchronized operation
    // (assumes the I/O handler running in another thread)
    //--------------------------------------------------------------------------
    int socket_connection::connect (const sock_addr& addr, unsigned timeout)
    {
        if (type() == SOCK_DGRAM)
            return connect_using_datagram (addr);

        if (io_handler().same_context()) {
            errno = EDEADLK;
            return -1;
        }

        TRACE ("connecting a stream socket");

        bool io_done = false;
        int errnum = 0;
        int retval = -1;
        errno = 0;

        // Initiate connection
        if (connect(addr,
                    [this, &errnum, &io_done](socket_connection& conn, int err){
                        std::unique_lock<std::mutex> lock (sync_mutex);
                        io_done = true;
                        errnum = err;
                        sync_cond.notify_one ();
                    },
                    timeout) == 0)
        {
            // Wait for connection to finish or timeout
            std::unique_lock<std::mutex> lock (sync_mutex);
            sync_cond.wait (lock, [&io_done]{return io_done;});
            if (!errnum)
                retval = 0;
            errno = errnum;
        }
        return retval;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    const bool socket_connection::is_connected () const
    {
        return connected;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    const bool socket_connection::is_bound () const
    {
        return bound;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int socket_connection::listen (int backlog)
    {
        if (handle() < 0) {
            TRACE ("listen() failed: Socket not open");
            errno = EBADF;
            return -1;
        }
        errno = 0;
        TRACE ("Set socket %d to listen state", handle());
        auto result = ::listen (handle(), backlog);
#ifdef TRACE_DEBUG
        if (result) {
            auto errnum = errno; // The log function may change errno
            TRACE ("listen() failed: %s", strerror(errnum));
            errno = errnum; // Restore errno
        }
#endif
        return result;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int socket_connection::accept (accept_cb_t callback, unsigned timeout)
    {
        if (handle() < 0) {
            TRACE ("accept() failed: Socket not open");
            errno = EBADF;
            return -1;
        }

        TRACE ("Accept connections to socket %d", handle());
        errno = 0;

        // Wait until we have incoming data before we make the
        // call to accept().
        wait_for_rx ([this, callback](io_result_t& ior)->bool{
                handle_accept_result (callback, ior.errnum);
                return false;
            }, timeout);
        return 0;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void socket_connection::handle_accept_result (accept_cb_t cb, int errnum)
    {
        if (!cb)
            return;

        // Allocate a new client socket for the incoming connection
        auto client_sock = std::make_shared<socket_connection> (io_handler());

        if (errnum == 0) {
            client_sock->peer_addr = local_addr->clone ();
            client_sock->peer_addr->clear ();
            socklen_t slen = client_sock->peer_addr->size ();
            auto result = ::accept4 (handle(),
                                     const_cast<struct sockaddr*>(client_sock->peer_addr->data()),
                                     &slen,
                                     SOCK_NONBLOCK);
            errnum = result<0 ? errno : 0;
            client_sock->fd = result;
            if (client_sock->handle() >= 0) {
                client_sock->connected  = true;
                client_sock->bound      = true;
                client_sock->local_addr = local_addr->clone ();
                TRACE ("Socket %d accepted a connection", handle());
            }
        }
#ifdef TRACE_DEBUG
        if (errnum != 0) {
            TRACE ("accept() Socket %d failed to accept a connection: %s",
                   handle(), strerror(errnum));
        }
#endif
        cb (*this, client_sock, errnum);
    }


    //--------------------------------------------------------------------------
    // Synchronized operation
    // (assumes the I/O handler running in another thread)
    //--------------------------------------------------------------------------
    std::shared_ptr<socket_connection> socket_connection::accept (unsigned timeout)
    {
        bool io_done = false;
        int errnum = 0;
        std::shared_ptr<socket_connection> client;

        if (io_handler().same_context()) {
            errno = EDEADLK;
            return client;
        }
        errno = 0;

        // Start accepting connections
        if (accept([this, &errnum, &io_done, &client](socket_connection& s, std::shared_ptr<socket_connection> c, int err){
                    std::unique_lock<std::mutex> lock (sync_mutex);
                    io_done = true;
                    errnum = err;
                    if (!errnum)
                        client = c;
                    sync_cond.notify_one ();
                },
                timeout) == 0)
        {
            // Wait for a new connection or timeout
            std::unique_lock<std::mutex> lock (sync_mutex);
            sync_cond.wait (lock, [&io_done]{return io_done;});
            errno = errnum;
        }
        return client;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int socket_connection::recvfrom (void* buf, size_t size,
                                     peer_io_callback_t rx_cb,
                                     unsigned timeout)
    {
        if (handle() < 0) {
            TRACE ("recvfrom() failed: Socket not open");
            errno = EBADF;
            return -1;
        }
        errno = 0;
        return wait_for_rx ([this, buf, size, rx_cb](io_result_t& ior)->bool{
                auto peer = local_addr->clone ();
                peer->clear ();
                ior.buf = const_cast<void*> (buf);
                ior.size = size;
                if (ior.errnum == 0) {
                    ior.result = do_recvfrom (ior.buf, ior.size, 0, *peer);
                    ior.errnum = ior.result<0 ? errno : 0;
                }
                if (rx_cb)
                    rx_cb (*this, ior, *peer);
                else if (def_sock_rx_cb)
                    def_sock_rx_cb (*this, ior, *peer);
                return false;
            },
            timeout);
    }


    //--------------------------------------------------------------------------
    // Synchronized operation
    // (assumes the I/O handler running in another thread)
    //--------------------------------------------------------------------------
    std::shared_ptr<sock_addr> socket_connection::recvfrom (void* buf,
                                                            ssize_t& size,
                                                            unsigned timeout)
    {
        bool io_done = false;
        int errnum = 0;
        std::shared_ptr<sock_addr> peer;

        if (io_handler().same_context()) {
            errno = EDEADLK;
            return peer;
        }
        errno = 0;

        // Queue a read operation
        if (recvfrom(buf,
                     (size_t)size,
                     [this, &size, &io_done, &errnum, &peer](socket_connection& sock,
                                                             io_result_t& ior,
                                                             const sock_addr& pa)
                     {
                         std::unique_lock<std::mutex> lock (sync_mutex);
                         io_done = true;
                         errnum = ior.errnum;
                         size = ior.result;
                         if (!errnum)
                             peer = pa.clone();
                         sync_cond.notify_one ();
                     },
                     timeout) == 0)
        {
            // Wait for read operation to finish or timeout
            std::unique_lock<std::mutex> lock (sync_mutex);
            sync_cond.wait (lock, [&io_done]{return io_done;});
            errno = errnum;
        }else{
            size = -1;
        }
        return peer;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int socket_connection::sendto (const void* buf,
                                   size_t size,
                                   const sock_addr& peer,
                                   peer_io_callback_t tx_cb,
                                   unsigned timeout)
    {
        if (handle() < 0) {
            TRACE ("sendto() failed: Socket not open");
            errno = EBADF;
            return -1;
        }
        if (peer.family() != local_addr->family()) {
            TRACE ("sendto() failed: invalid peer socket type");
            errno = EAFNOSUPPORT;
            return -1;
        }

        errno = 0;
        return wait_for_tx ([this, buf, size, addr=peer.clone(), tx_cb](io_result_t& ior)->bool{
                ior.buf = const_cast<void*> (buf);
                ior.size = size;
                if (ior.result >= 0) {
                    ior.result = do_sendto (buf, size, 0, *addr);
                    if (ior.result < 0) {
                        ior.errnum = errno;
                    }
                    else if (local_addr->size()==0) {
                        // Update local address if not bound to one
                        local_addr = addr->clone ();
                        socklen_t slen = local_addr->size ();
                        if (getsockname(handle(), const_cast<struct sockaddr*>(local_addr->data()), &slen))
                            local_addr->clear ();
                    }
                }
                if (tx_cb)
                    tx_cb (*this, ior, *addr);
                else if (def_sock_tx_cb)
                    def_sock_tx_cb (*this, ior, *addr);
                return false;
            },
            timeout);
    }


    //--------------------------------------------------------------------------
    // Synchronized operation
    // (assumes the I/O handler running in another thread)
    //--------------------------------------------------------------------------
    ssize_t socket_connection::sendto (const void* buf, size_t size, const sock_addr& peer, unsigned timeout)
    {
        if (io_handler().same_context()) {
            errno = EDEADLK;
            return -1;
        }
        ssize_t result = -1;
        bool io_done = false;
        int errnum = 0;
        errno = 0;

        // Queue a send operation
        if (sendto(buf,
                   size,
                   peer,
                   [this, &result, &io_done, &errnum](socket_connection& sock,
                                                      io_result_t& ior,
                                                      const sock_addr& peer)
                   {
                       std::unique_lock<std::mutex> lock (sync_mutex);
                       result = ior.result;
                       errnum = ior.errnum;
                       io_done = true;
                       sync_cond.notify_one ();
                   },
                   timeout) == 0)
        {
            // Wait for the write operation to finish or timeout
            std::unique_lock<std::mutex> lock (sync_mutex);
            sync_cond.wait (lock, [&io_done]{return io_done;});
            errno = errnum;
        }
        return result;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t socket_connection::do_recvfrom (void* buf, size_t len, int flags, sock_addr& peer)
    {
        socklen_t peer_addr_size = addr().size ();
        return ::recvfrom (handle(), buf, len, 0, const_cast<sockaddr*>(peer.data()), &peer_addr_size);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t socket_connection::do_sendto (const void* buf, size_t len, int flags, const sock_addr& addr)
    {
        return ::sendto (handle(), buf, len, 0, addr.data(), addr.size());
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int socket_connection::getsockopt (int optname)
    {
        int optval;
        socklen_t optlen = sizeof (optval);
        if (::getsockopt(handle(), SOL_SOCKET, optname, &optval, &optlen))
            return -1;
        else
            return optval;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int socket_connection::getsockopt (int level, int optname)
    {
        int optval;
        socklen_t optlen = sizeof (optval);
        if (::getsockopt(handle(), level, optname, &optval, &optlen))
            return -1;
        else
            return optval;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int socket_connection::getsockopt (int optname, void* optval, socklen_t* optlen)
    {
        return ::getsockopt (handle(), SOL_SOCKET, optname, optval, optlen);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int socket_connection::getsockopt (int level, int optname, void* optval, socklen_t* optlen)
    {
        return ::getsockopt (handle(), level, optname, optval, optlen);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int socket_connection::setsockopt (int optname, const int value)
    {
        socklen_t len = sizeof (value);
        errno = 0;
        return ::setsockopt (handle(), SOL_SOCKET, optname, &value, len);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int socket_connection::setsockopt (int level, int optname, const int value)
    {
        socklen_t len = sizeof (value);
        errno = 0;
        return ::setsockopt (handle(), level, optname, &value, len);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int socket_connection::setsockopt (int optname, const void* optval, socklen_t optlen)
    {
        errno = 0;
        return ::setsockopt (handle(), SOL_SOCKET, optname, optval, optlen);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int socket_connection::setsockopt (int level, int optname, const void* optval, socklen_t optlen)
    {
        errno = 0;
        return ::setsockopt (handle(), level, optname, optval, optlen);
    }

}
