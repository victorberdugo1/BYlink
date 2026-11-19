cmake_minimum_required(VERSION 3.20)
project(BYlink VERSION 0.2.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(bylinkcore STATIC
  compiler/common/diagnostic.cpp
  compiler/common/process.cpp
  compiler/target/target.cpp
  compiler/types/types.cpp
  compiler/lexer/lexer.cpp
  compiler/parser/parser.cpp
  compiler/semantic/sema.cpp
  compiler/ir/ir.cpp
  compiler/ir/lower.cpp
  compiler/ir/verify.cpp
  compiler/ai/byai.cpp
  compiler/ai/byb.cpp
  compiler/optimizer/optimizer.cpp
  compiler/codegen/llvm_emitter.cpp
  compiler/codegen/toolchain.cpp
  compiler/driver/pipeline.cpp
  compiler/opengl/bygl.cpp
  compiler/opengl/bygl2.cpp
)
target_include_directories(bylinkcore PUBLIC compiler)
if(MSVC)
  target_compile_options(bylinkcore PRIVATE /W4)
else()
  target_compile_options(bylinkcore PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(bylc compiler/driver/main.cpp)
target_link_libraries(bylc PRIVATE bylinkcore)

enable_testing()
add_executable(bylink_tests
  tests/test_main.cpp
  tests/test_types.cpp
  tests/test_lexer.cpp
  tests/test_parser.cpp
  tests/test_semantic.cpp
  tests/test_ir.cpp
  tests/test_ai.cpp
  tests/test_robustness.cpp
  tests/test_portability.cpp
  tests/test_native.cpp
  tests/test_opengl.cpp
)
target_link_libraries(bylink_tests PRIVATE bylinkcore)
target_compile_definitions(bylink_tests PRIVATE
  BYLC_PATH="$<TARGET_FILE:bylc>"
  BYLINK_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples"
)
add_test(NAME bylink_tests COMMAND bylink_tests)

add_executable(bylink_ai_bench EXCLUDE_FROM_ALL bench/ai_bench.cpp)
target_link_libraries(bylink_ai_bench PRIVATE bylinkcore)
