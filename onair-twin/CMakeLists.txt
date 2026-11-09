cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)
find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs OBS::obs-frontend-api Qt6::Core Qt6::Widgets)
target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)
set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/twin-core.cpp
    src/twin-core.hpp
    src/obs-snapshot.cpp
    src/obs-snapshot.hpp
    src/snapshot-json.cpp
    src/snapshot-json.hpp
    src/twin-report.cpp
    src/twin-report.hpp
    src/twin-dock.cpp
    src/twin-dock.hpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

option(BUILD_TWIN_TESTS "Build the Qt serialization/report tests" OFF)
if(BUILD_TWIN_TESTS)
  add_executable(onair-twin-qt-tests tests/qt-tests.cpp src/twin-core.cpp src/snapshot-json.cpp src/twin-report.cpp)
  target_include_directories(onair-twin-qt-tests PRIVATE src)
  target_compile_features(onair-twin-qt-tests PRIVATE cxx_std_17)
  target_link_libraries(onair-twin-qt-tests PRIVATE Qt6::Core)

  add_executable(
    onair-twin-ui-tests
    tests/ui-tests.cpp
    tests/obs-snapshot-test.cpp
    src/twin-core.cpp
    src/obs-snapshot.cpp
    src/snapshot-json.cpp
    src/twin-report.cpp
    src/twin-dock.cpp
  )
  target_include_directories(onair-twin-ui-tests PRIVATE tests/fake-obs src)
  target_compile_definitions(onair-twin-ui-tests PRIVATE ONAIRTWIN_FAKE_NO_MAIN)
  target_compile_features(onair-twin-ui-tests PRIVATE cxx_std_17)
  target_link_libraries(onair-twin-ui-tests PRIVATE Qt6::Core Qt6::Widgets)
endif()
