cmake_minimum_required(VERSION 3.16)
project(regbus VERSION 0.1.0 LANGUAGES CXX)

add_library(regbus INTERFACE)
add_library(regbus::regbus ALIAS regbus)

target_include_directories(regbus INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features(regbus INTERFACE cxx_std_17)
target_compile_options(regbus INTERFACE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

option(REGBUS_BUILD_TESTS "Build regbus unit tests" ON)
option(REGBUS_BUILD_EXAMPLES "Build regbus examples" ON)

# -------- Tests --------
if (REGBUS_BUILD_TESTS)
  include(CTest)
  if (BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(test_dbreg tests/test_dbreg.cpp)
    target_link_libraries(test_dbreg gtest gtest_main regbus)
    add_test(NAME test_dbreg COMMAND test_dbreg)

    add_executable(test_cmdreg tests/test_cmdreg.cpp)
    target_link_libraries(test_cmdreg gtest gtest_main regbus)
    add_test(NAME test_cmdreg COMMAND test_cmdreg)

    add_executable(test_registry tests/test_registry.cpp)
    target_link_libraries(test_registry gtest gtest_main regbus)
    add_test(NAME test_registry COMMAND test_registry)
  endif()
endif()

# -------- Example --------
if (REGBUS_BUILD_EXAMPLES)
  add_executable(example_minimal examples/minimal_registry.cpp)
  target_link_libraries(example_minimal regbus)
endif()

# -------- Install (optional but nice) --------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS regbus EXPORT regbusTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT regbusTargets NAMESPACE regbus:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/regbus)

# Allow consumers to require a minimum regbus version
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/regbusConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/regbusConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/regbus)
