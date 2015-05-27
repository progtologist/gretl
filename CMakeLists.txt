cmake_minimum_required (VERSION 2.8.11)
project(gretl)

#################################
## Avoid source tree pollution ##
#################################

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(FATAL_ERROR "In-source builds are not permitted. Make a separate folder for building:\nmkdir build; cd build; cmake ..\nBefore that, remove the files already created:\nrm -rf CMakeCache.txt CMakeFiles")
endif(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

####################
## Create build.h ##
####################

string(TIMESTAMP DATE "%Y-%m-%d")
file(WRITE "${CMAKE_SOURCE_DIR}/include/${PROJECT_NAME}/build.h" "\#define BUILD_DATE \"${DATE}\"")

##################
## DEPENDENCIES ##
##################

find_package(GTK2)
if(GTK2_FOUND)
	include_directories(${GTK2_INCLUDE_DIRS})
	set(GRETL_LIBS ${GRETL_LIBS} ${GTK2_LIBRARIES})
else(GTK2_FOUND)
	find_package(GTK3)
	if(GTK3_FOUND)
		include_directories(${GTK3_INCLUDE_DIRS})
		set(USE_GTK3 ON)
	endif(GTK3_FOUND)
endif(GTK2_FOUND)

find_package(LIBDL)
if(LIBDL_FOUND)
	include_directories(${LIBDL_INCLUDE_DIR})
	set(GRETL_LIBS ${GRETL_LIBS} ${LIBDL_LIBRARIES})
endif(LIBDL_FOUND)

find_package(Gtksourceview2)
if(Gtksourceview2_FOUND)
	include_directories(${Gtksourceview2_INCLUDE_DIRS})
	set(GRETL_LIBS ${GRETL_LIBS} ${Gtksourceview2_LIBRARIES})
	set(USE_GTKSOURCEVIEW_2 ON)
else(Gtksourceview2_FOUND)
	find_package(Gtksourceview3)
	if(Gtksourceview3_FOUND)
		include_directories(${Gtksourceview3_INCLUDE_DIRS})
		set(GRETL_LIBS ${GRETL_LIBS} ${Gtksourceview3_LIBRARIES})
		set(USE_GTKSOURCEVIEW_3 ON)
	endif(Gtksourceview3_FOUND)
endif(Gtksourceview2_FOUND)

find_package(MPFR)
if(MPFR_FOUND)
	include_directories(${MPFR_INCLUDES})
	set(GRETL_LIBS ${GRETL_LIBS} ${MPFR_LIBRARIES})
	set(HAVE_MPFR ON)
endif(MPFR_FOUND)

find_package(LibXML2)
if(LIBXML2_FOUND)
	include_directories(${LibXML2_INCLUDE_DIR})
	set(GRETL_LIBS ${GRETL_LIBS} ${LibXML2_LIBRARIES})
endif(LIBXML2_FOUND)

find_package(Flite QUIET)
if(FLITE_FOUND)
	include_directories(${FLITE_INCLUDE_DIR})
	set(GRETL_LIBS ${GRETL_LIBS} ${FLITE_LIBRARIES})
	set(HAVE_FLITE ON)
endif(FLITE_FOUND)

find_package(CURL)
if(CURL_FOUND)
	include_directories(${CURL_INCLUDE_DIRS})
	set(USE_CURL ON)
	set(GRETL_LIBS ${GRETL_LIBS} ${CURL_LIBRARIES})
endif(CURL_FOUND)

find_package(MPI)
if(MPI_C_FOUND)
	include_directories(${MPI_C_INCLUDE_DIRS})
	set(GRETL_LIBS ${GRETL_LIBS} ${MPI_C_LIBRARIES})
	set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} ${MPI_C_COMPILE_FLAGS})
	set(HAVE_MPI ON)
endif(MPI_C_FOUND)
if(MPI_CXX_FOUND)
	include_directories(${MPI_CXX_INCLUDE_DIRS})
	set(GRETL_LIBS ${GRETL_LIBS} ${MPI_CXX_LIBRARIES})
	set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} ${MPI_CXX_COMPILE_FLAGS})
endif(MPI_CXX_FOUND)

find_package(R)
if(R_FOUND)
	include_directories(${R_INCLUDE_DIR})
	set(USE_RLIB ON)
	set(RLIBPATH ON)
	set(GRETL_LIBS ${GRETL_LIBS} ${R_LIBRARIES})
	message(STATUS "R libraries are ${R_LIBRARIES}")
endif(R_FOUND)

find_package(OpenMP)
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(OPENMP_BUILD ON)
endif(OPENMP_FOUND)

