set(CORE_SOURCES
    core/types.cpp
    core/paths.cpp
    core/pathutil.cpp
    core/xbel.cpp
    core/groups.cpp
    core/database.cpp
    core/search.cpp
    core/sorting.cpp
    core/progress.cpp
    core/scanner.cpp
    core/index.cpp
    core/settings.cpp
    core/launcher.cpp
    core/thumbnails.cpp
    core/thingyimport.cpp
)

add_library(sheafcore STATIC ${CORE_SOURCES})
target_include_directories(sheafcore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(sheafcore PUBLIC
    Qt6::Core Qt6::Gui Qt6::Sql Qt6::Concurrent Qt6::DBus)
if(SHEAF_HAVE_GIO)
    target_link_libraries(sheafcore PUBLIC PkgConfig::GIO)
endif()
set_target_properties(sheafcore PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_subdirectory(cli)
add_subdirectory(gui)

add_executable(sheaf main.cpp)
target_link_libraries(sheaf PRIVATE sheafgui sheafcli)
install(TARGETS sheaf RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
