cmake_minimum_required (VERSION 2.6)
project (imagineKatana)

# TODO: all these different projects (KatanaAPIs, ImagineCore) should all be in different CMakeLists.txt files,
#       as currently all the include dirs are getting added together creating a mess...

include(ExternalProject)

set(MODERN_GCC 1)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(MODERN_GCC 0)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING
	  "Choose the type of build, options are: None Debug Release"
	  FORCE)
endif(NOT CMAKE_BUILD_TYPE)
if(CMAKE_FIRST_RUN)
	message(STATUS "Setting Build Type to: ${CMAKE_BUILD_TYPE}")
endif()

IF(${MODERN_GCC} MATCHES "1")
	SET(ARCH "corei7")
	SET(SSE_FLAGS "-msse -msse2 -msse3 -mssse3 -msse4")
ELSE()
	SET(ARCH "core2")
	SET(SSE_FLAGS "-msse -msse2 -msse3 -mssse3 -msse4")
ENDIF(${MODERN_GCC} MATCHES "1")

# -fvisibility-inlines-hidden -fvisibility=hidden

# CFLAGS = -ffunction-sections
# LDFLAGS = -Wl,-gc-sections

SET(CMAKE_CXX_FLAGS_DEBUG "-g -mfpmath=sse -fPIC -ffast-math -msse -msse2 -msse3 -mssse3 -msse4")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -mfpmath=sse -fPIC -ffast-math -msse -msse2 -msse3 -mssse3 -msse4")

SET(KATANA_VERSION 2)
IF(${KATANA_VERSION} MATCHES 1)
# Katana APIs
	SET(KATANA_PLUGIN_API_PATH "${CMAKE_SOURCE_DIR}/katana_api")
ELSE()
	SET(KATANA_PLUGIN_API_PATH "${CMAKE_SOURCE_DIR}/katana_api_2")
ENDIF(${KATANA_VERSION} MATCHES 1)

SET(DISPLAY_DRIVER 1) # set this to 1 to build with zeromq support (and preview and liverender support) - this
#					#   includes zeromq support (which is embedded as a separate project which gets built by this CMakeLists.txt),
#					#   but also requires boost 1.42/1.43 support for threads and locks...

# boost stuff (needed for display driver)
SET(BOOST_CUST_PATH "/opt/boost_144/include")
SET(BOOST_EXT_LIBS "")
SET(BOOST_MANUAL 0)

IF(${BOOST_MANUAL} MATCHES 1)
	SET(BOOST_INCLUDE_DIR "/opt/boost_144/include")
	SET(BOOST_LIB_DIR "/opt/boost_144/lib")
	SET(BOOST_THREAD_LIB "libboost_thread.dylib")

	ADD_LIBRARY(${BOOST_THREAD_LIB} SHARED IMPORTED)
	SET_TARGET_PROPERTIES(${BOOST_THREAD_LIB} PROPERTIES IMPORTED_LOCATION "${BOOST_LIB_DIR}/${BOOST_THREAD_LIB}")
ENDIF(${BOOST_MANUAL} MATCHES 1)

FILE(GLOB_RECURSE katanaAPI_SOURCES_ORIGINAL "${KATANA_PLUGIN_API_PATH}/src/*.cpp")
SET(katanaAPI_SOURCES "")
IF(${DISPLAY_DRIVER} MATCHES 1)
	set(ZEROMQ_INCLUDE_DIRS "${PROJECT_BINARY_DIR}/external/dist/zeromq/include")
	set(ZEROMQ_LIBRARY ${PROJECT_BINARY_DIR}/external/dist/zeromq/lib/libzeromq.a)

	IF(${BOOST_MANUAL} MATCHES 1)
		SET(BOOST_EXT_LIBS ${BOOST_THREAD_LIB})

		INCLUDE_DIRECTORIES("${KATANA_PLUGIN_API_PATH}/include/" "${ZEROMQ_INCLUDE_DIRS}" "${BOOST_INCLUDE_DIR}")
	ELSE()
		SET(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} "${BOOST_CUST_PATH}")
		SET(Boost_USE_STATIC_LIBS ON)
		SET(Boost_USE_MULTITHREADED ON)
		FIND_PACKAGE(Boost 1.44 COMPONENTS thread)

		SET(BOOST_EXT_LIBS "${Boost_LIBRARIES}")

		INCLUDE_DIRECTORIES("${KATANA_PLUGIN_API_PATH}/include/" "${ZEROMQ_INCLUDE_DIRS}" "${Boost_INCLUDE_DIRS}")
	ENDIF(${BOOST_MANUAL} MATCHES 1)

	SET(katanaAPI_SOURCES ${katanaAPI_SOURCES_ORIGINAL})