find_package(ZLIB)
if(ZLIB_FOUND)
	include_directories(${ZLIB_INCLUDE_DIRS})
	set(GRETL_LIBS ${GRETL_LIBS} ${ZLIB_LIBRARIES})
	set(HAVE_ZLIB ON)
endif(ZLIB_FOUND)

find_package(Readline)
if(READLINE_FOUND)
	include_directories(${Readline_INCLUDE_DIR})
	set(GRETL_LIBS ${GRETL_LIBS} ${Readline_LIBRARY})
	set(HAVE_READLINE ON)
endif(READLINE_FOUND)

find_package(LATEX)
if(LATEX_FOUND)
	set(HAVE_LATEX ON)
endif(LATEX_FOUND)

find_package(UnixODBC)
if(UnixODBC_FOUND)
	include_directories(${UnixODBC_INCLUDE_DIRS})
	set(GRETL_LIBS ${GRETL_LIBS} ${UnixODBC_LIBRARIES})
endif(UnixODBC_FOUND)

find_package(LAPACK REQUIRED)
if(LAPACK_FOUND)
	set(GRETL_LIBS ${GRETL_LIBS} ${LAPACK_LIBRARIES})
endif(LAPACK_FOUND)

find_package(FFTW REQUIRED)
if(FFTW_FOUND)
	include_directories(${FFTW_INCLUDES})
	set(GRETL_LIBS ${GRETL_LIBS} ${FFTW_LIBRARIES})
endif(FFTW_FOUND)

find_package(GMP)
if(GMP_FOUND)
	include_directories(${GMP_INCLUDE_DIR})
	set(GRETL_LIBS ${GRETL_LIBS} ${GMP_LIBRARIES})
endif(GMP_FOUND)

find_package(StandardMathLibrary)
if(STANDARD_MATH_LIBRARY_FOUND)
	set(GRETL_LIBS ${GRETL_LIBS} -l${STANDARD_MATH_LIBRARY})
endif(STANDARD_MATH_LIBRARY_FOUND)

set(GRETL_PREFIX "${CMAKE_INSTALL_PREFIX}")

include(${CMAKE_SOURCE_DIR}/cmake/Modules/CheckHeaders.cmake)
configure_file(config.h.in ${CMAKE_SOURCE_DIR}/include/${PROJECT_NAME}/config.h)

# Localization
find_package(Gettext)
find_package(Msgfmt)
if(Gettext_FOUND AND Msgfmt_FOUND)
	message(STATUS "Localization enabled: Building and installing .mo files")
	add_subdirectory(lang)
else()
	message(STATUS "Localization disabled: Gettext or Msgfmt missing")
endif()

include_directories(
	include
)

# Create .lang file for syntax highlighting
add_custom_command(OUTPUT gretl.lang
                   POST_BUILD
                   COMMAND mklang > gretl.lang
                   WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                   COMMENT "Creating the language file"
                   DEPENDS mklang
                   VERBATIM)

add_custom_target(CreateTheLangFile ALL
                  DEPENDS gretl.lang
)

add_custom_target(CopyTheLangFile ALL
                  COMMAND cmake -E copy ${CMAKE_SOURCE_DIR}/share/${PROJECT_NAME}/gnuplot.lang ${CMAKE_BINARY_DIR}/gnuplot.lang
)

