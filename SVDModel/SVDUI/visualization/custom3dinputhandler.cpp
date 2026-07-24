#include "custom3dinputhandler.h"
#include "surfacegraph.h"

#include <QtDataVisualization/Q3DCamera>
#include <QtDataVisualization/QAbstract3DGraph>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QtMath>
#include <cmath>

Custom3dInputHandler::Custom3dInputHandler(QObject *parent) :
  Q3DInputHandler(parent),
  m_surfaceGraph(nullptr),
  m_isPanning(false)
{
}

void Custom3dInputHandler::mousePressEvent(QMouseEvent *event, const QPoint &mousePos)
{
    if (m_surfaceGraph) {
        m_surfaceGraph->setFocus(Qt::MouseFocusReason);
        m_surfaceGraph->activateWindow();
    }

    if ((event->modifiers() & Qt::ControlModifier) && (event->buttons() & Qt::LeftButton)) {
        m_isPanning = true;
        m_lastMousePos = mousePos;
        return;
    }

    if (event->button() == Qt::LeftButton) {
        scene()->setGraphPositionQuery(mousePos);
    }
    Q3DInputHandler::mousePressEvent(event, mousePos);
}

void Custom3dInputHandler::mouseMoveEvent(QMouseEvent *event, const QPoint &mousePos)
{
    if (m_isPanning && (event->buttons() & Qt::LeftButton)) {
        QPoint delta = mousePos - m_lastMousePos;
        m_lastMousePos = mousePos;

        auto *cam = scene()->activeCamera();
        if (cam) {
            float angleRad = qDegreesToRadians(cam->xRotation());
            float zoomFactor = std::max(1.0f, cam->zoomLevel() / 100.0f);
            float speed = 0.002f / sqrt(zoomFactor);

            float dy = -delta.y();
            float dx = (-delta.x() * cos(angleRad) + dy * sin(angleRad)) * speed;
            float dz = (-delta.x() * sin(angleRad) - dy * cos(angleRad)) * speed;

            QVector3D target = cam->target();
            target.setX(target.x() + dx);
            target.setZ(target.z() + dz);
            cam->setTarget(target);
        }
        return;
    }

    Q3DInputHandler::mouseMoveEvent(event, mousePos);
}

void Custom3dInputHandler::mouseReleaseEvent(QMouseEvent *event, const QPoint &mousePos)
{
    if (m_isPanning) {
        m_isPanning = false;
    }
    Q3DInputHandler::mouseReleaseEvent(event, mousePos);
}

void Custom3dInputHandler::wheelEvent(QWheelEvent *event)
{
    auto *cam = scene() ? scene()->activeCamera() : nullptr;
    if (!cam)
        return;

    int zoomLevel = cam->zoomLevel();
    double zoom_sq = sqrt(zoomLevel);
    int new_level = (zoom_sq + event->angleDelta().y() / 100) * (zoom_sq + event->angleDelta().y() / 100);
    int maxZoom = static_cast<int>(cam->maxZoomLevel());
    if (maxZoom < 50000) maxZoom = 50000;
    if (new_level > maxZoom) new_level = maxZoom;
    if (new_level < 10) new_level = 10;

    cam->setZoomLevel(new_level);

    auto *graph = m_surfaceGraph ? m_surfaceGraph->graph() : nullptr;
    if (graph && new_level > 1000 && !graph->isOrthoProjection()) {
        graph->setOrthoProjection(true);
    }
}
