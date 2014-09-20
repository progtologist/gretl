include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckTypeSize)
include(CheckLibraryExists)

check_include_files("stdlib.h"					HAVE_STDLIB_H)
check_include_files("stdarg.h"					HAVE_STDARG_H)
check_include_files("string.h"					HAVE_STRING_H)
check_include_files("float.h"					HAVE_FLOAT_H)
if((${HAVE_STDLIB_H} AND ${HAVE_STDARG_H}) AND (${HAVE_STRING_H} AND ${HAVE_FLOAT_H}))
	set(STDC_HEADERS ON)
endif()

check_include_files("netinet/in.h" 				HAVE_IN_ADDR)
check_include_files("netinet/in.h" 				HAVE_SOCKADDR_IN)
check_include_files("sys/socket.h" 				HAVE_SYS_SOCKET_H)
check_include_files("netdb.h" 					HAVE_NETDB_H)
if (${HAVE_SYS_SOCKET_H} AND ${HAVE_NETDB_H})
	set(ENABLE_MAILER ON)
endif()
check_include_files("stdint.h"					HAVE_STDINT_H)
check_include_files("unistd.h"					HAVE_UNISTD_H)
check_include_files("dirent.h"					HAVE_DIRENT_H)
check_include_files("fnmatch.h"					HAVE_FNMATCH_H)
check_include_files("sys/wait.h"				HAVE_SYS_WAIT_H)
check_include_files("sys/times.h"				HAVE_SYS_TIMES_H)
check_include_files("byteswap.h"				HAVE_BYTESWAP_H)
check_include_files("libproc.h"					HAVE_LIBPROC_H)
check_include_files("sys/proc/info.h"			HAVE_SYS_PROC_INFO_H)
check_include_files("immintrin.h"				HAVE_IMMINTRIN_H)

check_function_exists(mmap						HAVE_MMAP)
check_function_exists(vasprintf					HAVE_VASPRINTF)
check_function_exists(posix_memalign			HAVE_POSIX_MEMALIGN)
check_function_exists(getdomainname				GETDOMAINNAME)

check_type_size("long double" 					HAVE_LONG_DOUBLE)
check_type_size("int"							SIZEOF_INT)

check_library_exists(readline rl_completion_matches "" NEW_READLINE)

include(TestSignalType)
include(TestBigEndian)
test_big_endian(WORDS_BIGENDIAN)

If(NOT("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"))
	include(OptimizeForArchitecture)
	OptimizeForArchitecture()
endif()

if(NOT(GTK2_MINOR_VERSION LESS 20))
	set(HAVE_GTK_SPINNER ON)
endif()

if(LAPACK_FOUND)
	check_library_exists(lapack dgejsv_	"" HAVE_LAPACK_3_2)
endif(LAPACK_FOUND)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(OS_OSX ON)
	set(MAC_NATIVE ON)
endif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")

# Default Variables
if(NOT DEFINED PACKAGE)
	set(PACKAGE ON)
endif(NOT DEFINED PACKAGE)

if(NOT DEFINED ENABLE_NLS)
	set(ENABLE_NLS ON)
endif(NOT DEFINED ENABLE_NLS)

if(NOT DEFINED GRETL_PREFIX)
    if(HOME_BUILD)
        set(GRETL_PREFIX  "${PROJECT_SOURCE_DIR}")
    else(HOME_BUILD)
	set(GRETL_PREFIX "/usr/local")
    endif(HOME_BUILD)
endif(NOT DEFINED GRETL_PREFIX)

if(NOT DEFINED PKGBUILD)
	set(PKGBUILD OFF)
endif(NOT DEFINED PKGBUILD)

if(NOT DEFINED HAVE_X12A)
	set(HAVE_X12A ON)
endif(NOT DEFINED HAVE_X12A)

if(NOT DEFINED HAVE_TRAMO)
	set(HAVE_TRAMO ON)
endif(NOT DEFINED HAVE_TRAMO)

if(NOT DEFINED HAVE_AUDIO)
	set(HAVE_AUDIO OFF)
endif(NOT DEFINED HAVE_AUDIO)

if(NOT DEFINED OS_OSX)
	set(OS_OSX OFF)
endif(NOT DEFINED OS_OSX)

if(NOT DEFINED LOCALEDIR)
	set(LOCALEDIR "${GRETL_PREFIX}/share/locale")
endif(NOT DEFINED LOCALEDIR)
add_definitions(-DLOCALEDIR="${LOCALEDIR}")

if(NOT DEFINED (WIN32 OR WIN64))
	add_definitions(-DHAVE_CONFIG_H)
endif()
