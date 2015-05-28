#!/bin/bash

brew outdated cmake || brew upgrade cmake
brew install gnuplot json-glib fftw gfortran gtk+ readline libxml2 gtksourceview gmp curl openssl mpfr open-mpi unixodbc lcov gcc libzzip lzlib pkg-config brew-gem

# INSTALL LAPACK
pushd dependencies
wget http://www.netlib.org/lapack/lapack.tgz
tar xzf lapack.tgz
cd lapack-*/ && mkdir build && cd build
cmake ..
make -j4 -l4
sudo make install
popd

# INSTALL FLITE
pushd dependencies
wget http://www.speech.cs.cmu.edu/flite/packed/flite-1.4/flite-1.4-release.tar.bz2
tar jxf flite-1.4-release.tar.bz2
cd flite-*/
./configure
make
sudo make install
popd

gem install coveralls-lcov