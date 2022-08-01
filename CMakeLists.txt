#
#  Copyright (C) 2022 Dan Arrhenius <dan@ultramarin.se>
#
#  This file is part of libiomultiplex.
#
#  libiomultiplex is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
cmake_minimum_required (VERSION 3.18)


# Project name and info
#
project (libiomultiplex
    VERSION 0.1.2
    LANGUAGES CXX
    DESCRIPTION "A C++ library for I/O multiplexing."
    HOMEPAGE_URL https://github.com/alfmep/libiomultiplex
    )


# Set C++ standard
#
#set (CMAKE_CXX_STANDARD 17)
#set (CMAKE_CXX_STANDARD_REQUIRED True)


# Default GNU installation directories
#
include (GNUInstallDirs)


# Common compiler flags
#
if (NOT MSVC)
    add_compile_options (-pipe -Wall -O2 -D_GNU_SOURCE)
    if (CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
        add_compile_options (-g)
    endif()
endif()


# Options
#
option (BUILD_SHARED_LIBS "Build shared library." ON)
option (NO_TLS "Disable support for TLS." OFF)
option (BUILD_EXAMPLES "Build example applications." OFF)
option (BUILD_DOC "Generate documentation (if doxygen is found)." OFF)


# Dependencies
#
if (NO_TLS)
    message (STATUS "TLS support disabled (NO_TLS=True)")
else()
    find_package (OpenSSL)
endif()


# libiomultiplex
#
add_subdirectory (src)

# Example applications
#
if (BUILD_EXAMPLES)
    add_subdirectory (examples)
endif()

# Doxygen documentation
#
if (BUILD_DOC)
    find_package (Doxygen)
    if (DOXYGEN_FOUND)
        add_subdirectory (doc)
    endif()
endif()


# Print summary
#
message (STATUS "Summary:")
message (STATUS "    Installation prefix.................. ${CMAKE_INSTALL_PREFIX}")
if (OPENSSL_FOUND)
    message (STATUS "    OpenSSL enabled...................... yes")
else()
    message (STATUS "    OpenSSL enabled...................... no (libiomultiplex will not have class TlsAdapter)")
endif()
if (DOXYGEN_FOUND)
    message (STATUS "    Generate documentation............... yes")
else()
    message (STATUS "    Generate documentation............... no")
endif()
if (BUILD_EXAMPLES)
    message (STATUS "    Build example applications........... yes (example applications are not installed)")
else()
    message (STATUS "    Build example applications........... no")
endif()


# Un-installation
#
if (NOT TARGET uninstall)
    configure_file (
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY)

    add_custom_target (uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()


# Add targets for distributable files
#
include (CPack)
