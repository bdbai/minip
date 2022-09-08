cmake_minimum_required(VERSION 3.23)
project(minip)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)
pkg_check_modules(URING liburing>=2)

include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported OUTPUT error)

add_executable(minip main.cpp serve.cpp serve.h worker.cpp worker.h conn.cpp conn.h http_handler.cpp http_handler.h context.cpp context.h err.cpp err.h http_header.cpp http_header.h router.cpp router.h res_writer.cpp res_writer.h file_res_writer.cpp file_res_writer.h)

if (lto_supported)
    set_property(TARGET minip PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set_property(TARGET minip PROPERTY INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
    set_property(TARGET minip PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
endif ()

if (URING_FOUND)
    target_link_libraries(minip ${URING_LIBRARIES})
    target_include_directories(minip PRIVATE ${URING_INCLUDE_DIRS})
    target_compile_definitions(minip PRIVATE WITH_URING URING_VERSION=${URING_VERSION})
    message("liburing enabled")
else ()
    message("liburing not enabled due to package not existing")
endif ()

target_compile_options(minip PRIVATE -Wall -Wextra -Wpedantic)
string(APPEND CMAKE_${lang}_FLAGS_RELEASE " -march=native")
string(APPEND CMAKE_${lang}_FLAGS_MINSIZEREL " -march=native")
string(APPEND CMAKE_${lang}_FLAGS_RELWITHDEBINFO " -march=native")
