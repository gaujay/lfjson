# lfjson_benchmark
set(INCLUDE_FILES
	  bench_memory.h
    bench_deserialize.h
    bench_serialize.h
    bench_utils.h
)

set(SOURCE_FILES
    benchmark_main.cpp
)

add_executable(lfjson_benchmark
    ${INCLUDE_FILES}
    ${SOURCE_FILES}
)

target_include_directories(lfjson_benchmark
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/lib/rapidjson-1.1.0/include
        ${CMAKE_SOURCE_DIR}/lib/xxHash-0.8.1
)

if (MSVC)
  target_compile_options(lfjson_benchmark
    PRIVATE
      /W4
      /DBENCH_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/bench/examples/"
)
else()
  target_compile_options(lfjson_benchmark
    PRIVATE
      -Wall -Wextra -pedantic
      -DBENCH_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/bench/examples/"
)
endif()
