import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: loginPopup
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: 360
    height: 520
    anchors.centerIn: Overlay.overlay

    background: Rectangle {
        color: "#ffffff"
        radius: 16
        border.color: "#e5e7eb"
        border.width: 1
    }

    Connections {
        target: musicController
        function onLoginQrKeyReceived(key) {
            currentQrKey = key
            musicController.loginQrCreate(key)
        }
        function onLoginQrCreateReceived(qrimg, qrurl) {
            qrCodeImage.source = qrimg // base64
        }
        function onLoginQrCheckReceived(code, message, cookie) {
            // 800=expire, 801=waiting, 802=confirming, 803=success
            if (code === 800) {
                qrStatusMessage = qsTr("二维码已过期，请点击刷新")
                qrRefreshButton.visible = true
            } else if (code === 801) {
                qrStatusMessage = qsTr("等待扫码...")
            } else if (code === 802) {
                qrStatusMessage = qsTr("扫码成功，请在手机上确认")
            } else if (code === 803) {
                qrStatusMessage = qsTr("登录成功")
                loginPopup.close()
            }
        }
        function onCaptchaSentReceived(success, message) {
            if (success) {
                loginInfoMessage = qsTr("验证码已发送")
                loginInfoVisible = true
                loginErrorVisible = false
            } else {
                loginErrorMessage = message
                loginErrorVisible = true
                loginInfoVisible = false
            }
        }
        function onLoginSuccess(userId) {
            loginPopup.close()
        }
        function onLoginFailed(message) {
            loginErrorMessage = message
            loginErrorVisible = true
            loginInfoVisible = false
        }
    }

    property string currentQrKey: ""
    property string qrStatusMessage: qsTr("使用网易云音乐APP扫码登录")
    property bool isQrMode: stackLayout.currentIndex === 0
    property string loginErrorMessage: ""
    property bool loginErrorVisible: false
    property string loginInfoMessage: ""
    property bool loginInfoVisible: false

    onOpened: {
        if (!musicController.loggedIn && isQrMode) {
            refreshQrCode()
        }
    }

    function refreshQrCode() {
        qrStatusMessage = qsTr("加载二维码...")
        qrRefreshButton.visible = false
        loginErrorVisible = false
        loginInfoVisible = false
        musicController.loginQrKey()
    }

    Timer {
        id: qrCheckTimer
        interval: 3000
        repeat: true
        running: loginPopup.opened && isQrMode && !musicController.loggedIn && currentQrKey !== ""
        onTriggered: {
            musicController.loginQrCheck(currentQrKey)
        }
    }
    
    // Fix: We need to store the key to check status.
    // Let's modify the Connections above to store key.
    
	StackLayout {
		id: stackLayout
		anchors.fill: parent
		currentIndex: 0

		// Page 1: QR Code Login
		Item {
			id: qrPage

			// Close Button
			Button {
				text: "×"
				font.pixelSize: 24
				background: null
				anchors.top: parent.top
				anchors.right: parent.right
				anchors.margins: 10
				onClicked: loginPopup.close()
				HoverHandler {
					cursorShape: Qt.PointingHandCursor
				}
			}

			ColumnLayout {
				anchors.centerIn: parent
				spacing: 30

				Text {
					text: qsTr("扫码登录")
					font.pixelSize: 24
					font.weight: Font.Bold
					color: "#333333"
					Layout.alignment: Qt.AlignHCenter
				}

				Item {
					width: 200
					height: 200
					Layout.alignment: Qt.AlignHCenter
					
					Image {
						id: qrCodeImage
						anchors.fill: parent
						fillMode: Image.PreserveAspectFit
						source: "" // Will be set by logic
					}

					// Loading/Expired overlay
					Rectangle {
						anchors.fill: parent
						color: "white"
						visible: qrStatusMessage === qsTr("二维码已过期，请点击刷新") || qrStatusMessage === qsTr("加载二维码...")
						opacity: 0.9
						
						ColumnLayout {
							anchors.centerIn: parent
							Text {
								text: qrStatusMessage
								Layout.alignment: Qt.AlignHCenter
								wrapMode: Text.Wrap
								Layout.maximumWidth: 180
							}
							Button {
								id: qrRefreshButton
								text: qsTr("刷新")
								visible: qrStatusMessage === qsTr("二维码已过期，请点击刷新")
								Layout.alignment: Qt.AlignHCenter
								onClicked: refreshQrCode()
							}
						}
					}
				}

				Text {
					textFormat: Text.RichText
					text: qsTr("使用 <font color='#2b6e9b'>网易云音乐APP</font> 扫码登录")
					font.pixelSize: 14
					color: "#333333"
					Layout.alignment: Qt.AlignHCenter
				}
			}

			Text {
				text: qsTr("选择其它登录模式 >")
				font.pixelSize: 12
				color: "#666666"
				anchors.bottom: parent.bottom
				anchors.horizontalCenter: parent.horizontalCenter
				anchors.bottomMargin: 40
				MouseArea {
					anchors.fill: parent
					cursorShape: Qt.PointingHandCursor
					onClicked: stackLayout.currentIndex = 1
				}
			}
		}

		// Page 2: Phone/Other Login
		Item {
			id: phonePage

			// Top Left Corner (Back to QR)
			Image {
				width: 40
				height: 40
				anchors.top: parent.top
				anchors.left: parent.left
				anchors.margins: 10
				source: "qrc:/qt/qml/qtrewrite/ui-asset/black-backgroud/login/qrcode.svg"
				fillMode: Image.PreserveAspectFit
				mipmap: true
				
				MouseArea {
					anchors.fill: parent
					cursorShape: Qt.PointingHandCursor
					onClicked: stackLayout.currentIndex = 0
				}
			}

			// Close Button
			Button {
				text: "×"
				font.pixelSize: 24
				background: null
				anchors.top: parent.top
				anchors.right: parent.right
				anchors.margins: 10
				onClicked: loginPopup.close()
				HoverHandler {
					cursorShape: Qt.PointingHandCursor
				}
			}

			ColumnLayout {
				anchors.centerIn: parent
				width: parent.width - 60
				spacing: 20

				// Logo Area
				ColumnLayout {
					Layout.alignment: Qt.AlignHCenter
					spacing: 10
					
					Image {
						width: 80
						height: 80
						source: "qrc:/qt/qml/qtrewrite/ui-asset/black-backgroud/login/网易云音乐.svg"
						fillMode: Image.PreserveAspectFit
						Layout.alignment: Qt.AlignHCenter
						mipmap: true
					}
					Text {
						text: qsTr("AlgerReWrite")
						font.pixelSize: 20
						font.weight: Font.Bold
						color: "#333333"
						Layout.alignment: Qt.AlignHCenter
					}
				}

				ListModel {
					id: countryModel
					ListElement { name: "中国"; code: "86" }
					ListElement { name: "中国香港"; code: "852" }
					ListElement { name: "中国澳门"; code: "853" }
					ListElement { name: "中国台湾"; code: "886" }
					ListElement { name: "日本"; code: "81" }
					ListElement { name: "韩国"; code: "82" }
					ListElement { name: "英国"; code: "44" }
					ListElement { name: "美国"; code: "1" }
				}

				// Input Area
				ColumnLayout {
					Layout.fillWidth: true
					spacing: 15

					// Phone Input
					Rectangle {
						Layout.fillWidth: true
						height: 40
						border.color: "#e5e7eb"
						border.width: 1
						radius: 20 // Rounded pill shape

						RowLayout {
							anchors.fill: parent
							spacing: 0
							
							// Country Code
							ComboBox {
								id: countryCombo
								width: 80
								height: parent.height
								model: countryModel
								textRole: "code"
								displayText: "+" + currentText
								flat: true
								
								background: Rectangle {
									color: "transparent"
								}
								
								contentItem: Text {
									text: parent.displayText
									font.pixelSize: 14
									color: "#333333"
									verticalAlignment: Text.AlignVCenter
									horizontalAlignment: Text.AlignHCenter
								}
								
								popup: Popup {
									y: parent.height - 1
									width: 150
									height: 200
									padding: 1
									
									contentItem: ListView {
										clip: true
										implicitHeight: contentHeight
										model: countryCombo.delegateModel
										currentIndex: countryCombo.highlightedIndex
										ScrollIndicator.vertical: ScrollIndicator { }
									}
									
									background: Rectangle {
										border.color: "#e5e7eb"
										radius: 4
										color: "white"
									}
								}
								
								delegate: ItemDelegate {
									width: parent.width
									text: model.name + " +" + model.code
									font.pixelSize: 12
									palette.text: "#333333"
									highlighted: countryCombo.highlightedIndex === index
									
									background: Rectangle {
										color: parent.highlighted ? "#f5f5f5" : "transparent"
									}
								}
								
								HoverHandler {
									cursorShape: Qt.PointingHandCursor
								}
							}

							// Divider
							Rectangle {
								width: 1
								height: 20
								color: "#e5e7eb"
								Layout.alignment: Qt.AlignVCenter
							}

							// Text Field
							TextField {
								id: phoneInput
								placeholderText: qsTr("请输入手机号")
								Layout.fillWidth: true
								Layout.leftMargin: 10
								background: null
								font.pixelSize: 14
								color: "#333333"
								selectByMouse: true
								
								HoverHandler {
									cursorShape: Qt.IBeamCursor
								}
							}
						}
					}

					// Password/Captcha Input
					Rectangle {
						Layout.fillWidth: true
						height: 40
						border.color: "#e5e7eb"
						border.width: 1
						radius: 20
						// Always visible for Captcha mode

						RowLayout {
							anchors.fill: parent
							spacing: 0
							TextField {
								id: captchaInput
								Layout.fillWidth: true
								Layout.leftMargin: 15
								placeholderText: qsTr("请输入验证码")
								background: null
								font.pixelSize: 14
								color: "#333333"
								selectByMouse: true
								verticalAlignment: TextInput.AlignVCenter
								
								HoverHandler {
									cursorShape: Qt.IBeamCursor
								}
							}
							
							Rectangle {
								width: 1
								height: 20
								color: "#e5e7eb"
								Layout.alignment: Qt.AlignVCenter
							}
							
							Button {
								text: qsTr("获取验证码")
								background: null
								font.pixelSize: 12
								palette.buttonText: "#666666"
								onClicked: musicController.captchaSent(phoneInput.text, countryCombo.currentText)
								
								HoverHandler {
									cursorShape: Qt.PointingHandCursor
								}
							}
						}
					}

					// Auto Login
					RowLayout {
						Layout.fillWidth: true
						CheckBox {
							text: qsTr("自动登录")
							font.pixelSize: 12
							checked: true
							palette.windowText: "#999999"
							
							indicator: Rectangle {
								implicitWidth: 16
								implicitHeight: 16
								x: parent.leftPadding
								y: parent.height / 2 - height / 2
								radius: 3
								border.color: parent.down ? "#dd001b" : "#999999"
								
								Rectangle {
									width: 10
									height: 10
									x: 3
									y: 3
									radius: 2
									color: "#dd001b"
									visible: parent.parent.checked
								}
							}
							
							contentItem: Text {
								text: parent.text
								font: parent.font
								opacity: enabled ? 1.0 : 0.3
								color: parent.down ? "#dd001b" : "#999999"
								verticalAlignment: Text.AlignVCenter
								leftPadding: parent.indicator.width + 4
							}
							
							HoverHandler {
								cursorShape: Qt.PointingHandCursor
							}
						}
					}
					
					// Login Button
					Button {
						Layout.fillWidth: true
						Layout.preferredHeight: 40
						background: Rectangle {
							color: "#dd001b"
							radius: 20
						}
						contentItem: Text {
							text: qsTr("登录")
							color: "white"
							font.bold: true
							font.pixelSize: 16
							verticalAlignment: Text.AlignVCenter
							horizontalAlignment: Text.AlignHCenter
						}
						onClicked: {
							musicController.loginCellphoneCaptcha(phoneInput.text, captchaInput.text, countryCombo.currentText)
						}
						
						HoverHandler {
							cursorShape: Qt.PointingHandCursor
						}
					}
				}
				
				Text {
					id: errorText2
					visible: loginErrorVisible || loginInfoVisible
					text: loginErrorVisible ? loginErrorMessage : loginInfoMessage
					color: loginErrorVisible ? "red" : "green"
					font.pixelSize: 12
					Layout.alignment: Qt.AlignHCenter
				}
			}

			// Footer (Social & Agreement)
			ColumnLayout {
				anchors.bottom: parent.bottom
				anchors.horizontalCenter: parent.horizontalCenter
				anchors.bottomMargin: 20
				spacing: 15
				
				// Social Icons (Placeholders)
				RowLayout {
					Layout.alignment: Qt.AlignHCenter
					spacing: 20
					
					// Email login removed
				}
			}
		}
	}
}
