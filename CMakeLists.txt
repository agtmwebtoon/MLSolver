##
#  CMake script for the MLSolver program:
##

# Set the name of the project and target:
set(TARGET "main")

# Declare all source files the target consists of:
set(TARGET_SRC
  ${TARGET}.cc             # Main source file
  util/Parameters.cpp     # Parameters implementation file
  )

# Include directories for headers:
include_directories(
  util                  # Include directory for Parameters.h
  .                      # Current directory for main.cc and FEM.h
)

# Usually, you will not need to modify anything beyond this point...

cmake_minimum_required(VERSION 3.13.4)

# Find deal.II
find_package(deal.II 9.6.0
  HINTS ${deal.II_DIR} ${DEAL_II_DIR} ../ ../../ $ENV{DEAL_II_DIR}
  )
if(NOT ${deal.II_FOUND})
  message(FATAL_ERROR "\n"
    "*** Could not locate a (sufficiently recent) version of deal.II. ***\n\n"
    "You may want to either pass a flag -DDEAL_II_DIR=/path/to/deal.II to cmake\n"
    "or set an environment variable \"DEAL_II_DIR\" that contains this path."
    )
endif()

deal_ii_initialize_cached_variables()
project(${TARGET})
deal_ii_invoke_autopilot()