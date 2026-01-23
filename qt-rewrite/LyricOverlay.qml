import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.settings
import QtQuick.Window

Window {
    id: overlay
    width: settings.winWidth
    height: settings.winHeight
    x: settings.winX
    y: settings.winY
    visible: false
    visibility: Window.Windowed
    color: "transparent"
    flags: Qt.FramelessWindowHint | (settings.stayOnTop ? Qt.WindowStaysOnTopHint : 0) | (locked ? Qt.WindowTransparentForInput : 0)
    transientParent: null
    minimumWidth: 600
    minimumHeight: 200

    title: "Lyric Overlay"

    property bool locked: settings.locked
    property int fontSize: settings.fontSize
    property color baseTextColor: theme === "dark" ? "#e6e6e6" : "#383838"
    property color secondaryTextColor: theme === "dark" ? "#ffffffea" : "#282828ae"
    property color highlightColor: settings.highlightColor
    property string theme: settings.theme

    Settings {
        id: settings
        category: "lyricOverlay"
        property int winWidth: 800
        property int winHeight: 200
        property int winX: 240
        property int winY: 240
        property bool locked: false
        property int fontSize: 24
        property color highlightColor: "#1db954"
        property string theme: "dark"
        property bool stayOnTop: true
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        radius: 14
    }

    // 顶部控制栏（锁定时隐藏）
    Row {
        id: controlBar
        spacing: 12
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            leftMargin: 12
            rightMargin: 12
            topMargin: 8
        }
        visible: !locked

        Row {
            spacing: 8
            Button {
                text: "-"
                onClicked: {
                    if (fontSize > 12) {
                        fontSize -= 2
                        settings.fontSize = fontSize
                    }
                }
            }
            Button {
                text: "+"
                onClicked: {
                    if (fontSize < 48) {
                        fontSize += 2
                        settings.fontSize = fontSize
                    }
                }
            }
            Text {
                text: musicController ? musicController.currentSongTitle : ""
                color: baseTextColor
                font.pixelSize: 14
                elide: Text.ElideRight
            }
        }

        Item { Layout.fillWidth: true; width: 1; height: 1 }

        Row {
            spacing: 8
            Button {
                text: theme === "dark" ? "暗" : "亮"
                onClicked: {
                    theme = (theme === "dark") ? "light" : "dark"
                    settings.theme = theme
                }
            }
            Button {
                text: settings.stayOnTop ? "置顶" : "取消置顶"
                onClicked: settings.stayOnTop = !settings.stayOnTop
            }
            Button {
                text: locked ? "解锁" : "锁定"
                onClicked: {
                    locked = !locked
                    settings.locked = locked
                }
            }
            Button {
                text: "关闭"
                onClicked: overlay.visible = false
            }
        }
    }

    // 播放控制（居中，锁定时隐藏）
    Row {
        spacing: 12
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: controlBar.bottom
        anchors.topMargin: 6
        visible: !locked

        Button {
            text: "上一首"
            onClicked: if (musicController) musicController.playIndex(Math.max(0, (musicController.currentLyricIndex - 1)))
        }
        Button {
            text: musicController && musicController.playing ? "暂停" : "播放"
            onClicked: if (musicController) musicController.playing ? musicController.pause() : musicController.resume()
        }
        Button {
            text: "下一首"
            onClicked: if (musicController) musicController.playIndex((musicController.currentLyricIndex + 1))
        }
    }

    // 歌词显示
    ListView {
        id: lyricList
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            top: locked ? parent.top : controlBar.bottom
            leftMargin: 12
            rightMargin: 12
            bottomMargin: 10
            topMargin: locked ? 10 : 6
        }
        model: musicController ? musicController.lyricModel : null
        clip: true
        spacing: 4
        interactive: false

        delegate: Column {
            width: lyricList.width
            spacing: 2

            property bool current: musicController && index === musicController.currentLyricIndex
            property real startMs: timeMs
            property real nextMs: {
                let i = index + 1
                if (!musicController || i >= lyricList.count) return musicController ? musicController.durationMs : 0
                // next line's time isn't directly readable here; fallback to controller property
                return musicController.currentLyricNextMs
            }
            property real posMs: musicController ? musicController.positionMs : 0
            property real progress: {
                if (!current) return 0
                const dur = Math.max(1, nextMs - startMs)
                const p = (posMs - startMs) / dur
                return Math.max(0, Math.min(1, p))
            }

            // 主歌词文本
            Text {
                width: lyricList.width
                text: model.text
                color: current ? baseTextColor : secondaryTextColor
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: fontSize
                font.weight: current ? Font.DemiBold : Font.Normal
                opacity: current ? 1.0 : 0.6
                scale: current ? 1.05 : 1.0
            }

            // 进度条替代渐变高亮
            Rectangle {
                width: Math.round(progress * (lyricList.width * 0.6))
                height: 3
                color: highlightColor
                visible: current
                anchors.horizontalCenter: parent.horizontalCenter
                radius: 2
            }
        }

        Connections {
            target: musicController
            function onCurrentLyricIndexChanged() {
                if (!musicController) return
                if (musicController.currentLyricIndex >= 0)
                    lyricList.positionViewAtIndex(musicController.currentLyricIndex, ListView.Center)
            }
        }
    }

    // 拖动移动（非锁定）
    MouseArea {
        anchors.fill: parent
        enabled: !locked
        property point lastPos: Qt.point(0, 0)
        onPressed: lastPos = Qt.point(mouse.x, mouse.y)
        onPositionChanged: {
            const dx = mouse.x - lastPos.x
            const dy = mouse.y - lastPos.y
            overlay.x += dx
            overlay.y += dy
            lastPos = Qt.point(mouse.x, mouse.y)
        }
        onReleased: {
            settings.winX = overlay.x
            settings.winY = overlay.y
        }
    }
    Item {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 16
        height: 16
        visible: !locked
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.SizeFDiagCursor
            property real lastX: 0
            property real lastY: 0
            onPressed: {
                lastX = mouse.x
                lastY = mouse.y
            }
            onPositionChanged: {
                var dx = mouse.x - lastX
                var dy = mouse.y - lastY
                overlay.width = Math.max(overlay.minimumWidth, overlay.width + dx)
                overlay.height = Math.max(overlay.minimumHeight, overlay.height + dy)
                lastX = mouse.x
                lastY = mouse.y
            }
            onReleased: {
                settings.winWidth = overlay.width
                settings.winHeight = overlay.height
            }
        }
    }

    Component.onCompleted: {
        // 安全初始值
        if (fontSize <= 0) fontSize = 24
    }
    onWidthChanged: settings.winWidth = width
    onHeightChanged: settings.winHeight = height
}
