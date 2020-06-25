import os
import sys
import subprocess
import argparse

"""
Script to generate the framework. Please execute this instead of manual CMake calls.

Remarks:
- 64bit, only
"""

# #########################
# ######## Defines ########
# #########################
DEBUG_SUBDIR = "/debug"
RELEASE_SUBDIR = "/release"
GENERATED_DIR = "./_generated"
GENERATED_OPEN_CV_SUBDIR = "/opencv"
GENERATED_LEPTONICA_SUBDIR = "/leptonica"
GENERATED_TESSERACT_SUBDIR = "/tesseract"
GENERATED_SHOGUN_SUBDIR = "/shogun"
GENERATED_BUILD_SUBDIR = "/build"
GENERATED_INSTALL_SUBDIR = "/install"
BUILD_DIR = "./_build"

# #########################
# ######## Classes ########
# #########################

# Available generators
class Generator:
	MSVC2015, MSVC2017, MSVC2019, Make = range(4)
	def to_string(generator):
		if(generator == Generator.MSVC2015):
			return "Visual Studio 14 Win64"
		elif(generator == Generator.MSVC2017):
			return "Visual Studio 15 Win64"
		elif(generator == Generator.MSVC2019):
			return "Visual Studio 16"
		else:
			return "Unix Makefiles"

# Available configurations
class Configuration:
	Debug, Release = range(2)
	def to_string(config):
		if(config == Configuration.Debug):
			return "Debug"
		else:
			return "Release"

# ########################################
# ######## Command Line Arguments ########
# ########################################
			
# Parse command line arguments
parser = argparse.ArgumentParser()
parser.add_argument("-c", "--configuration", help="build configuration, either 'release' or 'debug'")
parser.add_argument("-g", "--generator", help="generator, either 'MSVC2015', 'MSVC2017' or 'MSVC2019' on Windows or 'make' on Linux")
parser.add_argument("-v", "--visualdebug", help="visual debugging mode. aka non-headless", action='store_true')
parser.add_argument("-d", "--deploy", help="deploy build binaries and corresponding resources", action='store_true')
parser.add_argument("-s", "--singlethreaded", help="execute tasks single- instead of multi-threaded", action='store_true')
parser.add_argument("-b", "--build", help="compiles the framework after generation", action='store_true')
args = parser.parse_args()

# #########################################
# ######## Platform Specific Setup ########
# #########################################

# Retrieve platform
cmake_exe = "cmake"
generator = Generator.Make
if sys.platform == "linux" or sys.platform == "linux2": # Linux

	# CMake exe path
	cmake_exe = "cmake"

	# Generator
	generator = Generator.Make # ignore command line argument, as there is only one generator supported
	print("Generator: 'Make'.")

elif sys.platform == "win32": # Windows

	# CMake exe path
	cmake_exe = "C:/Program Files/CMake/bin/cmake.exe" # TODO: might fail for 32bit CMake on 64bit systems

	# Generator
	if args.generator:
		if(args.generator == "MSVC2015"):
			generator = Generator.MSVC2015
			print("Generator: 'MSVC2015'.")
		elif(args.generator == "MSVC2017"):
			generator = Generator.MSVC2017
			print("Generator: 'MSVC2017'.")
		elif(args.generator == "MSVC2019"):
			generator = Generator.MSVC2019
			print("Generator: 'MSVC2019'.")
		else:
			generator = Generator.MSVC2015
			print("Provided generator unknown. Applying fallback to 'MSVC2015'.")
	else:
		generator = Generator.MSVC2015
		print("No generator provided. Applying fallback to 'MSVC2015'.")

# ###############################
# ######## Configuration ########
# ###############################

# Retrieve configuration
config = Configuration.Release
if args.configuration:
	if(args.configuration == "debug"):
		config = Configuration.Debug
		print("Configuration: 'debug'.")
	elif(args.configuration == "release"):
		config = Configuration.Release
		print("Configuration: 'release'.")
	else:
		config = Configuration.Release
		print("Provided configuration unknown. Applying fallback to 'release'.")
else:
	config = Configuration.Release
	print("No configuration provided. Applying fallback to 'release'.")

# #####################################
# ######## Further Information ########
# #####################################

