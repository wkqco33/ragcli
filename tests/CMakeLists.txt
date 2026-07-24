include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.17.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(ragcli_tests
  registry_test.cpp
  greet_test.cpp
  rag_test.cpp
  chat_test.cpp
  rag_runner_test.cpp
  chat_runner_test.cpp
  indexer_test.cpp
  ../src/utils/stb_image_impl.cpp
)
target_include_directories(ragcli_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ragcli_tests PRIVATE GTest::gtest_main wcppcli llm_client cpppdf::cpppdf)

find_package(JPEG)
if(JPEG_FOUND)
  target_compile_definitions(ragcli_tests PRIVATE CPPPDF_HAVE_JPEG)
endif()

find_package(Stb REQUIRED)
target_include_directories(ragcli_tests PRIVATE ${Stb_INCLUDE_DIR})

include(GoogleTest)
gtest_discover_tests(ragcli_tests)
