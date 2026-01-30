cmake_minimum_required(VERSION 3.15)
project(winexecsafe VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    set(CMAKE_CXX_FLAGS "/W4 /WX-")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /RTC1")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /GL /DNDEBUG")
    
    add_definitions(-DUNICODE -D_UNICODE)
else()
    message(FATAL_ERROR "winexecsafe is a Windows-only application")
endif()

# Main executable
add_executable(winexecsafe
    main.cpp
    src/common.cpp
    src/config.cpp
    src/appcontainer.cpp
    src/permissions.cpp
    src/capabilities.cpp
    src/process.cpp
)

if(WIN32)
    target_link_libraries(winexecsafe
        userenv.lib
        advapi32.lib
    )
endif()

set_target_properties(winexecsafe PROPERTIES
    OUTPUT_NAME "winexecsafe"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Enable testing
enable_testing()

# Test executable (simple assert-based unit tests)
add_executable(winexecsafe_tests
    tests/unit_tests.cpp
    src/common.cpp
    src/config.cpp
)

if(WIN32)
    target_link_libraries(winexecsafe_tests
        userenv.lib
        advapi32.lib
    )
endif()

set_target_properties(winexecsafe_tests PROPERTIES
    OUTPUT_NAME "winexecsafe_tests"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Add test
add_test(NAME UnitTests COMMAND winexecsafe_tests)

install(TARGETS winexecsafe
    RUNTIME DESTINATION bin
)

install(FILES config_sample.ini
    DESTINATION .
    RENAME config.ini
)