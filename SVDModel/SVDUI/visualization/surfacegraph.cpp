/********************************************************************************************
**    SVD - the scalable vegetation dynamics model
**    https://github.com/SVDmodel/SVD
**    Copyright (C) 2018-  Werner Rammer, Rupert Seidl
**
**    This program is free software: you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation, either version 3 of the License, or
**    (at your option) any later version.
**
**    This program is distributed in the hope that it will be useful,
**    but WITHOUT ANY WARRANTY; without even the implied warranty of
**    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**    GNU General Public License for more details.
**
**    You should have received a copy of the GNU General Public License
**    along with this program.  If not, see <http://www.gnu.org/licenses/>.
********************************************************************************************/


#include "surfacegraph.h"
#include "topographicseries.h"
#include "custom3dinputhandler.h"

#include <QtDataVisualization/QValue3DAxis>
#include <QtDataVisualization/Q3DTheme>
#include <QtDataVisualization/Q3DSurface>

#include <QApplication>
#include <QPainter>
#include <QMessageBox>
#include <QScreen>
#include <QLayout>
#include <QKeyEvent>
#include <QtMath>
#include <cmath>


// dummy axis formatter
class DummyAxisFormatter:  public QValue3DAxisFormatter {
public:
    virtual QValue3DAxisFormatter *createNewInstance() const { return new DummyAxisFormatter();}
    virtual void recalculate() {}
    virtual QStringList &labelStrings() const { return empty_list; }
    virtual QVector<float> &labelPositions() const { return empty_vec; }
private:
    static QStringList empty_list;
    static QVector<float> empty_vec;
};

QStringList DummyAxisFormatter::empty_list;
QVector<float> DummyAxisFormatter::empty_vec;


SurfaceGraph::SurfaceGraph(QWidget *parent) : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    Q3DSurface *graph = new Q3DSurface();
    m_graph = graph;
    m_topography = nullptr;
    m_maxZoomLevel = 500.0f;

    QWidget *container = QWidget::createWindowContainer(graph);
    QSize screenSize = graph->screen()->size();

    container->setMinimumSize(QSize(400, 300));
    container->setMaximumSize(screenSize);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    container->setFocusPolicy(Qt::StrongFocus);
    container->setParent(this);

    m_graph->installEventFilter(this);

    QHBoxLayout *hLayout = new QHBoxLayout(this);
    QVBoxLayout *vLayout = new QVBoxLayout();
    hLayout->addWidget(container, 1);
    hLayout->addLayout(vLayout);
    vLayout->setAlignment(Qt::AlignTop);

    if (!graph->hasContext()) {
        QMessageBox msgBox;
        msgBox.setText("Couldn't initialize the OpenGL context.");
        msgBox.exec();
    }

    m_graph->setAxisX(new QValue3DAxis);
    m_graph->setAxisY(new QValue3DAxis);
    m_graph->setAxisZ(new QValue3DAxis);

    m_graph->axisX()->setLabelAutoRotation(30);
    m_graph->axisY()->setLabelAutoRotation(90);
    m_graph->axisZ()->setLabelAutoRotation(30);

    m_graph->axisY()->setTitleVisible(false);

    m_graph->axisX()->setFormatter(new DummyAxisFormatter);
    m_graph->axisY()->setFormatter(new DummyAxisFormatter);
    m_graph->axisZ()->setFormatter(new DummyAxisFormatter);

    m_graph->activeTheme()->setType(Q3DTheme::ThemePrimaryColors);

    QFont font = m_graph->activeTheme()->font();
    font.setPointSize(12);
    m_graph->activeTheme()->setFont(font);

    Q3DTheme *theme = new Q3DTheme(Q3DTheme::ThemeDigia);
    theme->setBackgroundEnabled(false);
    theme->setGridEnabled(false);
    theme->setLabelTextColor(Qt::white);
    theme->setLabelBorderEnabled(false);
    theme->setLabelBackgroundEnabled(false);

    m_graph->setActiveTheme(theme);

    QObject::connect(m_graph->scene()->activeCamera(), &Q3DCamera::targetChanged, this, &SurfaceGraph::cameraChanged);
    QObject::connect(m_graph->scene()->activeCamera(), &Q3DCamera::zoomLevelChanged, this, &SurfaceGraph::cameraChanged);
    QObject::connect(m_graph->scene()->activeCamera(), &Q3DCamera::xRotationChanged, this, &SurfaceGraph::cameraChanged);
    QObject::connect(m_graph->scene()->activeCamera(), &Q3DCamera::yRotationChanged, this, &SurfaceGraph::cameraChanged);
    QObject::connect(m_graph, &Q3DSurface::aspectRatioChanged, this, &SurfaceGraph::cameraChanged);
    QObject::connect(m_graph->axisY(), &QValue3DAxis::maxChanged, this, &SurfaceGraph::cameraChanged);
    QObject::connect(m_graph, &Q3DSurface::queriedGraphPositionChanged, this, &SurfaceGraph::queryPositionChanged);

    Custom3dInputHandler *inputHandler = new Custom3dInputHandler(this);
    inputHandler->setSurfaceGraph(this);
    m_graph->setActiveInputHandler(inputHandler);
}

