#!/bin/bash

pushd build
lcov --directory ./ --base-directory ../include/gretl --capture --output-file coverage.info
lcov --remove coverage.info '/usr*' -o coverage.info
coveralls-lcov coverage.info
popd