###########
## FILES ##
###########
file(GLOB_RECURSE ALL_HEADERS ${PROJECT_SOURCE_DIR}/include/*.h)

file(GLOB_RECURSE RNG_H ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/rng/*.h)
file(GLOB_RECURSE RNG_C ${PROJECT_SOURCE_DIR}/src/rng/*.c)

file(GLOB_RECURSE PLUGIN_H ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/plugin/*.h)
file(GLOB_RECURSE PLUGIN_C ${PROJECT_SOURCE_DIR}/src/plugin/*.c)

file(GLOB_RECURSE MINPACK_H ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/minpack/*.h)
file(GLOB_RECURSE MINPACK_C ${PROJECT_SOURCE_DIR}/src/minpack/*.c)

file(GLOB_RECURSE GRETL_LIB_H ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/lib/*.h)
file(GLOB_RECURSE GRETL_LIB_C ${PROJECT_SOURCE_DIR}/src/lib/*.c)

file(GLOB_RECURSE GUI2_H ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/gui2/*.h)
file(GLOB_RECURSE GUI2_C ${PROJECT_SOURCE_DIR}/src/gui2/*.c)

file(GLOB_RECURSE CEPHES_H ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/cephes/*.h)
file(GLOB_RECURSE CEPHES_C ${PROJECT_SOURCE_DIR}/src/cephes/*.c)

file(GLOB_RECURSE DCMT_H ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/dcmt/*.h)
file(GLOB_RECURSE DCMT_C ${PROJECT_SOURCE_DIR}/src/dcmt/*.c)

file(GLOB_RECURSE EXTRA_C ${PROJECT_SOURCE_DIR}/src/extra/*.c)

#################
## Documenting ##
#################

# Documentation
option(BUILD_DOCS "Use Doxygen to create the HTML based API documentation" OFF)
if(BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        configure_file(${PROJECT_SOURCE_DIR}/doc/Doxyfile.in ${CMAKE_BINARY_DIR}/doc/Doxyfile @ONLY)
        add_custom_target(
            docs ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/doc/Doxyfile
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/doc/
            COMMENT "Generating API documentation with Doxygen" VERBATIM
        )
    endif(DOXYGEN_FOUND)
endif(BUILD_DOCS)

##############
## Building ##
##############
list(REMOVE_DUPLICATES GRETL_LIBS)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

add_library(rng SHARED ${RNG_H} ${RNG_C})
add_library(rng_static STATIC ${RNG_H} ${RNG_C})

add_library(plugin SHARED ${PLUGIN_H} ${PLUGIN_C})
target_link_libraries(plugin ${GRETL_LIBS})
add_library(plugin_static STATIC ${PLUGIN_H} ${PLUGIN_C})
target_link_libraries(plugin_static ${GRETL_LIBS})

add_library(minpack SHARED ${MINPACK_H} ${MINPACK_C})
add_library(minpack_static STATIC ${MINPACK_H} ${MINPACK_C})

add_library(cephes SHARED ${CEPHES_H} ${CEPHES_C})
add_library(cephes_static STATIC ${CEPHES_H} ${CEPHES_C})

add_library(dcmt SHARED ${DCMT_H} ${DCMT_C})
add_library(dcmt_static STATIC ${DCMT_H} ${DCMT_C})

add_library(gretl SHARED ${GRETL_LIB_H} ${GRETL_LIB_C})
target_link_libraries(gretl rng dcmt cephes minpack ${GRETL_LIBS})
add_library(gretl_static STATIC ${GRETL_LIB_H} ${GRETL_LIB_C})
target_link_libraries(gretl_static rng_static dcmt_static cephes_static minpack_static ${GRETL_LIBS})

add_library(gui2 SHARED ${GUI2_H} ${GUI2_C})
add_library(gui2_static STATIC ${GUI2_H} ${GUI2_C})

add_library(complete SHARED ${PROJECT_SOURCE_DIR}/src/executables/complete.c)
add_library(complete_static STATIC ${PROJECT_SOURCE_DIR}/src/executables/complete.c)

foreach(ITEM ${EXTRA_C})
	get_filename_component(filename ${ITEM} NAME_WE)
	add_executable(${filename} ${ITEM})
	target_link_libraries(${filename} gretl_static)
endforeach()

add_executable(gretlcli ${PROJECT_SOURCE_DIR}/src/executables/gretlcli.c)
target_link_libraries(gretlcli gretl_static complete_static plugin_static)

if(MPI_C_FOUND)
    add_executable(gretlcli-mpi ${PROJECT_SOURCE_DIR}/src/executables/gretlcli-mpi.c)
	target_link_libraries(gretlcli-mpi gretl_static plugin_static)
	if(MPI_C_COMPILE_FLAGS)
  		set_target_properties(gretlcli-mpi PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
	endif()
	if(MPI_C_LINK_FLAGS)
  		set_target_properties(gretlcli-mpi PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
	endif()
endif(MPI_C_FOUND)

add_executable(gretl_gui ${PROJECT_SOURCE_DIR}/src/executables/gretl_gui.c)
set_property(TARGET gretl_gui PROPERTY OUTPUT_NAME gretl)
target_link_libraries(gretl_gui gui2_static gretl_static plugin_static)

add_executable(mklang ${PROJECT_SOURCE_DIR}/src/executables/mklang.c)
target_link_libraries(mklang gui2_static gretl_static)

add_executable(mknews ${PROJECT_SOURCE_DIR}/src/executables/mknews.c)
target_link_libraries(mknews gui2_static gretl_static)

#############
## Testing ##
#############
option(BUILD_TESTS "Use Gtest to create the test cases for the code" OFF)
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest QUIET)
    if(NOT GTEST_FOUND)
        add_subdirectory(cmake/gtest)
    endif(NOT GTEST_FOUND)

    add_executable(nistcheck ${PROJECT_SOURCE_DIR}/test/nist/nistcheck.c)
    set_target_properties(nistcheck PROPERTIES
                          COMPILE_DEFINITIONS "STANDALONE")
    target_link_libraries(nistcheck gui2_static gretl_static ${GRETL_LIBS})
    add_custom_command(TARGET nistcheck PRE_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/test/nist $<TARGET_FILE_DIR:nistcheck>)

    add_test(NAME nistcheck
             COMMAND nistcheck)

    add_executable(nist-nls-test ${PROJECT_SOURCE_DIR}/test/nist-nls/nist-nls-test.c)
    target_link_libraries(nist-nls-test gui2_static gretl_static ${GRETL_LIBS})

    add_custom_command(TARGET nist-nls-test PRE_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/test/nist-nls ${PROJECT_BINARY_DIR}
                       COMMAND ${PROJECT_BINARY_DIR}/get_data.sh
                       WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                       VERBATIM)

    add_test(NAME nist-nls-test
             COMMAND ${PROJECT_BINARY_DIR}/run_tests.sh)

    # Depends on TestU01 and gcc
    if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    	find_package(TestU01 QUIET)
    	if(NOT TestU01_FOUND)
            add_subdirectory(cmake/testu01)
    	endif(NOT TestU01_FOUND)
    	include_directories(${TestU01_INCLUDE_DIR})

    	add_executable(rng-test ${PROJECT_SOURCE_DIR}/test/rng/gretltest.c)
    	add_dependencies(rng-test TestU01)
        target_link_libraries(rng-test gui2_static gretl_static ${TestU01_LIBRARIES} ${GRETL_LIBS})

    	add_test(NAME rng-test
                 COMMAND rng-test)
    endif()
endif(BUILD_TESTS)

##############
## Coverage ##
##############

option(ENABLE_COVERAGE "Enable code coverage generation" OFF)
if(${ENABLE_COVERAGE} AND "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_BUILD_TYPE "Debug")
    add_definitions(-fprofile-arcs -ftest-coverage)

    # link gretl libraries to support coverage
    target_link_libraries(gui2_static -fprofile-arcs -ftest-coverage)
    target_link_libraries(gretl_static -fprofile-arcs -ftest-coverage)
    target_link_libraries(plugin -fprofile-arcs -ftest-coverage)

    target_link_libraries(nistcheck -fprofile-arcs -ftest-coverage)
    target_link_libraries(nist-nls-test -fprofile-arcs -ftest-coverage)
    target_link_libraries(rng-test -fprofile-arcs -ftest-coverage)
endif()


################
## Installing ##
################
message(STATUS "${PROJECT_NAME} will be installed in ${CMAKE_INSTALL_PREFIX}")
if(CMAKE_INSTALL_PREFIX STREQUAL "/usr/local" OR CMAKE_INSTALL_PREFIX STREQUAL "/usr")
	# Installing system wide
	if(Gtksourceview2_FOUND)
		set(GTK_SYNTAX "gtksourceview-2.0/language-specs/")
	else(Gtksourceview2_FOUND)
		if(Gtksourceview3_FOUND)
			set(GTK_SYNTAX "gtksourceview-3.0/language-specs/")
		endif(Gtksourceview3_FOUND)
	endif(Gtksourceview2_FOUND)
	install(FILES ${CMAKE_BINARY_DIR}/gretl.lang ${CMAKE_BINARY_DIR}/gnuplot.lang
			DESTINATION share/${GTK_SYNTAX})
else()
	# Installing in user space
	if(Gtksourceview2_FOUND)
		set(GTK_SYNTAX "$ENV{HOME}/.local/share/gtksourceview-2.0/language-specs/")
	else(Gtksourceview2_FOUND)
		if(Gtksourceview3_FOUND)
			set(GTK_SYNTAX "$ENV{HOME}/.local/share/gtksourceview-3.0/language-specs/")
		endif(Gtksourceview3_FOUND)
	endif(Gtksourceview2_FOUND)
	install(FILES ${CMAKE_BINARY_DIR}/gretl.lang ${CMAKE_BINARY_DIR}/gnuplot.lang
			DESTINATION ${GTK_SYNTAX})
endif()

install(TARGETS gretlcli gretl_gui gretlcli-mpi 
		rng plugin minpack cephes dcmt gretl gui2 complete
		rng_static plugin_static minpack_static cephes_static 
		dcmt_static gretl_static gui2_static complete_static
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/
  		DESTINATION include
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/share/
		DESTINATION share
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/etc/
		DESTINATION etc
)
