import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Popup {
    id: loginPopup
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: 360
    height: 520
    anchors.centerIn: Overlay.overlay

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
        function onLoginQrKeyReceivedQQ(key) {
            console.log("QML: onLoginQrKeyReceivedQQ", key)
            currentQrKey = key
            musicController.loginQrCreateQQ(key)
        }
        function onLoginQrCreateReceivedQQ(qrimg, qrurl) {
            console.log("QML: onLoginQrCreateReceivedQQ", qrimg ? "img present" : "img empty")
            qrCodeImage.source = qrimg // base64
        }
        function onLoginQrCheckReceivedQQ(code, message, cookie) {
            console.log("QML: onLoginQrCheckReceivedQQ", code, message)
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
    property int loginPlatform: 0 // 0: Netease, 1: QQ Music
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
        if (loginPlatform === 0) {
            musicController.loginQrKey()
        } else {
            musicController.loginQrKeyQQ()
        }
    }

    Timer {
        id: qrCheckTimer
        interval: 3000
        repeat: true
        running: loginPopup.opened && isQrMode && !musicController.loggedIn && currentQrKey !== ""
        onTriggered: {
            if (loginPlatform === 0) {
                musicController.loginQrCheck(currentQrKey)
            } else {
                musicController.loginQrCheckQQ(currentQrKey)
            }
        }
    }
    
	StackLayout {
		id: stackLayout
		anchors.fill: parent
		currentIndex: 0
		onCurrentIndexChanged: {
			if (currentIndex === 1) {
				loginPlatform = 0
				loginErrorVisible = false
				loginInfoVisible = false
			}
		}

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
                
                // Platform Switch
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 30

                    // Netease Icon
                    Rectangle {
                        width: 48
                        height: 48
                        radius: 24
                        color: loginPlatform === 0 ? "#fce4e4" : "transparent"
                        border.color: loginPlatform === 0 ? "#dd001b" : "transparent"
                        border.width: 2
                        
                        Image {
                            anchors.centerIn: parent
                            width: 32
                            height: 32
                            source: "qrc:/qt/qml/qtrewrite/ui-asset/black-backgroud/login/网易云音乐.svg"
                            mipmap: true
                            fillMode: Image.PreserveAspectFit
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (loginPlatform !== 0) {
                                    loginPlatform = 0
                                    refreshQrCode()
                                }
                            }
                        }
                    }

                    // QQ Music Icon
                    Rectangle {
                        width: 48
                        height: 48
                        radius: 24
                        color: loginPlatform === 1 ? "#e1f5ea" : "transparent"
                        border.color: loginPlatform === 1 ? "#31c27c" : "transparent"
                        border.width: 2
                        
                        Image {
                            anchors.centerIn: parent
                            width: 32
                            height: 32
                            source: "qrc:/qt/qml/qtrewrite/ui-asset/black-backgroud/login/QQ-music.svg"
                            mipmap: true
                            fillMode: Image.PreserveAspectFit
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (loginPlatform !== 1) {
                                    loginPlatform = 1
                                    refreshQrCode()
                                }
                            }
                        }
                    }
                }

                Text {
                    textFormat: Text.RichText
                    text: loginPlatform === 0 
                        ? qsTr("使用 <font color='#dd001b'>网易云音乐APP</font> 扫码登录") 
                        : qsTr("使用 <font color='#31c27c'>QQ音乐APP</font> 扫码登录")
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

				ColumnLayout {
					Layout.alignment: Qt.AlignHCenter
					spacing: 12
					
					RowLayout {
						Layout.alignment: Qt.AlignHCenter
						spacing: 30

						Rectangle {
							width: 48
							height: 48
							radius: 24
							color: loginPlatform === 0 ? "#fce4e4" : "transparent"
							border.color: loginPlatform === 0 ? "#dd001b" : "transparent"
							border.width: 2
							
							Image {
								anchors.centerIn: parent
								width: 32
								height: 32
								source: "qrc:/qt/qml/qtrewrite/ui-asset/black-backgroud/login/网易云音乐.svg"
								mipmap: true
								fillMode: Image.PreserveAspectFit
							}
							
							MouseArea {
								anchors.fill: parent
								cursorShape: Qt.PointingHandCursor
								onClicked: {
									loginPlatform = 0
									loginErrorVisible = false
									loginInfoVisible = false
								}
							}
						}
					}

					Text {
						textFormat: Text.RichText
						text: qsTr("使用 <font color='#dd001b'>网易云音乐</font> 登录")
						font.pixelSize: 14
						color: "#333333"
						Layout.alignment: Qt.AlignHCenter
					}
				}

				ColumnLayout {
					Layout.fillWidth: true
					spacing: 15

					ColumnLayout {
						Layout.fillWidth: true
						spacing: 15
						visible: loginPlatform === 0

						// Phone Input
					Rectangle {
						Layout.fillWidth: true
						height: 44
						border.color: "#999999"
						border.width: 1
						radius: 22

						RowLayout {
							anchors.fill: parent
							spacing: 0
							
	                        Item {
	                            Layout.preferredWidth: 90
	                            Layout.fillHeight: true
	                            Text {
	                                anchors.left: parent.left
	                                anchors.leftMargin: 10
	                                anchors.verticalCenter: parent.verticalCenter
	                                text: "+86"
	                                color: "#000000"
	                                font.pixelSize: 16
	                                font.weight: Font.Medium
	                                verticalAlignment: Text.AlignVCenter
	                            }
	                        }

                        // Separator
                        Rectangle {
                            width: 1
                            height: 20
                            color: "#999999"
                            Layout.alignment: Qt.AlignVCenter
                        }
                            
                        TextField {
                            id: phoneField
                            Layout.fillWidth: true
                            placeholderText: qsTr("请输入手机号")
                            placeholderTextColor: "#999999"
                            background: null
                            font.pixelSize: 16
                            color: "#000000"
                            selectionColor: "#dd001b"
                            selectedTextColor: "#ffffff"
                            leftPadding: 10
                            cursorDelegate: Rectangle {
                                width: 2
                                color: "#000000"
                                visible: parent.activeFocus
                            }
                        }
                    }
                    }
                    
                    // Captcha Input
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        
                        Rectangle {
                            Layout.fillWidth: true
                            height: 44
                            border.color: "#999999"
                            border.width: 1
                            radius: 22
                            
                            TextField {
                                id: captchaField
                                anchors.fill: parent
                                anchors.margins: 10
                                placeholderText: qsTr("请输入验证码")
                                placeholderTextColor: "#999999"
                                background: null
                                font.pixelSize: 16
                                color: "#000000"
                                selectionColor: "#dd001b"
                                selectedTextColor: "#ffffff"
                                cursorDelegate: Rectangle {
                                    width: 2
                                    color: "#000000"
                                    visible: parent.activeFocus
                                }
                            }
                        }
                        
                        Button {
                            id: sendCaptchaBtn
                            text: qsTr("获取验证码")
                            height: 40
                            background: Rectangle {
                                color: enabled ? "#f2f2f2" : "#f5f5f5"
                                radius: 20
                                border.color: "#e5e7eb"
                                border.width: 1
                            }
                            contentItem: Text {
                                text: parent.text
                                color: parent.enabled ? "#333333" : "#999999"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: 12
                            }
                            onClicked: {
                                musicController.captchaSent(phoneField.text, "86")
                            }
                        }
                    }
                    
                    Button {
                        text: qsTr("登录")
                        Layout.fillWidth: true
                        height: 40
                        background: Rectangle {
                            color: "#dd001b"
                            radius: 20
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            musicController.loginCellphoneCaptcha(phoneField.text, captchaField.text, "86")
                        }
                    }
					}

                }
            }
        }
    }
}
