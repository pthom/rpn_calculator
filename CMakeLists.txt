cmake_minimum_required(VERSION 3.12)
project(rpn_calculator)
set(CMAKE_CXX_STANDARD 17)

if (NOT TARGET hello_imgui)
    # If CMAKE_CURRENT_LIST_DIR is a symlink, make it a real path
    get_filename_component(current_script_dir_real ${CMAKE_CURRENT_LIST_DIR} REALPATH)

    if (EXISTS ${current_script_dir_real}/../imgui_bundle/external/hello_imgui/hello_imgui)
        message(STATUS "Using hello_imgui from imgui_bundle")
        add_subdirectory(${current_script_dir_real}/../imgui_bundle/external/hello_imgui/hello_imgui hello_imgui)
    endif()
endif()

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
