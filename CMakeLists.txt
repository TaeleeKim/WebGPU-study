cmake_minimum_required(VERSION 2.8)

project(hello-webgpu)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-nonportable-include-path -fno-exceptions -fno-rtti")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -D_DEBUG=1 -Wno-unused -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g0 -DNDEBUG=1 -flto -O3")

file(GLOB sources src/*.cpp)
file(GLOB_RECURSE headers src/*.h)

if (EMSCRIPTEN)
	file(GLOB_RECURSE platform_sources src/ems/*)
	set(CMAKE_EXECUTABLE_SUFFIX ".html")

	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -s ASSERTIONS=2 -s DEMANGLE_SUPPORT=1 -s SAFE_HEAP=1 -s STACK_OVERFLOW_CHECK=2")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s ASSERTIONS=0 -s DISABLE_EXCEPTION_CATCHING=1 -s EVAL_CTORS=1 --closure 1")

	set(CMAKE_EXE_LINKER_FLAGS "-s ENVIRONMENT=web -s WASM=1 -s USE_WEBGPU=1 -s NO_EXIT_RUNTIME=1 -s STRICT=1 --shell-file ${CMAKE_CURRENT_LIST_DIR}/src/ems/shell.html")
elseif (WIN32)
	file(GLOB_RECURSE platform_sources src/win/*)
elseif (APPLE)
	file(GLOB_RECURSE platform_sources src/mac/*)
endif()

add_executable(hello-webgpu ${sources} ${platform_sources} ${headers})

target_include_directories(hello-webgpu PRIVATE "${CMAKE_CURRENT_LIST_DIR}/inc")