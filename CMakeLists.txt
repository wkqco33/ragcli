cmake_minimum_required(VERSION 3.20)

project(ragcli LANGUAGES CXX)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json" AND NOT CMAKE_TOOLCHAIN_FILE)
  message(WARNING "vcpkg.json detected but CMAKE_TOOLCHAIN_FILE is not set. Packages might not be found. Please run cmake with -DCMAKE_TOOLCHAIN_FILE=[vcpkg_path]/scripts/buildsystems/vcpkg.cmake")
endif()


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
  wcppcli
  GIT_REPOSITORY https://github.com/wkqco33/wcppcli.git
  GIT_TAG        af36314d12483fe4f9650d6eacd85becc571c9d0
)
set(WCPPCLI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WCPPCLI_BUILD_TOOL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(wcppcli)

include(FetchContent)
FetchContent_Declare(
  llm_client
  GIT_REPOSITORY https://gitlab.com/wkqco33/LLM_client.git
  GIT_TAG        main
  PATCH_COMMAND  ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> git apply --ignore-whitespace ${CMAKE_CURRENT_SOURCE_DIR}/cmake/llm_client-easily-swappable-params.patch || ${CMAKE_COMMAND} -E true
)
FetchContent_MakeAvailable(llm_client)

add_executable(ragcli src/main.cpp)
target_include_directories(ragcli PRIVATE src)
target_link_libraries(ragcli PRIVATE wcppcli)
target_link_libraries(ragcli PRIVATE llm_client)

target_compile_features(ragcli PRIVATE cxx_std_17)

include(CTest)
if (BUILD_TESTING)
  add_subdirectory(tests)
endif()

if (MSVC)
  target_compile_options(ragcli PRIVATE /W4 /permissive-)
else()
  target_compile_options(ragcli PRIVATE -Wall -Wextra -Wpedantic)
endif()
