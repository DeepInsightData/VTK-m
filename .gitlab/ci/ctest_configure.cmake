##=============================================================================
##
##  Copyright (c) Kitware, Inc.
##  All rights reserved.
##  See LICENSE.txt for details.
##
##  This software is distributed WITHOUT ANY WARRANTY; without even
##  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
##  PURPOSE.  See the above copyright notice for more information.
##
##=============================================================================

cmake_minimum_required(VERSION 3.8)

# Read the files from the build directory that contain
# host information ( name, parallel level, etc )
include("$ENV{CI_PROJECT_DIR}/build/CIState.cmake")
ctest_read_custom_files("${CTEST_BINARY_DIRECTORY}")

set(cmake_args
  -C "${CMAKE_CURRENT_LIST_DIR}/config/initial_config.cmake")

# Create an entry in CDash.
ctest_start(Experimental TRACK "${CTEST_TRACK}")

# Gather update information.
find_package(Git)
set(CTEST_UPDATE_VERSION_ONLY ON)
set(CTEST_UPDATE_COMMAND "${GIT_EXECUTABLE}")
ctest_update()

# Configure the project.
ctest_configure(APPEND
  OPTIONS "${cmake_args}"
  RETURN_VALUE configure_result)

# We can now submit because we've configured. This is a cmb-superbuild-ism.
ctest_submit(PARTS Update)
ctest_submit(PARTS Configure)

if (configure_result)
  message(FATAL_ERROR
    "Failed to configure")
endif ()
