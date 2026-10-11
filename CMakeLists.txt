cmake_minimum_required(VERSION 3.20)
project(potter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

# FTXUI (terminal UI) is not packaged on Arch, so download it at configure time.
FetchContent_Declare(ftxui
  URL https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v6.1.9.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ftxui)

# toml++ (config file): use the system package if present, otherwise download it.
find_package(tomlplusplus 3 QUIET)
if(NOT tomlplusplus_FOUND)
  FetchContent_Declare(tomlplusplus
    URL https://github.com/marzer/tomlplusplus/archive/refs/tags/v3.4.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP ON
  )
  FetchContent_MakeAvailable(tomlplusplus)
endif()

# Core logic with no UI dependency, shared by the app and the tests.
add_library(potter_core STATIC
  src/timer.cpp
  src/config.cpp
)
target_include_directories(potter_core PUBLIC src)
target_link_libraries(potter_core PUBLIC tomlplusplus::tomlplusplus)
target_compile_options(potter_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(potter
  src/main.cpp
  src/app.cpp
  src/notify.cpp
  src/ui/timer_view.cpp
  src/ui/settings_view.cpp
)
target_link_libraries(potter PRIVATE
  potter_core
  ftxui::screen
  ftxui::dom
  ftxui::component
)
target_compile_options(potter PRIVATE -Wall -Wextra -Wpedantic)
# The version lives only in project() above; the code reads it from here.
target_compile_definitions(potter PRIVATE POTTER_VERSION="${PROJECT_VERSION}")

install(TARGETS potter RUNTIME DESTINATION bin)

enable_testing()
add_executable(timer_test tests/timer_test.cpp)
target_link_libraries(timer_test PRIVATE potter_core)
add_test(NAME timer_test COMMAND timer_test)
