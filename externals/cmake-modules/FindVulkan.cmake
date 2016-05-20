# - Try to find Vulkan SDK
# Once done, this will define
#
#  Vulkan_FOUND - system has Vulkan SDK
#  Vulkan_INCLUDE_DIRS - the Vulkan include directories
#  Vulkan_LIBRARIES - link these to use Vulkan

include(LibFindMacros)

# Dependencies
#libfind_package(PACKAGE DEPENDANCY)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(Vulkan_PKGCONF Vulkan)

# Include dir
find_path(Vulkan_INCLUDE_DIR
  NAMES vulkan/vulkan.h
  PATHS ${Vulkan_PKGCONF_INCLUDE_DIRS}
)

# Finally the library itself
find_library(Vulkan_LIBRARY
  NAMES vulkan-1
  PATHS ${Vulkan_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(Vulkan_PROCESS_INCLUDES Vulkan_INCLUDE_DIR Vulkan_INCLUDE_DIRS)
set(Vulkan_PROCESS_LIBS Vulkan_LIBRARY Vulkan_LIBRARIES)
libfind_process(Vulkan)