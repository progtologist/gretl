cmake_minimum_required(VERSION 2.8.7)
project(TestU01_builder)
include(ExternalProject)

ExternalProject_Add ( 
    TestU01 
    GIT_REPOSITORY git@github.com:JamesHirschorn/TestU01-CMake.git 
    PREFIX ${CMAKE_CURRENT_BINARY_DIR} 
    INSTALL_COMMAND ""
)

ExternalProject_Get_Property(TestU01 SOURCE_DIR)
set(TestU01_INCLUDE_DIR ${SOURCE_DIR}/include PARENT_SCOPE)

ExternalProject_Get_Property(TestU01 BINARY_DIR)
set(TestU01_LIBS_DIR ${BINARY_DIR} PARENT_SCOPE)

set(TestU01_LIBRARIES ${BINARY_DIR}/libTestU01.a PARENT_SCOPE)