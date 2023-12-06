cmake_minimum_required(VERSION 3.12)
project(rpn_calculator)
set(CMAKE_CXX_STANDARD 17)


##########################################################
# Prepare imgui_bundle during configure time
##########################################################
# Download imgui_bundle if not part of the project
if (NOT TARGET imgui_bundle)
    include(FetchContent)
    Set(FETCHCONTENT_QUIET FALSE)
    FetchContent_Declare(imgui_bundle GIT_REPOSITORY https://github.com/pthom/imgui_bundle.git GIT_TAG main)
    FetchContent_MakeAvailable(imgui_bundle)
    # Uncomment the next line if you which to also automatically fetch and compile OpenCV for immvision support
    # set(IMMVISION_FETCH_OPENCV ON)
endif()

imgui_bundle_add_app(rpn_calculator rpn_calculator.cpp)
target_compile_options(rpn_calculator PUBLIC -fsanitize=address)
target_link_options(rpn_calculator PUBLIC -fsanitize=address)