print("Visual Debug: " + ("'ON'" if args.visualdebug else "'OFF'") + ".")
print("Deploy: " + ("'ON'" if args.deploy else "'OFF'") + ".")
print("Single-Threaded: " + ("'ON'" if args.singlethreaded else "'OFF'") + ".")
print("Build: " + ("'ON'" if args.build else "'OFF'") + ".")
input("Press Enter to continue...")

# #############################
# ######## Directories ########
# #############################

# Function to create directory if not yet existing
def create_dir(dir):
	if not os.path.exists(dir):
		os.mkdir(dir)

# Create directory structure for framework
config_subdir = ""
if config == Configuration.Debug: # debug configuration
	config_subdir = DEBUG_SUBDIR
else: # release configuration
	config_subdir = RELEASE_SUBDIR

# Top-level folders
create_dir(GENERATED_DIR)
create_dir(BUILD_DIR)

# Generated
create_dir(GENERATED_DIR + config_subdir)
create_dir(GENERATED_DIR + config_subdir + GENERATED_OPEN_CV_SUBDIR)
create_dir(GENERATED_DIR + config_subdir + GENERATED_OPEN_CV_SUBDIR + GENERATED_BUILD_SUBDIR)
create_dir(GENERATED_DIR + config_subdir + GENERATED_OPEN_CV_SUBDIR + GENERATED_INSTALL_SUBDIR)
create_dir(GENERATED_DIR + config_subdir + GENERATED_LEPTONICA_SUBDIR)
create_dir(GENERATED_DIR + config_subdir + GENERATED_LEPTONICA_SUBDIR + GENERATED_BUILD_SUBDIR)
create_dir(GENERATED_DIR + config_subdir + GENERATED_LEPTONICA_SUBDIR + GENERATED_INSTALL_SUBDIR)
create_dir(GENERATED_DIR + config_subdir + GENERATED_TESSERACT_SUBDIR)
create_dir(GENERATED_DIR + config_subdir + GENERATED_TESSERACT_SUBDIR + GENERATED_BUILD_SUBDIR)
create_dir(GENERATED_DIR + config_subdir + GENERATED_TESSERACT_SUBDIR + GENERATED_INSTALL_SUBDIR)
create_dir(GENERATED_DIR + config_subdir + GENERATED_SHOGUN_SUBDIR)
create_dir(GENERATED_DIR + config_subdir + GENERATED_SHOGUN_SUBDIR + GENERATED_BUILD_SUBDIR)
create_dir(GENERATED_DIR + config_subdir + GENERATED_SHOGUN_SUBDIR + GENERATED_INSTALL_SUBDIR)

# Own project
create_dir(BUILD_DIR + config_subdir)

# Generate absolute paths before going into directories to execute cmake
open_cv_build_dir = os.path.abspath(GENERATED_DIR + config_subdir + GENERATED_OPEN_CV_SUBDIR + GENERATED_BUILD_SUBDIR).replace("\\","/")
open_cv_install_dir = os.path.abspath(GENERATED_DIR + config_subdir + GENERATED_OPEN_CV_SUBDIR + GENERATED_INSTALL_SUBDIR).replace("\\","/")
leptonica_build_dir = os.path.abspath(GENERATED_DIR + config_subdir + GENERATED_LEPTONICA_SUBDIR + GENERATED_BUILD_SUBDIR).replace("\\","/")
leptonica_install_dir = os.path.abspath(GENERATED_DIR + config_subdir + GENERATED_LEPTONICA_SUBDIR + GENERATED_INSTALL_SUBDIR).replace("\\","/")
tesseract_build_dir = os.path.abspath(GENERATED_DIR + config_subdir + GENERATED_TESSERACT_SUBDIR + GENERATED_BUILD_SUBDIR).replace("\\","/")
tesseract_install_dir = os.path.abspath(GENERATED_DIR + config_subdir + GENERATED_TESSERACT_SUBDIR + GENERATED_INSTALL_SUBDIR).replace("\\","/")
shogun_build_dir = os.path.abspath(GENERATED_DIR + config_subdir + GENERATED_SHOGUN_SUBDIR + GENERATED_BUILD_SUBDIR).replace("\\","/")
shogun_install_dir = os.path.abspath(GENERATED_DIR + config_subdir + GENERATED_SHOGUN_SUBDIR + GENERATED_INSTALL_SUBDIR).replace("\\","/")
build_dir = os.path.abspath(BUILD_DIR + config_subdir).replace("\\","/")

