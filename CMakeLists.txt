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

###########
## FILES ##
###########

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


##############
## Building ##
##############

add_library(rng ${RNG_H} ${RNG_C})

add_library(plugin ${PLUGIN_H} ${PLUGIN_C})

add_library(minpack ${MINPACK_H} ${MINPACK_C})

add_library(cephes ${CEPHES_H} ${CEPHES_C})

add_library(dcmt ${DCMT_H} ${DCMT_C})

list(REMOVE_DUPLICATES GRETL_LIBS)
add_library(gretl ${GRETL_LIB_H} ${GRETL_LIB_C})
target_link_libraries(gretl rng dcmt cephes minpack ${GRETL_LIBS})

add_library(gui2 ${GUI2_H} ${GUI2_C})

add_library(complete ${PROJECT_SOURCE_DIR}/src/cli/complete.c)

foreach(ITEM ${EXTRA_C})
	get_filename_component(filename ${ITEM} NAME_WE)
	add_executable(${filename} ${ITEM})
	target_link_libraries(${filename} gretl)
endforeach()

add_executable(gretlcli ${PROJECT_SOURCE_DIR}/src/cli/gretlcli.c)
target_link_libraries(gretlcli gretl complete)

if(MPI_C_FOUND)
	add_executable(gretlcli-mpi ${PROJECT_SOURCE_DIR}/src/cli/gretlcli-mpi.c)
	target_link_libraries(gretlcli-mpi gretl)
	if(MPI_C_COMPILE_FLAGS)
  		set_target_properties(gretlcli-mpi PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
	endif()
	if(MPI_C_LINK_FLAGS)
  		set_target_properties(gretlcli-mpi PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
	endif()
endif(MPI_C_FOUND)

#############
## Testing ##
#############