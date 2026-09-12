cmake_minimum_required(VERSION 3.24)
project(AdventureLandReferenceOSDesktop VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
find_package(SQLite3 REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
if(NOT WIN32)
  find_package(CURL REQUIRED)
endif()

# Embedded JS evaluator for the fixed Adventure Land source allowlist.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(QJS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_CLI_STATIC OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_CLI_WITH_MIMALLOC OFF CACHE BOOL "" FORCE)
FetchContent_Declare(quickjs
  URL https://github.com/quickjs-ng/quickjs/archive/refs/tags/v0.15.1.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(quickjs)

# Immediate-mode native UI: only visible rows/widgets are drawn.
FetchContent_Declare(imgui
  URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(imgui)
add_library(al_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp)
target_include_directories(al_imgui PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${imgui_SOURCE_DIR}/misc/cpp)
target_link_libraries(al_imgui PUBLIC glfw OpenGL::GL)

set(AL_CORE_SOURCES
  src/database.cpp
  src/analysis_engine.cpp
  src/sha256.cpp)
set(AL_APP_SOURCES
  src/main.cpp
  src/app.cpp
  src/source_sync.cpp
  ${AL_CORE_SOURCES})
if(WIN32)
  list(APPEND AL_APP_SOURCES src/http_windows.cpp)
else()
  list(APPEND AL_APP_SOURCES src/http_linux.cpp)
endif()

add_executable(adventure-land-reference-os ${AL_APP_SOURCES})
target_include_directories(adventure-land-reference-os PRIVATE src)
target_link_libraries(adventure-land-reference-os PRIVATE
  SQLite3::SQLite3
  nlohmann_json::nlohmann_json
  qjs
  al_imgui)
if(WIN32)
  target_compile_definitions(adventure-land-reference-os PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(adventure-land-reference-os PRIVATE winhttp)
else()
  target_link_libraries(adventure-land-reference-os PRIVATE CURL::libcurl ${CMAKE_DL_LIBS})
endif()
if(MSVC)
  target_compile_options(adventure-land-reference-os PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
  target_compile_options(adventure-land-reference-os PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS adventure-land-reference-os RUNTIME DESTINATION .)
install(DIRECTORY resources DESTINATION .)

option(AL_BUILD_TESTS "Build native core tests" ON)
if(AL_BUILD_TESTS)
  add_executable(al-core-test tests/core_test.cpp ${AL_CORE_SOURCES})
  target_include_directories(al-core-test PRIVATE src)
  target_link_libraries(al-core-test PRIVATE SQLite3::SQLite3 nlohmann_json::nlohmann_json)
  if(MSVC)
    target_compile_options(al-core-test PRIVATE /W4 /permissive- /EHsc /utf-8)
  else()
    target_compile_options(al-core-test PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()
