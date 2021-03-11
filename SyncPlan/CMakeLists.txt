project(sync_plan)

#debug mode
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g2 -ggdb")

include_directories(includes /usr/local/cJSON)
aux_source_directory(src SRC_LIST)
add_executable(main ${SRC_LIST})
target_link_libraries(main cjson)
