add_library(corvuscore_c_api STATIC src/bridge.cpp)
target_compile_features(corvuscore_c_api PUBLIC cxx_std_20)
target_include_directories(corvuscore_c_api PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(corvuscore_c_api PUBLIC corvuscore)
target_compile_options(corvuscore_c_api PRIVATE ${CORVUSCORE_WARNING_OPTIONS})
target_compile_definitions(corvuscore_c_api PRIVATE CORVUSCORE_VERSION="${PROJECT_VERSION}")
set_target_properties(corvuscore_c_api PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(BUILD_TESTING)
    add_executable(corvuscore_c_api_tests tests/bridge_test.cpp)
    target_link_libraries(corvuscore_c_api_tests PRIVATE corvuscore_c_api)
    target_include_directories(corvuscore_c_api_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/cores/common/tests/support)
    target_compile_options(corvuscore_c_api_tests PRIVATE ${CORVUSCORE_WARNING_OPTIONS})
    target_compile_definitions(corvuscore_c_api_tests PRIVATE CORVUSCORE_VERSION="${PROJECT_VERSION}")
    add_test(NAME corvuscore_c_api COMMAND corvuscore_c_api_tests)

    add_executable(corvuscore_remote_link_tests tests/remote_link_test.cpp)
    target_link_libraries(corvuscore_remote_link_tests PRIVATE corvuscore_c_api)
    target_include_directories(corvuscore_remote_link_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/cores/common/tests/support)
    target_compile_options(corvuscore_remote_link_tests PRIVATE ${CORVUSCORE_WARNING_OPTIONS})
    add_test(NAME corvuscore_remote_link COMMAND corvuscore_remote_link_tests)
endif()
