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


set (PREDEFINED_IN_DOXYGEN "")
set (EXCLUDE_FROM_DOXYGEN "")
if (NOT OPENSSL_FOUND)
    set (EXCLUDE_FROM_DOXYGEN "${EXCLUDE_FROM_DOXYGEN} ${CMAKE_SOURCE_DIR}/src/iomultiplex/x509_t.hpp")
    set (EXCLUDE_FROM_DOXYGEN "${EXCLUDE_FROM_DOXYGEN} ${CMAKE_SOURCE_DIR}/src/iomultiplex/TlsConfig.hpp")
    set (EXCLUDE_FROM_DOXYGEN "${EXCLUDE_FROM_DOXYGEN} ${CMAKE_SOURCE_DIR}/src/iomultiplex/TlsAdapter.hpp")
endif()


# Doxygen config file
#
configure_file (doxygen.cfg.in doxygen.cfg @ONLY)


# Generate doxygen documentation
#
add_custom_command (OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/html/index.html
    COMMAND ${DOXYGEN_EXECUTABLE} doxygen.cfg
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation"
    VERBATIM
    )
add_custom_target (doxygen ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/html/index.html
    )


# Clean up
#
add_custom_target (clean-doxygen
    COMMENT "Cleaning API documentation"
    COMMAND ${CMAKE_COMMAND} "-E" "remove_directory" ${CMAKE_CURRENT_BINARY_DIR}/html
    )
# Remove html output on "make clean"
set_directory_properties (PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_CURRENT_BINARY_DIR}/html)


# Install documentation
#
if (UNIX)
    install (
        FILES ${CMAKE_CURRENT_BINARY_DIR}/html
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        )
endif()
