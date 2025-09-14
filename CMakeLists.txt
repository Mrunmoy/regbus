cmake_minimum_required(VERSION 3.16)
project(regbus LANGUAGES CXX)

add_library(regbus INTERFACE)
add_library(regbus::regbus ALIAS regbus)

target_include_directories(regbus INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features(regbus INTERFACE cxx_std_17)

# Nice defaults for consumers
target_compile_options(regbus INTERFACE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

# ---------- Options ----------
option(REGBUS_BUILD_TESTS "Build regbus unit tests" OFF)      # default OFF so submodule users don't pull gtest
option(REGBUS_BUILD_EXAMPLES "Build regbus examples" OFF)

# ---------- Tests ----------
if (REGBUS_BUILD_TESTS)
  include(CTest)
  if (BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(test_dbreg tests/test_dbreg.cpp)
    target_link_libraries(test_dbreg gtest gtest_main)
    target_link_libraries(test_dbreg regbus)
    add_test(NAME test_dbreg COMMAND test_dbreg)

    add_executable(test_cmdreg tests/test_cmdreg.cpp)
    target_link_libraries(test_cmdreg gtest gtest_main)
    target_link_libraries(test_cmdreg regbus)
    add_test(NAME test_cmdreg COMMAND test_cmdreg)

    add_executable(test_registry tests/test_registry.cpp)
    target_link_libraries(test_registry gtest gtest_main)
    target_link_libraries(test_registry regbus)
    add_test(NAME test_registry COMMAND test_registry)
  endif()
endif()

# ---------- Examples ----------
if (REGBUS_BUILD_EXAMPLES)
  add_executable(example_minimal examples/minimal_registry.cpp)
  target_link_libraries(example_minimal regbus)
endif()

# ---------- (Optional) install for package managers ----------
include(GNUInstallDirs)
install(TARGETS regbus EXPORT regbusTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT regbusTargets NAMESPACE regbus:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/regbus)
