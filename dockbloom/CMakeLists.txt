cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)
project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use OBS frontend API" ON)
option(ENABLE_QT "Use Qt" ON)
include(compilerconfig)
include(defaults)
include(helpers)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
add_library(${CMAKE_PROJECT_NAME} MODULE)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src third_party/obs-browser)
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
    src/workspace-config.cpp
    src/workspace-config.hpp
    src/workspace.cpp
    src/workspace.hpp
    src/workspace-ui.cpp
    src/workspace-ui.hpp
)
set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

option(BUILD_DOCKBLOOM_TESTS "Build fake-host tests and the disposable real-OBS test driver" OFF)
if(BUILD_DOCKBLOOM_TESTS)
  find_package(Qt6 REQUIRED COMPONENTS Network)
  add_executable(dockbloom-ui-tests tests/ui-tests.cpp src/workspace-config.cpp src/workspace.cpp src/workspace-ui.cpp)
  target_include_directories(dockbloom-ui-tests PRIVATE src)
  target_compile_features(dockbloom-ui-tests PRIVATE cxx_std_17)
  target_link_libraries(dockbloom-ui-tests PRIVATE Qt6::Core Qt6::Widgets)

  add_library(obs-dockbloom-test-driver MODULE tests/real-obs-driver.cpp)
  target_link_libraries(
    obs-dockbloom-test-driver
    PRIVATE OBS::libobs OBS::obs-frontend-api Qt6::Core Qt6::Widgets Qt6::Network
  )
  target_compile_features(obs-dockbloom-test-driver PRIVATE cxx_std_17)
  set_target_properties(obs-dockbloom-test-driver PROPERTIES PREFIX "")
endif()
