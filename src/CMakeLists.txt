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

set(COSMO_PATH ${CMAKE_CURRENT_SOURCE_DIR}
               ${CMAKE_CURRENT_BINARY_DIR})

target_sources(${PROJECT_NAME} PUBLIC main.c cpu.c disassemble.c optab.c roms.c) 
target_include_directories(${PROJECT_NAME} PRIVATE ${COSMO_PATH})
target_link_libraries(${PROJECT_NAME} PUBLIC m)

add_executable(asm1802)
target_sources(asm1802 PUBLIC asm1802.c optab.c) 
target_include_directories(asm1802 PRIVATE ${COSMO_PATH})

add_executable(dis1802)
target_sources(dis1802 PUBLIC dis1802.c optab.c) 
target_include_directories(dis1802 PRIVATE ${COSMO_PATH})

if (RUN_TESTS)
add_executable(cosmo_test)
target_sources(cosmo_test PUBLIC cpu.c disassemble.c optab.c roms.c test/main.c test/cpu_test.c)
target_include_directories(cosmo_test PRIVATE ${COSMO_PATH})
if(MSVC)
    target_link_libraries(cosmo_test winmm)
endif()
add_custom_command(TARGET cosmo_test 
	COMMAND cosmo_test
	COMMENT "Running Cosmo test" VERBATIM)
add_test(NAME cosmo_test COMMAND cosmo_test)
endif()