# OpenCV installation subdir containing third party static libs
open_cv_install_static_lib_subdir = ""
static_lib_extension = ""
if generator == Generator.MSVC2015:
	static_lib_prefix = ""
	open_cv_install_static_lib_subdir = "/x64/vc14/staticlib"
	if config == Configuration.Debug: # debug
		static_lib_extension = "d.lib"
	else: # release
		static_lib_extension = ".lib"
elif generator == Generator.MSVC2017:
	static_lib_prefix = ""
	open_cv_install_static_lib_subdir = "/x64/vc15/staticlib"
	if config == Configuration.Debug: # debug
		static_lib_extension = "d.lib"
	else: # release
		static_lib_extension = ".lib"
elif generator == Generator.MSVC2019:
	static_lib_prefix = ""
	open_cv_install_static_lib_subdir = "/staticlib"
	if config == Configuration.Debug: # debug
		static_lib_extension = "d.lib"
	else: # release
		static_lib_extension = ".lib"
else: # Linux / make
	static_lib_prefix = "lib"
	open_cv_install_static_lib_subdir = "/lib/opencv4/3rdparty"
	static_lib_extension = ".a"

# ########################
# ######## OpenCV ########
# ########################

print()
print("#######################################")
print("### Generating and compiling OpenCV ###")
print("#######################################")
print()

