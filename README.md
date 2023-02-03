# libiomultiplex - C++ I/O multiplexing library.

libiomultiplex is a C++ library for handling asynchronous or synchronous input and output from a number of different I/O connections in Linux. The I/O connections can for example be sockets, serial lines, timers, file notifications, and generic file handles.


## How to build and install

In no configure script is present, first run:
```
./autogen.sh
```
then:
```
./configure
make
make install
```
If the configure script finds doxygen, the generated API documentation can be viewed in a browser by opening file `[prefix]/share/doc/libiomultiplex/html/index.html`. To disable API documentation generation, use parameter `--disable-doxygen` when running the configure script.

The configure script by default looks for OpenSSL to include support for TLS in libiomultiplex. To disable support for TLS, use parameter `--disable-openssl`.

To enable example applications, configure with parameter `--enable-examples`. Example applications are *not* installed when running `make install`.

To see all configuration parameters, run:
```
./configure --help
```
