cmake_minimum_required(VERSION 3.20)
project(TypeBloom VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static MSVC runtime: the portable EXE does not need a separate VC++ runtime.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_compile_options(/W4 /permissive- /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_executable(meter_test tests/meter_test.cpp)
target_include_directories(meter_test PRIVATE src)
add_test(NAME meter COMMAND meter_test)

if(WIN32)
  enable_language(RC)
  add_executable(TypeBloom WIN32 src/main.cpp assets/typebloom.rc)
  target_include_directories(TypeBloom PRIVATE src assets)
  if(MSVC)
    # The checked-in resource contains our per-monitor-DPI/asInvoker manifest.
    target_link_options(TypeBloom PRIVATE /MANIFEST:NO)
  endif()
  target_compile_definitions(
    TypeBloom
    PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00 WINVER=0x0A00
  )
  target_link_libraries(
    TypeBloom
    PRIVATE gdiplus user32 gdi32 advapi32 wtsapi32
  )
  if(MINGW)
    target_link_options(TypeBloom PRIVATE -municode -static)
  endif()
  add_executable(typebloom_test_target WIN32 tests/windows_target.cpp)
  target_compile_definitions(typebloom_test_target PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  if(MINGW)
    target_link_options(typebloom_test_target PRIVATE -municode -static)
  endif()
endif()
