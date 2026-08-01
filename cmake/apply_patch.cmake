# Cross-platform patch applicator: runs `git apply` in the source dir and
# ignores failures (patch already applied / no-op) so the build doesn't break.
execute_process(
  COMMAND ${GIT_EXECUTABLE} apply --ignore-whitespace ${PATCH_FILE}
  WORKING_DIRECTORY ${SOURCE_DIR}
  RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
  message(STATUS "patch: git apply returned ${result}, ignoring (already applied or no-op)")
endif()