# Generate OpenCV project
os.chdir(open_cv_build_dir) # change into build directory of OpenCV
cmake_cmd = [
	cmake_exe, # cmake
	"-G", Generator.to_string(generator), # compiler
	"-Wno-dev", # supress CMake developer warnings
	"-D", "CMAKE_INSTALL_PREFIX=" + open_cv_install_dir, # set installation directory

	# Non-free (e.g., SIFT)
	"-D", "OPENCV_EXTRA_MODULES_PATH=../../../../third_party/opencv_contrib/modules",
	"-D", "OPENCV_ENABLE_NONFREE=ON",
	
	# ### OpenCV build setup ###
	
	# General TODO: incomplete. Probably more is built than necessary
	"-D", "BUILD_DOCS=OFF", # no documentation
	"-D", "BUILD_EXAMPLES=OFF", # no examples
	"-D", "BUILD_TESTS=OFF", # no tests
	"-D", "BUILD_PERF_TESTS=OFF",
	"-D", "BUILD_PACKAGE=OFF",
	"-D", "BUILD_SHARED_LIBS=OFF", # build static libs
	"-D", "BUILD_WITH_STATIC_CRT=OFF", # only for MSVC
	"-D", "BUILD_USE_SYMLINKS=OFF",
	"-D", "BUILD_WITH_DEBUG_INFO=OFF",
	"-D", "BUILD_WITH_DYNAMIC_IPP=OFF",
	"-D", "ENABLE_CXX11=ON",
	"-D", "ENABLE_PRECOMPILED_HEADERS=ON",
	"-D", "ENABLE_PYLINT=OFF",
	"-D", "ENABLE_SOLUTION_FOLDERS=OFF",
	"-D", "INSTALL_CREATE_DISTRIB=OFF",
	"-D", "INSTALL_C_EXAMPLES=OFF",
	"-D", "INSTALL_PYTHON_EXAMPLES=OFF",
	"-D", "INSTALL_TESTS=OFF",
	
	# Support
	"-D", "WITH_CUDA=OFF",
	"-D", "WITH_CUFFT=OFF",
	"-D", "WITH_CUBLAS=OFF",
	"-D", "WITH_GPHOTO2=OFF",
	"-D", "WITH_DIRECTX=OFF",
	"-D", "WITH_DSHOW=ON",
	"-D", "WITH_EIGEN=ON", # conversion functionality between OpenCV and EIGEN
	"-D", "WITH_FFMPEG=OFF", # using WebmM with VPX, only
	"-D", "WITH_GSTREAMER=OFF",
	"-D", "WITH_JPEG=ON",
	"-D", "WITH_JASPER=OFF",
	"-D", "WITH_MATLAB=OFF",
	"-D", "WITH_LAPACK=OFF",
	"-D", "WITH_OPENCL=OFF",
	"-D", "WITH_OPENCLAMDBLAS=OFF",
	"-D", "WITH_OPENCLAMDFFT=OFF",
	"-D", "WITH_OPENCL_SVM=OFF",
	"-D", "WITH_OPENEXR=OFF",
	"-D", "WITH_OPENGL=OFF",
	"-D", "WITH_OPENMP=OFF",
	"-D", "WITH_OPENNI=OFF",
	"-D", "WITH_OPENNI2=OFF",
	"-D", "WITH_OPENVX=OFF",
	"-D", "WITH_PNG=ON",
	"-D", "WITH_PROTOBUF=OFF",
	"-D", "WITH_PVAPI=OFF",
	"-D", "WITH_QT=OFF",
	"-D", "WITH_TBB=OFF",
	"-D", "WITH_TIFF=ON",
	"-D", "WITH_VFW=OFF",
	"-D", "WITH_VTK=OFF",
	"-D", "WITH_WEBP=OFF",
	"-D", "WITH_WIN32UI=" + ("ON" if args.visualdebug else "OFF"),
	"-D", "WITH_GTK=" + ("ON" if args.visualdebug else "OFF"),
	"-D", "WITH_XIMEA=OFF",
	"-D", "mdi=OFF",
	"-D", "next=OFF",
	"-D", "old-jpeg=OFF",
	"-D", "opencv_dnn_PERF_CAFFE=OFF",
	"-D", "opencv_dnn_PERF_CLCAFFE=OFF",
	"-D", "packbits=OFF",
	"-D", "thunder=OFF",

	# Libraries
	"-D", "BUILD_JPEG=ON",
	"-D", "BUILD_OPENEXR=OFF",
	"-D", "BUILD_PNG=ON",
	"-D", "BUILD_PROTOBUF=OFF",
	"-D", "BUILD_TBB=OFF",
	"-D", "BUILD_TIFF=ON",
	"-D", "BUILD_WEBP=OFF",
	"-D", "BUILD_ZLIB=ON",

	# Other
	"-D", "BUILD_CUDA_STUBS=OFF", # no CUDA stubs
	# "-D", "EIGEN_INCLUDE_PATH=" + "", # not required to set for EIGEN and OpenCV compatibility....

	# Modules
	"-D", "BUILD_opencv_apps=OFF",
	"-D", "BUILD_opencv_calib3d=ON",
	"-D", "BUILD_opencv_core=ON",
	"-D", "BUILD_opencv_dnn=OFF",
	"-D", "BUILD_opencv_features2d=ON",
	"-D", "BUILD_opencv_flann=ON", # required to build features2dx from non-free modules
	"-D", "BUILD_opencv_highgui=ON", # includes media io
	"-D", "BUILD_opencv_imgcodecs=ON",
	"-D", "BUILD_opencv_imgproc=ON",
	"-D", "BUILD_opencv_ml=ON",
	"-D", "BUILD_opencv_objdetect=OFF",
	"-D", "BUILD_opencv_photo=ON",
	"-D", "BUILD_opencv_shape=OFF",
	"-D", "BUILD_opencv_stitching=OFF",
	"-D", "BUILD_opencv_superres=OFF",
	"-D", "BUILD_opencv_ts=OFF", # test module
	"-D", "BUILD_opencv_video=ON",
	"-D", "BUILD_opencv_videoio=ON",
	"-D", "BUILD_opencv_videostab=OFF",
	"-D", "BUILD_opencv_world=OFF",

	# Non-free modules
	"-D", "BUILD_opencv_xfeatures2d=ON", # includes SIFT
	"-D", "BUILD_opencv_ximgproc=OFF",
	"-D", "BUILD_opencv_xobjdetect=OFF",
	"-D", "BUILD_opencv_xphoto=OFF",

	# Bindings
	"-D", "BUILD_JAVA=OFF", # no java support
	"-D", "BUILD_opencv_java_bindings_generator=OFF",
	"-D", "BUILD_opencv_python_bindings_generator=OFF",
	"-D", "BUILD_opencv_python3=OFF",
	"-D", "BUILD_opencv_js=OFF",
	"-D", "BUILD_opencv_js=OFF",

	# ##########################

	"../../../../third_party/opencv"] # source code directory
retCode = subprocess.check_call(cmake_cmd, stderr=subprocess.STDOUT, shell=False)

# Build and install OpenCV project
cmake_cmd = [
	cmake_exe, # cmake
	"--build", ".",
	"--config", Configuration.to_string(config),
	"--target", "install"]
retCode = subprocess.check_call(cmake_cmd, stderr=subprocess.STDOUT, shell=False)

