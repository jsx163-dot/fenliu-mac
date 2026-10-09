#include "logindialog.h"
#include "paymentview.h"
#include "userdata.h"
#include "credentialstore.h"
#include "fakecredentialstore.h"
#include <QApplication>
#include <QDialog>
#include <QFileInfo>
#include <QLabel>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
int runJourneyChecks(int argc, char **argv);
int runJourneyPreview();

class MacChecks : public QObject {
    Q_OBJECT
private:
    QTemporaryDir settings;
private slots:
    void initTestCase()
    {
        QVERIFY(settings.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("FenliuOfflineTests"));
        QCoreApplication::setApplicationName(QStringLiteral("YunyingChecks"));
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    }

    void init()
    {
        QSettings().clear();
        FakeCredentials::reset();
    }

    void noSavedPasswordByDefault()
    {
        LoginDialog dialog;
        dialog.setUp();
        QVERIFY(!dialog.keepPasswdCB->isChecked());
        QVERIFY(dialog.passwdLe->text().isEmpty());
        QCOMPARE(FakeCredentials::saves, 0);
        QCOMPARE(dialog.passwdLe->echoMode(), QLineEdit::Password);
    }

    void rememberedLoginAndForget()
    {
        QSettings().setValue(QStringLiteral("login/keep_passwd"), true);
        FakeCredentials::username = QStringLiteral("offline-test-user");
        FakeCredentials::password = QStringLiteral(" test-password ");
        LoginDialog dialog;
        dialog.setUp();
        QCOMPARE(dialog.userNameLe->text(), FakeCredentials::username);
        QCOMPARE(dialog.passwdLe->text(), FakeCredentials::password);
        dialog.keepPasswdCB->setChecked(false);
        QVERIFY(FakeCredentials::password.isEmpty());
        QVERIFY(!QSettings().value(QStringLiteral("login/keep_passwd")).toBool());
        QVERIFY(!QSettings().contains(QStringLiteral("login/passwd")));
    }

    void keychainFailureDoesNotPretendToForget()
    {
        QSettings().setValue(QStringLiteral("login/keep_passwd"), true);
        FakeCredentials::fail = true;
        LoginDialog dialog;
        dialog.setUp();
        QVERIFY(!dialog.remindLabel1->text().isEmpty());
        dialog.keepPasswdCB->setChecked(false);
        QVERIFY(dialog.keepPasswdCB->isChecked());
        QVERIFY(QSettings().value(QStringLiteral("login/keep_passwd")).toBool());
    }

    void failedSaveStopsBeforeNetworkLogin()
    {
        FakeCredentials::fail = true;
        LoginDialog dialog;
        dialog.setUp();
        dialog.keepPasswdCB->setChecked(true);
        dialog.userNameLe->setText(QStringLiteral("offline-test-user"));
        dialog.passwdLe->setText(QStringLiteral(" test-password "));
        dialog.onLogin();
        QCOMPARE(FakeCredentials::saves, 1);
        QVERIFY(!dialog.remindLabel1->text().isEmpty());
        QVERIFY(!QSettings().contains(QStringLiteral("login/passwd")));
        QCOMPARE(UserData::instance()->getUserLoginInfo().passwd, QStringLiteral(" test-password "));
    }

    void paymentUrlPolicy()
    {
        QVERIFY(isAllowedPaymentUrl(QUrl("https://epay.12306.cn/pay/webBusiness")));
        QVERIFY(isAllowedPaymentUrl(QUrl("https://cashier.alipay.com/")));
        QVERIFY(isAllowedPaymentUrl(QUrl("https://pay.weixin.qq.com/")));
        QVERIFY(!isAllowedPaymentUrl(QUrl("https://12306.cn.example.org/")));
        QVERIFY(!isAllowedPaymentUrl(QUrl("https://not12306.cn/")));
        QVERIFY(!isAllowedPaymentUrl(QUrl("http://epay.12306.cn/")));
        QVERIFY(!isAllowedPaymentUrl(QUrl("https://epay.12306.cn:8443/")));
        QVERIFY(!isAllowedPaymentUrl(QUrl("https://user@epay.12306.cn/")));
        QVERIFY(!isAllowedPaymentUrl(QUrl("alipay://pay")));
        QVERIFY(!isAllowedPaymentUrl(QUrl("file:///tmp/payment.html")));
    }

    void paymentStaysInApplication()
    {
        QWidget parent;
        parent.show();
        showPaymentPage(&parent, QByteArray("<html><body><h1>Offline payment preview</h1>"
            "<a href='https://invalid.example/'>Blocked destination</a></body></html>"),
            QUrl("https://epay.12306.cn/pay/webBusiness"), {});
        QDialog *payment = parent.findChild<QDialog *>();
        QVERIFY(payment);
        QTRY_VERIFY(payment->isVisible());
        QVERIFY(payment->width() <= payment->screen()->availableGeometry().width());
        QTest::qWait(1500);
        payment->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!parent.findChild<QDialog *>());
    }

    void paymentRejectsUntrustedRedirect()
    {
        QWidget parent;
        showPaymentPage(&parent, QByteArray("<html><body>Offline redirect test"
            "<script>setTimeout(function(){location.href='https://invalid.example/'},100)</script>"
            "</body></html>"), QUrl("https://epay.12306.cn/pay/webBusiness"), {});
        QDialog *payment = parent.findChild<QDialog *>();
        QVERIFY(payment);
        QLabel *status = payment->findChild<QLabel *>();
        QVERIFY(status);
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("invalid.example")), 5000);
        QVERIFY(status->text().startsWith(QStringLiteral("已阻止")));
        payment->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
};

int runLocalChecks(int argc, char **argv)
{
    if (argc == 2 && QString::fromUtf8(argv[1]) == QStringLiteral("--journey-preview")) return runJourneyPreview();
    if (argc == 2 && QString::fromUtf8(argv[1]) == QStringLiteral("--payment-preview")) {
        QWidget parent;
        showPaymentPage(&parent, QByteArray("<html><body style='font:18px system-ui;padding:32px'>"
            "<h1>Offline payment preview</h1><p>No account, order or payment data.</p>"
            "<a href='https://invalid.example/'>Blocked destination</a></body></html>"),
            QUrl("https://epay.12306.cn/pay/webBusiness"), {});
        QTimer::singleShot(60000, qApp, &QCoreApplication::quit);
        return qApp->exec();
    }
    MacChecks checks;
    const int original = QTest::qExec(&checks, argc, argv);
    return original | runJourneyChecks(argc, argv);
}

#include "macchecks.moc"
