#include <QCoreApplication>
#include <locale.h>

#include <QTimer>
#include "../SVDUI/version.h"
#include "consoleshell.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    // set linux to use c-locale (i.e. atof("0.9") -> should be 0.9 even on German machines)
    setlocale(LC_ALL, "C");


    printf("SVD console (%s)\n", currentVersion());
    printf("This is the console version of SVD, the Scaling Vegetation Dynamics model.\n");
    printf("More at: https://github.com/edfm-tum/SVD \n");
    printf("(c) Werner Rammer, Rupert Seidl, 2019-%s \n", buildYear().toLocal8Bit().data());
    printf("version: %s\n", verboseVersionHtml().toLocal8Bit().data());
    printf("****************************************\n\n");
    if (a.arguments().count()<3) {
        printf("Usage: \n");
        printf("SVDc.exe <config-file> <years> <...other options>\n");
        printf("Options:\n");
        printf("you specify key=value pairs to overwrite values given in the configuration file.\n");
        printf("E.g.: SVDc project.conf 100 climate.file=climate/historic.txt filemask.run=50\n");
        printf("See also https://github.com/edfm-tum/SVD\n.");
        return 0;
    }
    ConsoleShell svd_shell;

    QTimer::singleShot(0, &svd_shell, SLOT(run()));
    //a.installEventFilter(&iland_shell);
    return a.exec();

}
