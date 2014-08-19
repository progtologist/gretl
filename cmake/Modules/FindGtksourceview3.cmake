# - Try to find GtkSourceView3
# Once done, this will define
#
#  Gtksourceview3_FOUND - system has GtkSourceView
#  Gtksourceview3_INCLUDE_DIRS - the GtkSourceView include directories
#  Gtksourceview3_LIBRARIES - link these to use GtkSourceView
#
# See documentation on how to write CMake scripts at
# http://www.cmake.org/Wiki/CMake:How_To_Find_Libraries

include(LibFindMacros)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(Gtksourceview3_PKGCONF gtksourceview-3.0)

# Main include dir
find_path(Gtksourceview3_INCLUDE_DIR
	NAMES gtksourceview/gtksourceview.h
	PATHS ${Gtksourceview3_PKGCONF_INCLUDE_DIRS}
	PATH_SUFFIXES gtksourceview-3.0
)

# Find the library
find_library(Gtksourceview3_LIBRARY
	NAMES gtksourceview-3.0
	PATHS ${Gtksourceview3_PKGCONF_LIBRARY_DIRS}
)

set(Gtksourceview3_PROCESS_INCLUDES Gtksourceview3_INCLUDE_DIR)
set(Gtksourceview3_PROCESS_LIBS Gtksourceview3_LIBRARY)
libfind_process(Gtksourceview3)

