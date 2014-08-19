# Gretl CMake

[![Build Status](https://travis-ci.org/progtologist/gretl.svg?branch=master)](https://travis-ci.org/progtologist/gretl)

Initial attempt to change the default build system of Gretl from GNU Autotools to CMake.

All headers and source files moved to include and src folders appropriately.
All localization files are moved to lang folder. 
CMake scripts are under the cmake/Modules folder.
The rest of the files have been placed under etc (and are not yet considered).
The doc folder contains current documentation, eventually it will contain doxygen files.
The test folder will contain unit test (gtest).

**The gui version is not yet complete, only the cli version is functioning.**

## How to use:

### Under Linux

In a terminal type:
```Shell
git clone git@github.com:progtologist/gretl.git
mkdir -p gretl/build
cd gretl/build
cmake ..
make
./simple_client
```

### Under other platforms

Testing is needed to check compatibility issues