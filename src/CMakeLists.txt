cmake_minimum_required (VERSION 3.14)


# Add source to this project's executable.
add_library (SortOptimTask "hash_map.h" "hash_map.cpp" )
set_target_properties(SortOptimTask PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries (SortOptimTask)

# TODO: Add tests and install targets if needed.
