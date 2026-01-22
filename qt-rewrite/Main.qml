// 引入 Qt Quick 基础模块
import QtQuick
import QtQuick.Controls
import QtMultimedia

// 应用主窗口，当前为最小示例界面
Window {
	width: 800
	height: 600
	visible: true
	title: qsTr("Qt Rewrite Music Player")

	Column {
		anchors.fill: parent
		anchors.margins: 16
		spacing: 12

		Row {
			spacing: 8
			TextField {
				id: searchInput
				placeholderText: qsTr("搜索歌曲关键词")
				Layout.fillWidth: true
				onAccepted: musicController.search(text)
			}
			Button {
				text: qsTr("搜索")
				onClicked: musicController.search(searchInput.text)
			}
		}

		ListView {
			id: listView
			anchors.horizontalCenter: parent.horizontalCenter
			height: parent.height - 160
			model: musicController.songsModel
			delegate: Rectangle {
				width: listView.width
				height: 40
				color: index % 2 === 0 ? "#202020" : "#181818"
				border.color: "#303030"
				Row {
					anchors.verticalCenter: parent.verticalCenter
					spacing: 8
					Text { text: title; color: "white" }
					Text { text: "-"; color: "#aaaaaa" }
					Text { text: artists; color: "#cccccc" }
				}
				MouseArea {
					anchors.fill: parent
					onClicked: musicController.playIndex(index)
				}
			}
		}

		Row {
			spacing: 8
			anchors.horizontalCenter: parent.horizontalCenter
			Button {
				text: musicController.playing ? qsTr("暂停") : qsTr("播放")
				onClicked: musicController.playing ? musicController.pause() : musicController.resume()
			}
			Button {
				text: qsTr("停止")
				onClicked: musicController.stop()
			}
			Slider {
				from: 0
				to: 100
				value: musicController.volume
				onValueChanged: musicController.volume = value
				width: 150
			}
		}
	}
}
