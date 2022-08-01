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


include_directories (
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/src"
    )
link_libraries (iomultiplex)


add_executable (sync-file-read sync-file-read.cpp)
add_executable (async-file-read async-file-read.cpp)
add_executable (echo-tcp-server echo-tcp-server.cpp)
add_executable (echo-tcp-client echo-tcp-client.cpp)
add_executable (echo-tls-server echo-tls-server.cpp)
add_executable (echo-tls-client echo-tls-client.cpp)
add_executable (echo-udp-server echo-udp-server.cpp)
add_executable (echo-udp-client echo-udp-client.cpp)
add_executable (echo-dtls-server echo-dtls-server.cpp)
add_executable (echo-dtls-client echo-dtls-client.cpp)
add_executable (echo-server echo-server.cpp)
add_executable (echo-client echo-client.cpp)
add_executable (beacon-server beacon-server.cpp)
add_executable (tls-connect tls-connect.cpp)
add_executable (adapter-test
    adapter-test.cpp
    ObfuscateAdapter.cpp
    ObfuscateAdapter.hpp
    ShuffleAdapter.cpp
    ShuffleAdapter.hpp
    CaseAdapter.cpp
    CaseAdapter.hpp
    RobberAdapter.cpp
    RobberAdapter.hpp
    ReverseAdapter.cpp
    ReverseAdapter.hpp
    )

##
## Example of a TFTP server and client
##
set (IOM_TFTP_COMMON_SOURCES
    iom-tftp/tftp.cpp
    iom-tftp/tftp-session.cpp
    )
add_executable (iom-tftpd
    ${IOM_TFTP_COMMON_SOURCES}
    iom-tftp/iom-tftpd-options.cpp
    iom-tftp/iom-tftpd.cpp
    )
add_executable (iom-tftpc
    ${IOM_TFTP_COMMON_SOURCES}
    iom-tftp/iom-tftpc-options.cpp
    iom-tftp/iom-tftpc.cpp
    )
