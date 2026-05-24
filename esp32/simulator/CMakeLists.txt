cmake_minimum_required(VERSION 3.16)

project(aqualife_simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 QUIET)

if(NOT SDL2_FOUND AND EXISTS "/opt/homebrew/opt/sdl2")
  set(SDL2_INCLUDE_DIRS "/opt/homebrew/opt/sdl2/include")
  set(SDL2_LIBRARIES "/opt/homebrew/opt/sdl2/lib/libSDL2.dylib")
  set(SDL2_FOUND TRUE)
endif()

if(NOT SDL2_FOUND AND EXISTS "/usr/local/opt/sdl2")
  set(SDL2_INCLUDE_DIRS "/usr/local/opt/sdl2/include")
  set(SDL2_LIBRARIES "/usr/local/opt/sdl2/lib/libSDL2.dylib")
  set(SDL2_FOUND TRUE)
endif()

if(NOT SDL2_FOUND)
  message(FATAL_ERROR "SDL2 not found. Install it with: brew install sdl2")
endif()

add_executable(aqualife_simulator main.cpp)

target_include_directories(aqualife_simulator PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/compat
  ${SDL2_INCLUDE_DIRS}
  ${CMAKE_CURRENT_LIST_DIR}/../include
)

target_link_libraries(aqualife_simulator PRIVATE ${SDL2_LIBRARIES})

if(APPLE)
  target_link_libraries(aqualife_simulator PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()