# ###########################
# ######## Leptonica ########
# ###########################
# Required by Tesseract

print()
print("##########################################")
print("### Generating and compiling Leptonica ###")
print("##########################################")
print()

# Generate Leptonica project
os.chdir(leptonica_build_dir) # change into build directory of Leptonica
open_cv_third_party_src_dir = os.path.abspath("../../../../third_party/opencv/3rdparty").replace("\\","/")
open_cv_third_party_build_dir = os.path.abspath("../../../../_generated" + config_subdir + "/opencv/build/3rdparty").replace("\\","/")
cmake_cmd = [
	cmake_exe, # cmake
	"-G", Generator.to_string(generator), # compiler
	"-Wno-dev", # supress CMake developer warnings
	"-D", "CMAKE_INSTALL_PREFIX=" + leptonica_install_dir, # set installation directory

	# ### Forcing Leptonica to use local libraries from OpenCV ###
	"-D", "CMAKE_DISABLE_FIND_PACKAGE_PkgConfig=ON", # no nead for pkg config
	"-D", "CMAKE_DISABLE_FIND_PACKAGE_GIF=ON", # no need for GIF support
	"-D", "ZLIB_INCLUDE_DIR=" + open_cv_third_party_src_dir + "/zlib",
	"-D", "ZLIB_LIBRARY=" + open_cv_install_dir + open_cv_install_static_lib_subdir + "/" + static_lib_prefix + "zlib" + static_lib_extension,
	"-D", "ZLIB_FOUND=ON",
	"-D", "JPEG_INCLUDE_DIR=" + open_cv_third_party_src_dir + "/libjpeg",
	"-D", "JPEG_LIBRARY=" + open_cv_install_dir + open_cv_install_static_lib_subdir  + "/" + static_lib_prefix + "libjpeg-turbo" + static_lib_extension,
	"-D", "JPEG_FOUND=ON",
	"-D", "TIFF_INCLUDE_DIR=" + open_cv_third_party_src_dir + "/libtiff;" + open_cv_third_party_build_dir + "/libtiff", # requires some config file from build folder
	"-D", "TIFF_LIBRARY=" + open_cv_install_dir + open_cv_install_static_lib_subdir + "/" + static_lib_prefix + "libtiff" + static_lib_extension,
	"-D", "TIFF_FOUND=ON",
	"-D", "PNG_INCLUDE_DIRS=" + open_cv_third_party_src_dir + "/libpng",
	"-D", "PNG_PNG_INCLUDE_DIR=" + open_cv_third_party_src_dir + "/libpng", # according to some error message, this include must be set, too
	"-D", "PNG_LIBRARY=" + open_cv_install_dir + open_cv_install_static_lib_subdir + "/" + static_lib_prefix + "libpng" + static_lib_extension,
	"-D", "PNG_FOUND=ON",

	# ### Leptonica build setup ###
	"-D", "BUILD_PROG=OFF", # no utilities
	# #############################

	"../../../../third_party/leptonica"] # source code directory
retCode = subprocess.check_call(cmake_cmd, stderr=subprocess.STDOUT, shell=False)

# Build and install Leptonica project
cmake_cmd = [
	cmake_exe, # cmake
	"--build", ".",
	"--config", Configuration.to_string(config),
	"--target", "install"]
retCode = subprocess.check_call(cmake_cmd, stderr=subprocess.STDOUT, shell=False)

# ###########################
# ######## Tesseract ########
# ###########################

print()
print("##########################################")
print("### Generating and compiling Tesseract ###")
print("##########################################")
print()

# Generate Tesseract project
os.chdir(tesseract_build_dir) # change into build directory of Tesseract
cmake_cmd = [
	cmake_exe, # cmake
	"-G", Generator.to_string(generator), # compiler
	"-Wno-dev", # supress CMake developer warnings
	"-D", "CMAKE_INSTALL_PREFIX=" + tesseract_install_dir, # set installation directory
	
	# ### Tesseract build setup ###
	"-D", "Leptonica_DIR=" + leptonica_install_dir + "/cmake", # path to leptonica installation dir
	"-D", "BUILD_TESTS=OFF", # no tests
	"-D", "BUILD_TRAINING_TOOLS=OFF", # no training utilities
	# #############################

	"../../../../third_party/tesseract"] # source code directory
