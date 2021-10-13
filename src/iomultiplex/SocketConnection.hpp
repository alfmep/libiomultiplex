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
#ifndef IOMULTIPLEX_SOCKETCONNECTION_HPP
#define IOMULTIPLEX_SOCKETCONNECTION_HPP

#include <iomultiplex/FdConnection.hpp>
#include <iomultiplex/SockAddr.hpp>
#include <iomultiplex/IpAddr.hpp>
#include <functional>
#include <atomic>
#include <memory>


namespace iomultiplex {

    // Forward declarations
    class IOHandler;
    class SocketConnection;

    using SocketConnectionPtr = std::shared_ptr<SocketConnection>;


    const std::string  sock_family_to_string (const int family);
    const std::string  sock_type_to_string (const int type);
    const std::string& sock_proto_to_string (const int proto);
    int sock_proto_by_name (const std::string& protocol);


    /**
     * A TCP/IP network socket connection.
     */
    class SocketConnection : public FdConnection {
    public:
        /**
         * Callback that is called when a socket connection is made (or failed).
         * @param connection The connection object that attempted to connect to some host.
         * @param errnum The value of errno after the connection attempt. 0 if successful.
         */
        using connect_cb_t = std::function<void (SocketConnection& connection, int errnum)>;

        /**
         * Callback that is called when a socket connection is accepted (or failed to accept).
         * @param server_conn The connection object that has accepted the connection.
         * @param client_conn A pointer to a connection object representing the established connection.
         * @param errnum The value of errno after the connection attempt. 0 if successful.
         */
        using accept_cb_t = std::function<void (SocketConnection& server_conn,
                                                std::shared_ptr<SocketConnection> client_conn,
                                                int errnum)>;

        /**
         * Socket I/O callback.
         * @param ior I/O operation result.
         * @param peer_addr The address of the peer we are receiving from or sendig to.
         * @return <code>true</code> if the IOHandler should continue to try handling
         *         I/O operations on this connection before waiting for new events.
         */
        using peer_io_callback_t = std::function<bool (io_result_t& ior, const SockAddr& peer_addr)>;

        /**
         * Constructor.
         * @param io_handler The IOHandler object handling the I/O operations.
         */
        SocketConnection (IOHandler& io_handler);

        /**
         * Move constructor.
         */
        SocketConnection (SocketConnection&& rhs);

        /**
         * Destructor.
         */
        virtual ~SocketConnection ();

        /**
         * Move operator.
         */
        SocketConnection& operator= (SocketConnection&& rhs);

        /**
         * Open the socket.
         * @param domain The communication domain.
         *               Normally AF_INET or AT_INET6 for an IPv4 or IPv6 socket.
         * @param type The type of socket.
         *             Normally SOCK_STREAM or SOCK_DGRAM.
         * @param protocol The protocol to use.
         *                 Normally 0 for the default protocol used by the socket type.
         * @param close_on_exec If <code>true</code>,
         *                         Set the close-on-exec (FD_CLOEXEC) flag
         *                         on the new file descriptor.
         * @return 0 on success, -1 on error and <code>errno</code> is set.
         */
        int open (int domain, int type, int protocol=0, bool close_on_exec=false);

        /**
         * Close the socket.
         * Cancel all pending I/O operations and close the socket.
         */
        virtual void close ();

        /**
         * Return the type of socket. Normally SOCK_STREAM or SOCK_DGRAM.
         */
        int type ();

        /**
         * Return the socket protocol.
         */
        int protocol ();

        /**
         * Bind to a local address.
         * @param addr The address we want to bind the socket to.
         */
        int bind (const SockAddr& addr);

        /**
         * Make a connection to a remote address.
         * @param addr The host to connect to.
         * @param callback A function that is called when the connection
         *                 is finished or has failed.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         */
        int connect (const SockAddr& addr, connect_cb_t callback, unsigned timeout=-1);

        /**
         * Make a synchronized connection to a remote address.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         */
        int connect (const SockAddr& addr, unsigned timeout);

        /**
         * Put the socket in listening state.
         * @param backlog Maximum number of pending connections.
         */
        int listen (int backlog=5);

        /**
         * Accept an incoming connection.
         * @param callback A function that is called when a new
         *                 incoming connections is made.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 if the socket isn't open.
         */
        int accept (accept_cb_t callback, unsigned timeout=-1);

        /**
         * Accept an incoming connection.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         */
        std::shared_ptr<SocketConnection> accept (unsigned timeout);

        /**
         * Get the local address.
         */
        const SockAddr& addr () const;

        /**
         * Get the address of the peer we send to.
         * Return the address of the peer we are communicating with.
         */
        const SockAddr& peer () const;

        /**
         * Is the socket connected?
         */
        const bool is_connected () const;

        /**
         * Is the socket bound to a local address?
         */
        const bool is_bound () const;

        void default_sock_rx_callback (peer_io_callback_t rx_cb) {
            def_sock_rx_cb = rx_cb;
        }

        void default_sock_tx_callback (peer_io_callback_t tx_cb) {
            def_sock_tx_cb = tx_cb;
        }

