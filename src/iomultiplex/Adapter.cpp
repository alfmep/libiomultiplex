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
#include <iomultiplex/Adapter.hpp>
#include <iomultiplex/iohandler_base.hpp>
#include <stdexcept>


namespace iomultiplex {


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    Adapter::Adapter ()
        : slave {nullptr},
          slave_ptr {nullptr},
          close_slave_on_destruct {false}
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    Adapter::Adapter (Connection& conn, bool close_on_destruct)
        : slave {&conn},
          slave_ptr (nullptr),
          close_slave_on_destruct {close_on_destruct}
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    Adapter::Adapter (std::shared_ptr<Connection> conn_ptr)
        : slave {conn_ptr.get()},
          slave_ptr (conn_ptr),
          close_slave_on_destruct {false}
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    /*
    Adapter::Adapter (Adapter&& adapter)
    {
        slave = adapter.slave;
        close_slave_on_destruct = adapter.close_slave_on_destruct;
        slave_ptr = std::move (adapter.slave_ptr);
        adapter.slave = nullptr;
        adapter.close_slave_on_destruct = false;
    }
    */


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    Adapter::~Adapter ()
    {
        if (close_slave_on_destruct && slave)
            slave->close ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    /*
    Adapter& Adapter::operator= (Adapter&& rhs)
    {
        if (this != &rhs) {
            slave = rhs.slave;
            slave_ptr = std::move (rhs.slave_ptr);
            close_slave_on_destruct = rhs.close_slave_on_destruct;
            rhs.slave = nullptr;
            rhs.close_slave_on_destruct = false;
        }
        return *this;
    }
    */


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int Adapter::handle ()
    {
        return slave ? slave->handle() : -1;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool Adapter::is_open () const
    {
        return slave ? slave->is_open() : false;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    iohandler_base& Adapter::io_handler ()
    {
        if (slave)
            return slave->io_handler ();
        else
            throw std::runtime_error ("Missing slave connection in Adapter");
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void Adapter::cancel (bool cancel_rx, bool cancel_tx)
    {
        if (slave)
            slave->cancel (cancel_rx, cancel_tx);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void Adapter::close ()
    {
        if (slave)
            slave->close ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    Connection& Adapter::connection ()
    {
        if (slave)
            return *slave;
        else
            throw std::runtime_error ("Missing slave connection in Adapter");
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void Adapter::connection (Connection& conn, bool close_on_destruct)
    {
        if (close_slave_on_destruct && slave)
            slave->close ();
        slave = &conn;
        close_slave_on_destruct = close_on_destruct;
        slave_ptr.reset ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void Adapter::connection (std::shared_ptr<Connection> conn_ptr)
    {
        if (close_slave_on_destruct && slave)
            slave->close ();
        slave_ptr = conn_ptr;
        close_slave_on_destruct = false;
        slave = slave_ptr.get ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t Adapter::do_read (void* buf, size_t size, off_t offset, int& errnum)
    {
        if (slave) {
            return slave->do_read (buf, size, offset, errnum);
        }else{
            errnum = EBADF;
            return -1;
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t Adapter::do_write (const void* buf, size_t size, off_t offset, int& errnum)
    {
        if (slave) {
            return slave->do_write (buf, size, offset, errnum);
        }else{
            errnum = EBADF;
            return -1;
        }
    }



}
