# Elmo Trolla, 2019
# License: pick one - public domain / UNLICENSE (https://www.unlicense.org) / MIT (https://opensource.org/licenses/MIT).

cmake_minimum_required(VERSION 3.3)
project(aniplot)

set( D          ${PROJECT_SOURCE_DIR} )
set( DISTDIR    ${D}/dist )
set( BUILDDIR   ${D}/build )

#http://www.cmake.org/cmake/help/v3.0/module/FindOpenGL.html
find_package(OpenGL REQUIRED)
if (NOT OPENGL_FOUND)
	message(WARNING "OpenGL-NOT FOUND: " ${OPENGL_INCLUDE_DIR})
else()
	message(STATUS "OPENGL_INCLUDE_DIR: " ${OPENGL_INCLUDE_DIR})
	message(STATUS "OPENGL_gl_LIBRARY: " ${OPENGL_gl_LIBRARY})
endif()


set(CMAKE_VERBOSE_MAKEFILE "ON")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2")


set(SOURCE_FILES
	aniplotlib.cpp
	imgui_unitybuild.cpp)


if (MACOS)

	# whattheshit on macos (not tested on linux):
	#     find_library finds ${SDL2} /usr/local/lib/libSDL2.dylib
	#     and PKG_SEARCH_MODULE finds ${SDL2_INCLUDE_DIRS} /usr/local/include/SDL2
	#     neither finds the other. but we need both!
	find_library(SDL2 SDL2)
	INCLUDE(FindPkgConfig)
	PKG_SEARCH_MODULE(SDL2 REQUIRED sdl2)

	find_library(COREFOUNDATION CoreFoundation)

	#echo_all_cmake_variable_values()
	#message(STATUS "------------------------")
	#message(WARNING SDL2: ${SDL2})
	##if (SDL2-NOTFOUND)
	#message(WARNING bbbb SDL2_LIBRARIES: ${SDL2_LIBRARIES})
	#message(WARNING bbbb SDL2_INCLUDE_DIRS: ${SDL2_INCLUDE_DIRS}) # /usr/local/include/SDL2

	add_executable(${PROJECT_NAME} "aniplot.cpp")
	target_include_directories(${PROJECT_NAME} PRIVATE ${OPENGL_INCLUDE_DIR} "lib/imgui" "lib/imgui/backends" ${SDL2_INCLUDE_DIRS} "/usr/local/include") # ${CMAKE_SOURCE_DIR}
	target_link_libraries(${PROJECT_NAME} ${OPENGL_gl_LIBRARY} ${COREFOUNDATION} ${SDL2})

elseif (WIN32)

	link_directories           ("lib/SDL2-devel-2.0.4-VC/lib/x64/") # damnit this has to be above add_library or SDL2.lib is not found
	add_executable             (${PROJECT_NAME} "aniplot.cpp")
	target_include_directories (${PROJECT_NAME} PRIVATE ${OPENGL_INCLUDE_DIR} "lib/imgui" "lib/imgui/backends" "lib/SDL2-devel-2.0.4-VC/include")
	target_sources             (${PROJECT_NAME} PRIVATE ${SOURCE_FILES})
	target_link_libraries      (${PROJECT_NAME} ${OPENGL_gl_LIBRARY} "SDL2" "SDL2main" "ws2_32")

	set(EXECUTABLE_OUTPUT_PATH ${BUILDDIR})
	add_custom_command(TARGET aniplot POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/lib/SDL2-devel-2.0.4-VC/lib/x64/SDL2.dll ${BUILDDIR}/)

	#set_target_properties      (${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${DISTDIR})
	#set_target_properties      (${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${DISTDIR})
	# THESE JUST WON'T WORK DURING BUILD TIME!
	#file(COPY ${PROJECT_SOURCE_DIR}/lib/SDL2-devel-2.0.4-VC/lib/x64/SDL2.dll DESTINATION ${BUILDDIR}/)
	# copy to dist
	#file(REMOVE {$DIST}/*)
	#file(MAKE_DIRECTORY ${DISTDIR})
	#file(COPY ${BUILDDIR}/aniplot.exe DESTINATION ${DISTDIR}/)
	#file(COPY ${BUILDDIR}/SDL2.dll DESTINATION ${DISTDIR}/)

elseif (UNIX)
	message(WARNING "todo")
else ()
	message(WARNING "unknown platform")
endif ()


# -------------------------------------------------------------------------------------------------------------------
# - debugging -------------------------------------------------------------------------------------------------------
# -------------------------------------------------------------------------------------------------------------------


# need to list all generated and other variables that exist at a point in time? use this!
# from: https://blog.kitware.com/cmake-variable-values/
# echo_all_cmake_variable_values()
# NB! for some reason this does not print values set with target_link_libraries, target_sources, target_include_directories, ..
function(echo_all_cmake_variable_values)
	message(STATUS "")
	get_cmake_property(vs VARIABLES)
	foreach(v ${vs})
		message(STATUS "${v}='${${v}}'")
	endforeach(v)
	message(STATUS "")
endfunction()

#echo_all_cmake_variable_values()
