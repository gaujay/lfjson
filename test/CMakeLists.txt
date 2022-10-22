# lfjson_tests
include(GoogleTest)

set(SOURCE_FILES_LFJSON
    test_lfjson_main.cpp
)

set(gtest_force_shared_crt
    ON CACHE BOOL "" FORCE
)

add_executable(lfjson_tests
    ${SOURCE_FILES_LFJSON}
)

target_include_directories(lfjson_tests 
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/lib/googletest/googletest/include
        ${CMAKE_SOURCE_DIR}/lib/xxHash-0.8.1
)

target_link_libraries(lfjson_tests 
    PUBLIC 
        gtest
        gtest_main
)

if (MSVC)
  target_compile_options(lfjson_tests
    PRIVATE
      /W4
)
else()
  target_compile_options(lfjson_tests
    PRIVATE
      -Wall -Wextra -pedantic
)
endif()

#
gtest_discover_tests(lfjson_tests)
