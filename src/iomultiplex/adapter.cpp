/*
 * Copyright (C) 2021,2023,2025 Dan Arrhenius <dan@ultramarin.se>
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
#include <iomultiplex/adapter.hpp>
#include <iomultiplex/iohandler_base.hpp>
#include <stdexcept>


namespace iomultiplex {


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    adapter::adapter ()
        : slave {nullptr},
          slave_ptr {nullptr},
          close_slave_on_destruct {false}
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    adapter::adapter (connection& conn, bool close_on_destruct)
        : slave {&conn},
          slave_ptr (nullptr),
          close_slave_on_destruct {close_on_destruct}
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    adapter::adapter (std::shared_ptr<connection> conn_ptr)
        : slave {conn_ptr.get()},
          slave_ptr (conn_ptr),
          close_slave_on_destruct {false}
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    /*
    adapter::adapter (adapter&& adapter)
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
    adapter::~adapter ()
    {
        if (close_slave_on_destruct && slave)
            slave->close ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    /*
    adapter& adapter::operator= (adapter&& rhs)
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
    int adapter::handle ()
    {
        return slave ? slave->handle() : -1;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool adapter::is_open () const
    {
        return slave ? slave->is_open() : false;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    iohandler_base& adapter::io_handler ()
    {
        if (slave)
            return slave->io_handler ();
        else
            throw std::runtime_error ("Missing slave connection in adapter");
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void adapter::cancel (bool cancel_rx, bool cancel_tx, bool fast)
    {
        if (slave)
            slave->cancel (cancel_rx, cancel_tx, fast);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void adapter::close ()
    {
        if (slave)
            slave->close ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    connection& adapter::conn ()
    {
        if (slave)
            return *slave;
        else
            throw std::runtime_error ("Missing slave connection in adapter");
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void adapter::conn (connection& conn, bool close_on_destruct)
    {
        if (close_slave_on_destruct && slave)
            slave->close ();
        slave = &conn;
        close_slave_on_destruct = close_on_destruct;
        slave_ptr.reset ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void adapter::conn (std::shared_ptr<connection> conn_ptr)
    {
        if (close_slave_on_destruct && slave)
            slave->close ();
        slave_ptr = conn_ptr;
        close_slave_on_destruct = false;
        slave = slave_ptr.get ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t adapter::do_read (void* buf, size_t size, int& errnum)
    {
        if (slave) {
            return slave->do_read (buf, size, errnum);
        }else{
            errnum = EBADF;
            return -1;
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t adapter::do_write (const void* buf, size_t size, int& errnum)
    {
        if (slave) {
            return slave->do_write (buf, size, errnum);
        }else{
            errnum = EBADF;
            return -1;
        }
    }



}
