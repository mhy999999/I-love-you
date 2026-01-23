// 引入 Qt Quick 基础模块
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import QtQuick.Controls.Material

// 应用主窗口
ApplicationWindow {
	width: 960
	height: 640
	visible: true
	title: qsTr("Qt Rewrite Music Player")
	color: Material.background

	Material.theme: Material.Light
	Material.accent: "#22c55e"
	Material.background: "#f5f5f7"

	property string lastError: ""
	property int currentSongIndex: -1
	property int currentPlaylistIndex: -1

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

	header: ToolBar {
		contentHeight: 48
		background: Rectangle {
			color: "#ffffff"
			radius: 0
		}
		RowLayout {
			anchors.fill: parent
			anchors.margins: 16
			spacing: 12

			Label {
				text: qsTr("Qt Rewrite Music")
				color: "#111827"
				font.pixelSize: 20
				Layout.alignment: Qt.AlignVCenter
			}

			Item { Layout.fillWidth: true }

			TextField {
				id: globalSearchField
				placeholderText: qsTr("搜索歌曲或歌单")
				Layout.preferredWidth: 260
				onAccepted: {
					if (musicController) {
						if (tabBar.currentIndex === 0)
							musicController.search(text)
						else
							musicController.loadPlaylist(text)
					}
				}
			}
		}
	}

	ColumnLayout {
		anchors.fill: parent
		anchors.margins: 20
		spacing: 16

		TabBar {
			id: tabBar
			Layout.fillWidth: true
			TabButton { text: qsTr("搜索") }
			TabButton { text: qsTr("歌单") }
		}

		StackLayout {
			id: pages
			Layout.fillWidth: true
			Layout.fillHeight: true
			currentIndex: tabBar.currentIndex

			Item {
				Layout.fillWidth: true
				Layout.fillHeight: true
				ColumnLayout {
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

					RowLayout {
						spacing: 12
						Layout.fillWidth: true
						Layout.fillHeight: true

						Frame {
							Layout.fillWidth: true
							Layout.fillHeight: true
							background: Rectangle {
								color: "#ffffff"
								radius: 16
								border.color: "#e5e7eb"
							}
							ListView {
								id: listView
								anchors.fill: parent
								anchors.margins: 8
								model: musicController ? musicController.songsModel : null
								clip: true
								delegate: Rectangle {
									width: listView.width
									height: 56
									radius: 10
									property bool current: index === currentSongIndex
									property bool hovered: false
									color: current ? "#dcfce7" : (hovered ? "#f1f5f9" : "#ffffff")
									border.color: current ? "#22c55e" : "transparent"

									RowLayout {
										anchors.fill: parent
										anchors.margins: 12
										spacing: 10

										Text {
											text: index + 1
											color: "#9ca3af"
											font.pixelSize: 13
											horizontalAlignment: Text.AlignHCenter
											verticalAlignment: Text.AlignVCenter
											Layout.preferredWidth: 32
										}

										Text {
											text: title
											color: "#111827"
											font.pixelSize: 14
											elide: Text.ElideRight
											Layout.fillWidth: true
										}

										Text {
											text: artists
											color: "#6b7280"
											font.pixelSize: 13
											elide: Text.ElideRight
											Layout.fillWidth: true
										}

										Text {
											text: formatMs(duration)
											color: "#6b7280"
											font.pixelSize: 13
											horizontalAlignment: Text.AlignRight
											Layout.preferredWidth: 64
										}
									}

									MouseArea {
										anchors.fill: parent
										hoverEnabled: true
										onEntered: hovered = true
										onExited: hovered = false
										onClicked: {
											if (musicController) musicController.playIndex(index)
											currentSongIndex = index
											currentPlaylistIndex = -1
										}
									}
								}
							}
						}

						Frame {
							Layout.fillWidth: true
							Layout.fillHeight: true
							ColumnLayout {
								anchors.fill: parent
								spacing: 12

								Image {
									Layout.fillWidth: true
									Layout.preferredHeight: 200
									source: musicController ? musicController.coverSource : ""
									fillMode: Image.PreserveAspectFit
									visible: source !== ""
								}

								ListView {
									id: lyricView
									Layout.fillWidth: true
									Layout.fillHeight: true
									model: musicController ? musicController.lyricModel : null
									clip: true
									delegate: Text {
										width: lyricView.width
										text: model.text
										color: musicController && index === musicController.currentLyricIndex ? "#111827" : "#9ca3af"
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
					}

					
				}
			}

			Item {
				Layout.fillWidth: true
				Layout.fillHeight: true
				ColumnLayout {
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
						color: "#111827"
						font.pixelSize: 18
						elide: Text.ElideRight
						Layout.fillWidth: true
					}

					Frame {
						Layout.fillWidth: true
						Layout.fillHeight: true
						background: Rectangle {
							color: "#ffffff"
							radius: 16
							border.color: "#e5e7eb"
						}
						ListView {
							id: playlistView
							anchors.fill: parent
							anchors.margins: 8
							model: musicController ? musicController.playlistModel : null
							clip: true
							delegate: Rectangle {
								width: playlistView.width
								height: 56
								radius: 10
								property bool current: index === currentPlaylistIndex
								property bool hovered: false
								color: current ? "#dcfce7" : (hovered ? "#f1f5f9" : "#ffffff")
								border.color: current ? "#22c55e" : "transparent"

								RowLayout {
									anchors.fill: parent
									anchors.margins: 12
									spacing: 10

									Text {
										text: index + 1
										color: "#9ca3af"
										font.pixelSize: 13
										horizontalAlignment: Text.AlignHCenter
										verticalAlignment: Text.AlignVCenter
										Layout.preferredWidth: 32
									}

									Text {
										text: title
										color: "#111827"
										font.pixelSize: 14
										elide: Text.ElideRight
										Layout.fillWidth: true
									}

									Text {
										text: artists
										color: "#6b7280"
										font.pixelSize: 13
										elide: Text.ElideRight
										Layout.fillWidth: true
									}
								}

								MouseArea {
									anchors.fill: parent
									hoverEnabled: true
									onEntered: hovered = true
									onExited: hovered = false
									onClicked: {
										if (!musicController) return
										musicController.importPlaylistToQueue()
										musicController.playIndex(index)
										currentPlaylistIndex = index
										currentSongIndex = index
									}
								}
							}
						}
					}
				}
			}
		}

		Rectangle {
			Layout.fillWidth: true
			Layout.preferredHeight: 96
			color: "#ffffff"
			border.color: "#e5e7eb"

			RowLayout {
				anchors.fill: parent
				anchors.margins: 12
				spacing: 16

				RowLayout {
					spacing: 10
					Layout.preferredWidth: 260

					Rectangle {
						width: 48
						height: 48
						radius: 8
						color: "#f3f4f6"
						clip: true
						Image {
							anchors.fill: parent
							source: musicController ? musicController.coverSource : ""
							fillMode: Image.PreserveAspectCrop
							visible: source !== ""
						}
					}

					ColumnLayout {
						Layout.fillWidth: true
						spacing: 2
						Text {
							text: musicController ? musicController.currentSongTitle : qsTr("未播放")
							color: "#111827"
							font.pixelSize: 13
							elide: Text.ElideRight
							Layout.fillWidth: true
						}
						Text {
							text: musicController ? musicController.currentSongArtists : ""
							color: "#6b7280"
							font.pixelSize: 11
							elide: Text.ElideRight
							Layout.fillWidth: true
						}
					}
				}

				ColumnLayout {
					Layout.fillWidth: true
					spacing: 6

					RowLayout {
						Layout.alignment: Qt.AlignHCenter
						spacing: 14

						Button {
							id: prevButton
							text: "⏮"
							implicitWidth: 36
							implicitHeight: 36
							hoverEnabled: true
							background: Rectangle {
								radius: height / 2
								color: prevButton.down ? "#e5e7eb" : (prevButton.hovered ? "#f3f4f6" : "#ffffff")
								border.color: "#e5e7eb"
							}
							contentItem: Text {
								text: "⏮"
								color: "#374151"
								font.pixelSize: 16
								horizontalAlignment: Text.AlignHCenter
								verticalAlignment: Text.AlignVCenter
							}
							onClicked: {
								if (!musicController) return
								if (currentSongIndex > 0) {
									currentSongIndex = currentSongIndex - 1
									currentPlaylistIndex = -1
									musicController.playIndex(currentSongIndex)
								}
							}
						}

						Button {
							id: playButton
							text: musicController && musicController.playing ? qsTr("暂停") : qsTr("播放")
							implicitWidth: 46
							implicitHeight: 46
							hoverEnabled: true
							background: Rectangle {
								radius: height / 2
								color: playButton.down ? "#16a34a" : "#22c55e"
							}
							contentItem: Text {
								text: musicController && musicController.playing ? "⏸" : "▶"
								color: "#ffffff"
								font.pixelSize: 18
								horizontalAlignment: Text.AlignHCenter
								verticalAlignment: Text.AlignVCenter
							}
							onClicked: if (musicController) musicController.playing ? musicController.pause() : musicController.resume()
						}

						Button {
							id: nextButton
							text: "⏭"
							implicitWidth: 36
							implicitHeight: 36
							hoverEnabled: true
							background: Rectangle {
								radius: height / 2
								color: nextButton.down ? "#e5e7eb" : (nextButton.hovered ? "#f3f4f6" : "#ffffff")
								border.color: "#e5e7eb"
							}
							contentItem: Text {
								text: "⏭"
								color: "#374151"
								font.pixelSize: 16
								horizontalAlignment: Text.AlignHCenter
								verticalAlignment: Text.AlignVCenter
							}
							onClicked: {
								if (!musicController) return
								if (currentSongIndex >= 0 && listView.count > 0 && currentSongIndex < listView.count - 1) {
									currentSongIndex = currentSongIndex + 1
									currentPlaylistIndex = -1
									musicController.playIndex(currentSongIndex)
								}
							}
						}
					}

					RowLayout {
						Layout.fillWidth: true
						spacing: 10
						Text {
							color: "#6b7280"
							text: musicController ? formatMs(progressSlider.pressed ? progressSlider.value : musicController.positionMs) : "0:00"
							Layout.preferredWidth: 64
							horizontalAlignment: Text.AlignRight
						}
						Slider {
							id: progressSlider
							from: 0
							to: musicController ? Math.max(0, musicController.durationMs) : 0
							enabled: musicController && musicController.durationMs > 0
							Layout.fillWidth: true
							onMoved: if (pressed && musicController) musicController.seek(value)
							onPressedChanged: if (!pressed && musicController) musicController.seek(value)
							background: Rectangle {
								implicitHeight: 4
								radius: 2
								color: "#e5e7eb"
								Rectangle {
									width: progressSlider.visualPosition * parent.width
									height: parent.height
									radius: 2
									color: "#22c55e"
								}
							}
							handle: Rectangle {
								width: 12
								height: 12
								radius: 6
								color: "#22c55e"
								border.color: "#16a34a"
							}
							Binding {
								target: progressSlider
								property: "value"
								value: musicController ? musicController.positionMs : 0
								when: !progressSlider.pressed
							}
						}
						Text {
							color: "#6b7280"
							text: musicController ? formatMs(musicController.durationMs) : "0:00"
							Layout.preferredWidth: 64
							horizontalAlignment: Text.AlignLeft
						}
					}
				}

				RowLayout {
					spacing: 8
					Layout.preferredWidth: 260
					Layout.alignment: Qt.AlignRight

					Button {
						id: playlistButton
						text: qsTr("列表")
						implicitWidth: 52
						implicitHeight: 30
						hoverEnabled: true
						background: Rectangle {
							radius: 6
							color: playlistButton.down ? "#e5e7eb" : (playlistButton.hovered ? "#f3f4f6" : "#ffffff")
							border.color: "#e5e7eb"
						}
						contentItem: Text {
							text: qsTr("列表")
							color: "#374151"
							font.pixelSize: 12
							horizontalAlignment: Text.AlignHCenter
							verticalAlignment: Text.AlignVCenter
						}
					}

					Button {
						id: lyricButton
						text: qsTr("歌词")
						implicitWidth: 52
						implicitHeight: 30
						hoverEnabled: true
						background: Rectangle {
							radius: 6
							color: lyricButton.down ? "#e5e7eb" : (lyricButton.hovered ? "#f3f4f6" : "#ffffff")
							border.color: "#e5e7eb"
						}
						contentItem: Text {
							text: qsTr("歌词")
							color: "#374151"
							font.pixelSize: 12
							horizontalAlignment: Text.AlignHCenter
							verticalAlignment: Text.AlignVCenter
						}
					}

					Slider {
						id: volumeSlider
						from: 0
						to: 100
						value: musicController ? musicController.volume : 50
						Layout.fillWidth: true
						onMoved: if (musicController) musicController.volume = value
						onPressedChanged: if (!pressed && musicController) musicController.volume = value
						background: Rectangle {
							implicitHeight: 4
							radius: 2
							color: "#e5e7eb"
							Rectangle {
								width: volumeSlider.visualPosition * parent.width
								height: parent.height
								radius: 2
								color: "#9ca3af"
							}
						}
						handle: Rectangle {
							width: 12
							height: 12
							radius: 6
							color: "#9ca3af"
							border.color: "#6b7280"
						}
						Binding {
							target: volumeSlider
							property: "value"
							value: musicController ? musicController.volume : 50
							when: !volumeSlider.pressed && musicController
						}
					}
				}
			}
		}
	}
}
