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
#ifndef LANDSCAPEVISUALIZATION_H
#define LANDSCAPEVISUALIZATION_H

#include <QString>
#include <QImage>
#include <QVector3D>
#include "grid.h"
#include "expression.h"
#include "colorpalette.h"

class SurfaceGraph; // forward

class LandscapeVisualization : public QObject
{
Q_OBJECT
public:
    enum RenderType {RenderNone, RenderState, RenderExpression, RenderVariable};
    LandscapeVisualization(QObject *parent=nullptr);
    ~LandscapeVisualization();
    void setup(SurfaceGraph *graph, Legend *palette);

    bool isValid() const {return mIsValid; }
    void invalidate() { mIsValid = false;  mCurrentType = RenderNone; }

    RenderType renderType() const { return mCurrentType; }
    void setRenderType(RenderType type) { mCurrentType = type; update(); }

    void renderToFile(QString filename=QString());

    int viewCount() const;
    bool isViewValid(int camera);
    QString viewString(int camera);

    void setViewString(int camera, QString str);

    int alpha() { return mAlpha; }

public slots:
    /// renders the expression and sets the render type to "Expression"
    QString renderExpression(QString expression);

    /// renders the content of the cell variable
    QString renderVariable(QString variableName, QString description);

    bool renderToSavedGrid(QString filename);

    /// update the rendering
    void update();
    /// reset view to stored view
    void resetView(int camera=0);
    void saveView(int camera);
    void setFillColor(QColor col);
    void setAlpha(int alpha) { mAlpha = alpha; }
    void setStride(int stride) {mStride = stride; mNeedNewTexture=true; }
signals:
    void pointSelected(QVector3D world_coord); ///< coordinates of the point where a user clicks on the visualization

private:
    bool isRendering() {return mIsRendering; }
    /// render based on the result of an expression (cell variables)
    void doRenderExpression(bool auto_scale);
    /// render state colors
    void doRenderState();

    void checkTexture();
    void setupColorRamps();
    void setupStateColors();
    QRgb colorValue(double value) { return mColorLookup[ std::min(std::max(static_cast<int>(value*1000), 0), 999)]; }
    QColor mBGColor;
    int mAlpha;
    bool mNeedNewTexture;

    QVector<QRgb> mColorLookup;
    QVector<QRgb> mStateColorLookup;

    QImage mRenderTexture;
    QImage mUpscaleRenderTexture;
    double mUpscaleFactor;
    int mStride; ///< how many px to skip?

    SurfaceGraph *mGraph;
    Grid<float> mDem;
    float mMinHeight;
    float mMaxHeight;

    Expression mExpression;

    int mRenderCount;
    bool mIsValid;
    bool mIsRendering;
    RenderType mCurrentType;

    Legend *mLegend;

    Palette *mStatePalette;
    Palette *mContinuousPalette;

};

#endif // LANDSCAPEVISUALIZATION_H
