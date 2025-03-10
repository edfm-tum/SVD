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

#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "version.h"
AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    // fetch version information

    ui->version->setText( QString("Version: %1 %2").arg(currentVersion()).arg(compiler()) );
    //ui->svnversion->setText( QString("GIT-Revision: %1 - build date: %2").arg(gitVersion()).arg(bd));
    ui->svnversion->setText( verboseVersionHtml() );

    QString s = ui->info->toHtml();
    s.replace("XXXX", buildYear());
    ui->info->setHtml(s);

}

AboutDialog::~AboutDialog()
{
    delete ui;
}
