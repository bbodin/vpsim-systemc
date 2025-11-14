# Copyright (C) 2024 Commissariat à l'énergie atomique et aux énergies alternatives (CEA)

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# http://www.apache.org/licenses/LICENSE-2.0 

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.20)
project(vpsim_components)

set(CMAKE_CXX_STANDARD 20)

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unused-variable -Wno-unused-parameter -Wno-unused-private-field")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused-variable -Wno-unused-parameter -Wno-unused-private-field")
set(CMAKE_EXE_LINKER_FLAGS "-no-pie")

# This is for TTB
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-changes-meaning")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-changes-meaning")

# Doxygen package
find_package(Doxygen QUIET)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Check that interprocedural optimization is supported
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED)
if(IPO_SUPPORTED AND NOT (CMAKE_BUILD_TYPE STREQUAL Debug))
    set(IPO_ENABLED True)
    set(CMAKE_LINK_WHAT_YOU_USE ON)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
else()
    set(IPO_ENABLED False)
endif()

# Built-in GTest CMake handling
include(CTest)
enable_testing()
if(NOT GTEST_FOUND)
    set(CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/extern/gtest)
    find_package(GTest QUIET)
    include(GoogleTest)

    # This is a technique found here : http://crascit.com/2015/07/25/cmake-gtest/
    # Basically, it allows to download and configure GTest at configuration time.
    # This is a generic method which allows to use the CMake targets provided by GTest in that particular case.

    # Download configure and build steps for gtest
    # Download and unpack googletest at configure time
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CMake/CMakeLists-gtest.txt.in ${CMAKE_CURRENT_BINARY_DIR}/gtest-download/CMakeLists.txt)
    execute_process(COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gtest-download" )
    execute_process(COMMAND "${CMAKE_COMMAND}" --build .
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gtest-download" )

    # Prevent GoogleTest from overriding our compiler/linker options
    # when building with Visual Studio
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    # Add googletest directly to our build. This adds
    # the following targets: gtest, gtest_main, gmock
    # and gmock_main
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/extern/gtest)

    set(GTEST_FOUND TRUE)
endif()

#####################################################################
# Systemc installation
#
# Looks for SystemC on the system and install it locally if not found

set(CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/extern/systemc)
find_package(SystemCLanguage 2.3.2 EXACT QUIET)

if(NOT SystemCLanguage_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CMake/CMakeLists-systemc.txt.in
            ${CMAKE_CURRENT_BINARY_DIR}/systemc-download/CMakeLists.txt)
    execute_process(COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/systemc-download" )
    execute_process(COMMAND "${CMAKE_COMMAND}" --build .
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/systemc-download" )
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/extern/systemc)
    #add_library(SystemC::systemc ALIAS systemc)
    set_target_properties(systemc
        PROPERTIES
            CXX_STANDARD ${CMAKE_CXX_STANDARD}
            BUILD_TYPE ${CMAKE_BUILD_TYPE}
            POSITION_INDEPENDENT_CODE ON
    )

endif()

#####################################################################
# Static python installation

include(ExternalProject)
set(LINKFORSHARED_STRING "")
externalProject_Add(relocatable-python
    URL https://www.python.org/ftp/python/3.10.6/Python-3.10.6.tar.xz
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/relocatable-python"
    CONFIGURE_COMMAND
        echo ${CMAKE_CURRENT_SOURCE_DIR}/extern/relocatable-python/configure &&
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/relocatable-python/configure "CFLAGS=-fPIC" --prefix=${CMAKE_CURRENT_BINARY_DIR}/extern/relocatable-python-install
    BUILD_COMMAND  make
        #overriding LINKFORSHARED is useless as we do not compile a static library but a .a archive with relocable code
        #do not follow what is in wiki.python.org/moin/Build/Statically
        #make LINKFORSHARED=${LINKFORSHARED_STRING}
    INSTALL_COMMAND make install
    TEST_COMMAND ""
)



add_custom_command(
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/extern/relocatable-python-vpsim/lib/libpython3.10.a.vpsim
        ${CMAKE_CURRENT_BINARY_DIR}/extern/relocatable-python-vpsim/include/python3.10/
    COMMAND
        #get the translation for all symbols defined in the python library
        nm ${CMAKE_CURRENT_BINARY_DIR}/extern/relocatable-python-install/lib/libpython3.10.a > NmRawSyms &&
        ${CMAKE_CURRENT_SOURCE_DIR}/CMake/GenerateSymsTranslation.py &&
        #rename symbols in the library
        mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/extern/relocatable-python-vpsim/lib/ &&
        objcopy --redefine-syms=SymsTranslation ${CMAKE_CURRENT_BINARY_DIR}/extern/relocatable-python-install/lib/libpython3.10.a ${CMAKE_CURRENT_BINARY_DIR}/extern/relocatable-python-vpsim/lib/libpython3.10.a.vpsim &&
        #rename symbols in sources
        cp -r ${CMAKE_CURRENT_BINARY_DIR}/extern/relocatable-python-install/include ${CMAKE_CURRENT_BINARY_DIR}/extern/relocatable-python-vpsim/ &&
        ${CMAKE_CURRENT_SOURCE_DIR}/CMake/RenameSymbolsInSourceFiles.py NmRawSyms ${CMAKE_CURRENT_BINARY_DIR}/extern/relocatable-python-vpsim/include/
    DEPENDS
        relocatable-python
    COMMENT "Renaming Python functions "
)


add_custom_target(python-vpsim
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/extern/relocatable-python-vpsim/lib/libpython3.10.a.vpsim
        ${CMAKE_CURRENT_BINARY_DIR}/extern/relocatable-python-vpsim/include/python3.10/
)

#####################################################################
# elfio installation
#
# Small header only library

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CMake/CMakeLists-elfio.txt.in
        ${CMAKE_CURRENT_BINARY_DIR}/elfio-download/CMakeLists.txt)
execute_process(COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/elfio-download" )
execute_process(COMMAND "${CMAKE_COMMAND}" --build .
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/elfio-download" )

add_library(elfio INTERFACE)
target_include_directories(elfio INTERFACE extern/elfio)

#####################################################################
# tbb installation

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CMake/CMakeLists-tbb.txt.in
        ${CMAKE_CURRENT_BINARY_DIR}/tbb-download/CMakeLists.txt)

execute_process(COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tbb-download" )

execute_process(COMMAND "${CMAKE_COMMAND}" --build .
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tbb-download" )

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/extern/tbb)

#####################################################################
# RapidXML installation
#
# Small header only library

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CMake/CMakeLists-rapidxml.txt.in
        ${CMAKE_CURRENT_BINARY_DIR}/RapidXML-download/CMakeLists.txt)
execute_process(COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/RapidXML-download" )
execute_process(COMMAND "${CMAKE_COMMAND}" --build .
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/RapidXML-download" )

add_library(RapidXML INTERFACE)
target_include_directories(RapidXML INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/extern/rapidxml)

#############################################################
# VPSIM_CORE

file(GLOB VPSIM_CORE_SOURCES
        CONFIGURE_DEPENDS
        core/include/core/*.hpp
        core/logger/include/logger/*.hpp
        core/logger/*.cpp
        core/vpsimModule/include/vpsimModule/*.hpp
        core/vpsimModule/moduleParameters.cpp
        core/vpsimModule/paramAppointment.cpp
        core/vpsimModule/parameterSet.cpp
        core/vpsimModule/parameterSetModifier.cpp
        core/vpsimModule/paramManager.cpp
        core/vpsimModule/paramScheduler.cpp
        core/vpsimModule/vpsimModule.cpp
        core/vpsimModule/VpsimIp.cpp
        core/AddrSpace.cpp
        core/global.cpp
        core/InitiatorIf.cpp
        core/LatencyIf.cpp
        core/Payload.cpp
        core/quantum.cpp
        core/TargetIf.cpp
        core/platform_builder/include/platform_builder/*.hpp
        core/platform_builder/*.cpp
        core/TargetIf.cpp
        core/vpsimModule/ForwardSimpleSocket.cpp

)

add_library(vpsim_core STATIC ${VPSIM_CORE_SOURCES})

target_include_directories(vpsim_core PUBLIC
        core/include/core
        core/logger/include/logger
        core/logger/include
        core/vpsimModule/include/vpsimModule
        core/vpsimModule/include
        core/platform_builder/include
        core/include
        )


target_link_libraries(vpsim_core PUBLIC systemc)


target_link_libraries(vpsim_core PUBLIC RapidXML)

if(IPO_ENABLED)
    set_target_properties(vpsim_core PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

set_target_properties(vpsim_core PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

#############################################################
# COMPONENTS
file(GLOB VPSIM_COMPONENTS_SOURCES
        CONFIGURE_DEPENDS
        #components/compute/include/compute/arm.hpp
        components/compute/InterruptIf.cpp
        #components/compute/include/compute/*.hpp
        components/compute/issFinder.cpp
        components/compute/issLibWrapper.cpp
        components/compute/issWrapper.cpp
        components/connect/interconnect.cpp
        #components/connect/include/connect/interconnect.hpp
        components/connect/CoherenceInterconnect.cpp
        #components/connect/include/connect/CoherenceInterconnect.hpp
        #components/include/components/*.hpp
        #components/include/components/*.h
        #components/memory/include/memory/Cache.hpp
        #components/memory/include/memory/CacheBase.hpp
        #components/memory/include/memory/CacheLine.hpp
        #components/memory/include/memory/CacheSet.hpp
        #components/memory/include/memory/CoherenceExtension.hpp
        #components/memory/include/memory/elfloader.hpp
        #components/memory/include/memory/memory.hpp
        components/memory/elfloader.cpp
        components/memory/memory.cpp
        components/peripherals/include/peripherals/*.hpp
        components/peripherals/*.cpp
        components/clean_components/SmartUart.cpp
        #components/clean_components/include/components/SmartUart.hpp
        #components/clean_components/include/components/CallbackRegister.hpp
        components/clean_components/CallbackRegister.cpp
)

add_library(vpsim_components STATIC ${VPSIM_COMPONENTS_SOURCES})

target_include_directories(vpsim_components PUBLIC
        components/compute/include/compute
        components/compute/include/compute/dynamic
        components/compute/include
        components/connect/include/connect
        components/connect/include
        components/include/components
        components/memory/include/
        components/memory/include/memory
        components/peripherals/include
        components/peripherals/include/peripherals
        components/templates/include
        components/clean_components/include
        components/include
        extern/tbb/include/tbb
        )

target_include_directories(vpsim_components PRIVATE
        components/templates
        ${CMAKE_CURRENT_BINARY_DIR}/extern/relocatable-python-vpsim/include/python3.10
        )

target_link_libraries(vpsim_components PUBLIC
        vpsim_core
        elfio
        tbb
        util
        ${CMAKE_DL_LIBS})

add_dependencies(vpsim_components python-vpsim)


set_target_properties(vpsim_components PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_link_libraries(vpsim_components PUBLIC systemc)

if(IPO_ENABLED)
    set_target_properties(vpsim_components PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()


#############################################################
# VPSIM_BINARY
add_executable(vpsim
         main.cpp)

target_compile_definitions(vpsim PRIVATE MAIN_EXECUTABLE=1)

add_library(vpsim_subsystem SHARED
        vpsim_subsystem.hpp
        vpsim_subsystem.cpp)


target_link_libraries(vpsim PUBLIC
    vpsim_components
    vpsim_core
    ${CMAKE_CURRENT_BINARY_DIR}/extern/relocatable-python-vpsim/lib/libpython3.10.a.vpsim
    )
target_include_directories(vpsim PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/extern/relocatable-python-vpsim/include/python3.10
)


# VPSIM_SUBSYSTEM
target_link_libraries(vpsim_subsystem PUBLIC -Wl,--whole-archive
    vpsim_components
    vpsim_core
    systemc
    ${CMAKE_CURRENT_BINARY_DIR}/extern/relocatable-python-vpsim/lib/libpython3.10.a.vpsim
    -Wl,--no-whole-archive
    )
target_include_directories(vpsim_subsystem PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/extern/relocatable-python-vpsim/include/python3.10
)

if(IPO_ENABLED)
    set_target_properties(vpsim PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

#############################################################
# Tests

macro(add_gtest_test test_name sources)
    if(GTEST_FOUND)
        add_executable(${test_name} ${sources})
        target_compile_options(${test_name} PRIVATE -g -O0 -DDEBUG)

        target_link_libraries(${test_name} PRIVATE gtest vpsim_core)
        gtest_add_tests(TARGET ${test_name}
         TEST_PREFIX "vpsim."
         PROPERTIES
        ENVIRONMENT "SYSTEMC_DISABLE_COPYRIGHT_MESSAGE=1")
        if(IPO_ENABLED)
            set_target_properties(${test_name} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif()
    endif(GTEST_FOUND)
endmacro(add_gtest_test)

# add_gtest_test(moduleParameters_test
#         ${CMAKE_CURRENT_SOURCE_DIR}/core/vpsimModule/test/moduleParameters_test.cpp)

# add_gtest_test(parameterSet_test
#         ${CMAKE_CURRENT_SOURCE_DIR}/core/vpsimModule/test/parameterSet_test.cpp)

# add_gtest_test(parameterSetModifier_test
#         ${CMAKE_CURRENT_SOURCE_DIR}/core/vpsimModule/test/parameterSetModifier_test.cpp)

add_gtest_test(appointment_test     ${CMAKE_CURRENT_SOURCE_DIR}/tests/appointment_test.cpp)
add_gtest_test(logger_test          ${CMAKE_CURRENT_SOURCE_DIR}/tests/logger_test.cpp)
add_gtest_test(loggerCore_test      ${CMAKE_CURRENT_SOURCE_DIR}/tests/loggerCore_test.cpp)
add_gtest_test(loggerScheduler_test ${CMAKE_CURRENT_SOURCE_DIR}/tests/loggerScheduler_test.cpp)


#############################################################
# Doxygen documentation

if(DOXYGEN_FOUND)
    doxygen_add_docs(vpsim_doc core components)
endif()
