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

#include "../Predictor/dnnshell.h"

#include <QString>
#include <QRegularExpression>

static const char *version = "1.0";
const char *currentVersion(){ return version;}
// compiler version
#ifdef Q_CC_MSVC
#define MYCC "MSVC"
#endif
#ifdef Q_CC_GNU
#define MYCC "GCC"
#endif
#ifdef Q_CC_INTEL
#define MYCC "Intel"
#endif
#ifndef MYCC
#define MYCC "unknown"
#endif

// http://stackoverflow.com/questions/1505582/determining-32-vs-64-bit-in-c
// Check windows
#if _WIN32 || _WIN64
#if _WIN64
#define ENVIRONMENT64
#else
#define ENVIRONMENT32
#endif
#endif

// Check GCC
#if __GNUC__
#if __x86_64__ || __ppc64__
#define ENVIRONMENT64
#else
#define ENVIRONMENT32
#endif
#endif

#ifdef ENVIRONMENT32
#define BITS "32 bit"
#else
#define BITS "64 bit"
#endif


QString compiler()
{
    return QString("%1 %2 Qt %3 TF %4").arg(MYCC).arg(BITS).arg(qVersion()).arg(DNNShell::tensorFlowVersion());
}

QString verboseVersion()
{
    QString s = QString("branch: %1, hash: %2, date: %3").arg(GIT_BRANCH).arg(GIT_HASH).arg(BUILD_TIMESTAMP);
    return s;
}

QString verboseVersionHtml()
{
    QString s = QString("branch: %1, hash: <a href='https://github.com/edfm-tum/SVD/tree/%2'>%2</a>, date: %3").arg(GIT_BRANCH).arg(GIT_HASH).arg(BUILD_TIMESTAMP);
    return s;

}
QString buildYear()
{
    QString s(BUILD_TIMESTAMP);
    QRegularExpression yearRegex("(\\d{4})");
    QRegularExpressionMatch match = yearRegex.match(s);
    if (match.hasMatch()) {
        QString year = match.captured(1); // Access the captured year
        return year;
    }
    return s; // default
}
