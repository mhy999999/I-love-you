// 引入 Qt Quick 基础模块
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

// 应用主窗口，当前为最小示例界面
Window {
	width: 800
	height: 600
	visible: true
	title: qsTr("Qt Rewrite Music Player")

	property string lastError: ""

	Connections {
		target: musicController
		function onErrorOccurred(message) {
			lastError = message
			errorDialog.open()
		}
	}

	Dialog {
		id: errorDialog
		title: qsTr("错误")
		modal: true
		standardButtons: Dialog.Ok
		onClosed: lastError = ""
		contentItem: Text {
			text: lastError
			wrapMode: Text.Wrap
			width: 420
		}
	}

	Column {
		anchors.fill: parent
		anchors.margins: 16
		spacing: 12

		BusyIndicator {
			running: musicController && musicController.loading
			visible: running
			anchors.horizontalCenter: parent.horizontalCenter
		}

		Row {
			spacing: 8
			TextField {
				id: searchInput
				placeholderText: qsTr("搜索歌曲关键词")
				Layout.fillWidth: true
				onAccepted: if (musicController) musicController.search(text)
			}
			Button {
				text: qsTr("搜索")
				onClicked: if (musicController) musicController.search(searchInput.text)
			}
		}

		ListView {
			id: listView
			anchors.horizontalCenter: parent.horizontalCenter
			height: parent.height - 160
			model: musicController ? musicController.songsModel : null
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
					onClicked: if (musicController) musicController.playIndex(index)
				}
			}
		}

		Row {
			spacing: 8
			anchors.horizontalCenter: parent.horizontalCenter
			Button {
				text: musicController && musicController.playing ? qsTr("暂停") : qsTr("播放")
				onClicked: if (musicController) musicController.playing ? musicController.pause() : musicController.resume()
			}
			Button {
				text: qsTr("停止")
				onClicked: if (musicController) musicController.stop()
			}
			Slider {
				id: volumeSlider
				from: 0
				to: 100
				value: 50
				width: 150
				onMoved: if (musicController) musicController.volume = value
			}
		}
	}
}
