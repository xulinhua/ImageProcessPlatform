cmake_minimum_required(VERSION 3.16)
project(RemoteClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(OpenSSL REQUIRED)
find_package(X11 REQUIRED)
find_package(JPEG REQUIRED)
find_package(Qt5 COMPONENTS Widgets REQUIRED)

find_library(XTEST_LIB Xtst REQUIRED)
find_library(XEXT_LIB Xext REQUIRED)

add_executable(RemoteClient
    src/main.cpp
    src/Identity.cpp
    src/TlsLink.cpp
    src/Desk.cpp
    src/Beacon.cpp
    src/AgentWindow.cpp
    src/AgentApp.cpp
    src/FileBox.cpp
    src/protocol/Frame.cpp
    src/protocol/Messages.cpp
    remoteclient.qrc
)

target_include_directories(RemoteClient PRIVATE
    src
    src/protocol
    ${JPEG_INCLUDE_DIR}
)

target_link_libraries(RemoteClient PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    ${X11_LIBRARIES}
    ${XEXT_LIB}
    ${XTEST_LIB}
    ${JPEG_LIBRARIES}
    Qt5::Widgets
    pthread
)
