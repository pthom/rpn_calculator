set(CMAKE_CXX_STANDARD 20)
imgui_bundle_add_app(rpn_calculator rpn_calculator.cpp)
target_compile_options(rpn_calculator PUBLIC -fsanitize=address)
target_link_options(rpn_calculator PUBLIC -fsanitize=address)
