# Gretl

[![Build Status](https://travis-ci.org/progtologist/gretl.svg?branch=master)](https://travis-ci.org/progtologist/gretl)

Initial attempt to change the default build system of Gretl from GNU Autotools to CMake.

All headers and source files moved to include and src folders appropriately.
All localization files are moved to lang folder. 
CMake scripts are under the cmake/Modules folder.
The rest of the files have been placed under etc (and are not yet considered).
The doc folder contains current documentation, eventually it will contain doxygen files.
The test folder will contain unit test (gtest).
The share folder contains the xml UI files.

## How to use:

### Under Linux

In a terminal type:
```Shell
git clone git@github.com:progtologist/gretl.git
mkdir -p gretl/build
cd gretl/build
cmake ..
make
./gretl_gui
```

To build doxygen documentation
In the build directory type:
```Shell
make docs
```

Tested under
 - Ubuntu Linux 12.04 x86_64
   - gcc 4.6
   - clang 3.0
   - clang 3.4
 - Ubuntu Linux 14.04 x86_64
   - gcc 4.8

### Under other platforms

Testing is needed to check compatibility issues