        /**
         * Queue reception of a message from the socket
         * and get the source address of the message.
         * This is mainly used for datagram(UDP) sockets.
         * @param buf The buffer where to store the data.
         * @param size The maximum number of bytes to read.
         * @param rx_cb A callback to be called when the data is read,
         *              or <code>nullptr</code> for no callback.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success, -1 if the read operation can't be queued.
         *         <br/>Note that a return value of 0 means that the read operation
         *         was queued, not that the actual read operation was successful.
         */
        int recvfrom (void* buf, size_t size, peer_io_callback_t rx_cb, unsigned timeout=-1);

        /**
         * Synchronized wait for a message from the socket
         */
        std::shared_ptr<SockAddr> recvfrom (void* buf, ssize_t& size, unsigned timeout);

        /**
         * Queue a write operation to a specific address.
         * This is mainly used for datagram(UDP) sockets.
         * @param buf The buffer from where to write data.
         * @param size The maximum number of bytes to write.
         * @param tx_cb A callback to be called when the data is written,
         *              or <code>nullptr</code> for no callback.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success, -1 if the write operation can't be queued.
         *         <br/>Note that a return value of 0 means that the write operation
         *         was queued, not that the actual write operation was successful.
         */
        int sendto (const void* buf, size_t size, const SockAddr& peer,
                    peer_io_callback_t tx_cb, unsigned timeout=-1);
        /**
         * Synchronized send a message to a peer.
         */
        ssize_t sendto (const void* buf, size_t size, const SockAddr& peer, unsigned timeout);

        /**
         * Read an <i>int</i> socket option at level SOL_SOCKET.
         * @param optname The <i>name</i> of the socket option to read.
         * @return The <i>int</i> value of the socket option.
         *         On error, -1 is returned and <code>errno</code> is set.
         */
        int getsockopt (int optname);

        /**
         * Read an <i>int</i> socket option.
         * @param level The socket API level.<br/>
         *              Normally one of SOL_SOCKET, IPPROTO_IP, or IPPROTO_IPV6.
         * @param optname The <i>name</i> of the socket option to read.
         * @return The <i>int</i> value of the socket option.
         *         On error, -1 is returned and <code>errno</code> is set.
         */
        int getsockopt (int level, int optname);

        /**
         * Read a socket option at level SOL_SOCKET.
         * @param optname The <i>name</i> of the socket option to read.
         * @param optval A pointer where to store the value of the socket option.
         * @param optlen Set to the maximum size of the option value to read.
         *               Filled with the actual number of bytes stored in <code>optval</code>.
         * @return 0 on success, -1 on error and <code>errno</code> is set.
         */
        int getsockopt (int optname, void* optval, socklen_t* optlen);

        /**
         * Read a socket option.
         * @param level The socket API level.<br/>
         *              Normally one of SOL_SOCKET, IPPROTO_IP, or IPPROTO_IPV6.
         * @param optname The <i>name</i> of the socket option to read.
         * @param optval A pointer where to store the value of the socket option.
         * @param optlen Set to the maximum size of the option value to read.
         *               Filled with the actual number of bytes stored in <code>optval</code>.
         * @return 0 on success, -1 on error and <code>errno</code> is set.
         */
        int getsockopt (int level, int optname, void* optval, socklen_t* optlen);

        /**
         * Set an <i>int</i> socket option at level SOL_SOCKET.
         * @param optname The <i>name</i> of the socket option to set.
         * @param value The value of the socket option.
         * @return 0 on success, -1 on error and <code>errno</code> is set.
         */
        int setsockopt (int optname, int value);

        /**
         * Set an <i>int</i> socket option.
         * @param level The socket API level.<br/>
         *              Normally one of SOL_SOCKET, IPPROTO_IP, or IPPROTO_IPV6.
         * @param optname The <i>name</i> of the socket option to set.
         * @param value The value of the socket option.
         * @return 0 on success, -1 on error and <code>errno</code> is set.
         */
        int setsockopt (int level, int optname, int value);

        /**
         * Set a socket option at level SOL_SOCKET.
         * @param optname The <i>name</i> of the socket option to set.
         * @param optval A pointer where to find the value of the socket option.
         * @param optlen The size in bytes of the socket option.
         * @return 0 on success, -1 on error and <code>errno</code> is set.
         */
        int setsockopt (int optname, const void* optval, socklen_t optlen);

        /**
         * Set a socket option.
         * @param level The socket API level.<br/>
         *              Normally one of SOL_SOCKET, IPPROTO_IP, or IPPROTO_IPV6.
         * @param optname The <i>name</i> of the socket option to set.
         * @param optval A pointer where to find the value of the socket option.
         * @param optlen The size in bytes of the socket option.
         * @return 0 on success, -1 on error and <code>errno</code> is set.
         */
        int setsockopt (int level, int optname, const void* optval, socklen_t optlen);


    protected:
        virtual ssize_t do_recvfrom (void* buf, size_t len, int flags, SockAddr& addr);
        virtual ssize_t do_sendto (const void* buf, size_t len, int flags, const SockAddr& addr);

    private:
        SocketConnection (const SocketConnection& c) = delete;
        SocketConnection& operator= (const SocketConnection& conn) = delete;

        void handle_accept_result (accept_cb_t cb, int errnum);

        std::atomic_bool connected;            // Connected to a peer
        std::atomic_bool bound;                // Bound to a local address
        std::shared_ptr<SockAddr> local_addr;  // Local address
        std::shared_ptr<SockAddr> peer_addr;   // Address of peer

        peer_io_callback_t def_sock_rx_cb;
        peer_io_callback_t def_sock_tx_cb;
    };


}


#endif
