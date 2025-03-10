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

#include "topographicseries.h"

//using namespace QtDataVisualization;

//! [0]
// Value used to encode height data as RGB value on PNG file
const float packingFactor = 11983.0f;
//! [0]

TopographicSeries::TopographicSeries()
{
    setDrawMode(QSurface3DSeries::DrawSurface);
    //setDrawMode(QSurface3DSeries::DrawSurfaceAndWireframe);
    setFlatShadingEnabled(true);
    setBaseColor(Qt::white);
}

TopographicSeries::~TopographicSeries()
{
}

void TopographicSeries::setTopographyFile(const QString file, float width, float height)
{
//! [1]
    QImage heightMapImage(file);
    uchar *bits = heightMapImage.bits();
    int imageHeight = heightMapImage.height();
    int imageWidth = heightMapImage.width();
    int widthBits = imageWidth * 4;
    float stepX = width / float(imageWidth);
    float stepZ = height / float(imageHeight);

    QSurfaceDataArray *dataArray = new QSurfaceDataArray;
    dataArray->reserve(imageHeight);
    for (int i = 0; i < imageHeight; i++) {
        int p = i * widthBits;
        float z = height - float(i) * stepZ;
        QSurfaceDataRow *newRow = new QSurfaceDataRow(imageWidth);
        for (int j = 0; j < imageWidth; j++) {
            uchar aa = bits[p + 0];
            uchar rr = bits[p + 1];
            uchar gg = bits[p + 2];
            uint color = uint((gg << 16) + (rr << 8) + aa);
            float y = float(color) / packingFactor;
            (*newRow)[j].setPosition(QVector3D(float(j) * stepX, y, z));
            p = p + 4;
        }
        *dataArray << newRow;
    }

    dataProxy()->resetArray(dataArray);
//! [1]

    m_sampleCountX = float(imageWidth);
    m_sampleCountZ = float(imageHeight);
}

void TopographicSeries::setGrid(const Grid<float> &grid, float min_value)
{
    RectF extent = grid.metricRect();
    m_gridRect = extent;
    QSurfaceDataArray *dataArray = new QSurfaceDataArray;
    dataArray->reserve(grid.sizeY());
    for (int y=0;y<grid.sizeY();++y) {
        QSurfaceDataRow *newRow = new QSurfaceDataRow(grid.sizeX());
        for (int x=0;x<grid.sizeX();++x) {
            Point pidx(x,y);
            const PointF p = grid.cellCenterPoint(pidx);
            float hval = std::max(grid(x,y), min_value);
            (*newRow)[x].setPosition(QVector3D(p.x() - extent.left(),hval, p.y()-extent.top()));
        }
        *dataArray << newRow;
    }
    dataProxy()->resetArray(dataArray);

    m_sampleCountX = float(grid.sizeX());
    m_sampleCountZ = float(grid.sizeY());

}

QVector3D TopographicSeries::getCoordsFromRelative(const QVector3D &rel)
{
    QVector3D world;
    if (m_gridRect.isNull())
        return world;

    world.setX(static_cast<float> (m_gridRect.left() + (rel.x()+1.)/2. * (m_gridRect.right()-m_gridRect.left()) ));
    world.setY(static_cast<float>( m_gridRect.top() - (rel.z()+1.)/2. * (m_gridRect.top()-m_gridRect.bottom()) ));
    return world;
}