ELSE()
	INCLUDE_DIRECTORIES("${KATANA_PLUGIN_API_PATH}/include/")

	# strip out display driver stuff from katanaAPI_SOURCES
	FOREACH(file ${katanaAPI_SOURCES_ORIGINAL})
		STRING(REGEX MATCH "([/]FnDisplayDriver[/])" test1 ${file})
		IF(test1)
		ELSE(test1)
			LIST(APPEND katanaAPI_SOURCES ${file})
		ENDIF(test1)
	ENDFOREACH(file ${katanaAPI_SOURCES_ORIGINAL})
ENDIF(${DISPLAY_DRIVER} MATCHES 1)

ADD_LIBRARY(katanaAPI ${katanaAPI_SOURCES})

SET(EXTERNAL_DEPENDS "OPENEXR_LOCAL")

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	SET(CMAKE_SHARED_LINKER_FLAGS_RELEASE "-install_name @executable_path")
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

SET(HAVE_LIBTIFF 0)
SET(FIND_LIBTIFF 0) ## change this one to try to find libtiff

FIND_PACKAGE(ZLIB REQUIRED) # for OpenEXR and compression within Imagine

set(ILMBASE_DIST ${PROJECT_BINARY_DIR}/external/dist/ilmbase)
set(OPENEXR_DIST ${PROJECT_BINARY_DIR}/external/dist/openexr)

SET(OPENEXR_SHARED_LIBS_FLAGS "")
IF (APPLE)
	# if debug, use shared OpenEXR libs, otherwise static for release...
	# This means the debug environment will need DYLD_LIBRARY_PATH set to the external/dist paths
	# for IlmBase and OpenEXR in order to run in debugger...
	IF (CMAKE_BUILD_TYPE MATCHES "Release")
		SET(OPENEXR_SHARED_LIBS_FLAGS "-DBUILD_SHARED_LIBS=OFF")
	ENDIF (CMAKE_BUILD_TYPE MATCHES "Release")
ENDIF(APPLE)

set(ILMBASE_VERSION 2.0.1)
ExternalProject_Add(ILMBASE_LOCAL
	URL ${CMAKE_SOURCE_DIR}/external/ilmbase-${ILMBASE_VERSION}.tar.bz2
	BINARY_DIR external/build/ilmbase
	INSTALL_DIR external/dist/ilmbase
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}/external/dist/ilmbase ${OPENEXR_SHARED_LIBS_FLAGS}
)

#

set(OPENEXR_VERSION 2.0.1)
ExternalProject_Add(OPENEXR_LOCAL
	URL ${CMAKE_SOURCE_DIR}/external/openexr-${OPENEXR_VERSION}.tar.bz2
	BINARY_DIR external/build/openexr
	INSTALL_DIR ${OPENEXR_DIST}
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${OPENEXR_DIST}
	CMAKE_ARGS -DILMBASE_PACKAGE_PREFIX=${ILMBASE_DIST}
	CMAKE_ARGS  ${OPENEXR_SHARED_LIBS_FLAGS}
)

ADD_DEPENDENCIES(OPENEXR_LOCAL ILMBASE_LOCAL)

#

set(ILMBASE_LIB ${ILMBASE_DIST}/lib)
set(OPENEXR_LIB ${OPENEXR_DIST}/lib)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	# As above for OpenEXR, debug on OS X should use shared, but release static libs
	IF (CMAKE_BUILD_TYPE MATCHES "Release")
		set(EXTERNAL_OPENEXR_LIBRARIES ${ILMBASE_LIB}/libHalf.a ${ILMBASE_LIB}/libIex-2_0.a ${ILMBASE_LIB}/libIlmThread-2_0.a ${ILMBASE_LIB}/libImath-2_0.a ${OPENEXR_LIB}/libImf-2_0.a)
	ELSE()
		set(EXTERNAL_OPENEXR_LIBRARIES ${ILMBASE_LIB}/libHalf.dylib ${ILMBASE_LIB}/libIex-2_0.dylib ${ILMBASE_LIB}/libIlmThread-2_0.dylib ${ILMBASE_LIB}/libImath-2_0.dylib ${OPENEXR_LIB}/libImf-2_0.dylib)
	ENDIF(CMAKE_BUILD_TYPE MATCHES "Release")
ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(EXTERNAL_OPENEXR_LIBRARIES ${ILMBASE_LIB}/libHalf.so ${ILMBASE_LIB}/libIex-2_0.so ${ILMBASE_LIB}/libIlmThread-2_0.so ${ILMBASE_LIB}/libImath-2_0.so ${OPENEXR_LIB}/libImf-2_0.so)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