retCode = subprocess.check_call(cmake_cmd, stderr=subprocess.STDOUT, shell=False)

# Build and install Tesseract project
cmake_cmd = [
	cmake_exe, # cmake
	"--build", ".",
	"--config", Configuration.to_string(config),
	"--target", "install"]
retCode = subprocess.check_call(cmake_cmd, stderr=subprocess.STDOUT, shell=False)

# ########################
# ######## Shogun ########
# ########################

print()
print("#######################################")
print("### Generating and compiling Shogun ###")
print("#######################################")
print()

# Generate Shogun project
os.chdir(shogun_build_dir) # change into build directory of Shogun
cmake_cmd = [
	cmake_exe, # cmake
	"-G", Generator.to_string(generator), # compiler
	"-Wno-dev", # supress CMake developer warnings
	"-D", "CMAKE_INSTALL_PREFIX=" + shogun_install_dir, # set installation directory

	# ### Shogun build setup ###
	"-D", "LIBSHOGUN=ON",
	"-D", "LIBSHOGUN_BUILD_STATIC=ON",
	"-D", "BUILD_BENCHMARKS=OFF", # no benchmarks
	"-D", "BUILD_DASHBOARD_REPORTS=OFF", # no dashboard reports
	"-D", "BUILD_EXAMPLES=OFF", # no examples
	"-D", "BUILD_META_EXAMPLES=OFF", # no meta examples
	"-D", "DISABLE_UNIT_TESTS=ON", # no unit tests
	"-D", "LICENSE_GPL_SHOGUN=OFF", # no GPL code
	"-D", "USE_SVMLIGHT=OFF", # must be disabled when no GPL code is allowed
	"-D", "INCREMENTAL_LINKING=OFF",
	"-D", "INTERFACE_CSHARP=OFF",
	"-D", "INTERFACE_JAVA=OFF",
	"-D", "INTERFACE_LUA=OFF",
	"-D", "INTERFACE_OCTAVE=OFF",
	"-D", "INTERFACE_PERL=OFF",
	"-D", "INTERFACE_PYTHON=OFF",
	"-D", "INTERFACE_R=OFF",
	"-D", "INTERFACE_RUBY=OFF",
	"-D", "INTERFACE_SCALA=OFF",
	"-D", "OpenCV=OFF",
	"-D", "ARPACK_LIBRARIES=''",
	"-D", "CPLEX_LIBRARY=''",
	"-D", "CTAGS_EXECUTABLE=''",
	"-D", "EIGEN_INCLUDE_DIRS=''", # maybe use EIGEN from ext folder of framework?
	"-D", "GDB_ROOT_DIR=''",
	"-D", "GLPK_LIBRARY=''",
	"-D", "GLPK_ROOT_DIR=''",
	"-D", "JSON_LIBRAY=''",
	"-D", "MOSEK_DIR=''"
	"-D", "MKL_RT=''",
	"-D", "MKL_INCLUDE_DIR=''",
	"-D", "OPENCL_INCLUDE_DIR=''",
	"-D", "OPENCL_LIBRARY=''",
	"-D", "PANDOC_EXECUTABLE=''",
	"-D", "Protobuf_SRC_ROOT_FOLDER=''",
	"-D", "SNAPPY_LIBRARIES=''",
	"-D", "SNAPPY_INCLUDE_DIR=''",
	"-D", "SPHINX_EXECUTABLE=''",
	"-D", "TFLogger_DIR=''",
	"-D", "VIENNA_PATH_WIN32=''",
	"-D", "VIENNACL_WITH_OPENCL=OFF",
	"-D", "TRACE_MEMORY_ALLOCS=OFF",
	"-D", "TRAVIS_DISABLE_LIBSHOGUN_TESTS=OFF",
	"-D", "TRAVIS_DISABLE_UNIT_TESTS=OFF",
	"-D", "TRAVIS_DISABLE_META_CPP=OFF",
	"-D", "REDUCE_SWIG_DEBUG=OFF",
	"-D", "DISABLE_META_INTEGRATION_TESTS=ON",
	"-D", "DISABLE_SSE=OFF",
	"-D", "BUNDLE_JSON=OFF",
	"-D", "BUNDLE_NLOPT=OFF",
	"-D", "ENABLE_ARPACK=OFF",
	"-D", "ENABLE_ARPREC=OFF",
	"-D", "ENABLE_BZIP2=OFF",
	"-D", "ENABLE_CCACHE=ON",
	"-D", "ENABLE_COLPACK=OFF",
	"-D", "ENABLE_COVERAGE=OFF",
	"-D", "ENABLE_CPLEX=OFF",
	"-D", "ENABLE_CURL=OFF",
	"-D", "ENABLE_EIGEN_LAPACK=ON",
	"-D", "ENABLE_GLPK=OFF",
	"-D", "ENABLE_JSON=OFF",
	"-D", "ENABLE_LIBLZMA=OFF",
	"-D", "ENABLE_LIBXML2=OFF",
	"-D", "ENABLE_LPSOLVE=ON",
	"-D", "ENABLE_LTO=OFF",
	"-D", "ENABLE_LZO=OFF",
	"-D", "ENABLE_MOSEK=OFF",
	"-D", "ENABLE_NLOPT=OFF",
	"-D", "ENABLE_PROTOBUF=OFF",
	"-D", "ENABLE_PYTHON_DEBUG=OFF",
	"-D", "ENABLE_SNAPPY=OFF",
	"-D", "ENABLE_TESTING=OFF",
	"-D", "ENABLE_VIENNACL=OFF",
	"-D", "ENABLE_ZLIB=OFF",
	"-D", "HAVE_ARPACK=OFF",
	"-D", "HAVE_ARPREC=OFF",
	"-D", "HAVE_COLPACK=OFF",
	"-D", "HAVE_CURL=OFF",
	"-D", "HAVE_JSON=OFF",
	"-D", "HAVE_NLOPT=OFF",
	"-D", "HAVE_PROTOBUF=OFF",
	"-D", "HAVE_VIENNACL=OFF",
	"-D", "HAVE_XML=OFF",
	"-D", "HAVE_MKL=OFF",
	"-D", "USE_BIGSTATES=ON",
	"-D", "USE_BZIP2=OFF",
	"-D", "USE_CPLEX=OFF",
	"-D", "USE_GLPK=OFF",
	"-D", "USE_GZIP=OFF",
	"-D", "USE_HMMCACHE=ON",
	"-D", "USE_HMMDEBUG=OFF",
	"-D", "USE_HMMPARALLEL=OFF",
	"-D", "USE_LOGCACHE=OFF",
	"-D", "USE_LOGSUMARRAY=OFF",
	"-D", "USE_LPSOLVE=ON",
	"-D", "USE_LZMA=OFF",
	"-D", "USE_LZO=OFF",
	"-D", "USE_MOSEK=OFF",
	"-D", "USE_PATHDEBUG=OFF",
	"-D", "USE_SHORTREAL_KERNELCACHE=ON",
	"-D", "USE_SNAPPY=OFF",
	"-D", "USE_SWIG_DIRECTORS=OFF",
	# ##########################
	"../../../../third_party/shogun"] # source code directory
