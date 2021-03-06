cmake_minimum_required(VERSION 3.9)
project(test_)

enable_testing()

find_package (Boost COMPONENTS unit_test_framework REQUIRED)
find_package (Threads)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY build)
set (CMAKE_BUILD_TYPE Debug)
set (LIBR_PATH /net/factory/users/aag/code/mculib3)

include_directories (
   ${LIBR_PATH}/test/mocks
   ${LIBR_PATH}/src
   ${LIBR_PATH}/src/bits
   ${LIBR_PATH}/src/periph
   ${PROJECT_SOURCE_DIR}
   ${PROJECT_SOURCE_DIR}/../src
   ${Boost_INCLUDE_DIRS}
)

set (CMAKE_CXX_FLAGS "-Wno-register -Wno-overflow")


function(test MCU name)
   add_executable (${name} ${name}.cpp)
   if (MCU STREQUAL STM32F0)
      target_include_directories (${name} PRIVATE
         ${LIBR_PATH}/STM32F0_files
         ${LIBR_PATH}/STM32F0_files/CMSIS
      )
   elseif (MCU STREQUAL STM32F1)
   target_include_directories (${name} PRIVATE
      ${LIBR_PATH}/STM32F1_files
      ${LIBR_PATH}/STM32F1_files/CMSIS
   )
   elseif (MCU STREQUAL STM32F4)
      target_include_directories (${name} PRIVATE
         ${LIBR_PATH}/STM32F4_files
         ${LIBR_PATH}/STM32F4_files/CMSIS
      )
   elseif (MCU STREQUAL STM32F7)
      target_include_directories (${name} PRIVATE
         ${LIBR_PATH}/STM32F7_files
         ${LIBR_PATH}/STM32F7_files/CMSIS
      )
   endif()

   target_link_libraries (${name} 
      ${Boost_LIBRARIES}
      ${CMAKE_THREAD_LIBS_INIT}
      ${LIBR_PATH}
   )

   set_target_properties (${name} PROPERTIES
      CXX_STANDARD 17
      CXX_STANDARD_REQUIRED ON
      COMPILE_FLAGS -O0
      COMPILE_OPTIONS -Wno-pedantic -Wall -Wextra
      COMPILE_DEFINITIONS BOOST_TEST_DYN_LINK
   )
   add_test(${name} build/${name})
endfunction(test)

test (STM32F4 test_)



