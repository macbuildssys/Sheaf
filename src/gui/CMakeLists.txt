add_library(sheafgui STATIC
    guiutil.cpp
    thumbnailloader.cpp
    documentmodel.cpp
    carddelegate.cpp
    sidebar.cpp
    detailspanel.cpp
    tagdialog.cpp
    groupsdialog.cpp
    settingsdialog.cpp
    quickopen.cpp
    singleinstance.cpp
    mainwindow.cpp
    guimain.cpp
    resources.qrc
)
target_include_directories(sheafgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sheafgui PUBLIC sheafcore Qt6::Widgets Qt6::Network)
set_target_properties(sheafgui PROPERTIES POSITION_INDEPENDENT_CODE ON)
