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


if (OPENSSL_FOUND)
    set (REQUIRE_IN_PC_FILE "openssl")
    set (TLS_ADAPTER_HEADER_FILES "#include <iomultiplex/x509_t.hpp>
#include <iomultiplex/TlsConfig.hpp>
#include <iomultiplex/TlsAdapter.hpp>"
        )
else()
    set (REQUIRE_IN_PC_FILE "")
    set (TLS_ADAPTER_HEADER_FILES "")
endif()


# Add library libiomultiplex
#
add_library (iomultiplex)


# Set library version properties
#
set_target_properties (iomultiplex PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    )


# Link flags (libraries to link with)
#
if (OPENSSL_FOUND)
    if (MSVC)
        target_link_libraries (iomultiplex PRIVATE
            OpenSSL::applink
            )
    else()
        target_link_libraries (iomultiplex PRIVATE
            OpenSSL::SSL
            OpenSSL::Crypto
            )
    endif()
endif()


# Set include path
#
target_include_directories (iomultiplex PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    )


#
# Source files
#
target_sources (iomultiplex PRIVATE
    # Source files
    iomultiplex/utils.cpp
    iomultiplex/Log.cpp
    iomultiplex/SockAddr.cpp
    iomultiplex/IpAddr.cpp
    iomultiplex/UxAddr.cpp
    iomultiplex/BufferPool.cpp
    iomultiplex/Resolver.cpp
    iomultiplex/PollDescriptors.cpp
    iomultiplex/Connection.cpp
    iomultiplex/IOHandler_Poll.cpp
    iomultiplex/IOHandler_Epoll.cpp
    iomultiplex/FdConnection.cpp
    iomultiplex/FileConnection.cpp
    iomultiplex/FileNotifier.cpp
    iomultiplex/termios_cfg.cpp
    iomultiplex/SerialConnection.cpp
    iomultiplex/TimerConnection.cpp
    iomultiplex/SocketConnection.cpp
    iomultiplex/TimerSet.cpp
    iomultiplex/Adapter.cpp
    iomultiplex/ChunkAdapter.cpp
)
if (OPENSSL_FOUND)
    target_sources (iomultiplex PRIVATE
        iomultiplex/x509_t.cpp
        iomultiplex/TlsAdapter.cpp
        )
endif()

#
# Header files
#
configure_file (iomultiplex.hpp.in iomultiplex.hpp @ONLY)

set (MAIN_PUBLIC_HEADER_FILE
    ${CMAKE_CURRENT_BINARY_DIR}/iomultiplex.hpp
    )
set (PUBLIC_HEADER_FILES
    iomultiplex/types.hpp
    iomultiplex/utils.hpp
    iomultiplex/Log.hpp
    iomultiplex/SockAddr.hpp
    iomultiplex/IpAddr.hpp
    iomultiplex/UxAddr.hpp
    iomultiplex/BufferPool.hpp
    iomultiplex/Resolver.hpp
    iomultiplex/io_result_t.hpp
    iomultiplex/PollDescriptors.hpp
    iomultiplex/Connection.hpp
    iomultiplex/iohandler_base.hpp
    iomultiplex/IOHandler_Poll.hpp
    iomultiplex/IOHandler_Epoll.hpp
    iomultiplex/FdConnection.hpp
    iomultiplex/FileConnection.hpp
    iomultiplex/FileNotifier.hpp
    iomultiplex/termios_cfg.hpp
    iomultiplex/SerialConnection.hpp
    iomultiplex/TimerConnection.hpp
    iomultiplex/SocketConnection.hpp
    iomultiplex/TimerSet.hpp
    iomultiplex/Adapter.hpp
    iomultiplex/ChunkAdapter.hpp
    )
if (OPENSSL_FOUND)
    set (PUBLIC_HEADER_FILES
        ${PUBLIC_HEADER_FILES}
        iomultiplex/x509_t.hpp
        iomultiplex/TlsConfig.hpp
        iomultiplex/TlsAdapter.hpp
        )
endif()


#
# Installation
#

# Library files
#
install (TARGETS iomultiplex
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

# Header files
#
install (
    FILES ${MAIN_PUBLIC_HEADER_FILE}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )
install (
    FILES ${PUBLIC_HEADER_FILES}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/iomultiplex"
    )

# pc file
#
if (UNIX)
    configure_file (iomultiplex.pc.in iomultiplex.pc @ONLY)
    install (
        FILES ${CMAKE_CURRENT_BINARY_DIR}/iomultiplex.pc
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
        )
endif()
