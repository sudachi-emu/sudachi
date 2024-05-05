add_library(tracy STATIC public/TracyClient.cpp)
target_include_directories(tracy PUBLIC public)
target_compile_definitions(tracy PRIVATE TRACY_ENABLE TRACY_ON_DEMAND)