retCode = subprocess.check_call(cmake_cmd, stderr=subprocess.STDOUT, shell=False)

# Build and install Shogun project
cmake_cmd = [
	cmake_exe, # cmake
	"--build", ".",
	"--config", Configuration.to_string(config),
	"--target", "install"]
retCode = subprocess.check_call(cmake_cmd, stderr=subprocess.STDOUT, shell=False)

# ###########################
# ######## Framework ########
# ###########################

print()
print("############################")
print("### Generating Framework ###")
print("############################")
print()

# Generate framework project
os.chdir(build_dir) # change into build directory of project
cmake_cmd = [
	cmake_exe, # cmake
	"-G", Generator.to_string(generator), # compiler
	"-D", "CONFIG=" + Configuration.to_string(config),
	"-D", "VISUAL_DEBUG=" + ("ON" if args.visualdebug else "OFF"),
	"-D", "DEPLOY=" + ("ON" if args.deploy else "OFF"),
	"-D", "MT_TASK=" + ("OFF" if args.singlethreaded else "ON"),
	"../../code"] # source code directory
retCode = subprocess.check_call(cmake_cmd, stderr=subprocess.STDOUT, shell=False)

# Compile framework project (installation is implicitly performed if "deploy"-mode has been chosen)
if args.build:
	cmake_cmd = [
		cmake_exe, # cmake
		"--build", ".",
		"--config", Configuration.to_string(config)]
	retCode = subprocess.check_call(cmake_cmd, stderr=subprocess.STDOUT, shell=False)