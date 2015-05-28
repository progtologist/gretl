cmake_minimum_required(VERSION 2.8.7)
project(gtest_builder C CXX)
include(ExternalProject)

ExternalProject_Add(googletest
    SVN_REPOSITORY http://googletest.googlecode.com/svn/trunk
    CMAKE_ARGS -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG:PATH=DebugLibs
               -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE:PATH=ReleaseLibs
               -DCMAKE_CXX_FLAGS=${MSVC_COMPILER_DEFS}
               -Dgtest_force_shared_crt=ON
     PREFIX "${CMAKE_CURRENT_BINARY_DIR}"
# Disable install step
    INSTALL_COMMAND ""
)
ExternalProject_Get_Property(googletest source_dir)
ExternalProject_Get_Property(googletest binary_dir)
# Specify include dir
set(GTEST_INCLUDE_DIRS ${source_dir}/include PARENT_SCOPE)
set(GTEST_LIBS_DIR ${binary_dir} PARENT_SCOPE)
set(GTEST_LIBRARIES ${binary_dir}/libgtest.a ${binary_dir}/libgtest_main.a PARENT_SCOPE)
