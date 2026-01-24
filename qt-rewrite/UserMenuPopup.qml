import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Effects

Popup {
    id: userMenuPopup
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: 280
    height: 400
    // Position will be set by the caller usually, but we can default to center or relative to parent
    // For now, let's make it appear near the avatar if possible, or center
    anchors.centerIn: Overlay.overlay

    background: Rectangle {
        color: "#ffffff"
        radius: 8
        border.color: "#e5e7eb"
        border.width: 1
        
        layer.enabled: true
        layer.effect: Rectangle {
            color: "transparent"
            radius: 8
            border.color: "#000000"
            border.width: 1
            opacity: 0.1
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
                        visible: false
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
                
                Text {
                    text: musicController.nickname
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    color: "#333333"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
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
