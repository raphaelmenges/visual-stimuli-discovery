cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# Macros
include(${CMAKE_MODULE_PATH}/macros.cmake)

# Override install macro to prohibit any installation
macro(install)
endmacro()
set(CMAKE_INSTALL_PREFIX "" CACHE INTERNAL "")

# Preprocessor commands
add_definitions(-DEIGEN_MPL2_ONLY) # include no LGPL code from EIGEN

#---------------------------------------------------------------------------------------
# Version
#---------------------------------------------------------------------------------------

# Version of framework (use GM namespace on C++-side), as printed by Master (note: previously, the framework was called GazeMiner)
set(MAJOR_VERSION "0")
set(MINOR_VERSION "1")
set(PATCH_VERSION "0")
add_definitions(-DGM_MAJOR_VERSION=${MAJOR_VERSION})
add_definitions(-DGM_MINOR_VERSION=${MINOR_VERSION})
add_definitions(-DGM_PATCH_VERSION=${PATCH_VERSION})
add_definitions(-DGM_VERSION="${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}")

#---------------------------------------------------------------------------------------
# Configuration
#---------------------------------------------------------------------------------------

# Configuration
set(CONFIG "Debug" CACHE STRING "Build configuration (Debug / Release).")
set_property(CACHE CONFIG PROPERTY STRINGS Debug Release) # values config can be assigned
string(COMPARE EQUAL "${CONFIG}" "Debug" cmp)
if(cmp)
	set(CMAKE_CONFIGURATION_TYPES "Debug" CACHE INTERNAL "" FORCE)
else()
	set(CMAKE_CONFIGURATION_TYPES "Release" CACHE INTERNAL "" FORCE) # fallback
	add_definitions(-DNDEBUG) # disable asserts in release mode
endif()
set(CMAKE_BUILD_TYPE "${CONFIG}") # tell CMake the build type
message(STATUS "Build Configuration = '${CMAKE_CONFIGURATION_TYPES}'")
string(TOLOWER "${CMAKE_CONFIGURATION_TYPES}" CONFIG_LOWER_CASE)

#---------------------------------------------------------------------------------------
# Project
#---------------------------------------------------------------------------------------

# Set project name
project(VisualStimuliDiscovery_${CONFIG_LOWER_CASE})

# Set root directory of framework
set(FRAMEWORK_DIR "${CMAKE_SOURCE_DIR}/..")

# Activate C++11 standard
if(NOT MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
endif(NOT MSVC)

# Visual debugging (whether GUI through OpenCV is available or not)
set(VISUAL_DEBUG OFF CACHE BOOL "Whether GUI through OpenCV is used (ON) or not (OFF).")

# Outputs
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/exe)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<1:${EXECUTABLE_OUTPUT_PATH}>) # to avoid configuration subfolders in MSVC
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

#---------------------------------------------------------------------------------------
# Definitions
#---------------------------------------------------------------------------------------

# Deployment information
set(DEPLOY OFF CACHE BOOL "Use deployment configuration.")
if(WIN32)
	set(DEPLOY_PATH "C:/VisualStimuliDiscovery" CACHE STRING "Path were to deploy to.")
elseif(UNIX)
	set(DEPLOY_PATH "/opt/VisualStimuliDiscovery" CACHE STRING "Path were to deploy to.")
endif()

# Output path
if(WIN32)
	set(OUT_PATH "C:/vsd_out" CACHE STRING "Path were to store output in. Directory must exist.")
elseif(UNIX)
	set(OUT_PATH "~/vsd_out" CACHE STRING "Path were to store output in. Directory must exist.")
endif()
add_definitions(-DGM_OUT_PATH="${OUT_PATH}/")

# Multi-threaded task execution
set(MT_TASK ON CACHE BOOL "Whether tasks are executed multi- (ON) or single-threaded (OFF).")

# Project paths
set(EXT_PATH ${CMAKE_SOURCE_DIR}/ext)
set(RES_PATH ${CMAKE_SOURCE_DIR}/res)
set(EXE_PATH ${CMAKE_SOURCE_DIR}/src/exe)
set(LIB_PATH ${CMAKE_SOURCE_DIR}/src/lib)
set(TEST_PATH ${CMAKE_SOURCE_DIR}/src/test)

