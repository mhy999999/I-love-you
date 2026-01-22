cmake_minimum_required(VERSION 3.16)

project(qt_rewrite VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
	add_compile_options(/Zc:__cplusplus)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Network Multimedia)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appqtrewrite
	main.cpp
	src/logger.cpp
	src/http_client.cpp
	src/json_utils.cpp
	src/netease_provider.cpp
	src/provider_manager.cpp
	src/song_list_model.cpp
	src/music_controller.cpp
)

qt_add_qml_module(appqtrewrite
    URI qtrewrite
    QML_FILES
        Main.qml
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appqtrewrite PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appmusci
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_include_directories(appqtrewrite
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(appqtrewrite
	PRIVATE Qt6::Quick Qt6::Network Qt6::Multimedia
)

include(GNUInstallDirs)
install(TARGETS appqtrewrite
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
