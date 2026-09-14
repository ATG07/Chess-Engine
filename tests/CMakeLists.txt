include(FetchContent)

# Prefer an existing GoogleTest installation. Fetch it only when one is not
# available so distro/package-manager builds remain offline and reproducible.
find_package(GTest CONFIG QUIET)

if(NOT GTest_FOUND)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    # Prevent GoogleTest from overriding the parent project's CRT settings on
    # Windows.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

include(GoogleTest)

add_executable(chess_unit_tests
    types_test.cpp
    board_test.cpp
)
target_link_libraries(chess_unit_tests PRIVATE chess_engine GTest::gtest_main)
gtest_discover_tests(chess_unit_tests)

add_executable(chess_perft_tests perft_test.cpp)
target_link_libraries(chess_perft_tests PRIVATE chess_engine GTest::gtest_main)
gtest_discover_tests(chess_perft_tests PROPERTIES LABELS perft)
