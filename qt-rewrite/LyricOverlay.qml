import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import QtQuick.Window

Window {
    id: overlay
    width: settings.winWidth
    height: settings.winHeight
    x: settings.winX
    y: settings.winY
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | (settings.stayOnTop ? Qt.WindowStaysOnTopHint : 0) | (settings.locked ? Qt.WindowTransparentForInput : 0)
    transientParent: null
    minimumWidth: 600
    minimumHeight: 200

    title: "Lyric Overlay"

    property bool dimmed: false
    property bool showControls: false
    property bool controlsPinned: false
    property int fontSize: settings.fontSize
    property int controlIconSize: 22
    property color baseTextColor: theme === "dark" ? "#e6e6e6" : "#383838"
    property color secondaryTextColor: theme === "dark" ? "#ffffffea" : "#282828ae"
    property color highlightColor: settings.highlightColor
    property string theme: settings.theme
    property url iconPrev: Qt.resolvedUrl("ui-asset/LyricsFloatingWindow/上一首.svg")
    property url iconNext: Qt.resolvedUrl("ui-asset/LyricsFloatingWindow/下一首.svg")
    property url iconPlay: Qt.resolvedUrl("ui-asset/LyricsFloatingWindow/播放.svg")
    property url iconPause: Qt.resolvedUrl("ui-asset/LyricsFloatingWindow/暂停.svg")
    property url iconLyricOffsetPlus: Qt.resolvedUrl("ui-asset/LyricsFloatingWindow/歌词调整 0.5秒.svg")
    property url iconLyricOffsetMinus: Qt.resolvedUrl("ui-asset/LyricsFloatingWindow/歌词调整-0.5秒.svg")
    property url iconLock: Qt.resolvedUrl("ui-asset/LyricsFloatingWindow/锁定.svg")
    property url iconUnlock: Qt.resolvedUrl("ui-asset/LyricsFloatingWindow/解锁.svg")
    property url iconSettings: Qt.resolvedUrl("ui-asset/LyricsFloatingWindow/设置.svg")
    property url iconClose: Qt.resolvedUrl("ui-asset/LyricsFloatingWindow/关闭.svg")
    property url iconSettingsMore: Qt.resolvedUrl("ui-asset/LyricsFloatingWindow/InSeting/white/更多.svg")
    property url iconSettingsPalette: Qt.resolvedUrl("ui-asset/LyricsFloatingWindow/InSeting/white/配色.svg")
    property url iconSettingsT: Qt.resolvedUrl("ui-asset/LyricsFloatingWindow/InSeting/white/T.svg")

    component IconButton: Item {
        property url source
        property int size: overlay.controlIconSize
        property bool enabled: true
        property real iconOpacity: 1.0
        signal clicked()

        width: size
        height: size

        Image {
            anchors.fill: parent
            source: parent.source
            fillMode: Image.PreserveAspectFit
            opacity: parent.enabled ? parent.iconOpacity : (0.4 * parent.iconOpacity)
        }
        HoverHandler { cursorShape: parent.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
        TapHandler {
            enabled: parent.enabled
            onTapped: parent.clicked()
        }
    }

    component SettingsItem: Item {
        property url iconSource
        property string label
        signal clicked()

        height: 36
        width: parent ? parent.width : 220

        Rectangle {
            anchors.fill: parent
            radius: 8
            color: itemHover.hovered ? "#f3f4f6" : "transparent"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 10
            Image {
                source: iconSource
                width: 18
                height: 18
                fillMode: Image.PreserveAspectFit
                Layout.alignment: Qt.AlignVCenter
            }
            Text {
                text: label
                color: "#111827"
                font.pixelSize: 13
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                elide: Text.ElideRight
            }
        }

        HoverHandler { id: itemHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: clicked() }
    }
    Timer {
        id: dimTimer
        interval: 2500
        repeat: false
        running: false
        onTriggered: {
            dimmed = false
            showControls = false
            controlsPinned = false
        }
    }
    onDimmedChanged: {
        if (dimmed) {
            if (typeof overlayHover !== "undefined" && overlayHover.hovered) {
                dimTimer.stop()
            } else {
                dimTimer.restart()
            }
        } else {
            dimTimer.stop()
            showControls = false
            controlsPinned = false
        }
    }

    Settings {
        id: settings
        category: "lyricOverlay"
        property int winWidth: 800
        property int winHeight: 200
        property int winX: 240
        property int winY: 240
        property int fontSize: 24
        property color highlightColor: "#1db954"
        property string theme: "dark"
        property bool stayOnTop: true
        property bool locked: false
    }

    Rectangle {
        anchors.fill: parent
        color: dimmed ? "#808080" : "transparent"
        radius: 14
        HoverHandler {
            id: overlayHover
            onHoveredChanged: {
                if (hovered) {
                    dimTimer.stop()
                } else if (dimmed) {
                    dimTimer.restart()
                }
            }
        }
    }

    function updateSettingsPopupPosition() {
        const p = settingsButton.mapToItem(overlay.contentItem, 0, 0)
        settingsPopup.x = Math.round(p.x + settingsButton.width - settingsPopup.width)
        settingsPopup.y = Math.round(p.y - settingsPopup.height - 10)
        settingsPopup.x = Math.max(8, Math.min(settingsPopup.x, overlay.width - settingsPopup.width - 8))
        settingsPopup.y = Math.max(8, settingsPopup.y)
    }

    function updateLockWindowPosition() {
        if (!settings.locked || !overlay.visible)
            return
        lockWindow.x = Math.round(overlay.x + (overlay.width - lockWindow.width) / 2)
        lockWindow.y = Math.round(overlay.y + 8)
    }

    Connections {
        target: settings
        function onLockedChanged() {
            if (settings.locked) {
                dimTimer.stop()
                dimmed = false
                showControls = false
                controlsPinned = false
                settingsPopup.close()
            }
            updateLockWindowPosition()
        }
    }

    onVisibleChanged: {
        if (!visible)
            return
        if (settings.locked) {
            dimTimer.stop()
            dimmed = false
            showControls = false
            controlsPinned = false
            settingsPopup.close()
        }
        updateLockWindowPosition()
    }

    onXChanged: {
        settings.winX = x
        updateLockWindowPosition()
    }
    onYChanged: {
        settings.winY = y
        updateLockWindowPosition()
    }
    onWidthChanged: {
        settings.winWidth = width
        if (settingsPopup.opened)
            updateSettingsPopupPosition()
        updateLockWindowPosition()
    }
    onHeightChanged: {
        settings.winHeight = height
        if (settingsPopup.opened)
            updateSettingsPopupPosition()
        updateLockWindowPosition()
    }

    Rectangle {
        id: controlBarBackground
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: showControls ? 52 : 0
        color: "#808080"
        opacity: 0.65
        visible: showControls
    }

    RowLayout {
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
        visible: showControls
        IconButton {
            source: iconLyricOffsetMinus
            enabled: !!musicController
            onClicked: if (musicController) musicController.adjustLyricOffsetMs(-500)
        }
        IconButton {
            source: iconLyricOffsetPlus
            enabled: !!musicController
            onClicked: if (musicController) musicController.adjustLyricOffsetMs(500)
        }
        Item { Layout.fillWidth: true }
        Row {
            spacing: 14
            IconButton {
                source: iconPrev
                enabled: !!musicController
                onClicked: if (musicController) musicController.playPrev()
            }
            IconButton {
                id: playPauseButton
                source: musicController && musicController.playing ? iconPause : iconPlay
                enabled: !!musicController
                onClicked: if (musicController) musicController.playing ? musicController.pause() : musicController.resume()
            }
            IconButton {
                source: iconNext
                enabled: !!musicController
                onClicked: if (musicController) musicController.playNext()
            }
        }
        Item { Layout.fillWidth: true }
        IconButton {
            id: lockButton
            source: iconLock
            iconOpacity: settings.locked ? 1.0 : 0.55
            onClicked: settings.locked = !settings.locked
        }
        IconButton {
            id: settingsButton
            source: iconSettings
            onClicked: {
                if (settingsPopup.opened) {
                    settingsPopup.close()
                    return
                }
                updateSettingsPopupPosition()
                settingsPopup.open()
            }
        }
        IconButton {
            source: iconClose
            onClicked: overlay.close()
        }
    }

    Popup {
        id: settingsPopup
        parent: overlay.contentItem
        padding: 10
        width: 220
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 12
            color: "#ffffff"
            border.color: "#e5e7eb"
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 6
            SettingsItem {
                iconSource: iconSettingsPalette
                label: "配色"
                onClicked: settingsPopup.close()
            }
            SettingsItem {
                iconSource: iconSettingsMore
                label: "更多"
                onClicked: settingsPopup.close()
            }
            SettingsItem {
                iconSource: iconSettingsT
                label: "切换竖排歌词"
                onClicked: settingsPopup.close()
            }
        }
    }

    

    // 歌词显示
    ListView {
        id: lyricList
        anchors {
            left: parent.left
            right: parent.right
            top: controlBarBackground.bottom
            bottom: parent.bottom
            leftMargin: 12
            rightMargin: 12
            topMargin: 6
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
            property real posMs: musicController ? (musicController.positionMs + musicController.lyricOffsetMs) : 0
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

    Item {
        anchors {
            left: parent.left
            right: parent.right
            top: controlBarBackground.bottom
            bottom: parent.bottom
        }
        DragHandler {
            id: windowDrag
            enabled: !settings.locked
            onActiveChanged: {
                if (active) {
                    overlay.startSystemMove()
                    dimmed = true
                    showControls = true
                    if (overlayHover.hovered) dimTimer.stop(); else dimTimer.restart()
                }
            }
        }
        TapHandler {
            enabled: !settings.locked
            onTapped: {
                dimmed = true
                showControls = true
                controlsPinned = true
                if (overlayHover.hovered) dimTimer.stop(); else dimTimer.restart()
            }
        }
    }
    Item {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 16
        height: 16
        visible: !settings.locked
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.SizeFDiagCursor
            enabled: !settings.locked
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

    Window {
        id: lockWindow
        width: overlay.controlIconSize
        height: overlay.controlIconSize
        visible: settings.locked && overlay.visible
        color: "transparent"
        flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
        transientParent: overlay

        HoverHandler { id: lockHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: settings.locked = false }

        Image {
            anchors.fill: parent
            source: iconLock
            fillMode: Image.PreserveAspectFit
            opacity: lockHover.hovered ? 1.0 : 0.0
        }
    }
}