SurfaceGraph::~SurfaceGraph()
{
    delete m_graph;
}

void SurfaceGraph::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void SurfaceGraph::setup(Grid<float> &dem, float min_h, float max_h)
{

    float longer_side = static_cast<float>(std::max(dem.metricSizeX(), dem.metricSizeY()));
    float max_range = 10000 + pow(longer_side, 0.9f); // a rule of thumb
    m_maxZoomLevel = std::max(500.0f, longer_side / 10.0f);
    m_graph->scene()->activeCamera()->setMaxZoomLevel(m_maxZoomLevel);

    m_graph->axisX()->setLabelFormat("%i");
    m_graph->axisZ()->setLabelFormat("%i");
    m_graph->axisX()->setRange(0.0f, static_cast<float>(dem.metricSizeX()));
    m_graph->axisY()->setRange(min_h, min_h + std::max(max_h, max_range));
    m_graph->axisZ()->setRange(0.0f, static_cast<float>(dem.metricSizeY()));

    qDebug() << "set y-range: max-side:"<< longer_side << "min:" << min_h << "max:" << m_graph->axisY()->max();
    m_topography = new TopographicSeries();

    m_topography->setGrid(dem, min_h);

    m_topography->setItemLabelFormat(QStringLiteral("@yLabel m"));


    m_graph->addSeries(m_topography);

    mDefaultViews.clear();
    ViewParams vp;
    for (int i=0;i<4;++i) {
        mDefaultViews.push_back(vp);
        mDefaultViews[i].camera = new Q3DCamera();
        mDefaultViews[i].camera->copyValuesFrom(*m_graph->scene()->activeCamera());
        mDefaultViews[i].aspectRatio = m_graph->aspectRatio();
        mDefaultViews[i].maxAxisYRange = m_graph->axisY()->max();

    }

}


void SurfaceGraph::clickCamera()
{
    int preset = int(m_graph->scene()->activeCamera()->cameraPreset());
    qDebug() << preset;
    // m_graph->scene()->activeCamera()->setCameraPreset(Q3DCamera::CameraPreset( preset + 1) );
    QVector3D target = m_graph->scene()->activeCamera()->target();
    target.setX(target.y() - 0.1f);
    m_graph->scene()->activeCamera()->setTarget(target);
    m_graph->scene()->activeCamera()->setMaxZoomLevel(5000);
    m_graph->scene()->activeCamera()->setZoomLevel( m_graph->scene()->activeCamera()->zoomLevel() + 100 );

}

bool SurfaceGraph::isCameraValid(int cameraPreset)
{
    if (cameraPreset>0 && cameraPreset<mDefaultViews.size())
        return mDefaultViews[cameraPreset].valid;
    return false;
}

QString SurfaceGraph::cameraString(int cameraPreset)
{
    if (cameraPreset<mDefaultViews.size()) {
        return mDefaultViews[cameraPreset].asString();
    } else {
        return QString();
    }

}

void SurfaceGraph::setCameraString(int cameraPreset, QString str)
{
    if (cameraPreset<mDefaultViews.size()) {
        return mDefaultViews[cameraPreset].setFromString(str);
    }
}

void SurfaceGraph::queryPositionChanged(const QVector3D &pos)
{
    if (!m_topography)
        return;

    QVector3D world_pos = m_topography->getCoordsFromRelative(pos);
    //spdlog::get("main")->info("Grid: x: {}, y: {}, z: {} World: x: {}, y: {}, z: {} ", pos.x(), pos.y(), pos.z(), world_pos.x(), world_pos.y(), world_pos.z());
    emit pointSelected(world_pos);
}

