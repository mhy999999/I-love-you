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

	function formatMs(ms) {
		var v = Math.max(0, Math.floor(ms || 0))
		var totalSec = Math.floor(v / 1000)
		var sec = totalSec % 60
		var min = Math.floor(totalSec / 60)
		var s = sec < 10 ? ("0" + sec) : ("" + sec)
		return min + ":" + s
	}

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
		implicitWidth: 460
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

		TabBar {
			id: tabBar
			width: parent.width
			TabButton { text: qsTr("搜索") }
			TabButton { text: qsTr("歌单") }
		}

		StackLayout {
			id: pages
			width: parent.width
			height: parent.height - tabBar.height - 8
			currentIndex: tabBar.currentIndex

			Item {
					Layout.fillWidth: true
					Layout.fillHeight: true
				Column {
					anchors.fill: parent
					spacing: 12

					BusyIndicator {
						running: musicController && musicController.loading
						visible: running
						anchors.horizontalCenter: parent.horizontalCenter
					}

					RowLayout {
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

					Row {
						spacing: 12
						height: parent.height - 140
						width: parent.width

						ListView {
							id: listView
							width: parent.width * 0.55
							height: parent.height
							model: musicController ? musicController.songsModel : null
							clip: true
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

						Column {
							width: parent.width - listView.width - 12
							height: parent.height
							spacing: 12

							Image {
								width: parent.width
								height: 200
								source: musicController ? musicController.coverSource : ""
								fillMode: Image.PreserveAspectFit
								visible: source !== ""
							}

							ListView {
								id: lyricView
								width: parent.width
								height: parent.height - 212
								model: musicController ? musicController.lyricModel : null
								clip: true
								delegate: Text {
									width: lyricView.width
									text: model.text
									color: musicController && index === musicController.currentLyricIndex ? "#ffffff" : "#aaaaaa"
									wrapMode: Text.Wrap
								}
							}

							Connections {
								target: musicController
								function onCurrentLyricIndexChanged() {
									if (!musicController) return
									if (musicController.currentLyricIndex >= 0)
										lyricView.positionViewAtIndex(musicController.currentLyricIndex, ListView.Center)
								}
							}
						}
					}

					Row {
						spacing: 8
						anchors.horizontalCenter: parent.horizontalCenter
						width: parent.width
						Text {
							color: "#cccccc"
							text: musicController ? formatMs(progressSlider.pressed ? progressSlider.value : musicController.positionMs) : "0:00"
							width: 48
							horizontalAlignment: Text.AlignRight
						}
						Slider {
							id: progressSlider
							from: 0
							to: musicController ? Math.max(0, musicController.durationMs) : 0
							enabled: musicController && musicController.durationMs > 0
							width: parent.width - 160
							onMoved: if (pressed && musicController) musicController.seek(value)
							onPressedChanged: if (!pressed && musicController) musicController.seek(value)
							Binding {
								target: progressSlider
								property: "value"
								value: musicController ? musicController.positionMs : 0
								when: !progressSlider.pressed
							}
						}
						Text {
							color: "#cccccc"
							text: musicController ? formatMs(musicController.durationMs) : "0:00"
							width: 48
							horizontalAlignment: Text.AlignLeft
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

			Item {
					Layout.fillWidth: true
					Layout.fillHeight: true
				Column {
					anchors.fill: parent
					spacing: 12

					BusyIndicator {
						running: musicController && musicController.playlistLoading
						visible: running
						anchors.horizontalCenter: parent.horizontalCenter
					}

					RowLayout {
						spacing: 8
						TextField {
							id: playlistInput
							placeholderText: qsTr("输入歌单ID")
							Layout.fillWidth: true
							onAccepted: if (musicController) musicController.loadPlaylist(text)
						}
						Button {
							text: qsTr("加载")
							onClicked: if (musicController) musicController.loadPlaylist(playlistInput.text)
						}
						Button {
							text: qsTr("更多")
							enabled: musicController && musicController.playlistHasMore
							onClicked: if (musicController) musicController.loadMorePlaylist()
						}
						Button {
							text: qsTr("导入队列")
							onClicked: if (musicController) musicController.importPlaylistToQueue()
						}
					}

					Text {
						text: musicController ? musicController.playlistName : ""
						color: "white"
						font.pixelSize: 18
						elide: Text.ElideRight
						width: parent.width
					}

					ListView {
						id: playlistView
						width: parent.width
						height: parent.height - 120
						model: musicController ? musicController.playlistModel : null
						clip: true
						delegate: Rectangle {
							width: playlistView.width
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
								onClicked: {
									if (!musicController) return
									musicController.importPlaylistToQueue()
									musicController.playIndex(index)
								}
							}
						}
					}
				}
			}
		}
	}
}
