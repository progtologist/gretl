# Gretl

[![Build Status](https://img.shields.io/travis/progtologist/gretl.svg?style=plastic)](https://travis-ci.org/progtologist/gretl)
[![Coverage Status](https://img.shields.io/coveralls/progtologist/gretl/master.svg?style=plastic)](https://coveralls.io/r/progtologist/gretl?branch=master)
[![Open Issues](https://img.shields.io/github/issues/progtologist/gretl.svg?style=plastic)](https://github.com/progtologist/gretl/issues)
[![Licence](https://img.shields.io/github/license/progtologist/gretl.svg?style=plastic)](https://github.com/progtologist/gretl/blob/master/LICENCE.md)

Initial attempt to change the default build system of Gretl from GNU Autotools to CMake.

All headers and source files moved to include and src folders appropriately.
All localization files are moved to lang folder. 
CMake scripts are under the cmake/Modules folder.
The rest of the files have been placed under etc (and are not yet considered).
The doc folder contains current documentation, with support for doxygen.
The test folder contains gretl default tests. It has support for the GoogleTest Framework.
The share folder contains the xml UI files and common files of the project.

## How to compile:

### CMake

**Ubuntu 12.04**

 Gretl requires cmake version >=2.8.11 which is not available in the official ubuntu repositories.
To install it you must add a ppa, just type
```Shell
sudo add-apt-repository -y ppa:kalakris/cmake
sudo apt-get update -qq
```

**Ubuntu 14.04**

 The latest LTS includes a newer version of cmake, so no ppa is needed. 

### Dependencies

To install all required and optional dependencies just type
```Shell
sudo apt-get install -y cmake gnuplot libjson-glib-dev libfftw3-dev \
 liblapack-dev gfortran libxml2-dev libgtk2.0-dev zlib1g-dev libreadline-dev \
 libgtksourceview2.0-dev libgmp-dev curl libcurl4-openssl-dev libmpfr-dev \
 flite1-dev libflite1 libopenmpi-dev mpi-default-bin mpi-default-dev r-base-dev \
 libgomp1 libzzip-dev unixodbc-dev
```

### Home Compilation

You can install the package anywhere you want by specifying the CMAKE_INSTALL_PREFIX variable. By default the installation folder is /usr/local

If the package is installed in userspace (not system-wide), the syntax highlighting files for GtkSourceView are installed in ~/.local

To install locally in a terminal type:
```Shell
git clone git@github.com:progtologist/gretl.git
mkdir -p gretl/build
cd gretl/build
cmake .. -DCMAKE_INSTALL_PREFIX=../install
make
make install
```

### Documentation Compilation

To build doxygen documentation
In the build directory type:
```Shell
cmake .. -DBUILD_DOCS=ON -DCMAKE_INSTALL_PREFIX=../install
make docs
```

### Tests Compilation

To build doxygen documentation
In the build directory type:
```Shell
cmake .. -DBUILD_TESTS=ON -DCMAKE_INSTALL_PREFIX=../install
make test
```

### To generate coverage

To build coverage you must have lcov installed and compile with gcc only (clang is not supported)
In the build directory type:
```Shell
cmake .. -DBUILD_TESTS=ON -DENABLE_COVERAGE=ON -DCMAKE_INSTALL_PREFIX=../install
make test
lcov --directory ./ --base-directory ../include/gretl --capture --output-file coverage.info
lcov --remove coverage.info '/usr*' -o coverage.info
genhtml coverage.info --output-directory ./coverage
xdg-open coverage/index.html
```

### All Compilation

To build everything (without debugging symbols)
In the build directory type:
```Shell
cmake .. -DBUILD_DOCS=ON -DBUILD_TESTS=ON -DCMAKE_INSTALL_PREFIX=../install
make
make docs
make test
```

## How to run

Just navigate to the installation folder and double click on the compiled file **gretl**

To see if any error occurs you can run the executable via a terminal by typing (in the build directory)
```Shell
./gretl
```

## Tested

**Linux**

So far it has been successfully tested under
 - Ubuntu Linux 12.04 x86_64
   - gcc 4.6
   - clang 3.0
   - clang 3.4
 - Ubuntu Linux 14.04 x86_64
   - gcc 4.8

**OSX**

Testing is needed to check compatibility issues

**Windows**

It probably does not compile yet.

## Future work

Should add more tests to improve coverage. Should check compatibility with other systems (Windows/OSX)