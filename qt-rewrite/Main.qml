// 引入 Qt Quick 基础模块
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.FluentWinUI3
import QtQuick.Layouts
import QtMultimedia

// 应用主窗口
ApplicationWindow {
	width: 960
	height: 640
	visible: true
	title: qsTr("Qt Rewrite Music Player")
	color: "#f5f5f7"

	property string lastError: ""
	property int currentSongIndex: -1
	property int currentPlaylistIndex: -1
	property url iconPrev: Qt.resolvedUrl("ui-asset/black-backgroud/上一首.svg")
	property url iconNext: Qt.resolvedUrl("ui-asset/black-backgroud/下一首.svg")
	property url iconPlay: Qt.resolvedUrl("ui-asset/black-backgroud/播放.svg")
	property url iconPause: Qt.resolvedUrl("ui-asset/black-backgroud/暂停.svg")
	property url iconLyric: Qt.resolvedUrl("ui-asset/black-backgroud/歌词.svg")
	property url iconLyricActive: Qt.resolvedUrl("ui-asset/black-backgroud/click_back/歌词_clickBack.svg")
	property url iconVolume: Qt.resolvedUrl("ui-asset/black-backgroud/声音.svg")
	property url iconVolumeMute: Qt.resolvedUrl("ui-asset/black-backgroud/声音静音.svg")
	property url iconList: Qt.resolvedUrl("ui-asset/black-backgroud/列表2.svg")
	property url iconSettings: Qt.resolvedUrl("ui-asset/black-backgroud/设置.svg")

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
		id: headerBar
		contentHeight: 48
		background: Rectangle {
			color: "#ffffff"
			radius: 0
			border.color: "#cbd5e1"
			border.width: 1
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

			Item {
				Layout.preferredWidth: 260
				implicitHeight: 32
				TextField {
					id: globalSearchField
					anchors.fill: parent
					placeholderText: qsTr("搜索歌曲或歌单")
					font.pixelSize: 14
					color: "#111827"
					background: Rectangle {
						radius: 6
						color: "#ffffff"
						border.color: "#cbd5e1"
						border.width: 1
					}
					onAccepted: {
						if (musicController) {
							if (tabBar.currentIndex === 0)
								musicController.search(text)
							else
								musicController.loadPlaylist(text)
						}
					}
				}
				Text {
					text: globalSearchField.placeholderText
					color: "#6b7280"
					anchors.verticalCenter: parent.verticalCenter
					anchors.left: parent.left
					anchors.leftMargin: 8
					visible: globalSearchField.text.length === 0
					elide: Text.ElideRight
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
			TabButton {
				text: qsTr("搜索")
				contentItem: Text {
					text: qsTr("搜索")
					color: "#111827"
					horizontalAlignment: Text.AlignHCenter
					verticalAlignment: Text.AlignVCenter
				}
			}
			TabButton {
				text: qsTr("歌单")
				contentItem: Text {
					text: qsTr("歌单")
					color: "#111827"
					horizontalAlignment: Text.AlignHCenter
					verticalAlignment: Text.AlignVCenter
				}
			}
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
						Layout.alignment: Qt.AlignHCenter
					}

					RowLayout {
						spacing: 8
						Item {
							Layout.fillWidth: true
							implicitHeight: 34
							TextField {
								id: searchInput
								anchors.fill: parent
								placeholderText: qsTr("搜索歌曲关键词")
								font.pixelSize: 14
								color: "#111827"
								background: Rectangle {
									radius: 6
									color: "#ffffff"
									border.color: "#cbd5e1"
									border.width: 1
								}
								onAccepted: if (musicController) musicController.search(text)
							}
							Text {
								text: searchInput.placeholderText
								color: "#6b7280"
								anchors.verticalCenter: parent.verticalCenter
								anchors.left: parent.left
								anchors.leftMargin: 8
								visible: searchInput.text.length === 0
								elide: Text.ElideRight
							}
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
							Layout.preferredWidth: 420
							Layout.fillWidth: true
							Layout.fillHeight: true
							background: Rectangle {
								color: "#ffffff"
								radius: 16
								border.color: "#cbd5e1"
								border.width: 1
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
											Layout.preferredWidth: 220
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
									id: coverImage
									Layout.fillWidth: true
									Layout.preferredHeight: 200
									source: musicController ? musicController.coverSource : ""
									fillMode: Image.PreserveAspectFit
									visible: status === Image.Ready
								}
								Image {
									Layout.alignment: Qt.AlignHCenter
									Layout.preferredHeight: 200
									width: 64
									height: 64
									source: iconPlay
									fillMode: Image.PreserveAspectFit
									visible: coverImage.status !== Image.Ready
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
										horizontalAlignment: Text.AlignHCenter
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
						Layout.alignment: Qt.AlignHCenter
					}

					RowLayout {
						spacing: 8
						Item {
							Layout.fillWidth: true
							implicitHeight: 34
							TextField {
								id: playlistInput
								anchors.fill: parent
								placeholderText: qsTr("输入歌单ID")
								font.pixelSize: 14
								color: "#111827"
								background: Rectangle {
									radius: 6
									color: "#ffffff"
									border.color: "#cbd5e1"
									border.width: 1
								}
								onAccepted: if (musicController) musicController.loadPlaylist(text)
							}
							Text {
								text: playlistInput.placeholderText
								color: "#6b7280"
								anchors.verticalCenter: parent.verticalCenter
								anchors.left: parent.left
								anchors.leftMargin: 8
								visible: playlistInput.text.length === 0
								elide: Text.ElideRight
							}
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
							border.color: "#cbd5e1"
							border.width: 1
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
										Layout.preferredWidth: 220
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

	}
	footer: ToolBar {
		id: footerBar
		contentHeight: 96
		property int controlIconSize: Math.min(44, Math.max(28, Math.round(contentHeight * 0.45)))
		property int controlButtonSize: controlIconSize + 12
		property int previousVolume: 50
		background: Rectangle {
			color: "#ffffff"
			radius: 0
			border.color: "#cbd5e1"
			border.width: 1
		}
		contentItem: GridLayout {
			anchors.fill: parent
			anchors.margins: 12
			columns: 3
			columnSpacing: 16
			rowSpacing: 0

			RowLayout {
				Layout.column: 0
				spacing: 10
				Layout.preferredWidth: 260
				Layout.maximumWidth: 260
				Layout.minimumWidth: 260

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
						visible: status === Image.Ready
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
				Layout.column: 1
				Layout.fillWidth: true
				spacing: 6

				RowLayout {
					Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
					spacing: 14

					ToolButton {
						id: prevButton
						text: ""
						font.pixelSize: 16
						implicitWidth: 46
						implicitHeight: 46
						HoverHandler { cursorShape: Qt.PointingHandCursor }
						contentItem: Image {
							source: iconPrev
							width: 22
							height: 22
							fillMode: Image.PreserveAspectFit
							anchors.centerIn: parent
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

					RoundButton {
						id: playButton
						text: ""
						implicitWidth: 46
						implicitHeight: 46
						font.pixelSize: 18
						highlighted: true
						HoverHandler { cursorShape: Qt.PointingHandCursor }
						icon.source: musicController && musicController.playing ? iconPause : iconPlay
						icon.width: 22
						icon.height: 22
						onClicked: if (musicController) musicController.playing ? musicController.pause() : musicController.resume()
					}

					ToolButton {
						id: nextButton
						text: ""
						font.pixelSize: 16
						implicitWidth: 46
						implicitHeight: 46
						HoverHandler { cursorShape: Qt.PointingHandCursor }
						contentItem: Image {
							source: iconNext
							width: 22
							height: 22
							fillMode: Image.PreserveAspectFit
							anchors.centerIn: parent
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
				Layout.column: 2
				spacing: Math.round(footerBar.controlIconSize * 0.3)
				Layout.preferredWidth: 260
				Layout.maximumWidth: 260
				Layout.minimumWidth: 260
				Layout.alignment: Qt.AlignRight

				Item {
					id: playlistBox
					width: footerBar.controlIconSize
					height: footerBar.controlIconSize
					Image {
						source: iconList
						x: 0; y: 0
						width: parent.width
						height: parent.height
						fillMode: Image.PreserveAspectFit
					}
					MouseArea {
						x: 0; y: 0
						width: parent.width
						height: parent.height
						hoverEnabled: true
						cursorShape: Qt.PointingHandCursor
						onClicked: {}
					}
				}

				Item {
					id: lyricBox
					property bool active: false
					width: footerBar.controlIconSize
					height: footerBar.controlIconSize
					Image {
						source: lyricBox.active ? iconLyricActive : iconLyric
						x: 0; y: 0
						width: parent.width
						height: parent.height
						fillMode: Image.PreserveAspectFit
					}
					MouseArea {
						x: 0; y: 0
						width: parent.width
						height: parent.height
						hoverEnabled: true
						cursorShape: Qt.PointingHandCursor
						onClicked: lyricBox.active = !lyricBox.active
					}
				}

				Item {
					id: volumeBox
					property bool muted: false
					property bool hovered: false
					width: footerBar.controlIconSize
					height: footerBar.controlIconSize
					Image {
						source: volumeBox.muted ? iconVolumeMute : iconVolume
						x: 0; y: 0
						width: parent.width
						height: parent.height
						fillMode: Image.PreserveAspectFit
					}
					Binding {
						target: volumeBox
						property: "muted"
						value: musicController ? musicController.volume === 0 : false
					}
					HoverHandler {
						id: volumeIconHover
						cursorShape: Qt.PointingHandCursor
						onHoveredChanged: {
							if (hovered) {
								volumePopup.open()
								volumePopupCloseDelay.stop()
							} else {
								volumePopupCloseDelay.restart()
							}

						}
					}
					MouseArea {
						x: 0; y: 0
						width: parent.width
						height: parent.height
						cursorShape: Qt.PointingHandCursor
						onClicked: {
							if (!musicController) return
							if (volumeBox.muted) {
								musicController.volume = footerBar.previousVolume
								volumeBox.muted = false
							} else {
								footerBar.previousVolume = musicController.volume
								musicController.volume = 0
								volumeBox.muted = true
							}
						}
					}
				}

				Item {
					id: settingsBox
					width: footerBar.controlIconSize
					height: footerBar.controlIconSize
					Image {
						source: iconSettings
						x: 0; y: 0
						width: parent.width
						height: parent.height
						fillMode: Image.PreserveAspectFit
					}
					MouseArea {
						x: 0; y: 0
						width: parent.width
						height: parent.height
						hoverEnabled: true
						cursorShape: Qt.PointingHandCursor
						onClicked: {}
					}
				}
				Popup {
					id: volumePopup
					parent: footerBar.contentItem
					width: 76
					height: 190
					modal: false
					focus: true
					closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
					Timer {
						id: volumePopupCloseDelay
						interval: 120
						repeat: false
						onTriggered: {
							if (!volumeIconHover.hovered && !popupHover.hovered) {
								volumePopup.close()
							}
						}
					}
					function updatePos() {
						var pCenter = volumeBox.mapToItem(parent, volumeBox.width / 2, 0)
						var pTopLeft = volumeBox.mapToItem(parent, 0, 0)
						x = pCenter.x - width / 2
						y = pTopLeft.y - height
					}
					onOpened: updatePos()
					onVisibleChanged: if (visible) updatePos()
					background: Rectangle {
						radius: 12
						color: "#ffffff"
						border.color: "#cbd5e1"
						border.width: 1
					}
					contentItem: ColumnLayout {
						anchors.fill: parent
						anchors.margins: 10
						spacing: 8
						HoverHandler {
							id: popupHover
							cursorShape: Qt.PointingHandCursor
							onHoveredChanged: {
								if (hovered) {
									volumePopupCloseDelay.stop()
								} else {
									volumePopupCloseDelay.restart()
								}
							}
						}
						Item {
							Layout.alignment: Qt.AlignHCenter
							implicitWidth: 36
							implicitHeight: 130
							Rectangle {
								id: volTrack
								width: 8
								height: parent.height
								radius: 4
								color: "#e5e7eb"
								x: Math.round((parent.width - width) / 2)
								Rectangle {
									x: 0
									anchors.bottom: parent.bottom
									width: parent.width
									height: volumePopupSlider.visualPosition * parent.height
									radius: 4
									color: "#ef4444"
								}
							}
							Rectangle {
								id: volHandle
								width: 18
								height: 18
								radius: 9
								color: "#ffffff"
								border.color: "#cbd5e1"
								x: Math.round(volTrack.x + (volTrack.width - width) / 2)
								y: (1 - volumePopupSlider.visualPosition) * (volTrack.height - height)
								MouseArea {
									x: 0
									y: 0
									width: parent.width
									height: parent.height
									cursorShape: Qt.PointingHandCursor
									drag.target: parent
									drag.axis: Drag.YAxis
									drag.minimumY: 0
									drag.maximumY: volTrack.height - volHandle.height
									onPositionChanged: {
										var pos = 1 - (volHandle.y / (volTrack.height - volHandle.height))
										var val = Math.round(pos * 100)
										if (musicController) musicController.volume = val
									}
								}
							}
							Slider {
								id: volumePopupSlider
								visible: false
								from: 0
								to: 100
								value: musicController ? musicController.volume : 50
								onMoved: if (musicController) musicController.volume = value
								onPressedChanged: if (!pressed && musicController) musicController.volume = value
							}
						}
						Text {
							Layout.alignment: Qt.AlignHCenter
							text: (musicController ? musicController.volume : 50) + "%"
							color: "#111827"
							font.pixelSize: 12
						}
						Binding {
							target: volumePopupSlider
							property: "value"
							value: musicController ? musicController.volume : 50
							when: !volumePopupSlider.pressed && musicController
						}
					}
				}
			}
		}
	}
	LyricOverlay {
		id: lyricOverlay
		visible: lyricBox.active
		onVisibleChanged: lyricBox.active = visible
	}
}
