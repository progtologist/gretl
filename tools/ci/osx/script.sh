#!/bin/bash

cmake -DBUILD_TESTS=ON -DENABLE_COVERAGE=ON -DCMAKE_INSTALL_PREFIX=../install ..
make -j4 -l4
make test
make install