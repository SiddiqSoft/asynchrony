if(${${PROJECT_NAME}_BUILD_TESTS})
    set(TESTPROJ ${PROJECT_NAME}_tests)

    set( CMAKE_CXX_STANDARD 23)
    set( CMAKE_CXX_STANDARD_REQUIRED On)
    set( CMAKE_CXX_EXTENSIONS Off)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    
    add_executable(${TESTPROJ})

    target_compile_features(${TESTPROJ} PRIVATE cxx_std_23)
    target_compile_options( ${TESTPROJ}
                            PRIVATE
                            $<$<CXX_COMPILER_ID:MSVC>:/std:c++latest> )

    # ASAN and Coverage only for Debug builds on Linux (not supported on Apple Xcode)
    if(((CMAKE_CXX_COMPILER_ID MATCHES [Cc][Ll][Aa][Nn][Gg]) 
        #OR (CMAKE_CXX_COMPILER_ID MATCHES [Gg][Nn][Uu])
        )
        AND (CMAKE_BUILD_TYPE MATCHES [Dd][Ee][Bb][Uu][Gg])
        #AND NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin"
        )
        # Enabling the sanitizer/leak on GCC causes failures whereas it works on Clang(non-XCODE).
        message(STATUS "  >> Using sanitizers for leak and address on ${CMAKE_CXX_COMPILER_ID} system ${CMAKE_SYSTEM_NAME} - ${CMAKE_SYSTEM_PROCESSOR}...")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,leak -fno-omit-frame-pointer")
        target_link_libraries(${TESTPROJ} PRIVATE -fsanitize=address,leak)
        #message(STATUS "  >> Enable instrumentation for code coverage")
        #target_compile_options(${TESTPROJ} PRIVATE -coverage -O0 -g)
        #target_link_options(${TESTPROJ} PRIVATE -coverage)
    endif()

    if( CMAKE_BUILD_TYPE MATCHES [Dd][Ee][Bb][Uu][Gg])
        message(STATUS "  >> Setting DEBUG macro..")
        target_compile_definitions(${TESTPROJ} PRIVATE DEBUG=1 _DEBUG=1)
    endif()

    # On macOS with Homebrew LLVM, the CURL::libcurl imported target adds the Xcode SDK
    # include path (-isystem .../MacOSX.sdk/usr/include) which conflicts with LLVM's own
    # libc++ headers. The SDK's C headers get found before LLVM's stddef.h wrapper,
    # causing "unknown type name 'size_t'" errors.
    # Fix: Tell CMake that the SDK include path is an implicit system directory so it
    # won't be emitted as an explicit -isystem flag on the command line.
    if(CMAKE_CXX_COMPILER_ID MATCHES [Cc][Ll][Aa][Nn][Gg] AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        execute_process(
            COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE _sdk_path
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(_sdk_path AND EXISTS "${_sdk_path}/usr/include")
            list(APPEND CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES "${_sdk_path}/usr/include")
        endif()
    endif()

    # Our tests depend on this preprocessor definition during CI/test mode to enable
    # access to protected members.
    target_compile_definitions(${TESTPROJ} PRIVATE ${PROJECT_NAME}_TESTING_MODE=1)

    # Dependencies
    CPMAddPackage("gh:google/googletest#v1.17.0")
    CPMAddPackage("gh:nlohmann/json#v3.12.0")
    target_link_libraries(${PROJECT_NAME} INTERFACE nlohmann_json::nlohmann_json)

    target_sources( ${TESTPROJ}
                    PRIVATE
                    ${PROJECT_SOURCE_DIR}/tests/periodic_worker.cpp
                    ${PROJECT_SOURCE_DIR}/tests/roundrobin_pool.cpp
                    ${PROJECT_SOURCE_DIR}/tests/simple_pool.cpp
                    ${PROJECT_SOURCE_DIR}/tests/resource_pool.cpp
                    ${PROJECT_SOURCE_DIR}/tests/simple_worker.cpp)

    # Link dependencies..
    target_link_libraries(${TESTPROJ} PRIVATE
            nlohmann_json::nlohmann_json
            RunOnEnd::RunOnEnd
            GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(${TESTPROJ} XML_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/tests/results")

    include(CTest)
    message(STATUS "  Finished configuring for ${PROJECT_NAME} -- ${PROJECT_NAME}_BUILD_TESTS = ${${PROJECT_NAME}_BUILD_TESTS}")
endif()