#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QSplashScreen>
#include <QScreen>
#include <QTimeZone>
#include <sysutil.h>
#include "nethelper.h"
#include "version.h"
#include "appstyle.h"
#ifdef LOCAL_CHECKS
int runLocalChecks(int argc, char **argv);
#endif

#define _ QStringLiteral

MainWindow *w;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QGuiApplication::setApplicationDisplayName(_("分流购票app"));
    AppStyle::apply(a);
#ifdef LOCAL_CHECKS
    return runLocalChecks(argc, argv);
#endif

    QCoreApplication::setOrganizationName(_("FenliuLocal"));
    QCoreApplication::setOrganizationDomain(_("fenliu.local"));
    QCoreApplication::setApplicationName(_("FenliuYunying"));

    w = new MainWindow;
    w->setUp();
    w->setWindowIcon(AppStyle::icon("train-front", QColor("#315ed4")));

    QList<QScreen *> screen = QGuiApplication::screens();
    if (!screen.isEmpty()) {
        QRect rect = screen[0]->availableGeometry();
        w->resize(qMin(w->width(), rect.width()), qMin(w->height(), rect.height()));
        w->move(rect.center() - w->rect().center());
    }

    NetHelper *nh = NetHelper::instance();
    nh->initLoginCookie();
    nh->getLoginConf();
    nh->leftTicketInit();
#ifdef HAS_CDN
    w->getCdn();
#endif
    w->syncTime();
    w->show();

    return a.exec();
}
