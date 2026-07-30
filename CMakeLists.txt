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
  PATCH_COMMAND  bash -c "git apply --ignore-whitespace ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ollama-native-image-format.patch || true"
)
FetchContent_MakeAvailable(llm_client)

include(FetchContent)
FetchContent_Declare(
  cpppdf
  GIT_REPOSITORY https://github.com/wkqco33/cpppdf.git
  GIT_TAG        7e049d7d36026f846ea60a1765ee2734deaefa68
)
set(CPPPDF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# cpppdf's own test suite resolves include paths off CMAKE_SOURCE_DIR, which
# points at ragcli's source tree (not cpppdf's) when pulled in via
# FetchContent, so keep BUILD_TESTING off for this fetch.
set(BUILD_TESTING OFF)
FetchContent_MakeAvailable(cpppdf)
unset(BUILD_TESTING) # restore visibility of the cached BUILD_TESTING value

# On Windows, cpppdf/src/renderer/terminal.cpp includes <sys/ioctl.h> which
# does not exist in the MinGW-w64 headers.  Inject a shim that provides the
# POSIX ioctl/winsize interface via the Windows Console API.
if(WIN32)
  target_include_directories(cpppdf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/win-stubs)
endif()

set(BUILD_TESTING ON CACHE BOOL "" FORCE)

add_executable(ragcli
  src/main.cpp
  src/utils/stb_image_impl.cpp
)
target_include_directories(ragcli PRIVATE src)
target_link_libraries(ragcli PRIVATE wcppcli)
target_link_libraries(ragcli PRIVATE llm_client)
target_link_libraries(ragcli PRIVATE cpppdf::cpppdf)

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
add_compile_definitions(SPDLOG_FMT_EXTERNAL)
target_link_libraries(ragcli PRIVATE fmt::fmt spdlog::spdlog)

find_package(JPEG)
if(JPEG_FOUND)
  target_compile_definitions(ragcli PRIVATE CPPPDF_HAVE_JPEG)
endif()

find_package(Stb REQUIRED)
target_include_directories(ragcli PRIVATE ${Stb_INCLUDE_DIR})

include(CTest)
if (BUILD_TESTING)
  add_subdirectory(tests)
endif()

if (MSVC)
  target_compile_options(ragcli PRIVATE /W4 /permissive-)
else()
  target_compile_options(ragcli PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (MINGW)
  # Release packaging ships a single ragcli.exe with no accompanying DLLs, so
  # the MinGW runtime (libstdc++/libgcc/winpthread) must be linked statically
  # rather than assuming it's present on the target machine.
  target_link_options(ragcli PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
