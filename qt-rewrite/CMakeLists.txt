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

find_package(Qt6 REQUIRED COMPONENTS Quick Network Multimedia Svg QuickControls2)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appqtrewrite
	main.cpp
	src/logger.cpp
	src/http_client.cpp
	src/json_utils.cpp
	src/disk_cache.cpp
	src/netease_provider.cpp
	src/gdstudio_provider.cpp
	src/provider_manager.cpp
	src/song_list_model.cpp
	src/lyric_list_model.cpp
	src/music_controller.cpp
	src/provider.h
)

qt_add_qml_module(appqtrewrite
    URI qtrewrite
    QML_FILES
        Main.qml
        LyricOverlay.qml
        LoginPopup.qml
        UserMenuPopup.qml
    RESOURCES
        "ui-asset/black-backgroud/上一首.svg"
        "ui-asset/black-backgroud/下一首.svg"
        "ui-asset/black-backgroud/停止.svg"
        "ui-asset/black-backgroud/播放.svg"
        "ui-asset/black-backgroud/暂停.svg"
        "ui-asset/black-backgroud/静音.svg"
        "ui-asset/black-backgroud/音量.svg"
        "ui-asset/black-backgroud/声音.svg"
        "ui-asset/black-backgroud/声音静音.svg"
        "ui-asset/black-backgroud/歌词.svg"
        "ui-asset/black-backgroud/click_back/歌词_clickBack.svg"
        "ui-asset/black-backgroud/列表2.svg"
        "ui-asset/black-backgroud/设置.svg"
        "ui-asset/black-backgroud/music_playerModes/单曲循环.svg"
        "ui-asset/black-backgroud/music_playerModes/顺序.svg"
        "ui-asset/black-backgroud/music_playerModes/随机.svg"
        "ui-asset/black-backgroud/music_playerModes/心动模式.svg"
        "ui-asset/black-backgroud/music_playerModes/循环.svg"
        "ui-asset/black-backgroud/login/网易云音乐.svg"
        "ui-asset/black-backgroud/login/网易邮箱.svg"
        "ui-asset/black-backgroud/login/二维码.svg"
        "ui-asset/black-backgroud/login/240显示、可见.svg"
        "ui-asset/black-backgroud/login/241隐藏、不可见.svg"
        "ui-asset/black-backgroud/login/登录.svg"
        "ui-asset/LyricsFloatingWindow/播放.svg"
        "ui-asset/LyricsFloatingWindow/暂停.svg"
        "ui-asset/LyricsFloatingWindow/上一首.svg"
        "ui-asset/LyricsFloatingWindow/下一首.svg"
        "ui-asset/LyricsFloatingWindow/歌词调整 0.5秒.svg"
        "ui-asset/LyricsFloatingWindow/歌词调整-0.5秒.svg"
        "ui-asset/LyricsFloatingWindow/锁定.svg"
        "ui-asset/LyricsFloatingWindow/设置.svg"
        "ui-asset/LyricsFloatingWindow/关闭.svg"
        "ui-asset/LyricsFloatingWindow/InSeting/white/更多.svg"
        "ui-asset/LyricsFloatingWindow/InSeting/white/配色.svg"
        "ui-asset/LyricsFloatingWindow/InSeting/white/T.svg"
        "ui-asset/white-backgroud/music_playerModes/单曲循环.svg"
        "ui-asset/white-backgroud/music_playerModes/顺序.svg"
        "ui-asset/white-backgroud/music_playerModes/随机.svg"
        "ui-asset/white-backgroud/music_playerModes/心动模式.svg"
        "ui-asset/white-backgroud/music_playerModes/循环.svg"
        "ui-asset/black-backgroud/删除.svg"
    NO_CACHEGEN
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
	PRIVATE Qt6::Quick Qt6::Network Qt6::Multimedia Qt6::Svg Qt6::QuickControls2
)

if (MSVC)
	target_compile_options(appqtrewrite PRIVATE /wd4828)
endif()

include(GNUInstallDirs)
install(TARGETS appqtrewrite
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
