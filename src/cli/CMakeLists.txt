add_library(sheafcli STATIC cli.cpp)
target_include_directories(sheafcli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sheafcli PUBLIC sheafcore)