#MESSAGE(${EXTERNAL_OPENEXR_LIBRARIES})


IF(${DISPLAY_DRIVER} MATCHES 1)
	set(ZEROMQ_VERSION 3.2.4)
	ExternalProject_Add(ZMQ_LOCAL
		URL ${CMAKE_SOURCE_DIR}/external/zmq-${ZEROMQ_VERSION}.tar.bz2
		BINARY_DIR external/build/zeromq/
		INSTALL_DIR external/dist
		CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}/external/dist
	)

	# This is a bit messy, as some stuff is done here, other stuff above for the KatanaAPI part...

	ADD_DEPENDENCIES(katanaAPI ZMQ_LOCAL)
	TARGET_LINK_LIBRARIES(katanaAPI ${ZEROMQ_LIBRARY})
ELSE()
	set(ZEROMQ_INCLUDE_DIRS "")
	set(ZEROMQ_LIBRARY "")
ENDIF(${DISPLAY_DRIVER} MATCHES 1)

IF (${FIND_LIBTIFF} MATCHES 1)
	FIND_PACKAGE(TIFF)

	IF (NOT ${TIFF_INCLUDE_DIR} MATCHES FALSE)
		set(LIBTIFF_INCLUDE_DIRS ${TIFF_INCLUDE_DIR})
		set(LIBTIFF_LIBRARY ${TIFF_LIBRARY})
		set(HAVE_LIBTIFF 1)
	ENDIF(NOT ${TIFF_INCLUDE_DIR} MATCHES FALSE)
ENDIF(${FIND_LIBTIFF} MATCHES 1)

IF(${HAVE_LIBTIFF} MATCHES 1)
	set(EXTERNAL_INCLUDE_DIRS ${EXTERNAL_INCLUDE_DIRS} ${LIBTIFF_INCLUDE_DIRS})
	set(EXTERNAL_LIBRARIES ${EXTERNAL_LIBRARIES} ${LIBTIFF_LIBRARY})
ENDIF(${HAVE_LIBTIFF} MATCHES 1)

set(EXTERNAL_INCLUDE_DIRS ${ILMBASE_DIST}/include/OpenEXR ${OPENEXR_DIST}/include/OpenEXR ${ZEROMQ_INCLUDE_DIRS})
set(EXTERNAL_COMPILE_FLAGS "")
set(EXTERNAL_LINK_FLAGS "")
set(EXTERNAL_LIBRARY_DIRS ${ILMBASE_DIST}/lib ${OPENEXR_DIST}/lib)
set(EXTERNAL_LIBRARIES ${EXTERNAL_OPENEXR_LIBRARIES} ${ZEROMQ_LIBRARY})

#	IF(${BOOST_MANUAL} MATCHES 1)
#		SET(EXTERNAL_LIBRARIES ${EXTERNAL_LIBRARIES} "${BOOST_LIB_DIR}/${BOOST_THREAD_LIB}")
#	ENDIF(${BOOST_MANUAL} MATCHES 1)

# add libtiff if it was available
IF(${HAVE_LIBTIFF} MATCHES 1)
	set(EXTERNAL_INCLUDE_DIRS ${EXTERNAL_INCLUDE_DIRS} ${LIBTIFF_INCLUDE_DIRS})
	set(EXTERNAL_LIBRARIES ${EXTERNAL_LIBRARIES} ${LIBTIFF_LIBRARY})
ENDIF(${HAVE_LIBTIFF} MATCHES 1)


# Imagine LIB

ADD_DEFINITIONS(-DIMAGINE_NO_GUI -DIMAGINE_EMBEDDED_MODE)

SET(IMAGINE_SRC_PATH "${CMAKE_SOURCE_DIR}/../imagine/src")
#SET(IMAGINE_SRC_PATH "/home/peter/Coding/Imagine/src")
#SET(IMAGINE_SRC_PATH "/Users/peter/Documents/Coding/imagine/src")

FILE(GLOB_RECURSE imagine_SOURCES_ORIGINAL "${IMAGINE_SRC_PATH}/*.cpp")

