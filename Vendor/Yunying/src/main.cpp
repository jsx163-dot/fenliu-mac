#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QSplashScreen>
#include <QScreen>
#include <QTimeZone>
#include <sysutil.h>
#include "nethelper.h"
#include "version.h"
#ifdef LOCAL_CHECKS
int runLocalChecks(int argc, char **argv);
#endif

#define _ QStringLiteral

MainWindow *w;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QGuiApplication::setApplicationDisplayName(_("分流购票app"));
#ifdef LOCAL_CHECKS
    return runLocalChecks(argc, argv);
#endif

    QFile qss(QStringLiteral(":/res/main.qss"));
    if (qss.open(QFile::ReadOnly)) {
        a.setStyleSheet(qss.readAll());
        qss.close();
    }

    QSplashScreen *splash = new QSplashScreen;
    splash->setPixmap(QPixmap(QStringLiteral(":/icon/images/splash.jpeg")));
    splash->show();

    splash->showMessage(QObject::tr("正在加载资源..."),
                        Qt::AlignBottom | Qt::AlignLeft, Qt::white);

    splash->showMessage(QObject::tr("正在加载站点数据..."),
                        Qt::AlignBottom | Qt::AlignLeft, Qt::white);

    QCoreApplication::setOrganizationName(_("FenliuLocal"));
    QCoreApplication::setOrganizationDomain(_("fenliu.local"));
    QCoreApplication::setApplicationName(_("FenliuYunying"));

    w = new MainWindow;
    w->setUp();
    w->setWindowIcon(QIcon(_(":/icon/images/ticket.ico")));

    QList<QScreen *> screen = QGuiApplication::screens();
    if (!screen.isEmpty()) {
        QRect rect = screen[0]->availableGeometry();
        w->resize(qMin(w->width(), rect.width()), qMin(w->height(), rect.height()));
        w->move(rect.center() - w->rect().center());
    }

    splash->showMessage(QObject::tr("加载完成"),
                        Qt::AlignBottom | Qt::AlignLeft, Qt::white);
    splash->finish(w);
    delete splash;

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
