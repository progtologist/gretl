====================
:index:`Quick Start`
====================

:index:`CMake Version`
----------------------

:index:`Ubuntu 12.04`
^^^^^^^^^^^^^^^^^^^^^

Gretl requires cmake version >=2.8.11 which is not available in the official ubuntu repositories. To install it you must add a ppa

.. code-block:: bash
    
    sudo add-apt-repository -y ppa:kalakris/cmake 
    sudo apt-get update -qq


:index:`Ubuntu 14.04`
^^^^^^^^^^^^^^^^^^^^^

The latest LTS includes a newer version of cmake, so no ppa is needed. 

:index:`Dependencies`
---------------------

To install all required and optional dependencies just type

.. code-block:: bash

    sudo apt-get install -y cmake gnuplot libjson-glib-dev libfftw3-dev \
    liblapack-dev gfortran libxml2-dev libgtk2.0-dev zlib1g-dev libreadline-dev \
    libgtksourceview2.0-dev libgmp-dev curl libcurl4-openssl-dev libmpfr-dev \
    flite1-dev libflite1 libopenmpi-dev mpi-default-bin mpi-default-dev r-base-dev \
    libgomp1 libzzip-dev unixodbc-dev

:index:`Compilation`
--------------------

You can install the package anywhere you want by specifying the CMAKE_INSTALL_PREFIX variable. 

.. note::
    
    By default the installation folder is **/usr/local**

If the package is installed in userspace (not system-wide), the syntax highlighting files for GtkSourceView are installed in ~/.local

To install locally in a terminal type:

.. code-block:: bash

    git clone git@github.com:progtologist/gretl.git
    mkdir -p gretl/build
    cd gretl/build
    cmake .. -DCMAKE_INSTALL_PREFIX=../install
    make
    make install

:index:`Documentation`
----------------------

To build doxygen documentation
In the build directory type:

.. code-block:: bash

    cmake .. -DBUILD_DOCS=ON -DCMAKE_INSTALL_PREFIX=../install
    make docs
    xdg-open doc/doxygen/html/index.html

To build sphinx documentation you need sphinx and the sphinx-rtd-theme. To install them you can use pip - the python package manager. 
To install them in ubuntu just type:

.. code-block:: bash

    sudo apt-get install -y python-pip
    sudo pip install sphinx sphinx_rtd_theme

Then, to build the sphinx documentation in the build directory type:

.. code-block:: bash

    cmake .. -DBUILD_SPHINX=ON -DCMAKE_INSTALL_PREFIX=../install
    make sphinx
    xdg-open doc/sphinx/html/index.html

:index:`Run tests`
------------------

To build and run the tests
In the build directory type:

.. code-block:: bash

    cmake .. -DBUILD_TESTS=ON -DCMAKE_INSTALL_PREFIX=../install
    make
    make test

:index:`Generate coverage report`
---------------------------------

To build coverage you must have lcov installed and compile with gcc only (clang is not supported)
In the build directory type:

.. code-block:: bash
    
    cmake .. -DBUILD_TESTS=ON -DENABLE_COVERAGE=ON -DCMAKE_INSTALL_PREFIX=../install
    make
    make test
    lcov --directory ./ --base-directory ../include/gretl --capture --output-file coverage.info
    lcov --remove coverage.info '/usr*' -o coverage.info
    genhtml coverage.info --output-directory ./coverage
    xdg-open coverage/index.html

:index:`Compile everything`
---------------------------

To build everything (without debugging symbols)
In the build directory type:

.. code-block:: bash
    
    cmake .. -DBUILD_DOCS=ON -DBUILD_SPHINX -DBUILD_TESTS=ON -DCMAKE_INSTALL_PREFIX=../install
    make
    make docs
    make sphinx
    make test
    make install

:index:`How to run`
-------------------

Just navigate to the installation folder and double click on the compiled file **gretl**

To see if any error occurs you can run the executable via a terminal by typing (in the build directory)

.. code-block:: bash

    ./gretl
