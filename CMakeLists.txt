cmake_minimum_required(VERSION 3.22)

project(DerbitTradingApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

find_package(Boost 1.74 REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)

# FetchContent for spdlog
include(FetchContent)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.x 
)

FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

include_directories(${INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIR})
include_directories(${OPENSSL_INCLUDE_DIR})

add_executable(DerbitTradingApp ${SOURCE_DIR}/main.cpp ${SOURCE_DIR}/Client.cpp)

target_link_libraries(DerbitTradingApp 
    ${Boost_LIBRARIES}
    OpenSSL::SSL
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)
