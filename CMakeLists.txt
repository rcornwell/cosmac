#
# Author:      Richard Cornwell (rich@sky-visions.com)
#
# Copyright 2025, Richard Cornwell
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

cmake_minimum_required(VERSION 3.22)

## Set our project name
set( PROJECT_NAME "cosmo" )
project(${PROJECT_NAME} VERSION 0.0.0 DESCRIPTION "Cosmac VIP Simulator")

## Set version number 
set (${PROJECT_NAME}_VERSION_MAJOR 1)
set (${PROJECT_NAME}_VERSION_MINOR 0)

# Generate Doxygen documentation files
option(BUILD_DOC "Build documenation" OFF)
option(RUN_TESTS "Run tests" ON)

# Add a custom CMake Modules directory
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

## Check if Doxygen is available.
if (BUILD_DOC)
    find_package(Doxygen REQUIRED)
    if (NOT DOXYGEN_FOUND)
	message(WARNING "Doxygen not found, unable to generate documetation")
    else()
        set(DOXY_IN ${CMAKE_HOME_DIRECTORY}/doc/doxyfile.in)                      ## used in the doxyfile.in
        set(DOXY_OUT ${CMAKE_HOME_DIRECTORY}/doc)    ## used in the doxyfile.in
	configure_file(${DOXY_IN} ${DOXY_OUT} @ONLY)
	add_custom_target(doxyDoc 
		COMMAND ${DOXYGEN_EXECUTABLE} ${DOXY_IN}
		WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/doc
                COMMENT "Building user's documentation into doxyDoc build dir..."
        )
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl2)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package (Threads)

if (WIN32)
    add_executable(${PROJECT_NAME} WIN32)
else()
    set (SDL2_BUILDING_LIBRARY 1)
    add_executable(${PROJECT_NAME})
#    target_link_libraries(${PROJECT_NAME} PUBLIC asan)
endif()

include(CheckIncludeFiles)
check_include_files(unistd.h HAVE_UNISTD_H)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/config.h)

find_package (SDL2 REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
find_package (SDL2_mixer REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC ${SDL2_LIBRARIES}
                                             ${SDL2_MIXER_LIBRARIES})

if (RUN_TESTS)
   enable_testing()
endif()

if (WIN32)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
else()
#    add_compile_options( -Wall -Wunused-result $<$<CONFIG:DEBUG>:-g> -fsanitize=address -fsanitize-recover=address)
    add_compile_options( -Wall -Wunused-result $<$<CONFIG:DEBUG>:-g>)
    add_compile_definitions($<$<CONFIG:DEBUG>:DEBUG>)
endif()
set(FONT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fonts)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_subdirectory(src)

find_program(CMAKE_C_CPPCHECK NAMES cppcheck)
if (CMAKE_C_CPPCHECK)
    list(
         APPEND CMAKE_C_CPPCHECK
             "--enable=warning"
             "--enable=style"
             "--enable=performance"
             "--enable=portability"
             "--enable=missingInclude"
             "--inline-suppr"
             "--std=c11")
endif()


if(UNIX)
    add_custom_target (distclean @echo cleaning for source distribution)

    add_custom_command(
        COMMENT "distribution clean"
        COMMAND make
        ARGS -C ${CMAKE_CURRENT_BINARY_DIR} clean
        COMMAND find
        ARGS ${CMAKE_CURRENT_BINARY_DIR} -name "CMakeCache.txt" | xargs rm -rf
        COMMAND find
        ARGS ${CMAKE_CURRENT_BINARY_DIR} -name "CMakeFiles" | xargs rm -rf
        COMMAND find
        ARGS ${CMAKE_CURRENT_BINARY_DIR} -name "Makefile" | xargs rm -rf
        COMMAND find
        ARGS ${CMAKE_CURRENT_BINARY_DIR} -name "*.cmake" | xargs rm -rf
        COMMAND find
        ARGS -rf ${CMAKE_CURRENT_BINARY_DIR}/install_manifest.txt
        TARGET  distclean
    )
endif(UNIX)

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_GENERATOR "DEB;NSIS")
if (WIN32)
include(InstallRequiredSystemLibraries)
add_custom_command(TARGET cosmo POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy
         -t "$<TARGET_RUNTIME_DLLS:cosmo>" "$<TARGET_FILE_DIR:cosmo>"
      COMMAND_EXPAND_LISTS)
#install(FILES ${SDL2_LIBRARY}/SDL2.dll DESTINATION bin)
#install(FILES ${SDL2_MIXER_LIBRARY}/SDL2_mixer.dll DESTINATION bin)
install(TARGETS cosmo RUNTIME DESTINATION bin)
install(TARGETS asm1802 RUNTIME DESTINATION bin)
else()
install(TARGETS cosmo RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/games")
install(TARGETS asm1802 RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/games")
endif()
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Simple Cosmac VIP Emulator")
set(CPACK_PACKAGE_VENDOR "http://sky-visions.com")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_PACKAGE_VERSION_MAJOR "${${PROJECT_NAME}_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${${PROJECT_NAME}_VERSION_MINOR}")
set(CPACK_PACKAGE_CONTACT "rich@sky-visions.com")
set(CPACK_NSIS_INSTALLED_ICON_NAME "${CMAKE_CURRENT_SOURCE_DIR}/image/icon.ico")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Richard Cornwell")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsld2-image-2.0-0 libsdl2-mixer-2.0-0")
#set(CPACK_DEBIAN_FILE_NAME DEB_DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)#ONE_PER_GROUP)
set(CPACK_DEB_COMPONENT_INSTALL YES)
set(CPACK_STRIP_FILES YES)

include(CPack)
