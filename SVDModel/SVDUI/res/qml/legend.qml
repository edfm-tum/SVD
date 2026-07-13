import QtQuick 2.15
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    //width: 250
    //height: 500
    //anchors.fill: parent
    id: main
    //anchors.fill: parent
    //color:  "gray"
    Image {
        id: splash_image
        source: "qrc:/SVD_splash.jpg"
        visible: legend.caption === '';
        fillMode: Image.PreserveAspectFit
        anchors.fill: parent
        verticalAlignment: Image.Top
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10
        visible: legend.caption !== '';


        ColumnLayout{
            Layout.fillWidth: true

            Text {
                id: rulerCaption
                text: legend.caption
                font.pixelSize: 16
            }
            Text {
                id: rulerDesc
                text: legend.description
                wrapMode: Text.WordWrap

            }

            //Rectangle { height: 10}


        }
        Rectangle {
            //color: "#ea6b6b"
            Layout.fillHeight: true
            Layout.fillWidth: true



            Rectangle {
                visible:  !legend.currentPalette.isFactor
                anchors.margins: 5
                anchors.leftMargin: 5
                anchors.topMargin: 10
                anchors.fill: parent


                ColumnLayout {
                    id: rulerDetailsLayout

                    CheckBox {
                        id: showRulerDetails
                        text: "Edit range"
                    }

                    GroupBox {
                        id: details
                        //flat: false
                        visible: showRulerDetails.checked
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? implicitHeight : 0
                        Layout.topMargin: 10

                        ColumnLayout {
                            anchors.horizontalCenter: parent.horizontalCenter
                            CheckBox {
                                id: rangeAuto
                                //anchors.left: maxValueSpin.right
                                //anchors.leftMargin: 5
                                text: "Auto"
                                checked: legend.autoScale
                                onClicked: legend.autoScale=rangeAuto.checked
                            }
                            RowLayout {
                                SpinBox {
                                    id: minValueSpin
                                    enabled: !rangeAuto.checked
                                    editable: true
                                    //decimals: 2
                                    from: -10000
                                    to: 1000000
                                    value: legend.minValue
                                    onValueChanged: if(legend.minValue !== value)
                                                        legend.minValue = value
                                }
                                SpinBox {
                                    id: maxValueSpin
                                    enabled: !rangeAuto.checked
                                    editable: true
                                    //decimals: 2
                                    from: -10000
                                    to: 1000000
                                    value: legend.maxValue
                                    onValueChanged: if (legend.maxValue !== value)
                                                            legend.maxValue = value
                                }
                            }

                        }

                        }

                    Row {
                        Column {
                            id: colorRamp
                            anchors.topMargin: 10
                            height: 202
                            width: 82
                            Rectangle {
                                border.width: 1
                                border.color: "#888888"
                                width: legendImage.width+2
                                height: legendImage.height+2
                                Image {
                                    id: legendImage
                                    anchors.centerIn: parent
                                    source: "image://colors/" + legend.currentPalette.name
                                    fillMode: Image.Stretch
                                    width: 80
                                    height: 200
                                }
                            }
                        }


                        Rectangle {
                            //color: "grey"
                            id: scalerect
                            width: colorRamp.width
                            height: colorRamp.height

                            Text {
                                id: maxValue
                                text: legend.rangeLabels[4]
                                anchors.top: parent.top
                                anchors.left: parent.left
                                leftPadding: 5
                            }
                            Text {
                                id: upperQuartileValue
                                text: legend.rangeLabels[3]
                                anchors.top: parent.top
                                anchors.topMargin: parent.height / 4 - height / 2
                                leftPadding: 5
                                visible: colorRamp.height>100
                            }
                            Text {
                                id: centerValue
                                text: legend.rangeLabels[2]
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                leftPadding: 5
                            }
                            Text {
                                id: lowerQuartileValue
                                text: legend.rangeLabels[1]
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: parent.height / 4 - height / 2
                                anchors.left: parent.left
                                leftPadding: 5
                                visible: colorRamp.height>100
                            }
                            Text {
                                id: minValue
                                text: legend.rangeLabels[0]
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                leftPadding: 5
                            }
                        }


                    }
                    RowLayout {

                        Text { text: "color ramp: " }
                        /* get the list of available color palettes and populate the ComboBox */
                        ComboBox {
                            id: paletteSelector
                            //visible: !legend.currentPalette.isFactor
                            model: legend.names
                            onCurrentIndexChanged: legend.paletteIndex = paletteSelector.currentIndex
                        }
                    }
                }



            }
            ScrollView {
                visible: legend.currentPalette.isFactor
                id: palFactorsList

                anchors.fill: parent
                anchors.leftMargin: 20

                ListView {
                    anchors.fill: parent
                    model: legend.currentPalette.factorLabels
                    delegate: Rectangle {
                        height: 20
                        width: 200
                        Rectangle {
                            id: delColorRect
                            height: 18; width: 50
                            color: legend.currentPalette.factorColors[index]
                        }

                        Text { text: modelData
                            anchors.top: delColorRect.top;
                            anchors.left: delColorRect.right;
                            anchors.verticalCenter: delColorRect.verticalCenter;
                            anchors.leftMargin: 5
                        }
                    }
                }
            }
        }

       /* Rectangle {

            id: scale
            height: 40

            //color: "grey"
            width: parent.width
            Text {
                //text: "Meter/px:" + legend.meterPerPixel
                text: "0m"
                height: 20
                anchors.top: scale.top
                anchors.topMargin: 30
            }


            Row{

                anchors.top: scale.top
                anchors.topMargin: 10


                Repeater {
                    id: scaleRep
                    width: parent.width
                    model: 4
                    property real cellWidth
                    cellWidth: { var n = legend.meterPerPixel*main.width/5;
                        var sig=1;
                        var mult = Math.pow(10, sig - Math.floor(Math.log(n) / Math.LN10) - 1);
                        var s= Math.round(n * mult) / mult;
                        //console.log("n: " + n + " s: " + s);
                        return s / legend.meterPerPixel;
                    }
                    Item {
                        width: scaleRep.cellWidth
                        height: 30
                        Rectangle {
                            width: scaleRep.cellWidth
                            height: 20
                            border.width: 1
                            border.color: "#808080"
                            color: index % 2==1?"#808080":"#a0a0a0"
                        }
                        Text {
                            text: Math.round(parent.width * (modelData+1) * legend.meterPerPixel)
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.topMargin: 20
                            anchors.leftMargin: scaleRep.cellWidth-contentWidth/2

                        }
                    }
                }
            }

        }
    } */
}
}