# Add resources folder as add_subdirectory
add_subdirectory(${RES_PATH})

# Defines for C++ side
if(${VISUAL_DEBUG})
	add_definitions(-DGM_VISUAL_DEBUG)
endif()
if(${DEPLOY})
	add_definitions(-DGM_DEPLOY)
endif()
if(${MT_TASK})
	add_definitions(-DGM_MT_TASK)
endif()
add_definitions(-DGM_RES_PATH="${RES_PATH}")

#---------------------------------------------------------------------------------------
# Third party frameworks
#---------------------------------------------------------------------------------------

# OpenCV
set(OPEN_CV_PATH ${FRAMEWORK_DIR}/_generated/${CONFIG_LOWER_CASE}/opencv/install)

# Find static OpenCV libraries
if (${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC) # Windows

	include_directories(${OPEN_CV_PATH}/include) # include dir

	# Using Visual Studio C++
	file(GLOB OPENCV_LIBS ${OPEN_CV_PATH}/x64/vc*/staticlib/*.lib ${OPEN_CV_PATH}/staticlib/*.lib) # assumes that only one Visual Studio version was used to build libraries

else() # Linux

	include_directories(${OPEN_CV_PATH}/include/opencv4) # include dir

	# GCC requires certain order at linking. Remember to add further OpenCV modules here
	set(OPENCV_LIBS "")
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/libopencv_core.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/libopencv_features2d.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/libopencv_xfeatures2d.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/libopencv_flann.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/libopencv_calib3d.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/libopencv_highgui.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/libopencv_imgproc.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/libopencv_ml.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/libopencv_imgcodecs.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/libopencv_videoio.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/libopencv_video.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/libopencv_photo.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/opencv4/3rdparty/libippiw.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/opencv4/3rdparty/libippicv.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/opencv4/3rdparty/libittnotify.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/opencv4/3rdparty/liblibjpeg-turbo.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/opencv4/3rdparty/liblibpng.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/opencv4/3rdparty/libzlib.a)
	list(APPEND OPENCV_LIBS ${OPEN_CV_PATH}/lib/libopencv_core.a)
	
	# GTK2
	if(${VISUAL_DEBUG})
		find_package(PkgConfig REQUIRED)
		pkg_check_modules(GTK REQUIRED gtk+-2.0)
		list(APPEND FINAL_LIBRARIES ${GTK_LIBRARIES})
	endif()
endif()
set(EXTERNAL_LIBRARIES ${EXTERNAL_LIBRARIES} ${OPENCV_LIBS}) # collect libraries to link into

# Tesseract
set(LEPTONICA_PATH ${FRAMEWORK_DIR}/_generated/${CONFIG_LOWER_CASE}/leptonica/install)
set(Leptonica_DIR ${LEPTONICA_PATH}/cmake)
set(TESSERACT_PATH ${FRAMEWORK_DIR}/_generated/${CONFIG_LOWER_CASE}/tesseract/install)
find_package(Tesseract REQUIRED PATHS ${TESSERACT_PATH} NO_DEFAULT_PATH)
include_directories(${Tesseract_INCLUDE_DIRS})
include_directories(${Leptonica_INCLUDE_DIRS})
set(EXTERNAL_LIBRARIES ${EXTERNAL_LIBRARIES} ${Tesseract_LIBRARIES}) # collect libraries to link into at compliation
set(Tesseract_DIR ${Tesseract_DIR} CACHE INTERNAL "") # hide tesseract_DIR from CMake GUI

# Remark: Linux links at compile and run time into the same .so.

# Collect libraries to link into at run time
if(${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC) # Windows
	file(GLOB LEPTONICA_SHARED_LIBS ${LEPTONICA_PATH}/bin/leptonica*.dll)
	file(GLOB TESSERACT_SHARED_LIBS ${TESSERACT_PATH}/bin/tesseract*.dll)
	set(SHARED_RUN_TIME_LIBRARIES 
		${SHARED_RUN_TIME_LIBRARIES}
		${LEPTONICA_SHARED_LIBS}
		${TESSERACT_SHARED_LIBS})
else() # Linux
	set(SHARED_RUN_TIME_LIBRARIES 
		${SHARED_RUN_TIME_LIBRARIES}
		${LEPTONICA_PATH}/lib/libleptonica.so.5.3.0
		${TESSERACT_PATH}/lib/libtesseract.so.4.0.0)
endif()

# Shogun
set(SHOGUN_PATH ${FRAMEWORK_DIR}/_generated/${CONFIG_LOWER_CASE}/shogun/install)
include_directories(${SHOGUN_PATH}/include) # include dir

# Operation system specific stuff
if(WIN32)
	set(EXTERNAL_LIBRARIES ${EXTERNAL_LIBRARIES} ${SHOGUN_PATH}/lib/shogun.lib)
	list(APPEND FINAL_LIBRARIES winmm.lib) # only for Windows, required by Shogun...
	include_directories(${SHOGUN_PATH}/include/shogun/lib/external/MSDirent) # required on Windows
else() # Linux
	set(EXTERNAL_LIBRARIES ${EXTERNAL_LIBRARIES} ${SHOGUN_PATH}/lib/libshogun.a)
	SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fopenmp") # EIGEN in Shogun needs OpenMP
endif()

#---------------------------------------------------------------------------------------
# Libraries
#---------------------------------------------------------------------------------------

# Include path to internal libraries folder
include_directories(${LIB_PATH})

# Libraries for GCC to link in the end
if (NOT ${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC) # Linux (GCC check does not work somehow)
	list(APPEND FINAL_LIBRARIES pthread)
	list(APPEND FINAL_LIBRARIES dl)
	list(APPEND FINAL_LIBRARIES stdc++fs)
endif()

# EIGEN
set(EIGEN_PATH ${EXT_PATH}/eigen-git-mirror)
include_directories(${EIGEN_PATH})

# cxxopts
set(CXXOPTS_PATH ${EXT_PATH}/cxxopts)
include_directories(${CXXOPTS_PATH}/include)

# libsimplewebm
set(SIMPLEWEBM_PATH ${EXT_PATH}/libsimplewebm)
add_subdirectory(${SIMPLEWEBM_PATH})
include_directories(${SIMPLEWEBM_PATH})
set(SIMPLE_WEBM_BUILD_EXAMPLE OFF CACHE INTERNAL "")
set(EXTERNAL_LIBRARIES ${EXTERNAL_LIBRARIES} libsimplewebm)

# cpptoml
include_directories(${EXT_PATH}/cpptoml/include)

# spdlog
include_directories(${EXT_PATH}/spdlog/include)

# cvui
include_directories(${EXT_PATH}/cvui)

# json
include_directories(${EXT_PATH}/json/include)

# ThreadPool
include_directories(${EXT_PATH}/ThreadPool)

# clip (only required for visual debugging)
if(${VISUAL_DEBUG})
	set(CLIP_PATH ${EXT_PATH}/clip)
	add_subdirectory(${CLIP_PATH})
	include_directories(${CLIP_PATH})
	set(CLIP_EXAMPLES OFF CACHE INTERNAL "" FORCE)
	set(CLIP_TESTS OFF CACHE INTERNAL "" FORCE)
	set(CLIP_X11_WITH_PNG OFF CACHE INTERNAL "" FORCE) # on adds path to system PNG library. If on, maybe link to local PNG version from OpenCV?
	set(EXTERNAL_LIBRARIES ${EXTERNAL_LIBRARIES} clip)
endif()

#---------------------------------------------------------------------------------------
# Subprojects
#---------------------------------------------------------------------------------------

# Project internal libraries
# set(INTERNAL_LIBRARIES "") # collect libraries to link later into executable
set(INTERNAL_LIBRARY_CODE "") # collect code of libraries to use in executables
message(STATUS "Libraries")
SUBDIRLIST(SUBDIRS ${LIB_PATH})
foreach(SUBDIR ${SUBDIRS})
	message(STATUS "- ${SUBDIR}")
	add_subdirectory(${LIB_PATH}/${SUBDIR})
endforeach()

# Collect available executables
message(STATUS "Executables")
SUBDIRLIST(SUBDIRS ${EXE_PATH})

# List of deprecated projects which should be not built
set(LEGACY_EXES "")
list(APPEND LEGACY_EXES "LogExplorer")
list(APPEND LEGACY_EXES "VideoWalker")

# Create variables to control which executable to build. Default for legacy ones is off
foreach(SUBDIR ${SUBDIRS})
	list (FIND LEGACY_EXES ${SUBDIR} _index)
	if (${_index} GREATER -1)
		set("EXE_${SUBDIR}" OFF CACHE BOOL "Decide whether executable is built.")
	else()
		set("EXE_${SUBDIR}" ON CACHE BOOL "Decide whether executable is built.")
	endif()
endforeach()

# Check which executables to build
foreach(SUBDIR ${SUBDIRS})
	if(${EXE_${SUBDIR}})
		message(STATUS "- ${SUBDIR}")
		add_subdirectory(${EXE_PATH}/${SUBDIR})
	endif()
endforeach()

#---------------------------------------------------------------------------------------
# Test
#---------------------------------------------------------------------------------------

set("BUILD_TESTS" OFF CACHE BOOL "Decide whether tests are built.")

if(${BUILD_TESTS})

	# Google Test
	set(GTEST_PATH ${EXT_PATH}/googletest)
	set(BUILD_GMOCK ON CACHE INTERNAL "")
	set(BUILD_GTEST OFF CACHE INTERNAL "")
	set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
	set(gmock_build_tests OFF CACHE INTERNAL "")
	set(gtest_build_samples OFF CACHE INTERNAL "")
	set(gtest_build_tests OFF CACHE INTERNAL "")
	set(gtest_disable_pthreads OFF CACHE INTERNAL "")
	set(gtest_force_shared_crt ON CACHE INTERNAL "")
	set(gtest_hide_internal_symbols ON CACHE INTERNAL "")
	add_subdirectory(${GTEST_PATH})
	set(INSTALL_GMOCK OFF CACHE INTERNAL "" FORCE) # must be called after add_subdirectory with force (as not set with 'set' but with other function)
	set(INSTALL_GTEST OFF CACHE INTERNAL "" FORCE) # must be called after add_subdirectory with force (as not set with 'set' but with other function)
	include_directories(${GTEST_PATH}/googlemock/include)
	include_directories(${GTEST_PATH}/googletest/include)
	set(TEST_LIBRARIES gmock_main gtest_main) # use the versions that provide main function

	# Tests
	set(TESTS "") # collect tests
	message(STATUS "Collecting Tests")
	SUBDIRLIST(SUBDIRS ${TEST_PATH})
	foreach(SUBDIR ${SUBDIRS})
		message(STATUS "- ${SUBDIR}")
		add_subdirectory(${TEST_PATH}/${SUBDIR})
	endforeach()

	# CTest
	enable_testing()
	foreach(TEST ${TESTS})

		# Accumulate path to tests
		set(TEST_PATH ${EXECUTABLE_OUTPUT_PATH})
		
		# Workaround for MSVC, where binaries are stored in subfolder named after configuration
		if(MSVC)
			set(TEST_PATH ${TEST_PATH}/${CONFIG_LOWER_CASE})
		endif(MSVC)

		# Add test
		add_test(GeneralTest ${TEST_PATH}/${TEST})
		
	endforeach()

endif()

#---------------------------------------------------------------------------------------
# Copy of shared libraries
#---------------------------------------------------------------------------------------

if (${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC) # Windows

	# Copy shared run time libraries to same folder as executables
	foreach(LIB ${SHARED_RUN_TIME_LIBRARIES})
		file(COPY ${LIB} DESTINATION ${EXECUTABLE_OUTPUT_PATH})
	endforeach()

else() # Linux

endif()

#---------------------------------------------------------------------------------------
# Deployment
#---------------------------------------------------------------------------------------

# Deployment execution
if(${DEPLOY})

	# Make path to deploy into is not yet existent
	file(MAKE_DIRECTORY ${DEPLOY_PATH})

	# Copy resources
	file(COPY ${RES_PATH} DESTINATION ${DEPLOY_PATH})

	# Copy shared run time libraries
	foreach(LIB ${SHARED_RUN_TIME_LIBRARIES})
		file(COPY ${LIB} DESTINATION ${DEPLOY_PATH})
	endforeach()

endif()