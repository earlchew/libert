libert
======

Application C Runtime Library

#### Background

This library provides an API over the Posix and ISO C API that provides
functionality that is more closely attuned to the needs of
applications. It includes functionality covers:

* Modelling files, pipes, sockets
* Normalised error processing
* Stack traceback on error

#### Supported Platforms

* Linux 32 bit
* Linux 64 bit

#### Dependencies

* GNU Make
* GNU Automake
* GNU C
* GNU C++ for tests

#### Build

* Run `autogen.sh`
* Configure using `configure`
* Build binaries using `make`
* Run tests using `make check`
