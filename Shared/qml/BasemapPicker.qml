
// Copyright 2016 ESRI
//
// All rights reserved under the copyright laws of the United States
// and applicable international laws, treaties, and conventions.
//
// You may freely redistribute and use this sample code, with or
// without modification, provided you include the original copyright
// notice and use restrictions.
//
// See the Sample code usage restrictions document for further information.
//

import QtQuick 2.6
import QtQuick.Controls 2.1
import QtQuick.Controls.Material 2.1
import QtQuick.Window 2.2
import Esri.DSA 1.0

DsaToolBase {
    signal basemapSelected();    

    width: Math.min(parent.width, ((basemapsList.cellWidth * 2) + (16 * scaleFactor)))
    color: Material.background
    title: qsTr("Basemaps")

    BasemapPickerController {
        id: toolController
    }

    Component {
        id: tileCacheDelegate
        Rectangle {
            width: basemapsList.cellWidth;
            height: basemapsList.cellHeight

            border.color: index === basemapsList.currentIndex ? Material.accent : Material.primary
            border.width: index === basemapsList.currentIndex ? 2 * scaleFactor : 0.5 * scaleFactor

            Image {
                source: thumbnailUrl;
                fillMode: Image.PreserveAspectCrop
                anchors{
                    fill: parent
                    margins: 4 * scaleFactor
                }
            }

            Rectangle{
                anchors.centerIn: tileCacheTitle
                width: tileCacheTitle.width + (8 * scaleFactor)
                height: tileCacheTitle.height + (8 * scaleFactor)
                color: Material.primary
                opacity: 0.5
                radius: 2 * scaleFactor
            }

            Label {
                id: tileCacheTitle
                text: title;
                anchors.centerIn: parent
                width: parent.width - (16 * scaleFactor)
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                wrapMode: Text.WrapAnywhere
                font.bold: true
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    toolController.basemapSelected(index);
                    basemapSelected();
                }
                onHoveredChanged: {
                    if (containsMouse) {
                        basemapsList.currentIndex = index
                        basemapsList.currentPath = path
                    }
                }

            }
        }
    }

    GridView {
        id: basemapsList

        property string currentPath: ""

        anchors{
            top: titleBar.bottom
            horizontalCenter: parent.horizontalCenter
            bottom: footerBar.top
            margins: 8 * scaleFactor
        }

        clip: true
        model: toolController.tileCacheModel

        cellWidth: 128 * scaleFactor
        cellHeight: 128 * scaleFactor
        width: 2 * cellWidth

        delegate: tileCacheDelegate
    }


    Rectangle {
        id: footerBar
        anchors{
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }
        color: Material.primary

        height: 32 * scaleFactor

        Label {
            id: pathText
            anchors.fill: parent
            text: basemapsList.currentPath
            wrapMode: Text.WrapAnywhere
            elide: Text.ElideRight
            font.pixelSize: 12 * scaleFactor

        }
    }

    Component.onCompleted: toolController.selectInitialBasemap();
}