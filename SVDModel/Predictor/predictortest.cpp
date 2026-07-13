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
#include "predictortest.h"

#include <fstream>
#include <vector>
#include <iomanip>

#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QDir>

#include "spdlog/spdlog.h"
#include "tensorhelper.h"
#include "dnn.h"

PredictorTest::PredictorTest()
{
}

PredictorTest::~PredictorTest()
{
}

bool PredictorTest::setup(QString model_path)
{
    // This was previously setting up a TF session.
    // For now, we can just log the intent.
    qDebug() << "PredictorTest::setup for model:" << model_path;
    return true;
}

QString PredictorTest::classifyImage(QString image_path)
{
    qDebug() << "PredictorTest::classifyImage (legacy) called for:" << image_path;
    return "ClassifyImage: TensorFlow-specific code removed.";
}

QString PredictorTest::insight()
{
    QStringList out;
    out << "PredictorTest::insight: TensorFlow-specific code removed.";
    
    // Demonstrate new TensorWrap
    TensorWrap2d<float> tw(4, 5);
    for (int i=0; i<20; ++i)
        tw.data()[i] = static_cast<float>(i);
    
    out << QString("TensorWrap2d sample: %1 - %2").arg(tw.example(0)[0]).arg(tw.example(0)[4]);
    
    return out.join("\n");
}

QString PredictorTest::runModel()
{
    QStringList out;
    out << "PredictorTest::runModel: TensorFlow-specific code removed.";
    return out.join("\n");
}

void PredictorTest::tensorTest()
{
    auto console = spdlog::get("main");
    console->info("PredictorTest::tensorTest: using new TensorWrap2d.");

    TensorWrap2d<float> tw(6, 4);
    for (int i=0;i<6;++i) {
        for (int j=0;j<4;++j)
            tw.example(i)[j] = i*100 + j;
    }
    std::stringstream ss;
    for (int i=0;i<6;++i) {
        ss << "Example " << i << ": ";
        for (int j=0;j<4;++j)
            ss << std::setw(8) << tw.example(i)[j];
        ss << std::endl;
    }
    console->debug("{}", ss.str());
}
