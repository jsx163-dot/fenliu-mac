#include "appstyle.h"
#include "mainwindow.h"
#include "journeywidget.h"
#include "fakecredentialstore.h"
#include "icondelegate.h"
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QHeaderView>
#include <QImage>
#include <QListWidget>
#include <QScrollArea>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>

extern MainWindow *w;

class UiChecks : public QObject {
    Q_OBJECT
public:
    explicit UiChecks(bool capture = false) : capture(capture) {}
private:
    bool capture;
    QTemporaryDir settings;
    void snapshot(QWidget &widget, const QString &name)
    {
        if (capture) QVERIFY(widget.grab().save(QStringLiteral("/tmp/fenliu-ui-%1.png").arg(name)));
    }
    void checkButtons(QWidget &widget)
    {
        for (auto *button : widget.findChildren<QPushButton *>()) {
            if (!button->isVisible() || button->text().isEmpty()) continue;
            const int icon = button->icon().isNull() ? 0 : button->iconSize().width() + 4;
            QVERIFY2(button->width() >= button->fontMetrics().horizontalAdvance(button->text()) + icon + 8,
                     qPrintable(button->text()));
        }
    }
    void populate(MainWindow &window)
    {
        auto *model = qobject_cast<QStandardItemModel *>(window.findChild<QTableView *>("regularResults")->model());
        const QStringList cells = {"G1958", QStringLiteral("太原南"), QStringLiteral("上海虹桥"), "08:40", "18:15", "09:35",
                                   QStringLiteral("无"), "--", QStringLiteral("有"), "16", "--", "--", "--", "--", "--", "--", "--", ""};
        model->insertRows(0, 3);
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < cells.size(); ++col) {
                model->setData(model->index(row, col), cells[col]);
                model->setData(model->index(row, col), cells[col], Qt::ToolTipRole);
            }
        auto *table = window.findChild<QTableWidget *>("journeyPlans");
        const QList<QStringList> rows = {
            {QStringLiteral("整程"), QStringLiteral("太原 → 上海"), QStringLiteral("Z198 软卧"), "10-10 19:20", "10-11 12:40",
             QStringLiteral("488.00 元"), "2", QStringLiteral("无需换铺\n09:30:00")},
            {QStringLiteral("同车两段"), QStringLiteral("太原 → 石家庄北\n石家庄北 → 上海"), QStringLiteral("Z198 软卧\nZ198 硬卧"),
             "10-10 19:20\n10-10 22:25", "10-10 22:10\n10-11 12:40", QStringLiteral("416.00 元"), "3", QStringLiteral("可能换铺\n09:30:03")}
        };
        table->setRowCount(rows.size());
        for (int row = 0; row < rows.size(); ++row)
            for (int col = 0; col < rows[row].size(); ++col) table->setItem(row, col, new QTableWidgetItem(rows[row][col]));
        table->resizeRowsToContents();
        table->selectRow(0);
        window.findChild<QLineEdit *>("regularFrom")->setText(QStringLiteral("太原南"));
        window.findChild<QLineEdit *>("regularTo")->setText(QStringLiteral("上海虹桥"));
        window.findChild<QLineEdit *>("journeyFrom")->setText(QStringLiteral("太原"));
        window.findChild<QLineEdit *>("journeyTo")->setText(QStringLiteral("上海"));
        for (const QString &name : {"journeyFromStations", "journeyToStations"}) {
            auto *list = window.findChild<QListWidget *>(name);
            const QStringList stations = name == "journeyFromStations"
                ? QStringList{QStringLiteral("太原"), QStringLiteral("太原南")}
                : QStringList{QStringLiteral("上海"), QStringLiteral("上海虹桥"), QStringLiteral("上海南")};
            for (const auto &station : stations) {
                auto *item = new QListWidgetItem(station, list);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Checked);
            }
        }
        window.findChild<QTextBrowser *>("activityLog")->setPlainText(QStringLiteral("09:30:00  离线界面预览\n09:30:03  太原南 → 上海虹桥"));
    }
