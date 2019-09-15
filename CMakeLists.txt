cmake_minimum_required (VERSION 2.6)
project (imagineKatana)

# TODO: all these different projects (KatanaAPIs, ImagineCore) should all be in different CMakeLists.txt files,
#       as currently all the include dirs are getting added together creating a mess...

include(ExternalProject)

SET(KATANA_V3 1)
IF (${KATANA_V3} MATCHES "1")
    SET(KATANA_PLUGIN_API_PATH "/opt/Katana3.0v6/plugin_apis")
    ADD_DEFINITIONS(-DKATANA_V3)
ELSE()
    SET(KATANA_PLUGIN_API_PATH "/opt/katana_2.5.3/plugin_apis")
ENDIF(${KATANA_V3} MATCHES "1")

#SET(IMAGINE_SRC_PATH "${CMAKE_SOURCE_DIR}/../imagine/src")
SET(IMAGINE_SRC_PATH "/home/peter/Coding/Imagine/src")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING
	  "Choose the type of build, options are: None Debug Release"
	  FORCE)
endif(NOT CMAKE_BUILD_TYPE)
if(CMAKE_FIRST_RUN)
	message(STATUS "Setting Build Type to: ${CMAKE_BUILD_TYPE}")
endif()

SET(CXX11 1)
IF(${CXX11} MATCHES 1)
	SET(CMAKE_CXX_STANDARD 11)
	SET(CMAKE_CXX_STANDARD_REQUIRED on)
	SET(QMAKE_CXXFLAGS "-std=c++11")
	SET(GENERAL_COMPILER_FLAGS "-std=c++11")
ENDIF(${CXX11} MATCHES 1)

#SET(ARCH "corei7")
SET(ARCH "native")
# -msse and -msse2 are enabled by default for 64-bit, but let's be explicit anyway...
# Note: with "-march=native" set, the below is somewhat pointless, as the correct things are generally
   #       set anyway...
SET(ARCH_FLAGS "-march=${ARCH} -mfpmath=sse -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2")

#SET(GENERAL_COMPILER_FLAGS " ${GENERAL_COMPILER_FLAGS} -fsanitize=address -mfpmath=sse -fPIC -ffast-math ${ARCH_FLAGS}")
SET(GENERAL_COMPILER_FLAGS "${GENERAL_COMPILER_FLAGS} -mfpmath=sse -fPIC -ffast-math ${ARCH_FLAGS}")

# -fvisibility-inlines-hidden -fvisibility=hidden

# CFLAGS = -ffunction-sections
# LDFLAGS = -Wl,-gc-sections

#SET(DEBUG_WARNINGS "-Wall -Wextra")
#SET(DEBUG_WARNINGS_SUPPRESS "-Wno-unused-function -Wno-unused-parameter -Wno-unused-variable")
#SET(CMAKE_CXX_FLAGS_DEBUG "-g -D_GLIBCXX_DEBUG ${DEBUG_WARNINGS} ${DEBUG_WARNINGS_SUPPRESS} ${GENERAL_COMPILER_FLAGS}")
SET(CMAKE_CXX_FLAGS_DEBUG "-g ${DEBUG_WARNINGS} ${DEBUG_WARNINGS_SUPPRESS} ${GENERAL_COMPILER_FLAGS}")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 ${GENERAL_COMPILER_FLAGS}")

FILE(GLOB_RECURSE katanaAPI_SOURCES_ORIGINAL "${KATANA_PLUGIN_API_PATH}/src/*.cpp")
SET(katanaAPI_SOURCES "")

set(ZEROMQ_INCLUDE_DIRS "${PROJECT_BINARY_DIR}/external/dist/zeromq/include")
set(ZEROMQ_LIBRARY ${PROJECT_BINARY_DIR}/external/dist/zeromq/lib/libzeromq.a)

INCLUDE_DIRECTORIES("${KATANA_PLUGIN_API_PATH}/include/" "${ZEROMQ_INCLUDE_DIRS}")

# strip out plugin dirs we don't want before adding them to katanaAPI_SOURCES
FOREACH(file ${katanaAPI_SOURCES_ORIGINAL})
	STRING(REGEX MATCH "([/]FnDefaultAttributeProducer[/]|[/]FnRenderOutputLocation[/]|[/]FnScenegraphGenerator[/]|FnViewer[/])" test1 ${file})
	IF(test1)
	ELSE(test1)
		LIST(APPEND katanaAPI_SOURCES ${file})
	ENDIF(test1)
ENDFOREACH(file ${katanaAPI_SOURCES_ORIGINAL})

#SET(katanaAPI_SOURCES ${katanaAPI_SOURCES_ORIGINAL})

ADD_LIBRARY(katanaAPI ${katanaAPI_SOURCES})


# Imagine Core stuff

SET(EXTERNAL_DEPENDS "OPENEXR_LOCAL")

SET(HAVE_LIBTIFF 0)
SET(FIND_LIBTIFF 0) ## change this one to try to find libtiff

FIND_PACKAGE(ZLIB REQUIRED) # for OpenEXR and compression within Imagine

set(ILMBASE_DIST ${PROJECT_BINARY_DIR}/external/dist/ilmbase)
set(OPENEXR_DIST ${PROJECT_BINARY_DIR}/external/dist/openexr)

SET(STATIC_OPENEXR 1)

SET(OPENEXR_LIBS_FLAGS1 "")
SET(OPENEXR_LIBS_FLAGS2 "")
IF (APPLE)
	# Because we can't get DYLD_LIBRARY_PATH via LLDB any more, we have to always link statically
	SET(STATIC_OPENEXR 1)
