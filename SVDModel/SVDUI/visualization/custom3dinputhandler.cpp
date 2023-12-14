#include "custom3dinputhandler.h"


#include <QtDataVisualization/Q3DCamera>
#include <math.h>

Custom3dInputHandler::Custom3dInputHandler(QObject *parent) :
  Q3DInputHandler(parent)
{
}

void Custom3dInputHandler::mouseMoveEvent(QMouseEvent *event, const QPoint &mousePos)
{
  Q_UNUSED(event)
    //setInputPosition(mousePos);
    Q3DInputHandler::mouseMoveEvent(event, mousePos);
}

void Custom3dInputHandler::mousePressEvent(QMouseEvent *event, const QPoint &mousePos)
{
    if (event->button() != Qt::LeftButton) {
        Q3DInputHandler::mousePressEvent(event, mousePos);
        return;
    }


    //setInputPosition(mousePos);
    scene()->setGraphPositionQuery(mousePos);
    Q3DInputHandler::mousePressEvent(event, mousePos);
}

void Custom3dInputHandler::wheelEvent(QWheelEvent *event)
{
  //Q3DInputHandler::wheelEvent(event);
  //return;

    // Adjust zoom level based on what zoom range we're in.
  int zoomLevel = scene()->activeCamera()->zoomLevel();
  double zoom_sq = sqrt(zoomLevel);
  int new_level = (zoom_sq + event->angleDelta().y() / 100) * (zoom_sq + event->angleDelta().y() / 100);
  if (new_level > 20000) new_level = 20000;
  if (new_level < 10) new_level = 10;

  scene()->activeCamera()->setZoomLevel(new_level);

//  if (zoomLevel > 100)
//      zoomLevel += event->angleDelta().y() / 12;
//  else if (zoomLevel > 50)
//      zoomLevel += event->angleDelta().y() / 60;
//  else
//      zoomLevel += event->angleDelta().y() / 120;
//  if (zoomLevel > 500)
//      zoomLevel = 500;
//  else if (zoomLevel < 10)
//      zoomLevel = 10;
  // change y (~elevation) dynamically with zoom...
  //QVector3D new_target = scene()->activeCamera()->target();
  //new_target.setY()

//  scene()->activeCamera()->setTarget(new_target);
}
