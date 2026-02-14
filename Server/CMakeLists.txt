cmake_minimum_required(VERSION 3.16)
project(RemoteControlServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

set(SOURCES
    src/main.cpp
    src/websocket_server.cpp
    src/keyboard_simulator.cpp
    src/logger.cpp
)

if(NOT APPLE)
    list(APPEND SOURCES src/sha1.cpp)
endif()
if(WIN32)
    list(APPEND SOURCES src/tray_win.cpp)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/app.ico)
        list(APPEND SOURCES src/app.rc)
    endif()
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} Threads::Threads)

if(APPLE)
    find_library(COREGRAPHICS_LIBRARY CoreGraphics)
    find_library(CARBON_LIBRARY Carbon)
    find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices)
    find_library(SECURITY_LIBRARY Security)
    target_link_libraries(${PROJECT_NAME}
        ${COREGRAPHICS_LIBRARY}
        ${CARBON_LIBRARY}
        ${APPLICATIONSERVICES_LIBRARY}
        ${SECURITY_LIBRARY}
    )
    target_include_directories(${PROJECT_NAME} PRIVATE
        /System/Library/Frameworks/CoreGraphics.framework/Headers
        /System/Library/Frameworks/Carbon.framework/Headers
        /System/Library/Frameworks/ApplicationServices.framework/Headers
    )
elseif(UNIX)
    find_package(X11 REQUIRED)
    find_library(X11_XTEST Xtst)
    target_link_libraries(${PROJECT_NAME}
        ${X11_LIBRARIES}
        ${X11_XTEST}
        pthread
    )
    target_include_directories(${PROJECT_NAME} PRIVATE ${X11_INCLUDE_DIR})
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
endif()
