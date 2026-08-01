cmake_minimum_required(VERSION 3.20)

project(ragcli LANGUAGES CXX)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json" AND NOT CMAKE_TOOLCHAIN_FILE)
  message(WARNING "vcpkg.json detected but CMAKE_TOOLCHAIN_FILE is not set. Packages might not be found. Please run cmake with -DCMAKE_TOOLCHAIN_FILE=[vcpkg_path]/scripts/buildsystems/vcpkg.cmake")
endif()


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
  # Windows.h 의 min/max 매크로가 std::min/std::max 등과 충돌(C2589) 하는 것을
  # 방지하기 위해 NOMINMAX 를 전역으로 정의한다. 이 정의는 FetchContent 로 끌어온
  # llm_client/cpppdf 등 서브프로젝트에도 적용된다.
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

include(FetchContent)
find_package(Git REQUIRED)
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
  PATCH_COMMAND  ${CMAKE_COMMAND}
                  -DSOURCE_DIR=<SOURCE_DIR>
                  -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/ollama-native-image-format.patch
                  -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                  -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_patch.cmake
)
# llm_client 의 샘플/테스트는 ragcli 에서 불필요.
set(LLM_CLIENT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLM_CLIENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(llm_client)
# llm_client 는 정적 라이브러리(llm_client) 와 공유 라이브러리(llm_client_shared,
# OUTPUT_NAME 을 "llm_client" 로 오버라이드) 를 동시에 정의한다. Ninja 생성기는
# 두 타겟의 산출물 이름이 같으면 "multiple rules generate" 에러를 낸다. ragcli 는
# 정적 라이브러리만 링크하므로 공유 타겟의 출력명을 분리해 충돌을 피한다.
if(TARGET llm_client_shared)
  set_target_properties(llm_client_shared PROPERTIES
    EXCLUDE_FROM_ALL TRUE
    OUTPUT_NAME llm_client_shared)
endif()

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
find_package(cpr CONFIG REQUIRED)
add_compile_definitions(SPDLOG_FMT_EXTERNAL)
# llm_client 는 자체 빌드된 정적 라이브러리지만 spdlog/cpr/fmt 의존성을
# INTERFACE 로 노출하지 않기 때문에, ragcli 최종 타겟에서 명시적으로 링크해야
# 링커 에러(undefined reference to spdlog::*/cpr::*/fmt::*) 가 해결된다.
target_link_libraries(ragcli PRIVATE fmt::fmt spdlog::spdlog cpr::cpr)

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
  target_compile_options(ragcli PRIVATE /W4 /permissive- /utf-8)
else()
  # 소스 파일 및 실행 문자셋을 UTF-8 로 고정. Windows 에서 한글 리터럴이
  # 시스템 코드 페이지(CP949) 로 해석되는 것을 방지한다.
  target_compile_options(ragcli PRIVATE -Wall -Wextra -Wpedantic
    -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

if (MINGW)
  # Release packaging ships a single ragcli.exe with no accompanying DLLs, so
  # the MinGW runtime (libstdc++/libgcc/winpthread) must be linked statically
  # rather than assuming it's present on the target machine.
  target_link_options(ragcli PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
