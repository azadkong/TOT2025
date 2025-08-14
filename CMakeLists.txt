cmake_minimum_required(VERSION 3.16)
project(GFCEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

# 尝试优先使用 Qt6，找不到则回退 Qt5
find_package(Qt6 COMPONENTS Widgets REQUIRED QUIET)
if(NOT Qt6_FOUND)
  find_package(Qt5 COMPONENTS Widgets REQUIRED)
  set(QT_LIB Qt5::Widgets)
else()
  set(QT_LIB Qt6::Widgets)
endif()

add_executable(GFCEditor
  src/main.cpp
  src/mainwindow.h
  src/mainwindow.cpp
  src/expressparser.h
  src/expressparser.cpp
  src/gfcparser.h
  src/gfcparser.cpp
)

target_include_directories(GFCEditor PRIVATE src)
target_link_libraries(GFCEditor PRIVATE ${QT_LIB})
