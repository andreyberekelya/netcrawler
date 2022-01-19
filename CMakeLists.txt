cmake_minimum_required(VERSION 3.21)
project(netcrawler)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBCURLPP REQUIRED curlpp)
pkg_check_modules(LIBXML2 REQUIRED libxml-2.0)
pkg_check_modules(LIBXMLPP REQUIRED libxml++-2.6)

INCLUDE_DIRECTORIES(
        ${LIBCURLPP_INCLUDE_DIRS}
        ${LIBXML2_INCLUDE_DIRS}
        ${LIBXMLPP_INCLUDE_DIRS}
)

add_executable(netcrawler main.cpp)
target_link_libraries(netcrawler
        pthread
        ${LIBCURLPP_LIBRARIES}
        ${LIBXML2_LIBRARIES}
        ${LIBXMLPP_LIBRARIES}
)
