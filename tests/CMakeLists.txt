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
)
target_include_directories(ragcli_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ragcli_tests PRIVATE GTest::gtest_main wcppcli llm_client)

include(GoogleTest)
gtest_discover_tests(ragcli_tests)
