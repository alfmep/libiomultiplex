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
#ifndef IOMULTIPLEX_SOCKET_CONNECTION_HPP
#define IOMULTIPLEX_SOCKET_CONNECTION_HPP

#include <iomultiplex/fd_connection.hpp>
#include <iomultiplex/sock_addr.hpp>
#include <iomultiplex/ip_addr.hpp>
#include <functional>
#include <atomic>
#include <memory>


namespace iomultiplex {

    // Forward declarations
    class iohandler_base;
    class socket_connection;

    const std::string  sock_family_to_string (const int family);
    const std::string  sock_type_to_string (const int type);
    const std::string& sock_proto_to_string (const int proto);
    int sock_proto_by_name (const std::string& protocol);


    /**
     * A TCP/IP network socket connection.
     */
    class socket_connection : public fd_connection {
    public:
        /**
         * Callback that is called when a socket connection is made (or failed).
         * @param connection The connection object that attempted to connect to some host.
         * @param errnum The value of errno after the connection attempt. 0 if successful.
         */
        using connect_cb_t = std::function<void (socket_connection& connection, int errnum)>;

        /**
         * Callback that is called when a socket connection is accepted (or failed to accept).
         * @param server_conn The connection object that has accepted the connection.
         * @param client_conn A pointer to a connection object representing the established connection.
         * @param errnum The value of errno after the connection attempt. 0 if successful.
         */
        using accept_cb_t = std::function<void (socket_connection& server_conn,
                                                std::shared_ptr<socket_connection> client_conn,
                                                int errnum)>;

        /**
         * Socket I/O callback.
         * @param sock The socket that performed sendto/recvfrom.
         * @param ior I/O operation result.
         * @param peer_addr The address of the peer we are receiving from or sendig to.
         */
        using peer_io_callback_t = std::function<void (socket_connection& sock,
                                                       io_result_t& ior,
                                                       const sock_addr& peer_addr)>;

        /**
         * Constructor.
         * @param io_handler The iohandler_base object handling the I/O operations.
         */
        socket_connection (iohandler_base& io_handler);

        /**
         * Move constructor.
         * @param rhs The socket_connection object to move.
         */
        socket_connection (socket_connection&& rhs);

        /**
         * Destructor.
         */
        virtual ~socket_connection ();

        /**
         * Move operator.
         * @param rhs The socket_connection object to move.
         * @return A reference to this object.
         */
        socket_connection& operator= (socket_connection&& rhs);

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
         * @return The type of socket.
         */
        int type ();

        /**
         * Return the socket protocol.
         * @return The socket protocol.
         */
        int protocol ();

        /**
         * Bind to a local address.
         * @param addr The address we want to bind the socket to.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        int bind (const sock_addr& addr);

        /**
         * Make a connection to a remote address.
         * @param addr The host to connect to.
         * @param callback A function that is called when the connection
         *                 is finished or has failed.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        int connect (const sock_addr& addr, connect_cb_t callback, unsigned timeout=-1);

        /**
         * Make a synchronized connection to a remote address.
         * @param addr The address to connect to.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        int connect (const sock_addr& addr, unsigned timeout=-1);

        /**
         * Put the socket in listening state.
         * @param backlog Maximum number of pending connections.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        int listen (int backlog=5);

        /**
         * Accept an incoming connection.
         * @param callback A function that is called when a new
         *                 incoming connections is made.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        int accept (accept_cb_t callback, unsigned timeout=-1);

        /**
         * Synchronized call to accept an incoming connection.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return A pointer to a socket_connection, or nullptr on error
         *         and <code>errno</code> is set.
         */
        std::shared_ptr<socket_connection> accept (unsigned timeout=-1);

        /**
         * Get the local address.
         * @return The local address.
         */
        const sock_addr& addr () const;

        /**
         * Get the address of the peer we send to.
         * @return the address of the peer we are communicating with.
         */
        const sock_addr& peer () const;

        /**
         * Is the socket connected?
         * @return <code>true</code> if the socket is connected to a peer.
         */
        const bool is_connected () const;

        /**
         * Is the socket bound to a local address?
         * @return <code>true</code> if the socket is bound to a local address.
         */
        const bool is_bound () const;

        /**
         * Set a default RX callback.
         * @param rx_cb The default RX callback.
         */
        void default_sock_rx_callback (peer_io_callback_t rx_cb) {
            def_sock_rx_cb = rx_cb;
        }

        /**
         * Set a default TX callback.
         * @param tx_cb The default TX callback.
         */
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
         * @param buf The buffer where to store the data.
         * @param size The maximum number of bytes to read.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return A pointer to a socket address, or nullptr if no message was received.
         */
        std::shared_ptr<sock_addr> recvfrom (void* buf, ssize_t& size, unsigned timeout=-1);

        /**
         * Queue a write operation to a specific address.
         * This is mainly used for datagram(UDP) sockets.
         * @param buf The buffer from where to write data.
         * @param size The maximum number of bytes to write.
         * @param peer The address to receive the data.
         * @param tx_cb A callback to be called when the data is written,
         *              or <code>nullptr</code> for no callback.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success, -1 if the write operation can't be queued.
         *         <br/>Note that a return value of 0 means that the write operation
         *         was queued, not that the actual write operation was successful.
         */
        int sendto (const void* buf, size_t size, const sock_addr& peer,
                    peer_io_callback_t tx_cb, unsigned timeout=-1);
        /**
         * Synchronized send a message to a peer.
         * @param buf The buffer from where to write data.
         * @param size The maximum number of bytes to write.
         * @param peer The address to receive the data.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return The number of bytes sent, or -1 on error and <code>errno</code> is set.
         */
        ssize_t sendto (const void* buf, size_t size, const sock_addr& peer, unsigned timeout=-1);

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
        virtual ssize_t do_recvfrom (void* buf, size_t len, int flags, sock_addr& addr);
        virtual ssize_t do_sendto (const void* buf, size_t len, int flags, const sock_addr& addr);

    private:
        socket_connection (const socket_connection& c) = delete;
        socket_connection& operator= (const socket_connection& conn) = delete;

        int connect_using_datagram (const sock_addr& addr);
        void handle_accept_result (accept_cb_t cb, int errnum);

        std::atomic_bool connected;            // Connected to a peer
        std::atomic_bool bound;                // Bound to a local address
        std::shared_ptr<sock_addr> local_addr; // Local address
        std::shared_ptr<sock_addr> peer_addr;  // Address of peer

        peer_io_callback_t def_sock_rx_cb;
        peer_io_callback_t def_sock_tx_cb;
    };


}


#endif
