/*
 * Copyright (C) 2021,2022 Dan Arrhenius <dan@ultramarin.se>
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
#ifndef IOMULTIPLEX_ADAPTER_HPP
#define IOMULTIPLEX_ADAPTER_HPP

#include <iomultiplex/Connection.hpp>
#include <memory>


namespace iomultiplex {


    /**
     * Base class for I/O adapters.
     * An adapter is an object that uses a Connection
     * (called a slave connection)
     * to perform some kind of manipulation of read and
     * written data. This can be any kind of manipulation,
     * such as compression, encryption, and more.
     * An Adapter has the same interface as a Connection
     * and can be used anywhere a Connection can be used.
     * <br/>
     * This class doesn't do anything useful by itself, it
     * is a base class for more specific adapters.
     */
    class Adapter : public Connection {
    public:
        /**
         * Default constructor.
         * Constructs an adapter without a slave connection.
         */
        Adapter ();

        /**
         * Constructor.
         * @param conn The slave connection used by this adapter.
         * @param close_on_destruct If <code>true</code>, the
         *                          slave connection used by this adapter
         *                          will be closed in the descructor of
         *                          this object. if <code>false</code>,
         *                          the slave connection will be kept
         *                          open.
         */
        Adapter (Connection& conn, bool close_on_destruct=false);

        /**
         * Constructor.
         * @param conn_ptr A shared ponter to the slave connection
         *                 used by this adapter. The slave connection
         *                 isn't closed by the adapter in the destructor,
         *                 it will be closed by the slave connection itself
         *                 when the shared pointer calls its destructor.
         */
        Adapter (std::shared_ptr<Connection> conn_ptr);

        /**
         * Move constructor.
         * @param adapter The adapter object to move to this instance.
         */
        //Adapter (Adapter&& adapter);

        /**
         * Destructor.
         * The slave connection is only closed by this
         * destructor if the constructor was called with
         * parameter <code>close_on_destruct</code> set
         * to <code>true</code>.
         * <br/>In any case the slave connection will
         * be closed when itself is destroyed.
         */
        virtual ~Adapter ();

        /**
         * Move operator.
         * @param adapter The adapter object to move to this instance.
         */
        //Adapter& operator= (Adapter&& adapter);

        /**
         * Get the file descriptor associated with the slave connection.
         * @return The file desctiptor associated with the slave connection.
         *         <br/>
         *         <b>Note:</b> If the slave connection is closed or if
         *         it doesn't exist, -1 is returned.
         */
        virtual int handle ();

        /**
         * Check if the connection is open or not.
         * @return <code>true</code> if the connection is open.
         */
        virtual bool is_open () const;

        /**
         * Return the iohandler_base object that this connection uses.
         * @return The iohandler_base object that manages the I/O operations
         *         for this connection.
         * @exception std::runtime_error If a slave connection doesn't
         *                               exist. This can happen if a
         *                               <code>nullptr</code> was passed
         *                               as a parameter in the constructor.
         */
        virtual iohandler_base& io_handler ();

        /**
         * Cancel I/O operations for this connection.
         * @param cancel_rx If <code>true</code> (default), cancel all RX operations.
         * @param cancel_tx If <code>true</code> (default), cancel all TX operations.
         */
        virtual void cancel (bool cancel_rx=true, bool cancel_tx=true);

        /**
         * Close the connection.
         */
        virtual void close ();

        /**
         * Get a reference to the slave connection.
         * @return A reference to the slave connection used
         *         by this adapter.
         * @exception std::runtime_error If a slave connection doesn't
         *                               exist. This can happen if a
         *                               <code>nullptr</code> was passed
         *                               as a parameter in the constructor.
         */
        Connection& connection ();

        /**
         * Set/replace the slave connection used by this adapter.
         * If <code>close_on_destruct</code> was set in the destructor or
         * in an earlier call to this method, the current slave connection
         * will be closed.
         * @param conn The slave connection to be used by this adapter.
         * @param close_on_destruct If <code>true</code>, the
         *                          slave connection used by this adapter
         *                          will be closed in the descructor of
         *                          this object. if <code>false</code>,
         *                          the slave connection will be kept
         *                          open.
         */
        void connection (Connection& conn, bool close_on_destruct=false);

        /**
         * Set/replace the slave connection used by this adapter.
         * If <code>close_on_destruct</code> was set in the destructor or
         * in an earlier call to this method, the current slave connection
         * will be closed.
         * @param conn_ptr A shared ponter to the slave connection
         *                 used by this adapter. The slave connection
         *                 isn't closed by the adapter in the destructor,
         *                 it will be closed by the slave connection itself
         *                 when the shared pointer calls its destructor.
         */
        void connection (std::shared_ptr<Connection> conn_ptr);

        /**
         * Read data from the slave connection.
         * The default implementation of this method calls
         * the <code>do_read</code> method of the slave
         * connection. Or if no slave connection exists, it
         * sets <code>errnum</code> to <code>EBADF</code> and returns -1.
         * <br/>
         * This method should be overridden by subclasses that
         * operates on input data. For example, to implement
         * decryption.
         *
         * @param buf A pointer to the memory area where data should be stored.
         * @param size The number of bytes to read.
         * @param errnum The value of <code>errno</code> after
         *               the read operation. Always 0 if no error occurred.
         *
         * @return The number of bytes the adapter stored in the supplied
         *         buffer. Note that this can differ from the actual number
         *         of bytes read by the slave connection if, for example,
         *         the adapter implements decompression of data.
         *         <br/>
         *         On error, -1 is returned and parameter <code>errnum</code>
         *         is set to some appropriate value.
         */
        virtual ssize_t do_read (void* buf, size_t size, int& errnum);

        /**
         * Write data on the slave connection.
         * The default implementation of this method calls
         * the <code>do_write</code> method of the slave
         * connection. Or if no slave connection exists, it
         * sets <code>errnum</code> to <code>EBADF</code> and returns -1.
         * <br/>
         * This method should be overridden by subclasses that
         * operates on output data. For example, to implement
         * encryption.
         *
         * @param buf A pointer to the memory that should be written.
         * @param size The number of bytes to write.
         * @param errnum The value of <code>errno</code> after
         *               the read operation. Always 0 if no error occurred.
         *
         * @return The number of bytes the adapter consumed from the supplied
         *         buffer. Note that this can differ from the actual number
         *         of bytes written on the slave connection if, for example,
         *         the adapter implements compression of data.
         *         <br/>
         *         On error, -1 is returned and parameter <code>errnum</code>
         *         is set to some appropriate value.
         */
        virtual ssize_t do_write (const void* buf, size_t size, int& errnum);


    protected:
        /**
         * A pointer to the slave connection.
         * <b>Important:</b> Do not free or modify this pointer in subclasses of this class.
         */
        Connection* slave;


    private:
        std::shared_ptr<Connection> slave_ptr;
        bool close_slave_on_destruct;
    };


}


#endif
