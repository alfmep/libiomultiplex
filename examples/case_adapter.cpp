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
#include "case_adapter.hpp"
#include <cstring>
#include <random>

#include <iostream>


namespace adapter_test {


    static std::random_device rnd_dev;
    static std::default_random_engine rnd_engine (rnd_dev());
    static std::uniform_int_distribution<uint16_t> rnd_val (0, 1);


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    case_adapter::case_adapter (iomultiplex::connection& conn,
                                bool close_on_destruct,
                                int new_case)
        : iomultiplex::adapter (conn, close_on_destruct),
          mode (new_case),
          wbuf_size (0)
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    case_adapter::case_adapter (std::shared_ptr<iomultiplex::connection> conn_ptr, int new_case)
        : iomultiplex::adapter (conn_ptr),
          mode (new_case),
          wbuf_size (0)
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t case_adapter::do_read (void* buf, size_t size, int& errnum)
    {
        auto result = iomultiplex::adapter::do_read (buf, size, errnum);
        if (result > 0) {
            char* ch = (char*) buf;
            for (size_t i=0; i<(size_t)result; ++i) {
                bool upper_case;
                switch (mode) {
                case uppercase:
                    upper_case = true;
                    break;
                case lowercase:
                    upper_case = false;
                    break;
                case randomcase:
                default:
                    upper_case = rnd_val (rnd_engine);
                    break;
                }
                if (isalpha(ch[i])) {
                    if (upper_case)
                        ch[i] = toupper (ch[i]);
                    else
                        ch[i] = tolower (ch[i]);
                }
            }
        }
        return result;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t case_adapter::do_write (const void* buf, size_t size, int& errnum)
    {
        if (size > wbuf_size) {
            wbuf.reset (new char[size]);
            wbuf_size = size;
        }
        if (size > 0) {
            for (size_t i=0; i<size; ++i) {
                char ch = ((char*)buf)[i];
                bool upper_case;
                switch (mode) {
                case uppercase:
                    upper_case = true;
                    break;
                case lowercase:
                    upper_case = false;
                    break;
                case randomcase:
                default:
                    upper_case = rnd_val (rnd_engine);
                    break;
                }
                if (isalpha(ch)) {
                    if (upper_case)
                        ch = toupper (ch);
                    else
                        ch = tolower (ch);
                }
                wbuf.get()[i] = ch;
            }
        }
        return iomultiplex::adapter::do_write (wbuf.get(), size, errnum);
    }


}
