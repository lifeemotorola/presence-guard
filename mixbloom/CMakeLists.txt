cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)
project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use OBS frontend API" ON)
option(ENABLE_QT "Use Qt" ON)
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
    src/mix-core.cpp
    src/mix-core.hpp
    src/obs-mixer.cpp
    src/obs-mixer.hpp
    src/mix-dock.cpp
    src/mix-dock.hpp
    src/mix-help.cpp
    src/mix-help.hpp
    src/mix-help.qrc
)
set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

option(BUILD_MIXBLOOM_TESTS "Build the fake-OBS Qt dock test" OFF)
if(BUILD_MIXBLOOM_TESTS)
  add_executable(
    mixbloom-ui-tests
    tests/ui-tests.cpp
    tests/fake-obs.cpp
    src/mix-core.cpp
    src/obs-mixer.cpp
    src/mix-dock.cpp
    src/mix-help.cpp
    src/mix-help.qrc
  )
  set_target_properties(mixbloom-ui-tests PROPERTIES AUTORCC ON)
  target_include_directories(mixbloom-ui-tests PRIVATE tests/fake-obs src tests)
  target_compile_features(mixbloom-ui-tests PRIVATE cxx_std_17)
  target_link_libraries(mixbloom-ui-tests PRIVATE Qt6::Core Qt6::Widgets)
endif()
