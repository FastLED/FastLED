get_filename_component(fastled_root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
get_filename_component(fastled_component "${fastled_root}" NAME)
idf_component_register(SRCS "main.cpp" REQUIRES "${fastled_component}")
