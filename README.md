# Gretl [![Build Status](https://img.shields.io/travis/progtologist/gretl.svg)](https://travis-ci.org/progtologist/gretl)

[![Documentation Status](https://readthedocs.org/projects/gretl/badge/?version=latest)](https://readthedocs.org/projects/gretl/?badge=latest)
[![Open Issues](https://img.shields.io/github/issues/progtologist/gretl.svg)](https://github.com/progtologist/gretl/issues)
[![Coverage Status](https://img.shields.io/coveralls/progtologist/gretl/master.svg)](https://coveralls.io/r/progtologist/gretl?branch=master)
[![License](https://img.shields.io/github/license/progtologist/gretl.svg)](https://github.com/progtologist/gretl/blob/master/LICENSE.md)


## Documentation

You can visit [readthedocs](http://gretl.readthedocs.org/) for an up-to-date guide on how to build and use this repository.

## General Information

Initial attempt to change the default build system of Gretl from GNU Autotools to CMake.

All headers and source files moved to include and src folders appropriately.
All localization files are moved to lang folder. 
CMake scripts are under the cmake/Modules folder.
The rest of the files have been placed under etc (and are not yet considered).
The doc folder contains current documentation, with support for doxygen and sphinx.
The test folder contains gretl default tests. It has support for the GoogleTest Framework.
The share folder contains the xml UI files and common files of the project.

1. Supports Continuous Integration (Travis CI - works with linux, supports OSX)
2. Supports Continuous Documentation
3. Supports Self-Testing

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

Should add more tests to improve coverage. Should check compatibility with other systems (Windows/OSX). Should support continuous deployment. Will test it with AppVeyor for CI in Windows.