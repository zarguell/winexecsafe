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

add_executable(winexecsafe
    main.cpp
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

install(TARGETS winexecsafe
    RUNTIME DESTINATION bin
)

install(FILES config_sample.ini
    DESTINATION .
    RENAME config.ini
)
