cmake_minimum_required(VERSION 3.23)

# Only interpret if() arguments as variables or keywords when unquoted.
cmake_policy(SET CMP0054 NEW)
# MSVC runtime library flags are selected by an abstraction.
cmake_policy(SET CMP0091 NEW)

set(WEBRTC_INCLUDE_DIR "" CACHE PATH "WebRTC のインクルードディレクトリ")
set(WEBRTC_LIBRARY_DIR "" CACHE PATH "WebRTC のライブラリディレクトリ")
set(WEBRTC_LIBRARY_NAME "webrtc" CACHE STRING "WebRTC のライブラリ名")
set(BOOST_ROOT "" CACHE PATH "Boost のルートディレクトリ")
set(SORA_DIR "" CACHE PATH "Sora のルートディレクトリ")
set(SDL2_DIR "" CACHE PATH "SDL2 のルートディレクトリ")
set(CLI11_DIR "" CACHE PATH "CLI11 のルートディレクトリ")

project(sora-sdl-sample C CXX)

list(APPEND CMAKE_PREFIX_PATH ${SORA_DIR} ${SDL2_DIR})
list(APPEND CMAKE_MODULE_PATH ${SORA_DIR}/share/cmake)

set(Boost_USE_STATIC_LIBS ON)

find_package(Boost REQUIRED COMPONENTS json filesystem)
find_package(WebRTC REQUIRED)
find_package(Sora REQUIRED)
#find_package(SDL2 REQUIRED)
find_package(Threads REQUIRED)
find_package(Libva REQUIRED)
find_package(Libdrm REQUIRED)

add_executable(sora_recv)
set_target_properties(sora_recv PROPERTIES CXX_STANDARD 20 C_STANDARD 20)
set_target_properties(sora_recv PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_sources(sora_recv PRIVATE sora_recv.cpp Sink.cpp AudioSink.cpp VideoSink.cpp MsgOut.cpp)

target_compile_options(sora_recv
  PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>"
    "$<$<COMPILE_LANGUAGE:CXX>:-isystem${LIBCXX_INCLUDE_DIR}>"
)
target_include_directories(sora_recv PRIVATE ${CLI11_DIR}/include)
target_link_libraries(sora_recv PRIVATE Sora::sora)
target_compile_definitions(sora_recv PRIVATE CLI11_HAS_FILESYSTEM=0)
