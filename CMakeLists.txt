cmake_minimum_required(VERSION 3.12)
project(rpn_calculator)
set(CMAKE_CXX_STANDARD 17)

# add_subdirectory(external/hello_imgui)

##########################################################
# Prepare hello_imgui during configure time
##########################################################
# Download hello_imgui if not part of the project
if (NOT TARGET hello_imgui)
    include(FetchContent)
    Set(FETCHCONTENT_QUIET FALSE)
    FetchContent_Declare(hello_imgui GIT_REPOSITORY https://github.com/pthom/hello_imgui.git GIT_TAG master)
    FetchContent_MakeAvailable(hello_imgui)
endif()

hello_imgui_add_app(rpn_calculator
    rpn_calculator.cpp
    rpn_calculator.h
    rpn_calculator_app.cpp
)
