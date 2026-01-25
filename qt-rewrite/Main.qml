// 引入 Qt Quick 基础模块
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.FluentWinUI3
import QtQuick.Layouts
import QtMultimedia
import QtQuick.Effects

// 应用主窗口
ApplicationWindow {
    id: appWindow
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
	property url iconDelete: Qt.resolvedUrl("ui-asset/black-backgroud/删除.svg")
	property url iconSettings: Qt.resolvedUrl("ui-asset/black-backgroud/设置.svg")
	property url iconModeSequence: Qt.resolvedUrl("ui-asset/black-backgroud/music_playerModes/顺序.svg")
	property url iconModeRandom: Qt.resolvedUrl("ui-asset/black-backgroud/music_playerModes/随机.svg")
	property url iconModeLoopAll: Qt.resolvedUrl("ui-asset/black-backgroud/music_playerModes/循环.svg")
	property url iconModeLoopOne: Qt.resolvedUrl("ui-asset/black-backgroud/music_playerModes/单曲循环.svg")
	property int navIndex: 1
	property url playbackModeIcon: {
		if (!musicController) return iconModeSequence
		if (musicController.playbackMode === 1) return iconModeRandom
		if (musicController.playbackMode === 2) return iconModeLoopAll
		if (musicController.playbackMode === 3) return iconModeLoopOne
		return iconModeSequence
	}
	ListModel {
		id: mainNavModel
		ListElement { pageIndex: 0; label: qsTr("首页"); glyph: "\uE80F" }
		ListElement { pageIndex: 1; label: qsTr("搜索"); glyph: "\uE721" }
		ListElement { pageIndex: 2; label: qsTr("歌单"); glyph: "\uE189" }
	}

	function formatMs(ms) {
		var v = Math.max(0, Math.floor(ms || 0))
		var totalSec = Math.floor(v / 1000)
		var sec = totalSec % 60
		var min = Math.floor(totalSec / 60)
		var s = sec < 10 ? ("0" + sec) : ("" + sec)
		return min + ":" + s
	}

	function getThumb(url, size) {
		if (!url) return ""
		var s = size || 100
		var u = url.toString()
		if (u.indexOf("http") === 0 && u.indexOf("?param=") === -1) {
			return u + "?param=" + s + "y" + s
		}
		return u
	}

	Connections {
		target: musicController
		function onCurrentSongIndexChanged() {
			currentSongIndex = musicController ? musicController.currentSongIndex : -1
		}
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
		}
	}

	RowLayout {
		anchors.fill: parent
		anchors.margins: 20
		spacing: 16

		Frame {
			id: leftNav
			Layout.preferredWidth: 86
			Layout.minimumWidth: 76
			Layout.maximumWidth: 120
			Layout.fillHeight: true
			leftPadding: 4
			rightPadding: 4
			topPadding: 10
			bottomPadding: 10
			background: Rectangle {
				color: "#ffffff"
				radius: 16
				border.color: "#cbd5e1"
				border.width: 1
			}

			ColumnLayout {
				anchors.fill: parent
				spacing: 10

				Rectangle {
					Layout.alignment: Qt.AlignHCenter
					width: 40
					height: 40
					radius: 12
					color: "transparent"
					
					property bool hasAvatar: musicController && musicController.loggedIn && musicController.avatarUrl != ""
					
					Image {
						anchors.fill: parent
						source: "qrc:/qt/qml/qtrewrite/ui-asset/black-backgroud/login/登录.svg"
						visible: !parent.hasAvatar
						fillMode: Image.PreserveAspectFit
						mipmap: true
					}
					
					Item {
						anchors.fill: parent
						visible: parent.hasAvatar
						
						Image {
							id: sidebarAvatarImg
							anchors.fill: parent
							source: getThumb(musicController.avatarUrl, 100)
							visible: true // Temporarily visible for debugging
							fillMode: Image.PreserveAspectCrop
							mipmap: true
							cache: true
						}
						
						// Temporary disable MultiEffect to debug
						/*
						MultiEffect {
							source: sidebarAvatarImg
							anchors.fill: parent
							maskEnabled: true
							maskSource: sidebarMask
						}
						*/
						
						Rectangle {
							id: sidebarMask
							anchors.fill: parent
							radius: 12
							visible: false
						}
					}
					
					MouseArea {
						anchors.fill: parent
						cursorShape: Qt.PointingHandCursor
						onClicked: {
							if (parent.hasAvatar) {
								userMenuPopup.open()
							} else {
								loginPopup.open()
							}
						}
					}
				}

				ListView {
					id: navList
					Layout.fillWidth: true
					Layout.preferredHeight: contentHeight
					model: mainNavModel
					clip: true
					spacing: 6
					ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }
					ScrollIndicator.vertical: ScrollIndicator { visible: false }

					delegate: ItemDelegate {
						id: navItem
						required property int pageIndex
						required property string label
						required property string glyph

						width: navList.width
						height: 58
						highlighted: navIndex === pageIndex
						onClicked: navIndex = pageIndex
						padding: 0
						HoverHandler { cursorShape: Qt.PointingHandCursor }

						background: Rectangle {
							anchors.fill: parent
							radius: 12
							color: navItem.highlighted ? "#eef2ff" : (navItem.hovered ? "#f3f4f6" : "transparent")
							Rectangle {
								width: 3
								height: 30
								radius: 2
								color: navItem.highlighted ? "#4f46e5" : "transparent"
								anchors.left: parent.left
								anchors.leftMargin: 0
								anchors.verticalCenter: parent.verticalCenter
							}
						}

						contentItem: Column {
							anchors.left: parent.left
							anchors.leftMargin: 12
							anchors.verticalCenter: parent.verticalCenter
							width: Math.min(56, parent.width - anchors.leftMargin)
							spacing: 4
							Text {
								text: glyph
								color: navIndex === pageIndex ? "#111827" : "#6b7280"
								font.family: "Segoe MDL2 Assets"
								font.pixelSize: 19
								horizontalAlignment: Text.AlignHCenter
								width: parent.width
							}
							Text {
								text: label
								color: navIndex === pageIndex ? "#111827" : "#6b7280"
								font.pixelSize: 11
								horizontalAlignment: Text.AlignHCenter
								width: parent.width
								elide: Text.ElideRight
							}
						}
					}
				}

				Item { Layout.fillHeight: true }

				Rectangle {
					Layout.fillWidth: true
					Layout.leftMargin: 6
					Layout.rightMargin: 6
					height: 1
					color: "#e5e7eb"
				}

				ItemDelegate {
					id: settingsNavItem
					Layout.fillWidth: true
					height: 58
					highlighted: navIndex === 3
					onClicked: navIndex = 3
					padding: 0
					HoverHandler { cursorShape: Qt.PointingHandCursor }

					background: Rectangle {
						anchors.fill: parent
						radius: 12
						color: settingsNavItem.highlighted ? "#eef2ff" : (settingsNavItem.hovered ? "#f3f4f6" : "transparent")
						Rectangle {
							width: 3
							height: 30
							radius: 2
							color: settingsNavItem.highlighted ? "#4f46e5" : "transparent"
							anchors.left: parent.left
							anchors.leftMargin: 0
							anchors.verticalCenter: parent.verticalCenter
						}
					}

					contentItem: Column {
						anchors.left: parent.left
						anchors.leftMargin: 10
						anchors.verticalCenter: parent.verticalCenter
						width: Math.min(56, parent.width - anchors.leftMargin)
						spacing: 4
						Text {
							text: "\uE713"
							color: navIndex === 3 ? "#111827" : "#6b7280"
							font.family: "Segoe MDL2 Assets"
							font.pixelSize: 19
							horizontalAlignment: Text.AlignHCenter
							width: parent.width
						}
						Text {
							text: qsTr("设置")
							color: navIndex === 3 ? "#111827" : "#6b7280"
							font.pixelSize: 11
							horizontalAlignment: Text.AlignHCenter
							width: parent.width
							elide: Text.ElideRight
						}
					}
				}
			}
		}

		StackLayout {
			id: pages
			Layout.fillWidth: true
			Layout.fillHeight: true
			currentIndex: navIndex

			Item {
				Layout.fillWidth: true
				Layout.fillHeight: true
				Frame {
					anchors.fill: parent
					background: Rectangle {
						color: "#ffffff"
						radius: 16
						border.color: "#cbd5e1"
						border.width: 1
					}
					ColumnLayout {
						anchors.fill: parent
						anchors.margins: 16
						spacing: 10

						Text {
							text: qsTr("首页")
							color: "#111827"
							font.pixelSize: 20
							font.weight: Font.DemiBold
						}
						Text {
							text: qsTr("这里预留给后续扩展的业务入口。")
							color: "#6b7280"
							font.pixelSize: 13
							wrapMode: Text.Wrap
							Layout.fillWidth: true
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
                                onTextChanged: {
                                    if (musicController && text.length > 0) {
                                        musicController.searchSuggest(text)
                                    }
                                }
                                Popup {
                                    id: searchSuggestPopup
                                    y: parent.height + 4
                                    width: parent.width
                                    padding: 4
                                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                                    visible: searchInput.activeFocus && searchInput.text.length > 0 && musicController && musicController.searchSuggestions.length > 0
                                    background: Rectangle {
                                        color: "#ffffff"
                                        radius: 8
                                        border.color: "#e2e8f0"
                                        border.width: 1
                                        layer.enabled: true
                                        layer.effect: MultiEffect {
                                            shadowEnabled: true
                                            shadowColor: "#40000000"
                                            shadowBlur: 1.0
                                            shadowHorizontalOffset: 0
                                            shadowVerticalOffset: 4
                                        }
                                    }
                                    contentItem: ListView {
                                        id: suggestListView
                                        implicitHeight: Math.min(count * 40, 320)
                                        model: musicController ? musicController.searchSuggestions : []
                                        clip: true
                                        delegate: ItemDelegate {
                                            id: delegateItem
                                            width: suggestListView.width
                                            height: 40
                                            padding: 12
                                            
                                            contentItem: Text {
                                                text: modelData
                                                font.pixelSize: 14
                                                color: "#1e293b" // slate-800
                                                elide: Text.ElideRight
                                                verticalAlignment: Text.AlignVCenter
                                            }

                                            background: Rectangle {
                                                color: delegateItem.hovered ? "#f1f5f9" : "transparent" // slate-100
                                                radius: 4
                                            }

                                            onClicked: {
                                                searchInput.text = modelData
                                                musicController.search(modelData)
                                                searchSuggestPopup.close()
                                                searchInput.focus = false
                                            }
                                        }
                                    }
                                }
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
							Layout.preferredWidth: 500
							Layout.fillWidth: true
							Layout.fillHeight: true
							background: Rectangle {
								color: "#ffffff"
								radius: 16
								border.color: "#cbd5e1"
								border.width: 1
							}
                            Popup {
                                id: songActionPopup
                                parent: appWindow.overlay
                                modal: false
                                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                                background: Rectangle {
                                    color: "#ffffff"
                                    radius: 12
                                    border.color: "#cbd5e1"
                                    border.width: 1
                                }
                                property real targetX: 0
                                property real targetY: 0
                                property bool logActive: false
                                property bool anchorLeft: true
                                property bool anchorBottom: true
                                property string anchorCorner: "topLeft"
                                property bool anchorContent: true
                                property real clickX: 0
                                property real clickY: 0
                                property int edgeMargin: 8
                                x: Math.max(edgeMargin, Math.min(anchorLeft ? targetX : (targetX - width), appWindow.width - width - edgeMargin))
                                y: Math.max(edgeMargin, Math.min(anchorBottom ? (targetY - height) : targetY, appWindow.height - height - edgeMargin))
                                property real phi: 1.618
                                property int baseW: Math.round(Math.min(360, Math.max(240, listView.width * 0.40)))
                                property int minW: 240
                                property int maxW: 340
                                property int headerSpacing: 10
                                width: Math.round(Math.min(maxW,
                                                          Math.max(minW,
                                                                   innerPadding * 2 + coverSize + headerSpacing + Math.max(titleText.implicitWidth, artistsText.implicitWidth))))
                                property int innerPadding: 10
                                property int contentSpacing: 10
                                property int dividerHeight: 1
                                property int titleSize: 15
                                property int artistSize: 12
                                property real lineHeightScale: 1.35
                                property int textSpacing: 6
                                property int coverSize: Math.round(titleSize * lineHeightScale + artistSize * lineHeightScale + textSpacing)
                                height: Math.round(innerPadding * 2 + coverSize + dividerHeight + contentSpacing * 3 + actionHeight * 2)
                                property int actionIconSize: Math.round(Math.max(18, Math.min(24, Math.round(width * 0.07))))
                                property int actionWidth: Math.round(width - 28)
                                property int actionHeight: Math.round(actionIconSize + 12)
                                property int songIndex: -1
                                property string songTitle: ""
                                property string songArtists: ""
                                property url songCover: ""
                                contentItem: ColumnLayout {
                                    anchors.margins: songActionPopup.innerPadding
                                    spacing: songActionPopup.contentSpacing
                                    RowLayout {
                                        spacing: songActionPopup.headerSpacing
                                        Image { source: getThumb(songActionPopup.songCover, 100); width: songActionPopup.coverSize; height: songActionPopup.coverSize; fillMode: Image.PreserveAspectCrop; sourceSize.width: songActionPopup.coverSize; sourceSize.height: songActionPopup.coverSize; visible: status === Image.Ready }
                                        ColumnLayout {
                                            spacing: 6
                                            Text { id: titleText; text: songActionPopup.songTitle; color: "#111827"; font.pixelSize: 15; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.preferredWidth: Math.round(songActionPopup.width - songActionPopup.coverSize - songActionPopup.headerSpacing) }
                                            Text { id: artistsText; text: songActionPopup.songArtists; color: "#6b7280"; font.pixelSize: 12; elide: Text.ElideRight; Layout.preferredWidth: Math.round(songActionPopup.width - songActionPopup.coverSize - songActionPopup.headerSpacing) }
                                        }
                                    }
                                    Rectangle { height: 1; color: "#e5e7eb"; Layout.fillWidth: true }
                                    RowLayout {
                                        id: actionPlayRow
                                        spacing: 8
                                        Layout.alignment: Qt.AlignLeft
                                        Layout.fillWidth: true
                                        MouseArea {
                                            id: playActionMouse
                                            acceptedButtons: Qt.LeftButton
                                            onClicked: {
                                                if (musicController) {
                                                    if (songActionPopup.sourceType === "queue") {
                                                        musicController.playIndex(songActionPopup.songIndex)
                                                    } else {
                                                        musicController.queuePlayFromSearchIndex(songActionPopup.songIndex)
                                                    }
                                                }
                                                songActionPopup.close()
                                            }
                                            cursorShape: Qt.PointingHandCursor
                                            anchors.fill: undefined
                                            width: songActionPopup.actionWidth; height: songActionPopup.actionHeight
                                            Rectangle { anchors.fill: parent; radius: 6; color: parent.pressed ? "#f3f4f6" : "transparent" }
                                            RowLayout { id: playContentRow; anchors.fill: parent; anchors.margins: 6; spacing: 10; Image { source: iconPlay; width: songActionPopup.actionIconSize; height: songActionPopup.actionIconSize; fillMode: Image.PreserveAspectFit; sourceSize.width: songActionPopup.actionIconSize; sourceSize.height: songActionPopup.actionIconSize } Text { text: qsTr("播放"); color: "#111827"; font.pixelSize: 13 } Item { Layout.fillWidth: true } }
                                        }
                                    }
                                    RowLayout {
                                        id: actionNextRow
                                        spacing: 8
                                        Layout.alignment: Qt.AlignLeft
                                        Layout.fillWidth: true
                                        MouseArea {
                                            id: nextActionMouse
                                            acceptedButtons: Qt.LeftButton
                                            onClicked: {
                                                if (musicController) {
                                                    if (songActionPopup.sourceType === "queue") {
                                                        musicController.queueRemoveAt(songActionPopup.songIndex)
                                                    } else {
                                                        musicController.queueAddFromSearchIndex(songActionPopup.songIndex, true)
                                                    }
                                                }
                                                songActionPopup.close()
                                            }
                                            cursorShape: Qt.PointingHandCursor
                                            width: songActionPopup.actionWidth; height: songActionPopup.actionHeight
                                            Rectangle { anchors.fill: parent; radius: 6; color: parent.pressed ? "#f3f4f6" : "transparent" }
                                            RowLayout {
                                                id: nextContentRow
                                                anchors.fill: parent
                                                anchors.margins: 6
                                                spacing: 10
                                                Image {
                                                    source: songActionPopup.sourceType === "queue" ? iconDelete : iconNext
                                                    width: songActionPopup.actionIconSize
                                                    height: songActionPopup.actionIconSize
                                                    fillMode: Image.PreserveAspectFit
                                                    sourceSize.width: songActionPopup.actionIconSize
                                                    sourceSize.height: songActionPopup.actionIconSize
                                                }
                                                Text {
                                                    text: songActionPopup.sourceType === "queue" ? qsTr("从队列删除") : qsTr("下一首播放")
                                                    color: "#111827"
                                                    font.pixelSize: 13
                                                }
                                                Item { Layout.fillWidth: true }
                                            }
                                        }
                                    }
                                }
                                property string sourceType: "search"

                                onOpened: if (logActive) {
                                    var TLx = x; var TLy = y
                                    var dxTL = TLx - clickX; var dyTL = TLy - clickY
                                }
                                onClosed: logActive = false
                                function openFor(index, title, artists, coverUrl, originItem, mouse, type) {
                                    songIndex = index; songTitle = title; songArtists = artists; songCover = coverUrl
                                    sourceType = type || "search"
                                    
                                    // Use popup's parent for coordinate mapping to ensure correct positioning
                                    var popupParent = songActionPopup.parent
                                    var p = originItem.mapToItem(popupParent, mouse.x, mouse.y)
                                    clickX = p.x
                                    clickY = p.y
                                    
                                    if (anchorCorner === "auto") {
                                        anchorLeft = (clickX + width + edgeMargin) <= appWindow.width
                                        anchorBottom = (clickY + height + edgeMargin) <= appWindow.height
                                    } else {
                                        anchorLeft = (anchorCorner === "topLeft" || anchorCorner === "bottomLeft")
                                        anchorBottom = (anchorCorner === "bottomLeft" || anchorCorner === "bottomRight")
                                    }
                                    var tX = clickX
                                    var tY = clickY
                                    if (anchorContent) {
                                        tX = clickX + (anchorLeft ? -innerPadding : innerPadding)
                                        tY = clickY + (anchorBottom ? innerPadding : -innerPadding)
                                    }
                                    targetX = tX
                                    targetY = tY
                                    logActive = true
                                    open()
                                }
                            }
                            ListView {
								id: listView
								anchors.fill: parent
								anchors.margins: 8
								model: musicController ? musicController.songsModel : null
								clip: true
								ScrollBar.vertical: ScrollBar { active: true }
                                onContentYChanged: {
                                    if (contentHeight > height && contentY > (contentHeight - height - 100)) {
                                        if (musicController && !musicController.loading && musicController.searchHasMore) {
                                            musicController.loadNextSearchPage()
                                        }
                                    }
                                }

                                footer: Item {
                                    width: listView.width
                                    height: 50
                                    visible: musicController && (musicController.loading || (musicController.searchHasMore && listView.count > 0) || (!musicController.searchHasMore && listView.count > 0))
                                    
                                    BusyIndicator {
                                        anchors.centerIn: parent
                                        running: musicController && musicController.loading
                                        visible: running
                                    }
                                    
                                    Text {
                                        anchors.centerIn: parent
                                        text: qsTr("没有更多了")
                                        visible: musicController && !musicController.loading && !musicController.searchHasMore && listView.count > 0
                                        color: "#9ca3af"
                                        font.pixelSize: 12
                                    }
                                }

								delegate: Rectangle {
									width: listView.width
									height: 56
									radius: 10
                                    property bool current: false
									property bool hovered: false
                                    // 搜索列表没有直接的 current 状态，暂不处理标题变色，或者如果能获取到 id
                                    // 这里假设搜索结果列表不显示“正在播放”状态，只响应播放操作
                                    property bool isPlayingThis: false // TODO: 需要通过 ID 判断

                                    color: (hovered || ListView.isCurrentItem) ? "#f1f5f9" : "#ffffff"
                                    border.color: "transparent"

                                    MouseArea {
                                        id: searchItemMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        onEntered: hovered = true
                                        onExited: hovered = false
                                        onClicked: function(mouse){
                                            listView.currentIndex = index
                                        }
                                        onDoubleClicked: function(mouse){
                                            if (mouse.button === Qt.LeftButton) {
                                                if (musicController) musicController.queuePlayFromSearchIndex(index)
                                                currentPlaylistIndex = -1
                                            }
                                        }
                                        onPressed: function(mouse){
                                            if (mouse.button === Qt.RightButton) {
                                                var cover = coverUrl
                                                songActionPopup.openFor(index, title, artists, cover, searchItemMouse, mouse)
                                            }
                                        }
                                    }

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

										Item {
											Layout.preferredWidth: 40
											Layout.preferredHeight: 40
											
											Rectangle {
												anchors.fill: parent
												color: "#e5e7eb"
												radius: 4
											}

											Image {
												source: getThumb(model.coverUrl, 100)
												anchors.fill: parent
												fillMode: Image.PreserveAspectCrop
											}
										}

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											
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
												font.pixelSize: 12
												elide: Text.ElideRight
												Layout.fillWidth: true
											}
										}

                                        Rectangle {
                                            Layout.preferredWidth: 80
                                            Layout.preferredHeight: 32
                                            radius: 16
                                            color: "#f3f4f6"
                                            
                                            RowLayout {
                                                anchors.fill: parent
                                                spacing: 0
                                                
                                                // Like Button
                                                Item {
                                                    Layout.fillHeight: true
                                                    Layout.preferredWidth: 40
                                                    
                                                    property bool isLiked: musicController && musicController.isLiked(model.id)
                                                    
                                                    Connections {
                                                        target: musicController
                                                        function onSongLikeStateChanged(songId, liked) {
                                                            if (songId === model.id) isLiked = liked
                                                        }
                                                    }
                                                    
                                                    Image {
                                                        anchors.centerIn: parent
                                                        width: 20
                                                        height: 20
                                                        source: parent.isLiked ? "file:///d:/project/I-love-you/qt-rewrite/ui-asset/black-backgroud/music_like/喜欢.svg" : "file:///d:/project/I-love-you/qt-rewrite/ui-asset/black-backgroud/music_like/not喜欢.svg"
                                                        fillMode: Image.PreserveAspectFit
                                                    }
                                                    MouseArea {
                                                        anchors.fill: parent
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: if (musicController) musicController.toggleLike(model.id)
                                                    }
                                                }
                                                
                                                // Play Button
                                                Item {
                                                    Layout.fillHeight: true
                                                    Layout.preferredWidth: 40
                                                    
                                                    Rectangle {
                                                        anchors.centerIn: parent
                                                        width: 28
                                                        height: 28
                                                        radius: 14
                                                        color: searchDelegate.isPlayingThis ? "#22c55e" : "transparent"
                                                    }
                                                    
                                                    Image {
                                                        id: searchPlayIcon
                                                        anchors.centerIn: parent
                                                        width: 20
                                                        height: 20
                                                        source: "file:///d:/project/I-love-you/qt-rewrite/ui-asset/black-backgroud/播放.svg"
                                                        fillMode: Image.PreserveAspectFit
                                                        visible: !searchDelegate.isPlayingThis
                                                    }
                                                    
                                                    MultiEffect {
                                                        source: searchPlayIcon
                                                        anchors.fill: searchPlayIcon
                                                        colorization: 1.0
                                                        colorizationColor: "#ffffff"
                                                        visible: searchDelegate.isPlayingThis
                                                    }

                                                    MouseArea {
                                                        anchors.fill: parent
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            if (musicController) musicController.queuePlayFromSearchIndex(index)
                                                            currentPlaylistIndex = -1
                                                        }
                                                    }
                                                }
                                            }
                                        }
									}
								}
							}
						}

						Frame {
							Layout.preferredWidth: 500
							Layout.fillWidth: true
							Layout.fillHeight: true
							ColumnLayout {
								anchors.fill: parent
								spacing: 12

								Image {
									id: coverImage
									Layout.fillWidth: true
									Layout.preferredHeight: 200
									source: musicController ? getThumb(musicController.coverSource, 200) : ""
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

                            Popup {
                                id: playlistSongActionPopup
                                parent: appWindow.overlay
                                modal: false
                                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                                background: Rectangle {
                                    color: "#ffffff"
                                    radius: 12
                                    border.color: "#cbd5e1"
                                    border.width: 1
                                }
                                property real targetX: 0
                                property real targetY: 0
                                property bool logActive: false
                                property bool anchorLeft: true
                                property bool anchorBottom: true
                                property string anchorCorner: "topLeft"
                                property bool anchorContent: true
                                property real clickX: 0
                                property real clickY: 0
                                property int edgeMargin: 8
                                x: Math.max(edgeMargin, Math.min(anchorLeft ? targetX : (targetX - width), appWindow.width - width - edgeMargin))
                                y: Math.max(edgeMargin, Math.min(anchorBottom ? (targetY - height) : targetY, appWindow.height - height - edgeMargin))
                                property real phi: 1.618
                                property int baseW: Math.round(Math.min(360, Math.max(240, playlistView.width * 0.40)))
                                property int minW: 240
                                property int maxW: 340
                                property int headerSpacing: 10
                                width: Math.round(Math.min(maxW,
                                                          Math.max(minW,
                                                                   innerPadding * 2 + coverSize + headerSpacing + Math.max(playlistTitleText.implicitWidth, playlistArtistsText.implicitWidth))))
                                property int innerPadding: 10
                                property int contentSpacing: 10
                                property int dividerHeight: 1
                                property int titleSize: 15
                                property int artistSize: 12
                                property real lineHeightScale: 1.35
                                property int textSpacing: 6
                                property int coverSize: Math.round(titleSize * lineHeightScale + artistSize * lineHeightScale + textSpacing)
                                height: Math.round(innerPadding * 2 + coverSize + dividerHeight + contentSpacing * 3 + actionHeight * 2)
                                property int actionIconSize: Math.round(Math.max(18, Math.min(24, Math.round(width * 0.07))))
                                property int actionWidth: Math.round(width - 28)
                                property int actionHeight: Math.round(actionIconSize + 12)
                                property int songIndex: -1
                                property string songTitle: ""
                                property string songArtists: ""
                                property url songCover: ""
                                contentItem: ColumnLayout {
                                    anchors.margins: playlistSongActionPopup.innerPadding
                                    spacing: playlistSongActionPopup.contentSpacing
                                    RowLayout {
                                        spacing: playlistSongActionPopup.headerSpacing
                                        Image { source: getThumb(playlistSongActionPopup.songCover, 100); width: playlistSongActionPopup.coverSize; height: playlistSongActionPopup.coverSize; fillMode: Image.PreserveAspectCrop; sourceSize.width: playlistSongActionPopup.coverSize; sourceSize.height: playlistSongActionPopup.coverSize; visible: status === Image.Ready }
                                        ColumnLayout {
                                            spacing: 6
                                            Text { id: playlistTitleText; text: playlistSongActionPopup.songTitle; color: "#111827"; font.pixelSize: 15; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.preferredWidth: Math.round(playlistSongActionPopup.width - playlistSongActionPopup.coverSize - playlistSongActionPopup.headerSpacing) }
                                            Text { id: playlistArtistsText; text: playlistSongActionPopup.songArtists; color: "#6b7280"; font.pixelSize: 12; elide: Text.ElideRight; Layout.preferredWidth: Math.round(playlistSongActionPopup.width - playlistSongActionPopup.coverSize - playlistSongActionPopup.headerSpacing) }
                                        }
                                    }
                                    Rectangle { height: 1; color: "#e5e7eb"; Layout.fillWidth: true }
                                    RowLayout {
                                        id: playlistActionPlayRow
                                        spacing: 8
                                        Layout.alignment: Qt.AlignLeft
                                        Layout.fillWidth: true
                                        MouseArea {
                                            id: playlistPlayActionMouse
                                            acceptedButtons: Qt.LeftButton
                                            onClicked: {
                                                if (musicController) {
                                                    musicController.playPlaylistTrack(playlistSongActionPopup.songIndex)
                                                }
                                                playlistSongActionPopup.close()
                                            }
                                            cursorShape: Qt.PointingHandCursor
                                            anchors.fill: undefined
                                            width: playlistSongActionPopup.actionWidth; height: playlistSongActionPopup.actionHeight
                                            Rectangle { anchors.fill: parent; radius: 6; color: parent.pressed ? "#f3f4f6" : "transparent" }
                                            RowLayout { id: playlistPlayContentRow; anchors.fill: parent; anchors.margins: 6; spacing: 10; Image { source: iconPlay; width: playlistSongActionPopup.actionIconSize; height: playlistSongActionPopup.actionIconSize; fillMode: Image.PreserveAspectFit; sourceSize.width: playlistSongActionPopup.actionIconSize; sourceSize.height: playlistSongActionPopup.actionIconSize } Text { text: qsTr("播放"); color: "#111827"; font.pixelSize: 13 } Item { Layout.fillWidth: true } }
                                        }
                                    }
                                    RowLayout {
                                        id: playlistActionNextRow
                                        spacing: 8
                                        Layout.alignment: Qt.AlignLeft
                                        Layout.fillWidth: true
                                        MouseArea {
                                            id: playlistNextActionMouse
                                            acceptedButtons: Qt.LeftButton
                                            onClicked: {
                                                if (musicController) {
                                                    musicController.queueAddFromPlaylistIndex(playlistSongActionPopup.songIndex, true)
                                                }
                                                playlistSongActionPopup.close()
                                            }
                                            cursorShape: Qt.PointingHandCursor
                                            width: playlistSongActionPopup.actionWidth; height: playlistSongActionPopup.actionHeight
                                            Rectangle { anchors.fill: parent; radius: 6; color: parent.pressed ? "#f3f4f6" : "transparent" }
                                            RowLayout {
                                                id: playlistNextContentRow
                                                anchors.fill: parent
                                                anchors.margins: 6
                                                spacing: 10
                                                Image {
                                                    source: iconNext
                                                    width: playlistSongActionPopup.actionIconSize
                                                    height: playlistSongActionPopup.actionIconSize
                                                    fillMode: Image.PreserveAspectFit
                                                    sourceSize.width: playlistSongActionPopup.actionIconSize
                                                    sourceSize.height: playlistSongActionPopup.actionIconSize
                                                }
                                                Text {
                                                    text: qsTr("下一首播放")
                                                    color: "#111827"
                                                    font.pixelSize: 13
                                                }
                                                Item { Layout.fillWidth: true }
                                            }
                                        }
                                    }
                                }
                                property string sourceType: "playlist"

                                onOpened: if (logActive) {
                                    var TLx = x; var TLy = y
                                    var dxTL = TLx - clickX; var dyTL = TLy - clickY
                                }
                                onClosed: logActive = false
                                function openFor(index, title, artists, coverUrl, originItem, mouse, type) {
                                    songIndex = index; songTitle = title; songArtists = artists; songCover = coverUrl
                                    sourceType = type || "playlist"
                                    
                                    var popupParent = playlistSongActionPopup.parent
                                    var p = originItem.mapToItem(popupParent, mouse.x, mouse.y)
                                    clickX = p.x
                                    clickY = p.y
                                    
                                    if (anchorCorner === "auto") {
                                        anchorLeft = (clickX + width + edgeMargin) <= appWindow.width
                                        anchorBottom = (clickY + height + edgeMargin) <= appWindow.height
                                    } else {
                                        anchorLeft = (anchorCorner === "topLeft" || anchorCorner === "bottomLeft")
                                        anchorBottom = (anchorCorner === "bottomLeft" || anchorCorner === "bottomRight")
                                    }
                                    var tX = clickX
                                    var tY = clickY
                                    if (anchorContent) {
                                        tX = clickX + (anchorLeft ? -innerPadding : innerPadding)
                                        tY = clickY + (anchorBottom ? innerPadding : -innerPadding)
                                    }
                                    targetX = tX
                                    targetY = tY
                                    logActive = true
                                    open()
                                }
                            }

				RowLayout {
					anchors.fill: parent
					spacing: 0

					// Left Panel: User Playlists
					Frame {
						Layout.preferredWidth: 260
						Layout.fillHeight: true
						background: Rectangle { color: "#f9fafb"; border.width: 0; }
						
						ColumnLayout {
							anchors.fill: parent
							anchors.margins: 0
							spacing: 0
							
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: 48
								color: "transparent"
								Text {
									text: qsTr("我的歌单")
									anchors.left: parent.left
									anchors.leftMargin: 16
									anchors.verticalCenter: parent.verticalCenter
									font.pixelSize: 16
									font.weight: Font.DemiBold
									color: "#111827"
								}
								Button {
									text: qsTr("刷新")
									anchors.right: parent.right
									anchors.rightMargin: 8
									anchors.verticalCenter: parent.verticalCenter
									flat: true
									onClicked: if (musicController) musicController.loadUserPlaylist()
								}
							}
							
							Component {
								id: userPlaylistDelegate
								Item {
									width: ListView.view.width
									height: 56
									Rectangle {
										anchors.fill: parent
										anchors.margins: 4
										radius: 8
										color: hoverHandler.hovered ? "#e5e7eb" : "transparent"
										
										HoverHandler { id: hoverHandler }

										MouseArea {
											anchors.fill: parent
											onClicked: {
												if (musicController) musicController.loadPlaylist(model.id)
											}
										}
										
										RowLayout {
											anchors.fill: parent
											anchors.leftMargin: 8
											anchors.rightMargin: 8
											spacing: 10
											
											Item {
												Layout.preferredWidth: 40
												Layout.preferredHeight: 40
												
												Rectangle {
													anchors.fill: parent
													color: "#e5e7eb"
													radius: 4
												}

												Image {
													source: getThumb(model.coverUrl, 100)
													anchors.fill: parent
													fillMode: Image.PreserveAspectCrop
												}
											}
											
											ColumnLayout {
												Layout.fillWidth: true
												spacing: 2
												Text {
													text: model.name
													elide: Text.ElideRight
													Layout.fillWidth: true
													color: "#111827"
													font.pixelSize: 13
													font.weight: Font.Medium
												}
												Text {
													text: model.trackCount + qsTr("首")
													color: "#6b7280"
													font.pixelSize: 11
												}
											}

											Button {
												text: qsTr("导入")
												Layout.preferredHeight: 24
												Layout.preferredWidth: 48
												font.pixelSize: 11
												flat: true
												background: Rectangle {
													color: parent.down ? "#d1d5db" : (parent.hovered ? "#e5e7eb" : "transparent")
													radius: 4
													border.color: "#d1d5db"
												}
												onClicked: {
													if (musicController) musicController.importPlaylistToQueue(model.id)
												}
											}
										}
									}
								}
							}

							ScrollView {
								id: userPlaylistScrollView
								Layout.fillWidth: true
								Layout.fillHeight: true
								clip: true
								ScrollBar.vertical: ScrollBar { }

								property bool createdExpanded: true
								property bool collectedExpanded: true

								ColumnLayout {
									width: userPlaylistScrollView.availableWidth
									spacing: 0
									
									// Header: Created
									Item {
										Layout.fillWidth: true
										Layout.preferredHeight: 32
										visible: createdPlaylistView.count > 0
										
										MouseArea {
											anchors.fill: parent
											cursorShape: Qt.PointingHandCursor
											onClicked: userPlaylistScrollView.createdExpanded = !userPlaylistScrollView.createdExpanded
										}

										RowLayout {
											anchors.fill: parent
											anchors.leftMargin: 16
											anchors.rightMargin: 16
											spacing: 4

											Text {
												text: qsTr("创建的歌单") + "(" + createdPlaylistView.count + ")"
												font.pixelSize: 12
												color: "#6b7280"
												font.weight: Font.Bold
											}

											Image {
												source: "file:///d:/project/I-love-you/qt-rewrite/ui-asset/black-backgroud/login/下拉.svg"
												Layout.preferredWidth: 12
												Layout.preferredHeight: 12
												fillMode: Image.PreserveAspectFit
												opacity: 0.6
												rotation: userPlaylistScrollView.createdExpanded ? 180 : 0
												Behavior on rotation { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }
											}
											
											Item { Layout.fillWidth: true }
										}
									}

									ListView {
										id: createdPlaylistView
										Layout.fillWidth: true
										Layout.preferredHeight: userPlaylistScrollView.createdExpanded ? contentHeight : 0
										opacity: userPlaylistScrollView.createdExpanded ? 1.0 : 0.0
										visible: opacity > 0
										interactive: false
										model: musicController ? musicController.createdPlaylistModel : null
										delegate: userPlaylistDelegate

										Behavior on Layout.preferredHeight { NumberAnimation { duration: 300; easing.type: Easing.InOutQuad } }
										Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.InOutQuad } }
									}

									// Header: Collected
									Item {
										Layout.fillWidth: true
										Layout.preferredHeight: 32
										visible: collectedPlaylistView.count > 0
										
										MouseArea {
											anchors.fill: parent
											cursorShape: Qt.PointingHandCursor
											onClicked: userPlaylistScrollView.collectedExpanded = !userPlaylistScrollView.collectedExpanded
										}

										RowLayout {
											anchors.fill: parent
											anchors.leftMargin: 16
											anchors.rightMargin: 16
											spacing: 4

											Text {
												text: qsTr("收藏的歌单") + "(" + collectedPlaylistView.count + ")"
												font.pixelSize: 12
												color: "#6b7280"
												font.weight: Font.Bold
											}

											Image {
												source: "file:///d:/project/I-love-you/qt-rewrite/ui-asset/black-backgroud/login/下拉.svg"
												Layout.preferredWidth: 12
												Layout.preferredHeight: 12
												fillMode: Image.PreserveAspectFit
												opacity: 0.6
												rotation: userPlaylistScrollView.collectedExpanded ? 180 : 0
												Behavior on rotation { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }
											}
											
											Item { Layout.fillWidth: true }
										}
									}

									ListView {
										id: collectedPlaylistView
										Layout.fillWidth: true
										Layout.preferredHeight: userPlaylistScrollView.collectedExpanded ? contentHeight : 0
										opacity: userPlaylistScrollView.collectedExpanded ? 1.0 : 0.0
										visible: opacity > 0
										interactive: false
										model: musicController ? musicController.collectedPlaylistModel : null
										delegate: userPlaylistDelegate

										Behavior on Layout.preferredHeight { NumberAnimation { duration: 300; easing.type: Easing.InOutQuad } }
										Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.InOutQuad } }
									}
								}
							}
							
							Rectangle {
								Layout.fillWidth: true
								height: 1
								color: "#e5e7eb"
							}
						}
						
						Rectangle {
							anchors.right: parent.right
							anchors.top: parent.top
							anchors.bottom: parent.bottom
							width: 1
							color: "#e5e7eb"
						}
					}

					// Right Panel: Playlist Content
					Item {
						Layout.fillWidth: true
						Layout.fillHeight: true
						
						ColumnLayout {
							anchors.fill: parent
							anchors.margins: 16
							spacing: 12

							BusyIndicator {
								running: musicController && musicController.playlistLoading
								visible: running
								Layout.alignment: Qt.AlignHCenter
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
									ScrollBar.vertical: ScrollBar { active: true }
                                    
                                    function updatePageSize() {
                                        if (musicController && height > 0) {
                                            // 56 is item height.
                                            var visibleCount = Math.ceil(height / 56)
                                            // Use a larger page size (min 50) to support fast scrolling
                                            var targetSize = Math.max(50, visibleCount * 3)
                                            musicController.playlistPageSize = targetSize
                                        }
                                    }

                                    onHeightChanged: updatePageSize()
                                    Component.onCompleted: updatePageSize()

									delegate: Rectangle {
                                        id: playlistDelegate
                                        // Notify controller about visible row to trigger prefetch/cleanup
                                        Component.onCompleted: {
                                            if (musicController) {
                                                musicController.onPlaylistRowRequested(index)
                                            }
                                        }
										width: playlistView.width
										height: 56
										radius: 10
										property bool current: index === currentPlaylistIndex
										property bool hovered: false
                                        property bool held: false
										color: (hovered || held || ListView.isCurrentItem) ? "#f1f5f9" : "#ffffff"
										border.color: "transparent"
                                        
                                        scale: held ? 1.02 : 1.0
                                        z: held ? 100 : 1
                                        Behavior on scale { NumberAnimation { duration: 100 } }

                                        Drag.active: held
                                        Drag.source: playlistDelegate
                                        Drag.hotSpot.x: width / 2
                                        Drag.hotSpot.y: height / 2

                                        MouseArea {
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            enabled: isLoaded
                                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                                            onEntered: hovered = true
                                            onExited: hovered = false
                                            
                                            drag.target: playlistDelegate.held ? playlistDelegate : undefined
                                            drag.axis: Drag.YAxis
                                            
                                            onPressAndHold: {
                                                if (mouse.button === Qt.LeftButton) playlistDelegate.held = true
                                            }
                                            onReleased: {
                                                playlistDelegate.held = false
                                            }
                                            
                                            onClicked: function(mouse) {
                                                if (playlistDelegate.held) return
                                                if (!musicController) return
                                                // Always select the item on click
                                                playlistView.currentIndex = index
                                                if (mouse.button === Qt.RightButton) {
                                                    var cover = coverUrl
                                                    playlistSongActionPopup.openFor(index, title, artists, cover, playlistDelegate, mouse, "playlist")
                                                }
                                            }
                                            onDoubleClicked: function(mouse) {
                                                if (playlistDelegate.held) return
                                                if (!musicController) return
                                                if (mouse.button === Qt.LeftButton) {
                                                    musicController.playPlaylistTrack(index)
                                                    currentPlaylistIndex = index
                                                }
                                            }
                                            
                                            onPositionChanged: function(mouse) {
                                                if (playlistDelegate.held) {
                                                    var targetIndex = playlistView.indexAt(playlistView.width / 2, playlistView.contentY + playlistDelegate.y + mouse.y)
                                                    if (targetIndex !== -1 && targetIndex !== index) {
                                                        if (musicController && musicController.playlistModel) {
                                                            musicController.playlistModel.move(index, targetIndex)
                                                        }
                                                    }
                                                }
                                            }
                                        }

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
                                                
                                                MouseArea {
                                                    anchors.fill: parent
                                                    cursorShape: Qt.OpenHandCursor
                                                    drag.target: playlistDelegate
                                                    
                                                    onPressed: {
                                                        playlistDelegate.held = true
                                                    }
                                                    onReleased: {
                                                        playlistDelegate.held = false
                                                    }
                                                }
											}

											Item {
												Layout.preferredWidth: 40
												Layout.preferredHeight: 40
												
												Rectangle {
													anchors.fill: parent
													color: "#e5e7eb"
													radius: 4
												}

												Image {
													source: getThumb(model.coverUrl, 100)
													anchors.fill: parent
													fillMode: Image.PreserveAspectCrop
													
													// Temporary disable MultiEffect
													// layer.enabled: true
													// layer.effect: MultiEffect {
													// 	maskEnabled: true
													// 	maskSource: playlistMask
													// }
												}
												/*
												Rectangle {
													id: playlistMask
													anchors.fill: parent
													radius: 4
													visible: false
												}
												*/
											}

											ColumnLayout {
												Layout.fillWidth: true
												spacing: 2
												
												Text {
													text: isLoaded ? title : qsTr("加载中...")
													color: (playlistDelegate.current) ? "#22c55e" : (isLoaded ? "#111827" : "#9ca3af")
													font.pixelSize: 14
													elide: Text.ElideRight
													Layout.fillWidth: true
												}
												
												Text {
													text: artists
													color: "#6b7280"
													font.pixelSize: 12
													elide: Text.ElideRight
													Layout.fillWidth: true
												}
											}

                                            Rectangle {
                                                Layout.preferredWidth: 80
                                                Layout.preferredHeight: 32
                                                radius: 16
                                                color: "#f3f4f6"
                                                
                                                RowLayout {
                                                    anchors.fill: parent
                                                    spacing: 0
                                                    
                                                    // Like Button
                                                    Item {
                                                        id: likeBtnItem
                                                        Layout.fillHeight: true
                                                        Layout.preferredWidth: 40
                                                        
                                                        property bool isLiked: musicController ? musicController.isLiked(songId) : false

                                                        Connections {
                                                            target: musicController
                                                            function onSongLikeStateChanged(id, state) {
                                                                if (id === songId) likeBtnItem.isLiked = state
                                                            }
                                                        }
                                                        
                                                        Image {
                                                            anchors.centerIn: parent
                                                            width: 20
                                                            height: 20
                                                            source: parent.isLiked ? "file:///d:/project/I-love-you/qt-rewrite/ui-asset/black-backgroud/music_like/喜欢.svg" : "file:///d:/project/I-love-you/qt-rewrite/ui-asset/black-backgroud/music_like/not喜欢.svg"
                                                            fillMode: Image.PreserveAspectFit
                                                        }
                                                        MouseArea {
                                                            anchors.fill: parent
                                                            cursorShape: Qt.PointingHandCursor
                                                            onClicked: if (musicController) musicController.toggleLike(songId)
                                                        }
                                                    }
                                                    
                                                    // Play Button
                                                    Item {
                                                        Layout.fillHeight: true
                                                        Layout.preferredWidth: 40
                                                        
                                                        Rectangle {
                                                            anchors.centerIn: parent
                                                            width: 28
                                                            height: 28
                                                            radius: 14
                                                            color: playlistDelegate.current ? "#22c55e" : "transparent"
                                                        }
                                                        
                                                        Image {
                                                            id: playlistPlayIcon
                                                            anchors.centerIn: parent
                                                            width: 20
                                                            height: 20
                                                            source: "file:///d:/project/I-love-you/qt-rewrite/ui-asset/black-backgroud/播放.svg"
                                                            fillMode: Image.PreserveAspectFit
                                                            visible: !playlistDelegate.current
                                                        }
                                                        
                                                        MultiEffect {
                                                            source: playlistPlayIcon
                                                            anchors.fill: playlistPlayIcon
                                                            colorization: 1.0
                                                            colorizationColor: "#ffffff"
                                                            visible: playlistDelegate.current
                                                        }

                                                        MouseArea {
                                                            anchors.fill: parent
                                                            cursorShape: Qt.PointingHandCursor
                                                            onClicked: {
                                                                if (musicController) musicController.playPlaylistTrack(index)
                                                                currentPlaylistIndex = index
                                                            }
                                                        }
                                                    }
                                                }
                                            }
										}
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
				Frame {
					anchors.fill: parent
					background: Rectangle {
						color: "#ffffff"
						radius: 16
						border.color: "#cbd5e1"
						border.width: 1
					}
					ColumnLayout {
						anchors.fill: parent
						anchors.margins: 16
						spacing: 10

						Text {
							text: qsTr("设置")
							color: "#111827"
							font.pixelSize: 20
							font.weight: Font.DemiBold
						}
						Text {
							text: qsTr("这里放应用设置、账号、缓存、音源等。")
							color: "#6b7280"
							font.pixelSize: 13
							wrapMode: Text.Wrap
							Layout.fillWidth: true
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
						source: musicController ? getThumb(musicController.coverSource, 140) : ""
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
						onClicked: if (musicController) musicController.playPrev()
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
						onClicked: if (musicController) musicController.playNext()
					}
				}

				RowLayout {
					Layout.fillWidth: true
					spacing: 10
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
					id: playbackModeBox
					width: footerBar.controlIconSize
					height: footerBar.controlIconSize
					Image {
						source: playbackModeIcon
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
						onClicked: if (musicController) musicController.cyclePlaybackMode()
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
						onClicked: {
                            var now = new Date().getTime()
                            if (now - queueDrawer.lastCloseTime < 300) return
							if (queueDrawer.opened) queueDrawer.close()
							else queueDrawer.open()
						}
					}
				}
				Drawer {
					id: queueDrawer
                    property real lastCloseTime: 0
					edge: Qt.RightEdge
					width: Math.round(Math.min(420, Math.min(parent.width * 0.36, (parent.height - 40) / 1.618)))
					height: Math.round(width * 1.618)
					y: Math.round((parent.height - height) / 2)
					modal: false
                    dim: false
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                    onAboutToHide: {
                        lastCloseTime = new Date().getTime()
                    }
					interactive: true
                    onOpened: {
                        if (musicController && musicController.currentSongIndex >= 0) {
                            queueListView.positionViewAtIndex(musicController.currentSongIndex, ListView.Center)
                        }
                    }
					background: Rectangle {
						radius: 12
						color: "#ffffff"
						border.color: "#cbd5e1"
						border.width: 1
					}
					contentItem: ColumnLayout {
						anchors.fill: parent
						anchors.margins: 12
						spacing: 10
						RowLayout {
							Layout.fillWidth: true
							Text {
								text: qsTr("播放列表")
								color: "#111827"
								font.pixelSize: 14
								font.weight: Font.DemiBold
								Layout.fillWidth: true
								elide: Text.ElideRight
							}
							Text {
								text: queueListView.count + qsTr(" 首")
								color: "#6b7280"
								font.pixelSize: 12
							}
							
							Rectangle {
								width: 28
								height: 28
								radius: 4
								color: clearBtnArea.containsMouse ? "#f3f4f6" : "transparent"
								visible: queueListView.count > 0

								Image {
									anchors.centerIn: parent
									source: iconDelete
									width: 16
									height: 16
									sourceSize: Qt.size(32, 32)
									opacity: 0.6
								}

								MouseArea {
									id: clearBtnArea
									anchors.fill: parent
									hoverEnabled: true
									cursorShape: Qt.PointingHandCursor
									onClicked: {
										if (musicController) musicController.queueClear()
									}
								}
								ToolTip.visible: clearBtnArea.containsMouse
								ToolTip.text: qsTr("清空列表")
							}
						}
						Frame {
							Layout.fillWidth: true
							Layout.fillHeight: true
							background: Rectangle {
								color: "#ffffff"
								radius: 10
								border.color: "#e5e7eb"
								border.width: 1
							}
                            ListView {
								id: queueListView
								anchors.fill: parent
								anchors.margins: 6
                                model: musicController ? musicController.queueModel : null
								clip: true
								ScrollBar.vertical: ScrollBar { active: true }
								delegate: Rectangle {
									id: delegateRoot
									width: queueListView.width
									height: 48
									radius: 8
									property bool current: musicController && index === musicController.currentSongIndex
									property bool hovered: false
									property bool held: false
									color: (hovered || held || ListView.isCurrentItem) ? "#f1f5f9" : "#ffffff"
									border.color: "transparent"
                                    
                                    scale: held ? 1.05 : 1.0
                                    z: held ? 100 : 1
                                    Behavior on scale { NumberAnimation { duration: 100 } }

                                    Drag.active: held
                                    Drag.source: delegateRoot
                                    Drag.hotSpot.x: width / 2
                                    Drag.hotSpot.y: height / 2

									MouseArea {
										id: dragArea
										anchors.fill: parent
										hoverEnabled: true
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
										onEntered: hovered = true
										onExited: hovered = false
                                        
                                        onPressed: function(mouse) {
                                            if (mouse.button === Qt.LeftButton) {
                                                // Long press detection could be better, but for now direct drag or click
                                            }
                                            if (mouse.button === Qt.RightButton) {
                                                var cover = (typeof coverUrl !== "undefined") ? coverUrl : ""
                                                songActionPopup.openFor(index, title, artists, cover, parent, mouse, "queue")
                                            }
                                        }

                                        onPressAndHold: {
                                            delegateRoot.held = true
                                        }
                                        onReleased: {
                                            delegateRoot.held = false
                                        }
                                        
                                        onClicked: function(mouse) {
                                            // Always select the item on click
                                            queueListView.currentIndex = index
                                        }
                                        onDoubleClicked: function(mouse) {
                                            if (mouse.button === Qt.LeftButton && !delegateRoot.held) {
                                                if (musicController) musicController.playIndex(index)
                                                queueDrawer.close()
                                            }
                                        }
                                        
                                        drag.target: delegateRoot.held ? delegateRoot : undefined
                                        drag.axis: Drag.YAxis
                                        
                                        onPositionChanged: function(mouse) {
                                            if (delegateRoot.held) {
                                                // Basic reordering logic
                                                var yPos = delegateRoot.y + delegateRoot.height / 2 + mouse.y
                                                // Map to list view coordinates if needed, but here we can check index
                                                var targetIndex = queueListView.indexAt(queueListView.width / 2, queueListView.contentY + delegateRoot.y + mouse.y)
                                                
                                                if (targetIndex !== -1 && targetIndex !== index) {
                                                    if (musicController && musicController.queueModel) {
                                                        musicController.queueModel.move(index, targetIndex)
                                                    }
                                                }
                                            }
                                        }
									}
									RowLayout {
										anchors.fill: parent
										anchors.margins: 10
										spacing: 8
										Text {
                                            id: indexText
											text: index + 1
											color: "#9ca3af"
											font.pixelSize: 12
											Layout.preferredWidth: 28
											horizontalAlignment: Text.AlignHCenter
											verticalAlignment: Text.AlignVCenter
                                            
                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.OpenHandCursor
                                                drag.target: delegateRoot
                                                
                                                onPressed: {
                                                    delegateRoot.held = true
                                                }
                                                onReleased: {
                                                    delegateRoot.held = false
                                                }
                                            }
										}

										Item {
											Layout.preferredWidth: 32
											Layout.preferredHeight: 32
											
											Rectangle {
												anchors.fill: parent
												color: "#e5e7eb"
												radius: 4
											}

											Image {
												source: getThumb(model.coverUrl, 80)
												anchors.fill: parent
												fillMode: Image.PreserveAspectCrop
											}
										}

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2
                                            
                                            Text {
                                                text: title
                                                color: delegateRoot.current ? "#22c55e" : "#111827"
                                                font.pixelSize: 13
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                            
                                            Text {
                                                text: artists
                                                color: "#6b7280"
                                                font.pixelSize: 12
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                        }

                                        Rectangle {
                                            Layout.preferredWidth: 80
                                            Layout.preferredHeight: 32
                                            radius: 16
                                            color: "#f3f4f6"
                                            
                                            RowLayout {
                                                anchors.fill: parent
                                                spacing: 0
                                                
                                                // Like Button
                                                Item {
                                                    id: queueLikeBtn
                                                    Layout.fillHeight: true
                                                    Layout.preferredWidth: 40
                                                    
                                                    property bool isLiked: musicController && musicController.isLiked(model.id)
                                                    
                                                    Connections {
                                                        target: musicController
                                                        function onSongLikeStateChanged(songId, liked) {
                                                            if (songId === model.id) queueLikeBtn.isLiked = liked
                                                        }
                                                    }
                                                    
                                                    Image {
                                                        anchors.centerIn: parent
                                                        width: 20
                                                        height: 20
                                                        source: parent.isLiked ? "file:///d:/project/I-love-you/qt-rewrite/ui-asset/black-backgroud/music_like/喜欢.svg" : "file:///d:/project/I-love-you/qt-rewrite/ui-asset/black-backgroud/music_like/not喜欢.svg"
                                                        fillMode: Image.PreserveAspectFit
                                                    }
                                                    MouseArea {
                                                        anchors.fill: parent
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: if (musicController) musicController.toggleLike(model.id)
                                                    }
                                                }
                                                
                                                // Play Button
                                                Item {
                                                    Layout.fillHeight: true
                                                    Layout.preferredWidth: 40
                                                    
                                                    Rectangle {
                                                        anchors.centerIn: parent
                                                        width: 28
                                                        height: 28
                                                        radius: 14
                                                        color: delegateRoot.current ? "#22c55e" : "transparent"
                                                    }
                                                    
                                                    Image {
                                                        id: queuePlayIcon
                                                        anchors.centerIn: parent
                                                        width: 20
                                                        height: 20
                                                        source: "file:///d:/project/I-love-you/qt-rewrite/ui-asset/black-backgroud/播放.svg"
                                                        fillMode: Image.PreserveAspectFit
                                                        visible: !delegateRoot.current
                                                    }
                                                    
                                                    MultiEffect {
                                                        source: queuePlayIcon
                                                        anchors.fill: queuePlayIcon
                                                        colorization: 1.0
                                                        colorizationColor: "#ffffff"
                                                        visible: delegateRoot.current
                                                    }

                                                    MouseArea {
                                                        anchors.fill: parent
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            if (musicController) musicController.playIndex(index)
                                                            // queueDrawer.close() // Optional, keeping open for now
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        Item {
                                             Layout.preferredWidth: 32
                                             Layout.preferredHeight: 32
                                             
                                             Image {
                                                 id: deleteIconSrc
                                                 source: iconDelete
                                                 anchors.fill: parent
                                                 anchors.margins: 6
                                                 fillMode: Image.PreserveAspectFit
                                                 visible: false
                                             }

                                             MultiEffect {
                                                 source: deleteIconSrc
                                                 anchors.fill: deleteIconSrc
                                                 colorization: 1.0
                                                 colorizationColor: deleteMouse.containsMouse ? "#ef4444" : "#3D3D3D"
                                                 Behavior on colorizationColor { ColorAnimation { duration: 150 } }
                                             }

                                             MouseArea {
                                                 id: deleteMouse
                                                 anchors.fill: parent
                                                 hoverEnabled: true
                                                 cursorShape: Qt.PointingHandCursor
                                                 onClicked: {
                                                     if (musicController) musicController.queueRemoveAt(index)
                                                 }
                                             }
                                         }
									}
								}
							}
						}
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
	LoginPopup {
		id: loginPopup
	}

    UserMenuPopup {
        id: userMenuPopup
    }

    Popup {
        id: toastPopup
        anchors.centerIn: Overlay.overlay
        width: implicitWidth
        height: implicitHeight
        margins: 0
        padding: 0
        modal: false
        focus: false
        closePolicy: Popup.NoAutoClose

        property string message: ""

        background: Rectangle {
            color: "#CC000000"
            radius: 8
        }
        
        contentItem: Text {
            text: toastPopup.message
            color: "white"
            font.pixelSize: 16
            padding: 20
        }

        Timer {
            id: toastTimer
            interval: 2000
            onTriggered: toastPopup.close()
        }

        function show(msg) {
            message = msg
            open()
            toastTimer.restart()
        }
        
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200 }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 200 }
        }
    }

    Connections {
        target: musicController
        function onToastMessage(message) {
            toastPopup.show(message)
        }
    }

    LyricOverlay {
		id: lyricOverlay
		visible: lyricBox.active
		onVisibleChanged: lyricBox.active = visible
	}
}