void SurfaceGraph::resetCameraPosition(int cameraPreset)
{
   if (cameraPreset>=mDefaultViews.length())
       return;

   //auto *camera = mDefaultViews[cameraPreset].camera;
   //spdlog::get("main")->info("set viewparams: target {}, {}, {}", camera->target().x(), camera->target().y(), camera->target().z());

   m_graph->scene()->activeCamera()->copyValuesFrom(*mDefaultViews[cameraPreset].camera);
    m_graph->scene()->activeCamera()->setTarget( mDefaultViews[cameraPreset].camera->target() );
    m_graph->setAspectRatio(mDefaultViews[cameraPreset].aspectRatio);
    m_graph->axisY()->setMax(mDefaultViews[cameraPreset].maxAxisYRange);

   // force a repaint of the scene
   float rot = m_graph->scene()->activeCamera()->xRotation();
   m_graph->scene()->activeCamera()->setXRotation(rot + 1.f);
   m_graph->scene()->activeCamera()->setXRotation(rot);

}

void SurfaceGraph::saveCameraPosition(int cameraPreset)
{
    if (cameraPreset>0 && cameraPreset<mDefaultViews.size()) {
        mDefaultViews[cameraPreset].camera->copyValuesFrom(*m_graph->scene()->activeCamera());
        mDefaultViews[cameraPreset].camera->setTarget(m_graph->scene()->activeCamera()->target());
        mDefaultViews[cameraPreset].aspectRatio = m_graph->aspectRatio();
        mDefaultViews[cameraPreset].maxAxisYRange = m_graph->axisY()->max();
        mDefaultViews[cameraPreset].valid = true;

    }

}

SurfaceGraph::ViewParams::ViewParams() : aspectRatio(0.), maxAxisYRange(0.f), valid(false)
{
    camera = nullptr;
}

SurfaceGraph::ViewParams::~ViewParams()
{
    //if (camera)
    //    delete camera;
}

QString SurfaceGraph::ViewParams::asString()
{
    QStringList res;
    res << QString("xRotation=%1").arg(camera->xRotation())
        << QString("yRotation=%1").arg(camera->yRotation())
        << QString("zoomLevel=%1").arg(camera->zoomLevel())
        << QString("targetX=%1").arg(camera->target().x())
        << QString("targetY=%1").arg(camera->target().y())
        << QString("targetZ=%1").arg(camera->target().z())
        << QString("aspectRatio=%1").arg(aspectRatio)
        << QString("maxYAxis=%1").arg(maxAxisYRange)
        << QString("backgroundColor=%1").arg(backgroundColor)
        << QString("valid=%1").arg(valid ? "true" : "false");
    return res.join(",");

}

void SurfaceGraph::ViewParams::setFromString(QString str)
{
    QStringList l = str.split(",");
    QMap<QString, QString> dat;
    for (auto s : l) {
        auto li = s.split("=");
        dat[li.first()]=li.last();
    }
    camera->setXRotation( dat["xRotation"].toFloat() );
    camera->setYRotation( dat["yRotation"].toFloat() );
    camera->setZoomLevel( dat["zoomLevel"].toFloat() );
    camera->setTarget(QVector3D(dat["targetX"].toFloat(),
                                dat["targetY"].toFloat(),
                                dat["targetZ"].toFloat()));
    aspectRatio = dat["aspectRatio"].toDouble();
    maxAxisYRange= dat["maxYAxis"].toFloat();
    backgroundColor = dat["backgroundColor"];
    valid = dat["valid"] == "true";

    //spdlog::get("main")->info("viewparams: target {}, {}, {}", camera->target().x(), camera->target().y(), camera->target().z());
}

void SurfaceGraph::keyPressEvent(QKeyEvent *event)
{
    handleCameraPanKeys(event);
    QWidget::keyPressEvent(event);
}

void SurfaceGraph::handleCameraPanKeys(QKeyEvent *event)
{
    auto *cam = m_graph ? m_graph->scene()->activeCamera() : nullptr;
    if (!cam)
        return;

    float angleRad = qDegreesToRadians(cam->xRotation());
    float zoomFactor = std::max(1.0f, cam->zoomLevel() / 100.0f);
    float step = 0.05f / sqrt(zoomFactor);

    float dx = 0.0f;
    float dz = 0.0f;

    switch (event->key()) {
    case Qt::Key_Up:
    case Qt::Key_W:
        dx = sin(angleRad) * step;
        dz = cos(angleRad) * step;
        break;
    case Qt::Key_Down:
    case Qt::Key_S:
        dx = -sin(angleRad) * step;
        dz = -cos(angleRad) * step;
        break;
    case Qt::Key_Left:
    case Qt::Key_A:
        dx = cos(angleRad) * step;
        dz = -sin(angleRad) * step;
        break;
    case Qt::Key_Right:
    case Qt::Key_D:
        dx = -cos(angleRad) * step;
        dz = sin(angleRad) * step;
        break;
    default:
        return;
    }

    QVector3D target = cam->target();
    target.setX(target.x() + dx);
    target.setZ(target.z() + dz);
    cam->setTarget(target);
}
