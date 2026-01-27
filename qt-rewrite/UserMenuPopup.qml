import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Effects
import Qt.labs.settings

Popup {
    id: userMenuPopup
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: 280
    height: 440
    // Position will be set by the caller usually, but we can default to center or relative to parent
    // For now, let's make it appear near the avatar if possible, or center
    // anchors.centerIn: Overlay.overlay

    property int yunbeiBalance: 0
    property int displayYunbei: yunbeiBalance
    Behavior on displayYunbei {
        NumberAnimation { duration: 1000; easing.type: Easing.OutQuart }
    }
    property bool signedInToday: false
    property int signDays: 0

    property int userLevel: 0

    Settings {
        id: signSettings
        category: "Yunbei"
        property string signInMap: "{}"
    }

    Connections {
        target: musicController
        
        function onUserLevelReceived(data) {
             if (data.code === 200 && data.data && data.data.level !== undefined) {
                 userLevel = data.data.level
             }
        }
        
        function onYunbeiAccountReceived(data) {
             if (data.code === 200 && data.userPoint) {
                 yunbeiBalance = data.userPoint.balance
             }
        }
        
        // /yunbei -> Get sign-in info (continuous days, etc.)
        function onYunbeiInfoReceived(data) {
             // Assuming structure based on user description
             // Often it returns { code: 200, signed: boolean, signDay: int, ... }
             // But if not standard, we might need to adjust.
             // For now, let's look for common keys.
             if (data.code === 200) {
                 var isSigned = false
                 if (data.isSign !== undefined) isSigned = data.isSign
                 if (data.mobileSign !== undefined) isSigned = isSigned || data.mobileSign
                 
                 if (isSigned && musicController.userId) {
                    var map = JSON.parse(signSettings.signInMap || "{}")
                    map[musicController.userId] = Qt.formatDate(new Date(), "yyyy-MM-dd")
                    signSettings.signInMap = JSON.stringify(map)
                 }
                 
                 // Check local settings to prevent overwriting with false if API doesn't return status
                 var today = Qt.formatDate(new Date(), "yyyy-MM-dd")
                 var mapLocal = JSON.parse(signSettings.signInMap || "{}")
                 var localSigned = (musicController.userId && mapLocal[musicController.userId] === today)
                 
                 signedInToday = localSigned || isSigned
                 
                 if (data.signDay !== undefined) signDays = data.signDay
             }
        }
        
        function onYunbeiSignReceived(data) {
            if (data.code === 200) {
                 signedInToday = true
                 if (musicController.userId) {
                    var map = JSON.parse(signSettings.signInMap || "{}")
                    map[musicController.userId] = Qt.formatDate(new Date(), "yyyy-MM-dd")
                    signSettings.signInMap = JSON.stringify(map)
                 }
                 signDays++ // Optimistic update
                 var msg = "签到成功"
                 if (data.point) msg += " +" + data.point
                 musicController.toastMessage(msg)
                 musicController.yunbeiAccount()
            } else if (data.code === -2) {
                 signedInToday = true
                 musicController.toastMessage("重复签到")
            } else {
                 musicController.toastMessage(data.msg || "签到失败")
            }
        }
    }
    
    onOpened: {
        var today = Qt.formatDate(new Date(), "yyyy-MM-dd")
        var map = JSON.parse(signSettings.signInMap || "{}")
        if (musicController.userId && map[musicController.userId] === today) {
            signedInToday = true
        } else {
            signedInToday = false
        }
        
        if (musicController.loggedIn) {
             musicController.yunbeiAccount()
             musicController.yunbeiInfo()
             musicController.userLevel()
        }
    }

    transformOrigin: Item.TopLeft

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.9; to: 1; duration: 200; easing.type: Easing.OutBack; easing.overshoot: 1.0 }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 150; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 1; to: 0.9; duration: 150; easing.type: Easing.OutCubic }
    }

    background: Rectangle {
        color: "#ffffff"
        radius: 8
        border.color: "#e5e7eb"
        border.width: 1
        
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: "#20000000"
            shadowBlur: 10
            shadowVerticalOffset: 4
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 0

        // User Info Header
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            
            RowLayout {
                anchors.fill: parent
                spacing: 12
                
                Item {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    
                    Image {
                        id: avatarImg
                        anchors.fill: parent
                        source: {
                            var url = musicController.avatarUrl
                            if (url && url.toString().indexOf("http") === 0 && url.toString().indexOf("?param=") === -1) {
                                return url + "?param=100y100"
                            }
                            return url
                        }
                        visible: true
                        fillMode: Image.PreserveAspectCrop
                        mipmap: true
                        cache: true
                    }
                    
                    MultiEffect {
                        source: avatarImg
                        anchors.fill: parent
                        maskEnabled: true
                        maskSource: maskRect
                    }
                    
                    Rectangle {
                        id: maskRect
                        anchors.fill: parent
                        radius: 20
                        visible: false
                    }
                }
                
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    
                    Text {
                        text: musicController.nickname
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        color: "#333333"
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    
                    Rectangle {
                        color: "#f0f0f0"
                        radius: 10
                        Layout.preferredWidth: levelText.contentWidth + 12
                        Layout.preferredHeight: 18
                        visible: userLevel > 0
                        
                        Text {
                            id: levelText
                            anchors.centerIn: parent
                            text: "Lv." + userLevel
                            font.pixelSize: 10
                            font.bold: true
                            font.italic: true
                            color: "#333"
                        }
                    }
                }
            }
        }
        
        // Cloud Coin / Sign-in Card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            Layout.topMargin: 5
            Layout.bottomMargin: 10
            radius: 8
            color: "#f9f9f9"
            border.color: "#f0f0f0"
            border.width: 1
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10
                
                Image {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    source: "file:///d:/project/I-love-you/qt-rewrite/ui-asset/black-backgroud/SignIn/shequ_ic_shell.svg"
                    fillMode: Image.PreserveAspectFit
                }
                
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    
                    Text {
                        text: "云贝: " + displayYunbei
                        font.bold: true
                        font.pixelSize: 14
                        color: "#333"
                    }
                    
                    Text {
                        text: signedInToday ? "已连续签到 " + signDays + " 天" : "签到领云贝"
                        font.pixelSize: 11
                        color: "#666"
                    }
                }
                
                Rectangle {
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 26
                    radius: 13
                    color: signedInToday ? "#e0e0e0" : "#ff3a3a"
                    
                    Text {
                        anchors.centerIn: parent
                        text: signedInToday ? "已签到" : "签到"
                        color: signedInToday ? "#999" : "#fff"
                        font.pixelSize: 11
                        font.bold: true
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        enabled: !signedInToday
                        onClicked: musicController.yunbeiSign()
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }
        }
        
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#f3f4f6"
            Layout.bottomMargin: 5
        }

        // Menu Items
        Repeater {
            model: [
                { text: "退出登录", icon: "", action: "logout" },
                { text: "设置", icon: "", action: "settings" },
                { text: "页面缩放", icon: "", action: "zoom" },
                { text: "主题", icon: "", action: "theme" },
                { text: "重启", icon: "", action: "restart" },
                { text: "刷新", icon: "", action: "refresh" },
                { text: "当前版本 5.0.0", icon: "", action: "version" }
            ]
            
            delegate: ItemDelegate {
                id: menuItem
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                
                contentItem: RowLayout {
                    spacing: 10
                    // Icon placeholder
                    // Image { source: modelData.icon ... }
                    
                    Text {
                        text: modelData.text
                        color: "#333333"
                        font.pixelSize: 14
                        Layout.fillWidth: true
                    }
                    
                    // Specific controls for some items
                    Label {
                        visible: modelData.action === "zoom"
                        text: "100%"
                        color: "#10b981" // Greenish
                        font.weight: Font.Bold
                        font.pixelSize: 12
                        padding: 4
                        background: Rectangle {
                            color: "#d1fae5"
                            radius: 4
                        }
                    }
                }
                
                background: Rectangle {
                    color: menuItem.highlighted || menuItem.hovered ? "#f3f4f6" : "transparent"
                    radius: 4
                }
                
                onClicked: {
                    if (modelData.action === "logout") {
                        musicController.logout()
                        userMenuPopup.close()
                    }
                }
                
                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }
        
        Item { Layout.fillHeight: true } // Spacer
    }
}
