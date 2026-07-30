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
  config_test.cpp
  pdf_summarize_test.cpp
  ocr_test.cpp
  rag_test.cpp
  chat_test.cpp
  rag_runner_test.cpp
  chat_runner_test.cpp
  provider_config_test.cpp
  rag_config_test.cpp
  directory_source_test.cpp
  document_source_factory_test.cpp
  collection_test.cpp
  pdf_document_test.cpp
  indexer_test.cpp
  ../src/utils/stb_image_impl.cpp
)
target_include_directories(ragcli_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ragcli_tests PRIVATE GTest::gtest_main wcppcli llm_client cpppdf::cpppdf)
target_compile_definitions(ragcli_tests PRIVATE
  RAGCLI_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

find_package(JPEG)
if(JPEG_FOUND)
  target_compile_definitions(ragcli_tests PRIVATE CPPPDF_HAVE_JPEG)
endif()

find_package(Stb REQUIRED)
target_include_directories(ragcli_tests PRIVATE ${Stb_INCLUDE_DIR})

include(GoogleTest)
gtest_discover_tests(ragcli_tests)
