cmake_minimum_required(VERSION 3.10)
project(cppweathermonitor LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

find_package(cpr CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)
add_executable(cweather ./src/cweather.cpp)
add_executable(monitor ./src/monitor.cpp)

target_link_libraries(cweather PRIVATE
    SQLite3::SQLite3
    cpr::cpr
)
target_link_libraries(monitor PRIVATE
    SQLite3::SQLite3
    cpr::cpr
)