if(${${PROJECT_NAME}_BUILD_TESTS})
    set(TESTPROJ ${PROJECT_NAME}_tests)

    set( CMAKE_CXX_STANDARD 23)
    set( CMAKE_CXX_STANDARD_REQUIRED On)
    set( CMAKE_CXX_EXTENSIONS Off)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_ENABLE_C_CLANG_TIDY ON)
    set(CMAKE_ENABLE_CXX_CLANG_TIDY ON)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    
    add_executable(${TESTPROJ})

    target_compile_features(${TESTPROJ} PRIVATE cxx_std_23)
    target_compile_options( ${TESTPROJ}
                            PRIVATE
                            $<$<CXX_COMPILER_ID:MSVC>:/std:c++latest> )

    # Check if mold is available on the system
    find_program(MOLD_LINKER mold)

    if(MOLD_LINKER)
        message(STATUS "  >> Found mold linker, using it for build.")
        # Apply mold as the linker for executables and shared libraries
        add_link_options("-fuse-ld=mold")
    else()
        message(STATUS "  >> mold not found, falling back to default linker.")
    endif()

    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")

    if(CLANG_TIDY_EXE AND CMAKE_BUILD_TYPE MATCHES [Dd][Ee][Bb][Uu][Gg])
        # This runs the analyzer with all default checks enabled
        message(STATUS "  >> Using clang-tidy checks=*")
        set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE} "-checks=*")
    endif()

    # UndefinedBehaviorSanitizer is not available on Windows
    if( CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" )
        message(STATUS "  >> UBSAN is not supported on Windows/MSVC, skipping..")
        message(STATUS "  >> Coverage flags is not supported on Windows/MSVC, skipping..")
        set(DEBUG_COMPILE_FLAGS /Zi /Od)
        set(DEBUG_LINK_FLAGS /DEBUG:FULL)
    else()
        set(ASAN_FLAGS "-fsanitize=address")
        set(UBSAN_FLAGS "-fsanitize=undefined")
        # Coverage Flags is not available on Windows
        set(COVERAGE_FLAGS "-coverage")
        set(OPTIMIZE_FLAGS "-O0")
        set(OTHER_FLAGS "-g")
    endif()

    # LeakSanitizer is not available on Windows and Darwin/macOS with AppleClang
    if( CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" OR CMAKE_SYSTEM_NAME STREQUAL "Windows" )
        message(STATUS "  >> LSAN is not supported on Windows/MSVC and AppleClang, skipping..")
    else()
        set(LSAN_FLAGS "-fsanitize=leak")
    endif()

    message(STATUS "  >> Enable compile options for ${TESTPROJ} -- ${OPTIMIZE_FLAGS} ${OTHER_FLAGS} ${COVERAGE_FLAGS} ${ASAN_FLAGS} ${LSAN_FLAGS} ${UBSAN_FLAGS}")
    target_compile_options(${TESTPROJ} PRIVATE  ${DEBUG_COMPILE_FLAGS}  ${COVERAGE_FLAGS} ${OPTIMIZE_FLAGS} ${OTHER_FLAGS} ${ASAN_FLAGS} ${LSAN_FLAGS} ${UBSAN_FLAGS})
    target_link_options(${TESTPROJ} PRIVATE ${DEBUG_LINK_FLAGS}  ${COVERAGE_FLAGS} ${OPTIMIZE_FLAGS} ${OTHER_FLAGS} ${ASAN_FLAGS} ${LSAN_FLAGS} ${UBSAN_FLAGS})

    if( CMAKE_BUILD_TYPE MATCHES [Dd][Ee][Bb][Uu][Gg])
        message(STATUS "  >> Setting DEBUG macro..")
        target_compile_definitions(${TESTPROJ} PRIVATE DEBUG=1 _DEBUG=1)
    endif()

    # Our tests depend on this preprocessor definition during CI/test mode to enable
    # access to protected members.
    target_compile_definitions(${TESTPROJ} PRIVATE ${PROJECT_NAME}_TESTING_MODE=1)

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

    # Dependencies
    CPMAddPackage("gh:google/googletest#v1.17.0")
    CPMAddPackage("gh:nlohmann/json#v3.12.0")

    target_sources( ${TESTPROJ}
                    PRIVATE
                    ${PROJECT_SOURCE_DIR}/tests/periodic_worker.cpp
                    ${PROJECT_SOURCE_DIR}/tests/callback_stress_tests.cpp
                    ${PROJECT_SOURCE_DIR}/tests/additional_bugs.cpp
                    ${PROJECT_SOURCE_DIR}/tests/bug_tests.cpp
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
