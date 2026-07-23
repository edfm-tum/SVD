#ifndef CUSTOM3DINPUTHANDLER_H
#define CUSTOM3DINPUTHANDLER_H


#include <QtDataVisualization/Q3DInputHandler>

class SurfaceGraph;

class Custom3dInputHandler : public Q3DInputHandler
{
    Q_OBJECT
public:
    explicit Custom3dInputHandler(QObject *parent = nullptr);

    void setSurfaceGraph(SurfaceGraph *graph) { m_surfaceGraph = graph; }

    virtual void mouseMoveEvent(QMouseEvent *event, const QPoint &mousePos) override;
    virtual void mousePressEvent(QMouseEvent *event, const QPoint &mousePos) override;
    virtual void mouseReleaseEvent(QMouseEvent *event, const QPoint &mousePos) override;
    virtual void wheelEvent(QWheelEvent *event) override;

private:
    SurfaceGraph *m_surfaceGraph;
    bool m_isPanning;
    QPoint m_lastMousePos;
};

#endif // CUSTOM3DINPUTHANDLER_H
