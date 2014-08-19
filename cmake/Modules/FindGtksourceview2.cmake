# - Try to find GtkSourceView3
# Once done, this will define
#
#  Gtksourceview2_FOUND - system has GtkSourceView
#  Gtksourceview2_INCLUDE_DIRS - the GtkSourceView include directories
#  Gtksourceview2_LIBRARIES - link these to use GtkSourceView
#
# See documentation on how to write CMake scripts at
# http://www.cmake.org/Wiki/CMake:How_To_Find_Libraries

include(LibFindMacros)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(Gtksourceview2_PKGCONF gtksourceview-2.0)

# Main include dir
find_path(Gtksourceview2_INCLUDE_DIR
	NAMES gtksourceview/gtksourceview.h
	PATHS ${Gtksourceview2_PKGCONF_INCLUDE_DIRS}
	PATH_SUFFIXES gtksourceview-2.0
)

# Find the library
find_library(Gtksourceview2_LIBRARY
	NAMES gtksourceview-2.0
	PATHS ${Gtksourceview2_PKGCONF_LIBRARY_DIRS}
)

set(Gtksourceview2_PROCESS_INCLUDES Gtksourceview2_INCLUDE_DIR)
set(Gtksourceview2_PROCESS_LIBS Gtksourceview2_LIBRARY)
libfind_process(Gtksourceview2)

