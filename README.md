# Gretl

[![Build Status](https://travis-ci.org/progtologist/gretl.svg?branch=master)](https://travis-ci.org/progtologist/gretl)

Initial attempt to change the default build system of Gretl from GNU Autotools to CMake.

All headers and source files moved to include and src folders appropriately.
All localization files are moved to lang folder. 
CMake scripts are under the cmake/Modules folder.
The rest of the files have been placed under etc (and are not yet considered).
The doc folder contains current documentation, eventually it will contain doxygen files.
The test folder will contain unit test (gtest).
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
make docs
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