#MESSAGE( "${imagine_SOURCES_ORIGINAL}" )

# Remove UI items and other stuff so that we can use Imagine's src directory directly without having
# a modified copy
SET(imagine_SOURCES "")
FOREACH(file ${imagine_SOURCES_ORIGINAL})
	STRING(REGEX MATCH "([/]ui[/]|[/]app[/]|[/]io[/]geometry[/]|[/]io[/]scene[/]|[/]scene_builders[/])" test1 ${file})
	IF(test1)

	ELSE(test1)
		LIST(APPEND imagine_SOURCES ${file})
	ENDIF(test1)
ENDFOREACH(file ${imagine_SOURCES_ORIGINAL})

# remove some other stuff
LIST(REMOVE_ITEM imagine_SOURCES ${IMAGINE_SRC_PATH}/io/geo_reader.cpp ${IMAGINE_SRC_PATH}/io/geometry/geo_reader_abc.cpp
		 ${IMAGINE_SRC_PATH}/io/image/image_reader_jpeg.cpp ${IMAGINE_SRC_PATH}/io/image/image_reader_png.cpp
		${IMAGINE_SRC_PATH}/io/image/image_writer_png.cpp ${IMAGINE_SRC_PATH}/objects/primitives/ocean.cpp
		${IMAGINE_SRC_PATH}/io/geo_helper_obj.cpp)

IF(${HAVE_LIBTIFF} MATCHES 0)
	LIST(REMOVE_ITEM imagine_SOURCES ${IMAGINE_SRC_PATH}/io/image/image_reader_tiff.cpp)
ENDIF(${HAVE_LIBTIFF} MATCHES 0)

#MESSAGE( "${imagine_SOURCES}" )

INCLUDE_DIRECTORIES("${IMAGINE_SRC_PATH}" ${EXTERNAL_INCLUDE_DIRS})
ADD_LIBRARY(imagineCore ${imagine_SOURCES})
ADD_DEPENDENCIES(imagineCore OPENEXR_LOCAL)

####

## katanaImagineRendererInfo
FILE(GLOB_RECURSE katanaImagineRendererInfo_SOURCES "${CMAKE_SOURCE_DIR}/src/imagineRendererInfo/*.cpp")

ADD_LIBRARY(katanaImagineRendererInfo SHARED ${katanaImagineRendererInfo_SOURCES})

ADD_DEPENDENCIES(katanaImagineRendererInfo katanaAPI imagineCore)
SET_TARGET_PROPERTIES(katanaImagineRendererInfo PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(katanaImagineRendererInfo katanaAPI imagineCore)

## katanaImagineRenderer

FILE(GLOB_RECURSE katanaImagineRender_SOURCES "${CMAKE_SOURCE_DIR}/src/imagineRender/*.cpp")

ADD_LIBRARY(katanaImagineRender SHARED ${katanaImagineRender_SOURCES})

IF(${HAVE_LIBTIFF} MATCHES 1)
	ADD_DEFINITIONS(-DHAVE_TIFF_SUPPORT)
ENDIF(${HAVE_LIBTIFF} MATCHES 1)

IF(${KATANA_VERSION} MATCHES 2)
	ADD_DEFINITIONS(-DKAT_V_2)
ENDIF(${KATANA_VERSION} MATCHES 2)

ADD_DEPENDENCIES(katanaImagineRender katanaAPI imagineCore)
SET_TARGET_PROPERTIES(katanaImagineRender PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(katanaImagineRender katanaAPI imagineCore ${EXTERNAL_LIBRARIES} ${ZLIB_LIBRARY} ${BOOST_EXT_LIBS})

# imagineViewerModifier

FIND_PACKAGE(OpenGL REQUIRED)

FILE(GLOB_RECURSE imagineViewerModifier_SOURCES "${CMAKE_SOURCE_DIR}/src/imagineViewerModifier/*.cpp")

ADD_LIBRARY(imagineViewerModifier SHARED ${imagineViewerModifier_SOURCES})

IF(${KATANA_VERSION} MATCHES 2)
	ADD_DEFINITIONS(-DKAT_V_2)
ENDIF(${KATANA_VERSION} MATCHES 2)

ADD_DEPENDENCIES(imagineViewerModifier katanaAPI)
SET_TARGET_PROPERTIES(imagineViewerModifier PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(imagineViewerModifier katanaAPI ${OPENGL_LIBRARY})