private slots:
    void initTestCase()
    {
        QVERIFY(settings.isValid());
        QCoreApplication::setOrganizationName("FenliuUiChecks");
        QCoreApplication::setApplicationName("OfflinePreview");
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    }
    void init() { QSettings().clear(); FakeCredentials::reset(); }
    void bundledThemeAndIcons()
    {
        QCOMPARE(qApp->palette().color(QPalette::Window), QColor("#ffffff"));
        QVERIFY(!qApp->styleSheet().contains("qlineargradient"));
        const QStringList names = {"train-front", "arrow-right-left", "search", "play", "square", "volume-2",
            "settings-2", "user-round", "clock-3", "ticket", "refresh-cw", "credit-card", "check", "arrow-left",
            "arrow-right", "arrow-up", "arrow-down", "chevrons-left", "chevrons-right", "chevron-down", "chevron-up"};
        for (const auto &name : names) {
            const QImage pixels = AppStyle::icon(name).pixmap(24, 24).toImage();
            QVERIFY2(!pixels.isNull(), qPrintable(name));
            bool visible = false;
            for (int y = 0; y < pixels.height(); ++y)
                for (int x = 0; x < pixels.width(); ++x) visible |= pixels.pixelColor(x, y).alpha() > 0;
            QVERIFY2(visible, qPrintable(name));
        }
    }
    void workspaceFits_data()
    {
        QTest::addColumn<QSize>("size");
        QTest::newRow("compact") << QSize(960, 700);
        QTest::newRow("desktop") << QSize(1200, 840);
        QTest::newRow("wide") << QSize(1440, 900);
    }
    void selectedCustomCellsRemainReadable()
    {
        QTableView view;
        QStandardItemModel model(1, ETRAINTABLECOLUMNENDTOTAL);
        model.setData(model.index(0, EFROMSTATIONCOL), QStringLiteral("太原南"), Qt::ToolTipRole);
        model.setData(model.index(0, ETOSTATIONCOL), QStringLiteral("上海虹桥"), Qt::ToolTipRole);
        model.setData(model.index(0, EARRIVETIMECOL), "18:15");
        IconDelegate delegate;
        view.ensurePolished();
        for (int column : {EFROMSTATIONCOL, ETOSTATIONCOL, EARRIVETIMECOL}) {
            QStyleOptionViewItem option;
            option.initFrom(&view);
            option.widget = &view;
            option.font = view.font();
            option.rect = QRect(0, 0, 140, 38);
            option.state |= QStyle::State_Selected | QStyle::State_Active;
            QImage pixels(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
            pixels.fill(Qt::white);
            QPainter painter(&pixels);
            delegate.paint(&painter, option, model.index(0, column));
            painter.end();
            if (capture) QVERIFY(pixels.save(QStringLiteral("/tmp/fenliu-selected-%1.png").arg(column)));
            const QColor text = option.palette.color(QPalette::HighlightedText);
            int readable = 0;
            for (int y = 8; y < 30; ++y)
                for (int x = 24; x < 120; ++x) {
                    const QColor pixel = pixels.pixelColor(x, y);
                    if (qAbs(pixel.red() - text.red()) < 20 && qAbs(pixel.green() - text.green()) < 20
                        && qAbs(pixel.blue() - text.blue()) < 20) ++readable;
                }
            QVERIFY2(readable > 20, qPrintable(QString::number(column)));
        }
    }
    void workspaceFits()
    {
        QFETCH(QSize, size);
        MainWindow window; w = &window; window.setUp(); populate(window);
        window.resize(size); window.show(); QTest::qWait(80);
        QCOMPARE(window.size(), size);
        auto *tabs = window.findChild<QTabWidget *>("bookingViews");
        auto *support = window.findChild<QDockWidget *>("supportDock");
        auto *controls = window.findChild<QDockWidget *>("bookingControlsDock");
        QVERIFY(tabs); QVERIFY(support); QVERIFY(controls);
        for (int page : {0, 1, 0}) {
            tabs->setCurrentIndex(page); QTest::qWait(30);
            QCOMPARE(controls->isVisible(), page == 0);
            QVERIFY(support->isVisible());
            const QRect content(tabs->mapTo(&window, QPoint()), tabs->size());
            QVERIFY(content.bottom() < support->geometry().top());
            QVERIFY(window.rect().contains(content));
            auto *results = page == 0 ? window.findChild<QTableView *>("regularResults")
                                     : window.findChild<QTableView *>("journeyPlans");
            QVERIFY(results->height() >= 130);
            QVERIFY(results->viewport()->height() >= 80);
            checkButtons(window);
            snapshot(window, QStringLiteral("%1-%2").arg(page == 0 ? "regular" : "smart").arg(size.width()));
        }
        auto *activity = window.findChild<QTabWidget *>("activityViews");
        activity->setCurrentIndex(1); QTest::qWait(40);
        QCOMPARE(controls->height(), 164);
        snapshot(window, QStringLiteral("network-%1").arg(size.width()));
        window.close(); w = nullptr;
    }
    void loginAndSmsFit()
    {
        LoginDialog dialog; dialog.setUp(); dialog.show(); QTest::qWait(30);
        QCOMPARE(dialog.windowTitle(), QStringLiteral("分流购票app · 登录"));
        QCOMPARE(dialog.loginPb->property("role").toString(), QStringLiteral("primary"));
        dialog.remindLabel1->setText(QStringLiteral("离线测试：登录提示可以换行显示，不包含真实账号信息。"));
        checkButtons(dialog);
        snapshot(dialog, "login");
        dialog.showSmsVerification(); QTest::qWait(30);
        for (auto *field : {dialog.userNameLe, dialog.passwdLe, dialog.idCardNumLe, dialog.smsLe}) {
            const QRect bounds(field->mapTo(&dialog, QPoint()), field->size());
            QVERIFY(dialog.rect().contains(bounds));
            QVERIFY(field->height() >= 32);
        }
        QVERIFY(dialog.remindLabel1->height() >= dialog.remindLabel1->heightForWidth(dialog.remindLabel1->width()));
        checkButtons(dialog);
        snapshot(dialog, "sms");
        dialog.close();
    }
    void settingsPagesAreScrollable()
    {
        MainWindow window; w = &window; window.setUp();
        auto *dialog = window.settingDialog;
        dialog->resize(900, 640); dialog->show(); QTest::qWait(40);
        auto *tabs = dialog->findChild<QTabWidget *>("settingsViews"); QVERIFY(tabs);
        QCOMPARE(tabs->count(), 6);
        for (int page = 0; page < tabs->count(); ++page) {
            tabs->setCurrentIndex(page); QTest::qWait(20);
            auto *scroll = qobject_cast<QScrollArea *>(tabs->widget(page)); QVERIFY(scroll);
            QVERIFY(scroll->widgetResizable());
            const QRect bounds(scroll->mapTo(dialog, QPoint()), scroll->size());
            QVERIFY(dialog->rect().contains(bounds));
            snapshot(*dialog, QStringLiteral("settings-%1").arg(page));
        }
        dialog->close(); window.close(); w = nullptr;
    }
    void selectionToolsHaveStableSizes()
    {
        MainWindow window; w = &window; window.setUp();
        const QList<QDialog *> dialogs = {window.passengerDialog, window.trainNoDialog, window.seatTypeDialog, window.seatDialog};
        for (int i = 0; i < dialogs.size(); ++i) {
            auto *dialog = dialogs[i]; dialog->show(); QTest::qWait(30);
            checkButtons(*dialog);
            for (auto *button : dialog->findChildren<QPushButton *>()) {
                if (button->property("role") != "icon") continue;
                QCOMPARE(button->size(), QSize(34, 34));
                QVERIFY(!button->toolTip().isEmpty());
                QVERIFY(!button->accessibleName().isEmpty());
            }
            snapshot(*dialog, QStringLiteral("selection-%1").arg(i));
            dialog->close();
        }
        window.close(); w = nullptr;
    }
};

int runUiChecks(int argc, char **argv)
{
    UiChecks checks; return QTest::qExec(&checks, argc, argv);
}

int runUiPreview()
{
    char name[] = "UiPreview";
    char *args[] = {name, nullptr};
    UiChecks checks(true); return QTest::qExec(&checks, 1, args);
}

#include "uichecks.moc"
