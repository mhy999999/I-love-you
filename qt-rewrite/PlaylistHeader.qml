import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
// import Qt5Compat.GraphicalEffects removed due to missing dependency

Item {
    id: root
    height: Math.max(240, contentLayout.implicitHeight + 40) // Auto height based on content
    Layout.fillWidth: true
    
    // Background logic can be added here
    
    RowLayout {
        id: contentLayout
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 0
        spacing: 24
        
        // Cover Image
        Item {
            Layout.preferredWidth: 200
            Layout.preferredHeight: 200
            Layout.alignment: Qt.AlignTop
            
            Image {
                id: coverImg
                anchors.fill: parent
                source: musicController.coverSource
                fillMode: Image.PreserveAspectCrop
                visible: true // Visible directly without mask
                onStatusChanged: if (status === Image.Error) console.log("Cover load error: " + source)
            }
            
            /* OpacityMask removed - image will be square
            Rectangle {
                id: mask
                anchors.fill: parent
                radius: 8
                visible: false
            }
            
            OpacityMask {
                anchors.fill: coverImg
                source: coverImg
                maskSource: mask
            }
            */
            
            // Play Count Overlay
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 4
                width: playCountRow.width + 12
                height: 20
                color: "#40000000"
                radius: 10
                
                Row {
                    id: playCountRow
                    anchors.centerIn: parent
                    spacing: 4
                    Text {
                        text: "▶"
                        color: "white"
                        font.pixelSize: 10
                    }
                    Text {
                        text: formatCount(musicController.playlistPlayCount)
                        color: "white"
                        font.pixelSize: 10
                    }
                }
            }
        }
        
        // Info Column
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignTop
            spacing: 8
            
            // Title
            RowLayout {
                spacing: 8
                Rectangle {
                    border.color: "#ec4141"
                    border.width: 1
                    color: "transparent"
                    radius: 2
                    width: 36
                    height: 20
                    Text {
                        anchors.centerIn: parent
                        text: "歌单"
                        color: "#ec4141"
                        font.pixelSize: 12
                    }
                }
                Text {
                    text: musicController.playlistName
                    font.pixelSize: 22
                    font.bold: true
                    color: "#333333"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
            
            // Creator Info
            RowLayout {
                spacing: 8
                
                Image {
                    source: musicController.playlistCreatorAvatar
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    
                    /* OpacityMask removed
                    layer.enabled: true
                    layer.effect: OpacityMask {
                        maskSource: Rectangle {
                            width: 24; height: 24; radius: 12
                        }
                    }
                    */
                }
                
                Text {
                    text: musicController.playlistCreatorName
                    color: "#507daf"
                    font.pixelSize: 12
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                    }
                }
                
                Text {
                    text: formatDate(musicController.playlistCreateTime) + "创建"
                    color: "#666666"
                    font.pixelSize: 12
                }
            }
            
            // Action Buttons
            RowLayout {
                spacing: 10
                Layout.topMargin: 4
                Layout.bottomMargin: 4
                
                Button {
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        color: "#ec4141"
                        radius: 16
                    }
                    contentItem: Row {
                        spacing: 4
                        anchors.centerIn: parent
                        Text { text: "+"; color: "white"; font.pixelSize: 16 }
                        Text { text: "播放全部"; color: "white"; font.pixelSize: 14 }
                    }
                    onClicked: musicController.playAll()
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: parent.onClicked()
                    }
                }
                
                Button {
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        color: musicController.playlistSubscribed ? "#f2f2f2" : "white"
                        border.color: "#d9d9d9"
                        radius: 16
                    }
                    contentItem: Row {
                        spacing: 4
                        anchors.centerIn: parent
                        Text { 
                            text: (musicController.playlistSubscribed ? "已收藏" : "收藏") + "(" + formatCount(musicController.playlistSubscribedCount) + ")"
                            color: "#333333" 
                            font.pixelSize: 14 
                        }
                    }
                    onClicked: musicController.togglePlaylistSubscribe()
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: parent.onClicked()
                    }
                }
                
                Button {
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        border.color: "#d9d9d9"
                        radius: 16
                    }
                    contentItem: Row {
                        spacing: 4
                        anchors.centerIn: parent
                        Text { 
                            text: "分享(" + formatCount(musicController.playlistShareCount) + ")"
                            color: "#333333" 
                            font.pixelSize: 14 
                        }
                    }
                    onClicked: musicController.sharePlaylist()
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: parent.onClicked()
                    }
                }
                
                Button {
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        border.color: "#d9d9d9"
                        radius: 16
                    }
                    contentItem: Row {
                        spacing: 4
                        anchors.centerIn: parent
                        Text { 
                            text: "下载全部"
                            color: "#333333" 
                            font.pixelSize: 14 
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }
            
            // Tags
            Text {
                text: "标签: " + (musicController.playlistTags.length > 0 ? musicController.playlistTags.join(" / ") : "无")
                color: "#666666"
                font.pixelSize: 12
                visible: musicController.playlistTags.length > 0
            }
            
            // Stats
            Text {
                text: "歌曲: " + musicController.playlistModel.rowCount() + "  播放: " + formatCount(musicController.playlistPlayCount)
                color: "#666666"
                font.pixelSize: 12
            }
            
            // Description
            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                
                Text {
                    id: descText
                    text: "简介: " + (musicController.playlistDescription ? musicController.playlistDescription : "无")
                    color: "#666666"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    Layout.fillWidth: true
                }
                
                Text {
                    text: "详情"
                    color: "#666666"
                    font.pixelSize: 12
                    visible: musicController.playlistDescription.length > 20
                    Layout.alignment: Qt.AlignTop | Qt.AlignRight
                    
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: descPopup.open()
                    }
                }
            }
        }
    }
    
    Popup {
        id: descPopup
        parent: Overlay.overlay
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
        width: Math.min(600, parent.width - 40)
        height: Math.min(400, parent.height - 40)
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        
        background: Rectangle {
            color: "white"
            radius: 8
            border.color: "#e5e7eb"
        }
        
        contentItem: ColumnLayout {
            spacing: 16
            
            Text {
                text: "歌单简介"
                font.bold: true
                font.pixelSize: 18
                color: "#333333"
                Layout.alignment: Qt.AlignHCenter
            }
            
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                
                TextArea {
                    text: musicController.playlistDescription
                    readOnly: true
                    wrapMode: Text.Wrap
                    font.pixelSize: 14
                    color: "#666666"
                    background: null
                    selectByMouse: true
                }
            }
            
            Button {
                text: "关闭"
                Layout.alignment: Qt.AlignRight
                onClicked: descPopup.close()
            }
        }
    }
    
    function formatCount(count) {
        if (count >= 100000000) {
            return (count / 100000000).toFixed(1) + "亿"
        } else if (count >= 10000) {
            return (count / 10000).toFixed(1) + "万"
        }
        return count
    }
    
    function formatDate(timestamp) {
        if (timestamp === 0) return ""
        var date = new Date(timestamp)
        return date.getFullYear() + "-" + 
               (date.getMonth() + 1).toString().padStart(2, '0') + "-" + 
               date.getDate().toString().padStart(2, '0')
    }
}
