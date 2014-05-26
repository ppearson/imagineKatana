cmake_minimum_required (VERSION 2.6)
project (imagineKatana)

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

SET(STAND_ALONE 1) # set this to 1 to build without Imagine in proxy/stub mode

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
	#	STRING(REGEX MATCH "([/]ui[/]|[/]app[/]|[/]io[/]geometry[/]|[/]scene_builders[/])" test1 ${file})
		STRING(REGEX MATCH "([/]ui[/]|[/]app[/]|[/]io[/]|[/]scene_builders[/])" test1 ${file})
		IF(test1)

		ELSE(test1)
			LIST(APPEND imagine_SOURCES ${file})
		ENDIF(test1)
	ENDFOREACH(file ${imagine_SOURCES_ORIGINAL})

	#MESSAGE( "${imagine_SOURCES}" )

	INCLUDE_DIRECTORIES("${IMAGINE_SRC_PATH}")
	ADD_LIBRARY(imagineCore ${imagine_SOURCES})
ENDIF(${STAND_ALONE} MATCHES 0)

####

## katanaImagineRendererInfo
FILE(GLOB_RECURSE katanaImagineRendererInfo_SOURCES "${CMAKE_SOURCE_DIR}/src/imagineRendererInfo/*.cpp")

ADD_LIBRARY(katanaImagineRendererInfo SHARED "${katanaImagineRendererInfo_SOURCES}")

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

FILE(GLOB_RECURSE katanaImagineRenderer_SOURCES "${CMAKE_SOURCE_DIR}/src/imagineRenderer/*.cpp")

ADD_LIBRARY(katanaImagineRenderer SHARED "${katanaImagineRenderer_SOURCES}")

IF(${STAND_ALONE} MATCHES 1)
	ADD_DEPENDENCIES(katanaImagineRenderer katanaAPI)
	SET_TARGET_PROPERTIES(katanaImagineRenderer PROPERTIES PREFIX "")
	TARGET_LINK_LIBRARIES(katanaImagineRenderer katanaAPI)
ELSE(${STAND_ALONE} MATCHES 0)
	ADD_DEPENDENCIES(katanaImagineRenderer katanaAPI imagineCore)
	SET_TARGET_PROPERTIES(katanaImagineRenderer PROPERTIES PREFIX "")
	TARGET_LINK_LIBRARIES(katanaImagineRenderer katanaAPI imagineCore)
ENDIF(${STAND_ALONE} MATCHES 1)

