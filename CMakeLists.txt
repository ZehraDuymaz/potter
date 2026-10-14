cmake_minimum_required(VERSION 3.20)
project(potter VERSION 0.5.1 LANGUAGES CXX)

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

# nlohmann/json (talking to mpv): system package if present, otherwise download it.
find_package(nlohmann_json 3 QUIET)
if(NOT nlohmann_json_FOUND)
  FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP ON
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

# Core logic with no UI dependency, shared by the app and the tests.
add_library(potter_core STATIC
  src/core/timer.cpp
  src/core/config.cpp
  src/core/tasks.cpp
  src/core/history.cpp
  src/core/music.cpp
  src/core/storage.cpp
)
target_include_directories(potter_core PUBLIC src)
target_link_libraries(potter_core PUBLIC tomlplusplus::tomlplusplus nlohmann_json::nlohmann_json)
target_compile_options(potter_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(potter
  src/main.cpp
  src/app.cpp
  src/system/notify.cpp
  src/system/process.cpp
  src/system/player.cpp
  src/system/search.cpp
  src/ui/timer_view.cpp
  src/ui/settings_view.cpp
  src/ui/task_view.cpp
  src/ui/history_view.cpp
  src/ui/music_view.cpp
  src/ui/common.cpp
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

add_executable(tasks_test tests/tasks_test.cpp)
target_link_libraries(tasks_test PRIVATE potter_core)
add_test(NAME tasks_test COMMAND tasks_test)

add_executable(history_test tests/history_test.cpp)
target_link_libraries(history_test PRIVATE potter_core)
add_test(NAME history_test COMMAND history_test)

add_executable(music_test tests/music_test.cpp)
target_link_libraries(music_test PRIVATE potter_core)
add_test(NAME music_test COMMAND music_test)
