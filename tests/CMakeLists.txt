set(TEST_NAME jxrcodec_tests)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)


add_subdirectory("${PROJECT_SOURCE_DIR}/extern/googletest" "extern/googletest")

set_target_properties(gtest PROPERTIES FOLDER extern)
set_target_properties(gtest_main PROPERTIES FOLDER extern)
set_target_properties(gmock PROPERTIES FOLDER extern)
set_target_properties(gmock_main PROPERTIES FOLDER extern)

set(TEST_SOURCES 
   test_jxrcodec.cpp
)

add_executable(${TEST_NAME} ${TEST_SOURCES})

target_include_directories(${TEST_NAME} PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(${TEST_NAME} jxrcodec gtest gmock gtest_main)

set_target_properties(${TEST_NAME} PROPERTIES FOLDER ${TEST_DIR})

add_test(${TEST_NAME} ${TEST_DIR}/${TEST_NAME})
set_tests_properties(${TEST_NAME} PROPERTIES
    ENVIRONMENT JPEGXR_TEST_DATA=${PROJECT_TEST_DATA_PATH})

if(MSVC)
	target_compile_definitions(${TEST_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()