ENDIF(APPLE)
IF (${STATIC_OPENEXR} MATCHES 1)
    SET(OPENEXR_LIBS_FLAGS1 "-DBUILD_SHARED_LIBS=OFF")
    # Because we're building a shared lib that wants to link static libs, and OpenEXR doesn't add -fPIC, we need to do this
    SET(OPENEXR_LIBS_FLAGS2 "-DCMAKE_POSITION_INDEPENDENT_CODE=ON")
ENDIF(${STATIC_OPENEXR} MATCHES 1)

set(ILMBASE_VERSION 2.0.99)
ExternalProject_Add(ILMBASE_LOCAL
	URL ${CMAKE_SOURCE_DIR}/external/ilmbase-${ILMBASE_VERSION}.tar.bz2
	BINARY_DIR external/build/ilmbase
	INSTALL_DIR external/dist/ilmbase
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}/external/dist/ilmbase
    CMAKE_ARGS ${OPENEXR_LIBS_FLAGS1}
    CMAKE_ARGS ${OPENEXR_LIBS_FLAGS2}
    CMAKE_CACHE_ARGS "-DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}" "-DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
)


#

set(OPENEXR_VERSION 2.0.99)
ExternalProject_Add(OPENEXR_LOCAL
	URL ${CMAKE_SOURCE_DIR}/external/openexr-${OPENEXR_VERSION}.tar.bz2
	BINARY_DIR external/build/openexr
	INSTALL_DIR ${OPENEXR_DIST}
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${OPENEXR_DIST}
	CMAKE_ARGS -DILMBASE_PACKAGE_PREFIX=${ILMBASE_DIST}
	CMAKE_ARGS ${OPENEXR_LIBS_FLAGS1}
    CMAKE_ARGS ${OPENEXR_LIBS_FLAGS2}
    CMAKE_CACHE_ARGS "-DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}" "-DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
)

ADD_DEPENDENCIES(OPENEXR_LOCAL ILMBASE_LOCAL)

#

set(ILMBASE_LIB ${ILMBASE_DIST}/lib)
set(OPENEXR_LIB ${OPENEXR_DIST}/lib)
IF (${STATIC_OPENEXR} MATCHES 1)
    # Link statically
	set(EXTERNAL_OPENEXR_LIBRARIES ${ILMBASE_LIB}/libHalf.a ${ILMBASE_LIB}/libImath-2_0.a ${OPENEXR_LIB}/libImf-2_0.a ${ILMBASE_LIB}/libIlmThread-2_0.a ${ILMBASE_LIB}/libIex-2_0.a)
    # We also need --whole-archive around them, so...
    SET (EXTERNAL_OPENEXR_LIBRARIES -Wl,--whole-archive ${EXTERNAL_OPENEXR_LIBRARIES} -Wl,--no-whole-archive)
ELSE()
    set(EXTERNAL_OPENEXR_LIBRARIES ${ILMBASE_LIB}/libHalf.so ${ILMBASE_LIB}/libIex-2_0.so ${ILMBASE_LIB}/libIlmThread-2_0.so ${ILMBASE_LIB}/libImath-2_0.so ${OPENEXR_LIB}/libImf-2_0.so)
ENDIF(${STATIC_OPENEXR} MATCHES 1)
#MESSAGE(${EXTERNAL_OPENEXR_LIBRARIES})

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

# add libtiff if it was available
IF(${HAVE_LIBTIFF} MATCHES 1)
	set(EXTERNAL_INCLUDE_DIRS ${EXTERNAL_INCLUDE_DIRS} ${LIBTIFF_INCLUDE_DIRS})
	set(EXTERNAL_LIBRARIES ${EXTERNAL_LIBRARIES} ${LIBTIFF_LIBRARY})
ENDIF(${HAVE_LIBTIFF} MATCHES 1)


# Imagine LIB

ADD_DEFINITIONS(-DIMAGINE_NO_GUI -DIMAGINE_EMBEDDED_MODE)

FILE(GLOB_RECURSE imagine_SOURCES_ORIGINAL "${IMAGINE_SRC_PATH}/*.cpp")

#MESSAGE( "${imagine_SOURCES_ORIGINAL}" )

# Remove UI items and other stuff so that we can use Imagine's src directory directly without having
# a modified copy
SET(imagine_SOURCES "")
FOREACH(file ${imagine_SOURCES_ORIGINAL})
	STRING(REGEX MATCH "([/]ui[/]|[/]app[/]|[/]io[/]geometry[/]|[/]io[/]scene[/]|[/]io[/]volume[/]|[/]scene_builders[/]|[/]volumes[/]|[/]objects[/]primitives[/]freeform[/])" test1 ${file})
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

ADD_DEPENDENCIES(katanaImagineRender katanaAPI imagineCore)
SET_TARGET_PROPERTIES(katanaImagineRender PROPERTIES PREFIX "")
#SET(CMAKE_SHARED_LINKER_FLAGS "-fsanitize=address -lasan")
TARGET_LINK_LIBRARIES(katanaImagineRender katanaAPI imagineCore ${EXTERNAL_LIBRARIES} ${ZLIB_LIBRARY})

## imagineViewerModifier

FIND_PACKAGE(OpenGL REQUIRED)

FILE(GLOB_RECURSE imagineViewerModifier_SOURCES "${CMAKE_SOURCE_DIR}/src/imagineViewerModifier/*.cpp")

ADD_LIBRARY(imagineViewerModifier SHARED ${imagineViewerModifier_SOURCES})

ADD_DEPENDENCIES(imagineViewerModifier katanaAPI)
SET_TARGET_PROPERTIES(imagineViewerModifier PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(imagineViewerModifier katanaAPI ${OPENGL_LIBRARY})

