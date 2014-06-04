cmake_minimum_required (VERSION 2.6)
project (imagineKatana)

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


SET(CMAKE_CXX_FLAGS_DEBUG "-g -mfpmath=sse -fPIC -ffast-math -msse -msse2 -msse3 -mssse3 -msse4")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -mfpmath=sse -fPIC -ffast-math -msse -msse2 -msse3 -mssse3 -msse4")

# Katana APIs
SET(KATANA_PLUGIN_API_PATH "${CMAKE_SOURCE_DIR}/katana_api")

FILE(GLOB_RECURSE katanaAPI_SOURCES "${KATANA_PLUGIN_API_PATH}/src/*.cpp")
INCLUDE_DIRECTORIES("${KATANA_PLUGIN_API_PATH}/include/")
ADD_LIBRARY(katanaAPI ${katanaAPI_SOURCES})

SET(STAND_ALONE 0) # set this to 1 to build without Imagine in proxy/stub mode

IF(${STAND_ALONE} MATCHES 0)
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

	set(EXTERNAL_INCLUDE_DIRS ${ILMBASE_DIST}/include/OpenEXR ${OPENEXR_DIST}/include/OpenEXR)
	set(EXTERNAL_COMPILE_FLAGS "")
	set(EXTERNAL_LINK_FLAGS "")
	set(EXTERNAL_LIBRARY_DIRS ${ILMBASE_DIST}/lib ${OPENEXR_DIST}/lib)
	set(EXTERNAL_LIBRARIES ${EXTERNAL_OPENEXR_LIBRARIES})

	# add libtiff if it was available
	IF(${HAVE_LIBTIFF} MATCHES 1)
		set(EXTERNAL_INCLUDE_DIRS ${EXTERNAL_INCLUDE_DIRS} ${LIBTIFF_INCLUDE_DIRS})
		set(EXTERNAL_LIBRARIES ${EXTERNAL_LIBRARIES} ${LIBTIFF_LIBRARY})
	ENDIF(${HAVE_LIBTIFF} MATCHES 1)

ENDIF(${STAND_ALONE} MATCHES 0)


# Imagine LIB

IF(${STAND_ALONE} MATCHES 0)
	ADD_DEFINITIONS(-DIMAGINE_NO_GUI)

	SET(IMAGINE_SRC_PATH "${CMAKE_SOURCE_DIR}/../imagine/src")
	#SET(IMAGINE_SRC_PATH "/home/peter/coding/imagine/src")
	#SET(IMAGINE_SRC_PATH "/Users/peter/Documents/Coding/imagine/src")

	FILE(GLOB_RECURSE imagine_SOURCES_ORIGINAL "${IMAGINE_SRC_PATH}/*.cpp")

	#MESSAGE( "${imagine_SOURCES_ORIGINAL}" )

	# Remove UI items so that we can use Imagine's src directory directly without having
	# a modified copy
	SET(imagine_SOURCES "")
	FOREACH(file ${imagine_SOURCES_ORIGINAL})
	#	STRING(REGEX MATCH "([/]ui[/]|[/]app[/]|abc|opengl)" test1 ${file})
		STRING(REGEX MATCH "([/]ui[/]|[/]app[/]|[/]io[/]geometry[/]|[/]io[/]scene[/]|[/]scene_builders[/])" test1 ${file})
	#	STRING(REGEX MATCH "([/]ui[/]|[/]app[/]|[/]io[/]|[/]scene_builders[/])" test1 ${file})
		IF(test1)

		ELSE(test1)
			LIST(APPEND imagine_SOURCES ${file})
		ENDIF(test1)
	ENDFOREACH(file ${imagine_SOURCES_ORIGINAL})

	# remove some other stuff
	LIST(REMOVE_ITEM imagine_SOURCES ${IMAGINE_SRC_PATH}/io/geo_reader.cpp ${IMAGINE_SRC_PATH}/io/geometry/geo_reader_abc.cpp
			 ${IMAGINE_SRC_PATH}/io/image/image_reader_jpeg.cpp ${IMAGINE_SRC_PATH}/io/image/image_reader_tiff.cpp
				${IMAGINE_SRC_PATH}/io/image/image_reader_png.cpp ${IMAGINE_SRC_PATH}/io/image/image_writer_png.cpp ${IMAGINE_SRC_PATH}/objects/primitives/ocean.cpp)

#	MESSAGE( "${imagine_SOURCES}" )

	INCLUDE_DIRECTORIES("${IMAGINE_SRC_PATH}" ${EXTERNAL_INCLUDE_DIRS})
	ADD_LIBRARY(imagineCore ${imagine_SOURCES})
	ADD_DEPENDENCIES(imagineCore OPENEXR_LOCAL)
ELSE(${STAND_ALONE} MATCHES 1)
	ADD_DEFINITIONS(-DSTAND_ALONE)
ENDIF(${STAND_ALONE} MATCHES 0)

####

## katanaImagineRendererInfo
FILE(GLOB_RECURSE katanaImagineRendererInfo_SOURCES "${CMAKE_SOURCE_DIR}/src/imagineRendererInfo/*.cpp")

ADD_LIBRARY(katanaImagineRendererInfo SHARED ${katanaImagineRendererInfo_SOURCES})

IF(${STAND_ALONE} MATCHES 1)
	ADD_DEPENDENCIES(katanaImagineRendererInfo katanaAPI)
	SET_TARGET_PROPERTIES(katanaImagineRendererInfo PROPERTIES PREFIX "")
	TARGET_LINK_LIBRARIES(katanaImagineRendererInfo katanaAPI)
ELSE(${STAND_ALONE} MATCHES 0)
	ADD_DEPENDENCIES(katanaImagineRendererInfo katanaAPI imagineCore)
	SET_TARGET_PROPERTIES(katanaImagineRendererInfo PROPERTIES PREFIX "")
	TARGET_LINK_LIBRARIES(katanaImagineRendererInfo katanaAPI imagineCore)
ENDIF(${STAND_ALONE} MATCHES 1)

## katanaImagineRenderer

FILE(GLOB_RECURSE katanaImagineRender_SOURCES "${CMAKE_SOURCE_DIR}/src/imagineRender/*.cpp")

ADD_LIBRARY(katanaImagineRender SHARED ${katanaImagineRender_SOURCES})

IF(${STAND_ALONE} MATCHES 1)
	ADD_DEPENDENCIES(katanaImagineRender katanaAPI)
	SET_TARGET_PROPERTIES(katanaImagineRender PROPERTIES PREFIX "")
	TARGET_LINK_LIBRARIES(katanaImagineRender katanaAPI)
ELSE(${STAND_ALONE} MATCHES 0)
	ADD_DEPENDENCIES(katanaImagineRender katanaAPI imagineCore)
	SET_TARGET_PROPERTIES(katanaImagineRender PROPERTIES PREFIX "")
	TARGET_LINK_LIBRARIES(katanaImagineRender katanaAPI imagineCore ${EXTERNAL_LIBRARIES} ${ZLIB_LIBRARY})
ENDIF(${STAND_ALONE} MATCHES 1)

