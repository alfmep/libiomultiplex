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
#include <iomultiplex/Log.hpp>
#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <sys/types.h>
#include <syscall.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timeb.h>



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static const char* log_lvl_to_str (unsigned int priority)
{
    switch (priority) {
    case LOG_EMERG:
        return "EMERG]   ";
    case LOG_ALERT:
        return "ALERT]   ";
    case LOG_CRIT:
        return "CRIT]    ";
    case LOG_ERR:
        return "ERR]     ";
    case LOG_WARNING:
        return "WARNING] ";
    case LOG_NOTICE:
        return "NOTICE]  ";
    case LOG_INFO:
        return "INFO]    ";
    case LOG_DEBUG:
        return "DEBUG]   ";
    default:
        return "n/a]     ";
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void example_log_callback (unsigned int priority, const char* message)
{
    struct timeval now;
    gettimeofday (&now, nullptr);

    struct tm t;
    gmtime_r (&now.tv_sec, &t);

    std::cerr << std::setfill('0')
              << std::setw(2) << t.tm_hour
              << ':' << std::setw(2) << t.tm_min
              << ':' << std::setw(2) << t.tm_sec
              << '.' << std::setw(3) << (now.tv_usec/1000)
              << " [" << std::setw(5) << (unsigned)syscall(SYS_gettid) << " - "
              << log_lvl_to_str(priority)
              << message << std::endl;
}
