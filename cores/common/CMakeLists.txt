add_library(corvuscore_common STATIC
    src/core.cpp
    src/remote_link.cpp
    src/sha256.cpp
)
target_include_directories(corvuscore_common PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
# Le câble distant attend son partenaire sur un autre fil.
find_package(Threads REQUIRED)
target_link_libraries(corvuscore_common PUBLIC corvuscore_options Threads::Threads)
set_target_properties(corvuscore_common PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(BUILD_TESTING)
    add_executable(corvuscore_common_tests tests/sha256_test.cpp)
    target_include_directories(corvuscore_common_tests PRIVATE tests/support)
    target_link_libraries(corvuscore_common_tests PRIVATE corvuscore_common)
    add_test(NAME corvuscore_common COMMAND corvuscore_common_tests)

    add_executable(corvuscore_binary_io_tests tests/binary_io_test.cpp)
    target_include_directories(corvuscore_binary_io_tests PRIVATE tests/support)
    target_link_libraries(corvuscore_binary_io_tests PRIVATE corvuscore_common)
    add_test(NAME corvuscore_binary_io COMMAND corvuscore_binary_io_tests)
endif()
