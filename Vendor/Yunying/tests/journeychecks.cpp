#include "journeywidget.h"
#include "userdata.h"
#include "mainwindow.h"
#include "nethelper.h"
#include <QApplication>
#include <QCheckBox>
#include <QCryptographicHash>
#include <QDir>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QJsonDocument>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QListWidget>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>
extern MainWindow *w;

namespace {
QByteArray stationFixture()
{
    return QStringLiteral("var station_names ='@tyu|太原|TYV|taiyuan|ty|1|0405|太原|||"
        "@tyn|太原南|TNV|taiyuannan|tyn|2|0405|太原|||"
        "@mid|中间站|MID|zhongjianzhan|zjz|3|0901|中间城市|||"
        "@sha|上海|SHH|shanghai|sh|4|0712|上海|||"
        "@fake|太原别处|BAD|taiyuanbiechu|tybc|5|0000|其他城市|||';").toUtf8();
}
Journey::Query ticketQuery(const QString &from = "TYV", const QString &to = "SHH", int day = 10)
{
    Journey::Query q; q.from = from; q.to = to; q.date = QDate(2026, 10, day); return q;
}
QString rawTicket(const Journey::Query &q, int count = 8, bool priced = true,
                  QString train = "RUN001", QString runDate = "20261010")
{
    QStringList f; for (int i = 0; i < ETRAININFOMAX; ++i) f.append(QString());
    f[ESECRETSTR] = count ? "fake-ticket-token" : ""; f[ETEXTINFO] = QStringLiteral("预订");
    f[ETRAINNO] = train; f[ESTATIONTRAINCODE] = "K001"; f[ESTARTTRAINDATE] = runDate;
    f[EFROMSTATIONTELECODE] = q.from; f[ETOSTATIONTELECODE] = q.to;
    f[EFROMSTATIONNO] = q.from == "MID" ? "02" : "01";
    f[ETOSTATIONNO] = q.to == "MID" ? "02" : "03";
    f[ESTARTTIME] = q.from == "MID" ? "08:10" : "22:00";
    f[EARRIVETIME] = q.to == "MID" ? "08:00" : "12:00";
    f[ESPENDTIME] = q.to == "MID" ? "10:00" : q.from == "MID" ? "03:50" : "14:00";
    f[ECANWEBBUY] = "Y"; f[ERWNUM] = QString::number(count); f[EYWNUM] = QString::number(count);
    f[EYPINFONEW] = priced ? "401000000830080000008" : "";
    return f.join('|');
}
QVariantMap ticketData(const Journey::Query &q, int count = 8, bool priced = true,
                       QString train = "RUN001", QString runDate = "20261010")
{
    return {{"result", QVariantList{rawTicket(q, count, priced, train, runDate)}},
            {"map", QVariantMap{{"TYV", QStringLiteral("太原")}, {"SHH", QStringLiteral("上海")}, {"MID", QStringLiteral("中间站")}}}};
}
QVariantMap stopData()
{
    return {{"data", QVariantList{
        QVariantMap{{"station_name", QStringLiteral("太原")}, {"station_no", "01"}, {"arrive_time", "----"}, {"start_time", "22:00"}},
        QVariantMap{{"station_name", QStringLiteral("中间站")}, {"station_no", "02"}, {"arrive_time", "08:00"}, {"start_time", "08:10"}},
        QVariantMap{{"station_name", QStringLiteral("上海")}, {"station_no", "03"}, {"arrive_time", "12:00"}, {"start_time", "----"}}}}};
}
Journey::Request preferences()
{
    Journey::Request r;
    r.from = {{QStringLiteral("太原"), "TYV", "0405", QStringLiteral("太原")}};
    r.to = {{QStringLiteral("上海"), "SHH", "0712", QStringLiteral("上海")}};
    r.date = QDate(2026, 10, 10); r.seats = Journey::defaultSeats(); r.passengers = {"private-person-ref"};
    return r;
}
Journey::Plan splitPlan()
{
    const auto a = ticketQuery("TYV", "MID"), b = ticketQuery("MID", "SHH", 11);
    const auto r = preferences();
    return {{Journey::availableLegs(Journey::parseTrains(ticketData(a), a), r).first(),
             Journey::availableLegs(Journey::parseTrains(ticketData(b), b), r).first()}, 1};
}
Journey::Plan directPlan()
{
    const auto q = ticketQuery();
    return {{Journey::availableLegs(Journey::parseTrains(ticketData(q), q), preferences()).first()}, 1};
}

class FakeGateway : public JourneyGateway {
public:
    explicit FakeGateway(int interval = 0) : JourneyGateway(nullptr, interval) {}
    QVector<Journey::Query> calls;
    QVector<Journey::Leg> submissions;
    QVector<Journey::Order> pending, paid;
    QMap<QString, QVariantMap> snapshots;
    QMap<int, QVariantMap> orderSnapshots;
    QMap<int, QVector<Journey::Order>> paidPages;
    int currentPage = 0;
    quint64 heldId = 0;
    Journey::Query heldQuery;
    bool hold = false, owned = false, silentSubmit = false, rejectSubmit = false;
    int firstCount = 8, secondCount = 8, wholeCount = 0, pays = 0;
    QString error;
    bool reserve(QString &message) override {
        if (owned) { message = "busy"; return false; } owned = true; return true;
    }
    void release() override { owned = false; }
    void pauseSubmission() override {}
    void submit(const Journey::Leg &leg, const QStringList &people) override {
        if (rejectSubmit) { emit submissionRejected("local validation failed"); return; }
        submissions.append(leg);
        if (silentSubmit) return;
        Journey::Order order; order.status = Journey::Order::Pending;
        order.id = "ORDER" + QString::number(submissions.size()); order.leg = leg; order.passengers = people;
        order.total = leg.price * people.size();
        pending = {order}; emit submissionFinished();
    }
    QVector<Journey::Order> orders(const QVariantMap &, bool unpaid) const override {
        return unpaid ? pending : paidPages.isEmpty() ? paid : paidPages.value(currentPage);
    }
    void pay(const QString &, const Journey::Leg &) override { ++pays; }
    void replyHeld(const QVariantMap &data = {}, const QString &err = {}) { complete(heldId, data, err); }
    void forceLate(quint64 id, const Journey::Query &query, const QVariantMap &data) { emit received(id, query, data, {}); }
    void markPaid() { auto order = pending.first(); order.status = Journey::Order::Paid; paid.append(order); pending.clear(); }
protected:
    void performRequest(quint64 id, const Journey::Query &q) override {
        calls.append(q);
        if (hold) { heldId = id; heldQuery = q; return; }
        QVariantMap data;
        if (q.kind == Journey::Query::PendingOrders) data = {{"orderDBList", QVariantList{}}};
        else if (q.kind == Journey::Query::PaidOrders) data = {{"OrderDTODataList", QVariantList{}}};
        else if (q.kind == Journey::Query::Stops) data = stopData();
        else data = snapshots.contains(q.key()) ? snapshots[q.key()]
            : ticketData(q, q.to == "MID" ? firstCount : q.from == "MID" ? secondCount : wholeCount);
        if (q.kind == Journey::Query::PaidOrders && !paidPages.isEmpty()) {
            currentPage = q.page; QVariantList rows; int total = 0;
            for (const auto &page : paidPages) total += page.size();
            for (int i = 0; i < paidPages.value(q.page).size(); ++i) rows.append(QVariantMap{{"fixture", true}});
            data = {{"OrderDTODataList", rows}, {"order_total_number", total}};
        }
        if (orderSnapshots.contains(q.kind)) data = orderSnapshots.value(q.kind);
        complete(id, data, error);
    }
};

class FixtureReply : public QNetworkReply {
    QByteArray bytes;
    qint64 offset = 0;
public:
    FixtureReply(const QNetworkRequest &request, const QByteArray &body, QObject *parent, const QUrl &redirect = {}) : QNetworkReply(parent), bytes(body) {
        setRequest(request); setUrl(request.url()); setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        if (!redirect.isEmpty()) {
            setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 302);
            setAttribute(QNetworkRequest::RedirectionTargetAttribute, redirect);
        }
        setHeader(QNetworkRequest::ContentTypeHeader, "application/json"); open(QIODevice::ReadOnly);
        QTimer::singleShot(0, this, [this] { setFinished(true); emit readyRead(); emit finished(); });
    }
    void abort() override { setError(OperationCanceledError, "cancelled"); }
    qint64 bytesAvailable() const override { return bytes.size() - offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char *target, qint64 size) override {
        const qint64 n = qMin(size, bytes.size() - offset);
        if (!n) return -1;
        memcpy(target, bytes.constData() + offset, size_t(n)); offset += n; return n;
    }
};
class FixtureManager : public QNetworkAccessManager {
public:
    explicit FixtureManager(QObject *parent = nullptr) : QNetworkAccessManager(parent) {}
    QVector<QNetworkRequest> calls;
    QVector<QByteArray> bodies;
    bool denyLogin = false, simulateBooking = false;
    QUrl paymentRedirect;
protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *outgoing) override {
        calls.append(request); bodies.append(outgoing ? outgoing->readAll() : QByteArray());
        if (!paymentRedirect.isEmpty()) return new FixtureReply(request, {}, this, paymentRedirect);
        if (simulateBooking) {
            const QString path = request.url().path();
            if (path.endsWith("/initDc")) return new FixtureReply(request,
                "globalRepeatSubmitToken = 'fake-repeat-token'; ticketInfoForPassengerForm = "
                "{'purpose_codes':'ADULT','train_location':'P2','isAsync':'0',"
                "'key_check_isChange':'fake-check-key','leftTicketStr':'fake-left-ticket'};", this);
            const QVariantMap data = path.endsWith("/checkUser") ? QVariantMap{{"flag", true}}
                : QVariantMap{{"submitStatus", true}};
            return new FixtureReply(request, QJsonDocument::fromVariant(QVariantMap{{"status", true}, {"data", data}}).toJson(), this);
        }
        const auto q = ticketQuery();
        const QVariantMap response{{"status", true}, {"data", denyLogin ? QVariantMap{{"flag", false}} : ticketData(q, 0)}};
        return new FixtureReply(request, QJsonDocument::fromVariant(response).toJson(), this);
    }
};
}

class JourneyChecks : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
private slots:
    void initTestCase()
    {
        QVERIFY(settings.isValid());
        QCoreApplication::setOrganizationName("FenliuJourneyOfflineTests");
        QCoreApplication::setApplicationName("JourneyChecks");
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    }
    void init() { QSettings().clear(); }
    void officialCityMembership()
    {
        const auto stations = Journey::parseStations(stationFixture());
        QCOMPARE(stations.size(), 5);
        QCOMPARE(stations[1].city, QStringLiteral("太原"));
        QCOMPARE(stations[4].city, QStringLiteral("其他城市"));
        const auto legacy = Journey::parseStations("var stations='@abc|Old|ABC|old|old|0';");
        QCOMPARE(legacy.size(), 1); QVERIFY(legacy[0].city.isEmpty());
    }
    void soldOutTrainRetained()
    {
        const auto q = ticketQuery(); const auto trains = Journey::parseTrains(ticketData(q, 0), q);
        QCOMPARE(trains.size(), 1); QVERIFY(!trains.first().route.bookable);
        QVERIFY(Journey::availableLegs(trains, preferences()).isEmpty());
    }
    void missingQuoteAndPassengerCount()
    {
        const auto q = ticketQuery(); auto r = preferences(); r.passengers.append("second-ref");
        QVERIFY(Journey::availableLegs(Journey::parseTrains(ticketData(q, 1), q), r).isEmpty());
        const auto leg = Journey::availableLegs(Journey::parseTrains(ticketData(q, 8, false), q), r).first();
        Journey::Plan plan{{leg}, 2}; QCOMPARE(plan.total(), -1);
        QVERIFY(Journey::validPlan(plan, r)); r.budget = 50000; QVERIFY(!Journey::validPlan(plan, r));
        FakeGateway network; JourneyBooking booking(&network); QVERIFY(!booking.begin(plan, r)); QVERIFY(network.calls.isEmpty());
    }
    void rankingAndBudget()
    {
        const auto q = ticketQuery(); auto r = preferences();
        const auto legs = Journey::availableLegs(Journey::parseTrains(ticketData(q), q), r);
        Journey::Plan hard{{legs[1]}, 1}, soft{{legs[0]}, 1};
        QVector<Journey::Plan> plans{splitPlan(), hard, soft}; Journey::sortPlans(plans, r);
        QCOMPARE(plans[0].key(), soft.key()); QCOMPARE(plans[1].key(), hard.key()); QCOMPARE(plans[2].legs.size(), 2);
        r.budget = 7999; QVERIFY(!Journey::validPlan(hard, r));
        r.budget = 8000; QVERIFY(Journey::validPlan(hard, r));
        r.earliest = QTime(23, 0); QVERIFY(!Journey::validPlan(hard, r));
    }
    void crossMidnightAndTrainIdentity()
    {
        const auto q = ticketQuery(); const auto train = Journey::parseTrains(ticketData(q, 0), q).first();
        const auto queries = Journey::splitQueries(train, stopData(), Journey::parseStations(stationFixture()));
        QCOMPARE(queries.size(), 1); QCOMPARE(queries[0].second.date, QDate(2026, 10, 11));
        auto plan = splitPlan(); QVERIFY(Journey::validPlan(plan, preferences()));
        plan.legs[1].runDate = plan.legs[1].runDate.addDays(1); QVERIFY(!Journey::validPlan(plan, preferences()));
        plan = splitPlan(); plan.legs[1].trainNo = "OTHER"; QVERIFY(!Journey::validPlan(plan, preferences()));
        plan = splitPlan(); plan.legs[1].fromIndex++; QVERIFY(!Journey::validPlan(plan, preferences()));
        auto broken = stopData(); auto list = broken["data"].toList(); auto stop = list.last().toMap();
        stop["arrive_time"] = "13:00"; list.last() = stop; broken["data"] = list;
        QVERIFY(Journey::splitQueries(train, broken, Journey::parseStations(stationFixture())).isEmpty());
    }
    void searchFindsTwoLegs()
    {
        FakeGateway network; JourneySearch search(&network);
        search.start(preferences(), Journey::parseStations(stationFixture()));
        QTRY_VERIFY(!search.running()); QCOMPARE(search.plans().size(), 4);
        QCOMPARE(network.calls.size(), 4); QCOMPARE(search.plans().first().legs.size(), 2);
        for (const auto &p : search.plans()) QVERIFY(Journey::validPlan(p, preferences()));
        QVERIFY(network.submissions.isEmpty());
    }
    void stopsAndOldResponsesAreIgnored()
    {
        FakeGateway network; network.hold = true; JourneySearch search(&network);
        search.start(preferences(), Journey::parseStations(stationFixture()));
        QTRY_VERIFY(network.heldId != 0); const auto id = network.heldId; const auto query = network.heldQuery;
        search.stop(); network.forceLate(id, query, ticketData(query)); QVERIFY(search.plans().isEmpty());
        search.start(preferences(), Journey::parseStations(stationFixture()));
        network.forceLate(id, query, ticketData(query)); QVERIFY(search.plans().isEmpty()); search.stop();
    }
    void deduplicatedQueriesAndThrottling()
    {
        FakeGateway network(50); QElapsedTimer clock; clock.start();
        network.request(ticketQuery()); network.request(ticketQuery());
        QTRY_COMPARE(network.calls.size(), 2); QVERIFY(clock.elapsed() >= 50);
        FakeGateway dedup; JourneySearch search(&dedup); auto r = preferences(); r.from.append(r.from[0]);
        search.start(r, Journey::parseStations(stationFixture())); QTRY_VERIFY(!search.running()); QCOMPARE(dedup.calls.size(), 4);
    }
    void officialRequestsOnly()
    {
        for (int kind = Journey::Query::Tickets; kind <= Journey::Query::PaidOrders; ++kind) {
            auto q = ticketQuery(); q.kind = Journey::Query::Kind(kind); q.trainNo = "RUN001";
            const auto spec = LocalJourneyGateway::requestSpec(q);
            QCOMPARE(spec.first.url().scheme(), QString("https")); QCOMPARE(spec.first.url().host(), QString("kyfw.12306.cn"));
            QCOMPARE(spec.first.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(), int(QNetworkRequest::ManualRedirectPolicy));
            const auto combined = spec.first.url().toEncoded() + spec.second;
            QVERIFY(!combined.contains("private-person-ref")); QVERIFY(!combined.contains("password")); QVERIFY(!combined.contains("ftqq"));
        }
    }
    void realTransportWithMockNetwork()
    {
        FixtureManager manager; LocalJourneyGateway gateway(nullptr, &manager);
        bool received = false; QString error;
        connect(&gateway, &JourneyGateway::received, this, [&](quint64, const Journey::Query &, const QVariantMap &data, const QString &e) {
            error = e; received = data.contains("result");
        });
        gateway.request(ticketQuery()); QTRY_VERIFY(received); QVERIFY(error.isEmpty());
        QCOMPARE(manager.calls.size(), 1); QCOMPARE(manager.calls[0].url().host(), QString("kyfw.12306.cn"));
        QVERIFY(manager.bodies[0].isEmpty()); QVERIFY(!manager.calls[0].hasRawHeader("Cookie"));
    }
    void confirmedSequentialOrders()
    {
        FakeGateway network; JourneyBooking booking(&network);
        QVERIFY(booking.begin(splitPlan(), preferences())); QVERIFY(!booking.begin(splitPlan(), preferences()));
        QTRY_COMPARE(booking.state(), JourneyBooking::AwaitingPayment); QCOMPARE(network.submissions.size(), 1);
        booking.reconcile(preferences().passengers);
        QTRY_COMPARE(booking.state(), JourneyBooking::AwaitingPayment); QCOMPARE(network.submissions.size(), 1);
        QCOMPARE(network.pays, 0); booking.pay(); QCOMPARE(network.pays, 1);
        network.markPaid(); booking.reconcile(preferences().passengers);
        QTRY_COMPARE(network.submissions.size(), 2); QTRY_COMPARE(booking.state(), JourneyBooking::AwaitingPayment);
        QCOMPARE(network.submissions[1].date, QDate(2026, 10, 11));
        network.markPaid(); booking.reconcile(preferences().passengers);
        QTRY_COMPARE(booking.state(), JourneyBooking::Complete); QVERIFY(!network.owned);
        QCOMPARE(network.submissions.size(), 2);
    }
    void disappearedOrderIsNotPayment()
    {
        FakeGateway network; network.silentSubmit = true; JourneyBooking booking(&network);
        QVERIFY(booking.begin(splitPlan(), preferences())); QTRY_COMPARE(booking.state(), JourneyBooking::Submitting);
        booking.pause(); booking.reconcile(preferences().passengers);
        QTRY_COMPARE(booking.state(), JourneyBooking::Paused); QCOMPARE(network.submissions.size(), 1);
        QVERIFY(!booking.reset()); QVERIFY(booking.message().contains(QStringLiteral("不自动重试")));
    }
    void changedQuoteStopsBeforeOrder()
    {
        FakeGateway network; network.snapshots[ticketQuery("TYV", "MID").key()] = ticketData(ticketQuery("TYV", "MID"), 8, false);
        JourneyBooking booking(&network); QVERIFY(booking.begin(splitPlan(), preferences()));
        QTRY_COMPARE(booking.state(), JourneyBooking::Paused); QVERIFY(network.submissions.isEmpty()); QVERIFY(booking.reset());
    }
    void secondLegLostAfterPayment()
    {
        FakeGateway network; JourneyBooking booking(&network); QVERIFY(booking.begin(splitPlan(), preferences()));
        QTRY_COMPARE(booking.state(), JourneyBooking::AwaitingPayment); network.markPaid(); network.secondCount = 0;
        booking.reconcile(preferences().passengers); QTRY_COMPARE(booking.state(), JourneyBooking::Partial);
        QCOMPARE(network.submissions.size(), 1); QCOMPARE(booking.legIndex(), 1);
    }
    void previousPaidLegMustStillExist_data()
    {
        QTest::addColumn<int>("change");
        QTest::newRow("missing") << 0;
        QTest::newRow("cancelled") << 1;
        QTest::newRow("unknown") << 2;
        QTest::newRow("wrong-passenger") << 3;
    }
    void previousPaidLegMustStillExist()
    {
        QFETCH(int, change);
        FakeGateway network; JourneyBooking booking(&network);
        QVERIFY(booking.begin(splitPlan(), preferences()));
        QTRY_COMPARE(booking.state(), JourneyBooking::AwaitingPayment);
        bool intercepted = false;
        connect(&booking, &JourneyBooking::changed, &network, [&] {
            if (!intercepted && booking.legIndex() == 1 && booking.state() == JourneyBooking::Reconciling) {
                intercepted = true; network.hold = true;
            }
        });
        network.markPaid(); booking.reconcile(preferences().passengers);
        QTRY_VERIFY(network.heldId != 0);
        QCOMPARE(booking.legIndex(), 1);
        if (change == 0) network.paid.clear();
        else if (change == 1) network.paid[0].status = Journey::Order::Cancelled;
        else if (change == 2) network.paid[0].status = Journey::Order::Unknown;
        else network.paid[0].passengers = {"different-person-ref"};
        network.hold = false;
        network.replyHeld({{"orderDBList", QVariantList{}}});
        QTRY_VERIFY(booking.state() == JourneyBooking::Partial || booking.state() == JourneyBooking::AwaitingPayment);
        QCOMPARE(network.submissions.size(), 1);
        QCOMPARE(booking.state(), JourneyBooking::Partial);
    }
    void firstLegCancelled()
    {
        FakeGateway network; JourneyBooking booking(&network); QVERIFY(booking.begin(splitPlan(), preferences()));
        QTRY_COMPARE(booking.state(), JourneyBooking::AwaitingPayment);
        auto order = network.pending.first(); order.status = Journey::Order::Cancelled; network.pending.clear(); network.paid = {order};
        booking.reconcile(preferences().passengers); QTRY_COMPARE(booking.state(), JourneyBooking::Paused);
        QCOMPARE(network.submissions.size(), 1); QVERIFY(booking.reset());
    }
    void foreignOrderAndUnknownPaidState()
    {
        FakeGateway network; Journey::Order other; other.id = "FOREIGN"; network.pending = {other};
        JourneyBooking booking(&network); QVERIFY(booking.begin(splitPlan(), preferences()));
        QTRY_COMPARE(booking.state(), JourneyBooking::Paused); QVERIFY(network.submissions.isEmpty());
        QVERIFY(booking.reset()); network.pending.clear(); QVERIFY(booking.begin(splitPlan(), preferences()));
        QTRY_COMPARE(booking.state(), JourneyBooking::AwaitingPayment);
        auto order = network.pending.first(); order.status = Journey::Order::Unknown; network.pending.clear(); network.paid = {order};
        booking.reconcile(preferences().passengers); QTRY_COMPARE(booking.state(), JourneyBooking::Paused);
        QCOMPARE(network.submissions.size(), 1);
    }
    void existingOrderNotBoughtAgain()
    {
        FakeGateway network; auto p = directPlan(); Journey::Order order;
        order.id = "EXISTING"; order.status = Journey::Order::Paid; order.leg = p.legs.first(); order.passengers = preferences().passengers;
        network.paid = {order}; JourneyBooking booking(&network); QVERIFY(booking.begin(p, preferences()));
        QTRY_COMPARE(booking.state(), JourneyBooking::Paused); QVERIFY(network.submissions.isEmpty());
    }
    void restartRequiresConfirmationAndNoSensitivePersistence()
    {
        FakeGateway network;
        { JourneyBooking booking(&network); QVERIFY(booking.begin(splitPlan(), preferences())); QTRY_COMPARE(booking.state(), JourneyBooking::AwaitingPayment); }
        const QByteArray stored = QSettings().value("journey/progress").toByteArray();
        QVERIFY(!stored.contains("private-person-ref")); QVERIFY(!stored.contains("fake-ticket-token")); QVERIFY(!stored.contains("password"));
        FakeGateway next; JourneyBooking restored(&next); QCOMPARE(restored.state(), JourneyBooking::Paused);
        QTest::qWait(20); QVERIFY(next.calls.isEmpty()); QVERIFY(next.submissions.isEmpty());
    }
    void timeoutDoesNotRetry()
    {
        FakeGateway network; network.silentSubmit = true; JourneyBooking booking(&network, nullptr, 30);
        QVERIFY(booking.begin(splitPlan(), preferences())); QTRY_COMPARE(booking.state(), JourneyBooking::Paused);
        QCOMPARE(network.submissions.size(), 1); QVERIFY(!booking.reset());
    }
    void locallyRejectedSubmissionCanBeReconfirmed()
    {
        FakeGateway network; network.rejectSubmit = true; JourneyBooking booking(&network);
        QVERIFY(booking.begin(splitPlan(), preferences())); QTRY_COMPARE(booking.state(), JourneyBooking::Paused);
        QVERIFY(network.submissions.isEmpty()); QVERIFY(booking.reset());
    }
    void changedSecondQuoteNeedsExplicitConfirmation()
    {
        FakeGateway network; JourneyBooking booking(&network); QVERIFY(booking.begin(splitPlan(), preferences()));
        QTRY_COMPARE(booking.state(), JourneyBooking::AwaitingPayment); network.markPaid();
        const auto q = ticketQuery("MID", "SHH", 11);
        auto data = ticketData(q); auto rows = data["result"].toList(); auto raw = rows[0].toString();
        raw.replace("4010000008", "4011000008"); rows[0] = raw; data["result"] = rows; network.snapshots[q.key()] = data;
        booking.reconcile(preferences().passengers); QTRY_COMPARE(booking.state(), JourneyBooking::Partial);
        QVERIFY(booking.hasNewQuote()); QCOMPARE(network.submissions.size(), 1);
        QCOMPARE(booking.proposedPlan().total(), 21000);
        booking.acceptNewQuote(); QTRY_COMPARE(booking.state(), JourneyBooking::AwaitingPayment);
        QCOMPARE(network.submissions.size(), 2); QCOMPARE(network.submissions.last().price, 11000);
    }
    void restorePaidFirstLegDoesNotBuyItAgain()
    {
        FakeGateway old;
        { JourneyBooking booking(&old); QVERIFY(booking.begin(splitPlan(), preferences())); QTRY_COMPARE(booking.state(), JourneyBooking::AwaitingPayment);
          old.markPaid(); old.secondCount = 0; booking.reconcile(preferences().passengers); QTRY_COMPARE(booking.state(), JourneyBooking::Partial); }
        FakeGateway next; next.paid = old.paid; JourneyBooking restored(&next);
        QCOMPARE(restored.state(), JourneyBooking::Paused); QVERIFY(next.submissions.isEmpty());
        restored.reconcile(preferences().passengers); QTRY_COMPARE(restored.state(), JourneyBooking::AwaitingPayment);
        QCOMPARE(next.submissions.size(), 1); QCOMPARE(next.submissions[0].from, QString("MID"));
    }
    void serverOrderParsingRequiresExplicitPaidStatus()
    {
        QVariantMap ticket{{"stationTrainDTO", QVariantMap{{"station_train_code", "K001"}, {"from_station_telecode", "TYV"}, {"to_station_telecode", "MID"}}},
            {"start_train_date_page", "2026-10-10"}, {"seat_type_code", "4"},
            {"passengerDTO", QVariantMap{{"passenger_id_type_code", "1"}, {"passenger_id_no", "FAKE-DOCUMENT-NUMBER"}}},
            {"ticket_status_name", QStringLiteral("已支付")}, {"str_ticket_price_page", "123.45"}};
        QVariantMap order{{"sequence_no", "ORDER"}, {"tickets", QVariantList{ticket}}};
        auto parsed = LocalJourneyGateway::decodeOrders({{"OrderDTODataList", QVariantList{order}}}, false);
        QCOMPARE(parsed.size(), 1); QCOMPARE(parsed[0].status, Journey::Order::Paid); QCOMPARE(parsed[0].total, 12345);
        ticket["ticket_status_name"] = QStringLiteral("处理中"); order["tickets"] = QVariantList{ticket};
        parsed = LocalJourneyGateway::decodeOrders({{"OrderDTODataList", QVariantList{order}}}, false);
        QCOMPARE(parsed[0].status, Journey::Order::Unknown);
    }
    void malformedOrderListsCannotStartPurchase()
    {
        FakeGateway network; network.orderSnapshots[Journey::Query::PendingOrders] = {{"orderDBList", true}};
        JourneyBooking booking(&network); QVERIFY(booking.begin(splitPlan(), preferences()));
        QTRY_COMPARE(booking.state(), JourneyBooking::Paused); QVERIFY(network.submissions.isEmpty());
    }
    void allPaidOrderPagesAreChecked()
    {
        FakeGateway network;
        for (int i = 0; i < 8; ++i) { Journey::Order other; other.id = "OTHER" + QString::number(i); network.paidPages[0].append(other); }
        Journey::Order existing; existing.id = "EXISTING"; existing.status = Journey::Order::Paid;
        existing.leg = directPlan().legs[0]; existing.passengers = preferences().passengers;
        network.paidPages[1] = {existing}; JourneyBooking booking(&network);
        QVERIFY(booking.begin(directPlan(), preferences())); QTRY_COMPARE(booking.state(), JourneyBooking::Paused);
        QCOMPARE(network.calls.size(), 3); QVERIFY(network.submissions.isEmpty());
    }
    void paidAmountIsRequiredForBudget()
    {
        FakeGateway network; auto r = preferences(); r.budget = splitPlan().total(); JourneyBooking booking(&network);
        QVERIFY(booking.begin(splitPlan(), r)); QTRY_COMPARE(booking.state(), JourneyBooking::AwaitingPayment);
        network.markPaid(); network.paid[0].total = -1; booking.reconcile(r.passengers);
        QTRY_COMPARE(booking.state(), JourneyBooking::Partial); QCOMPARE(network.submissions.size(), 1);
        network.paid[0].total = 14000; booking.reconcile(r.passengers);
        QTRY_COMPARE(booking.state(), JourneyBooking::Partial); QCOMPARE(network.submissions.size(), 1);
        network.paid[0].total = 10000; booking.reconcile(r.passengers);
        QTRY_COMPARE(booking.state(), JourneyBooking::AwaitingPayment); QCOMPARE(network.submissions.size(), 2);
    }
    void malformedNetworkReplyPauses()
    {
        FakeGateway network; network.error = "verification required"; JourneySearch search(&network);
        search.start(preferences(), Journey::parseStations(stationFixture())); QTRY_VERIFY(!search.running());
        QCOMPARE(network.calls.size(), 1); QVERIFY(network.submissions.isEmpty());
    }
    void cityExclusionsSurviveSearch()
    {
        FakeGateway network; JourneyWidget widget(nullptr, &network); widget.setStations(stationFixture());
        auto *from = widget.findChild<QLineEdit *>("journeyFrom"), *to = widget.findChild<QLineEdit *>("journeyTo");
        from->setText(QStringLiteral("太原")); to->setText(QStringLiteral("上海"));
        QMetaObject::invokeMethod(from, "editingFinished"); QMetaObject::invokeMethod(to, "editingFinished");
        widget.setPassengerOptions({{"private-person-ref", QStringLiteral("测试乘客")}});
        widget.findChild<QListWidget *>("journeyPassengers")->item(0)->setCheckState(Qt::Checked);
        auto *origins = widget.findChild<QListWidget *>("journeyFromStations");
        QVERIFY(origins); QCOMPARE(origins->count(), 2);
        QCOMPARE(origins->item(0)->checkState(), Qt::Checked); QCOMPARE(origins->item(1)->checkState(), Qt::Checked);
        const auto *allowed = widget.findChild<QListWidget *>("journeySeats");
        QCOMPARE(allowed->item(0)->checkState(), Qt::Checked); QCOMPARE(allowed->item(1)->checkState(), Qt::Checked);
        QCOMPARE(allowed->item(0)->data(Qt::UserRole).toString(), QString("4"));
        QCOMPARE(allowed->item(1)->data(Qt::UserRole).toString(), QString("3"));
        for (int i = 2; i < allowed->count(); ++i) QCOMPARE(allowed->item(i)->checkState(), Qt::Unchecked);
        origins->item(1)->setCheckState(Qt::Unchecked);
        auto *button = widget.findChild<QPushButton *>("journeySearchButton"); QVERIFY(button); button->click();
        QCOMPARE(widget.searchEngine()->request().from.size(), 1);
    }
    void confirmationUsesCurrentInputs_data()
    {
        QTest::addColumn<int>("change");
        QTest::newRow("unchanged") << 0;
        QTest::newRow("changed-passenger") << 1;
        QTest::newRow("lower-budget") << 2;
    }
    void confirmationUsesCurrentInputs()
    {
        QFETCH(int, change);
        FakeGateway network; network.wholeCount = 8;
        JourneyWidget widget(nullptr, &network); widget.setStations(stationFixture());
        auto *from = widget.findChild<QLineEdit *>("journeyFrom"), *to = widget.findChild<QLineEdit *>("journeyTo");
        from->setText(QStringLiteral("太原")); to->setText(QStringLiteral("上海"));
        QMetaObject::invokeMethod(from, "editingFinished"); QMetaObject::invokeMethod(to, "editingFinished");
        widget.setPassengerOptions({{"private-person-ref", QStringLiteral("原乘客")}, {"new-person-ref", QStringLiteral("新乘客")}});
        auto *people = widget.findChild<QListWidget *>("journeyPassengers"); people->item(0)->setCheckState(Qt::Checked);
        widget.findChild<QDateEdit *>("journeyDate")->setDate(preferences().date);
        widget.searchEngine()->start(preferences(), Journey::parseStations(stationFixture()));
        QTRY_VERIFY(!widget.searchEngine()->running());
        QVERIFY(!widget.searchEngine()->plans().isEmpty());
        widget.findChild<QTableWidget *>("journeyPlans")->selectRow(0);
        if (change == 1) {
            people->item(0)->setCheckState(Qt::Unchecked); people->item(1)->setCheckState(Qt::Checked);
        } else if (change == 2) widget.findChild<QDoubleSpinBox *>()->setValue(1);
        bool opened = false, confirmation = false;
        QString message;
        QTimer::singleShot(0, &widget, [&] {
            if (auto *dialog = widget.findChild<QMessageBox *>()) {
                opened = true; message = dialog->text();
                confirmation = dialog->standardButtons().testFlag(QMessageBox::Yes);
                dialog->button(confirmation ? QMessageBox::Yes : QMessageBox::Ok)->click();
            }
        });
        bool clicked = false;
        for (auto *button : widget.findChildren<QPushButton *>())
            if (button->text() == QStringLiteral("确认并下单")) { clicked = true; button->click(); break; }
        QVERIFY(clicked); QVERIFY(opened);
        if (change == 0) {
            QVERIFY(confirmation);
            QVERIFY(message.contains(QStringLiteral("原乘客")));
            QTRY_COMPARE(network.submissions.size(), 1);
            return;
        }
        QVERIFY(!confirmation);
        QTest::qWait(100);
        QVERIFY(network.submissions.isEmpty());
        QVERIFY(!network.owned);
    }
    void nativeWindowAndSubmissionIsolation_data()
    {
        QTest::addColumn<int>("mode");
        QTest::newRow("login-rejected-before-mutation") << 0;
        QTest::newRow("submitted-without-notification-or-autopay") << 1;
        QTest::newRow("stopped-before-late-reply") << 2;
        QTest::newRow("legacy-request-blocks-reservation") << 3;
        QTest::newRow("untrusted-payment-redirect-blocked") << 4;
    }
    void nativeWindowAndSubmissionIsolation()
    {
        QFETCH(int, mode);
        w = new MainWindow; w->setUp();
        auto *tabs = qobject_cast<QTabWidget *>(w->centralWidget()); QVERIFY(tabs);
        QCOMPARE(tabs->count(), 2); QCOMPARE(tabs->tabText(1), QStringLiteral("智能备选"));
        w->setStationNameCompleter(stationFixture());
        NetHelper *nh = NetHelper::instance();
        auto *original = nh->nam;
        auto *manager = new FixtureManager(nh); manager->denyLogin = mode == 0 || mode == 3;
        manager->simulateBooking = mode == 1 || mode == 2;
        if (mode == 4) manager->paymentRedirect = QUrl("https://invalid.example/");
        nh->nam = manager;
        connect(manager, &QNetworkAccessManager::finished, nh, &NetHelper::networkReplyHandle);
        nh->setJourneyAuthenticated(true);
        UserData::instance()->runStatus = EIDLE;
        const auto oldNotify = UserData::instance()->notifySetting;
        const auto oldPayment = UserData::instance()->paySetting;
        UserData::instance()->notifySetting.wxNotify.enable = true;
        UserData::instance()->notifySetting.wxNotify.sendKey = "fake-unused-notification-key";
        UserData::instance()->paySetting.activePay = true;
        PassengerInfo person; person.passName = QStringLiteral("测试乘客"); person.passIdTypeCode = "1";
        person.passIdNo = "FAKE-DOCUMENT-NUMBER"; person.passType = "1"; person.allEncStr = "fake-encoded-info";
        UserData::instance()->passenger = {person};
        const QString ref = QString::fromLatin1(QCryptographicHash::hash("1|FAKE-DOCUMENT-NUMBER", QCryptographicHash::Sha256).toHex());
        auto q = ticketQuery(); q.date = QDateTime::currentDateTimeUtc().date().addDays(1);
        const QString raw = rawTicket(q, 8, true, "RUN001", q.date.toString("yyyyMMdd"));
        auto data = ticketData(q); data["result"] = QVariantList{raw};
        auto r = preferences(); r.date = q.date;
        const auto leg = Journey::availableLegs(Journey::parseTrains(data, q), r).first();
        QString error;
        if (mode == 3) {
            nh->get(QUrl("https://kyfw.12306.cn/otn/confirmPassenger/initDc"), &NetHelper::ignoreReply);
            QVERIFY(!nh->reserveJourney(error));
            QTRY_VERIFY(nh->replyMap.isEmpty());
        }
        QVERIFY(nh->reserveJourney(error));
        QSignalSpy rejected(nh, &NetHelper::journeySubmissionRejected);
        QSignalSpy finished(nh, &NetHelper::journeySubmissionFinished);
        if (mode == 4) nh->payJourneyOrder("FAKE-ORDER", "K001,12:00;");
        else QVERIFY(nh->submitJourneyLeg(leg, raw.split('|'), {ref}, error));
        if (mode == 0 || mode == 3) QTRY_COMPARE(rejected.size(), 1);
        else if (mode == 1) QTRY_COMPARE(finished.size(), 1);
        else if (mode == 2) nh->pauseJourneySubmission();
        QTest::qWait(50);
        int submissions = 0; bool identitySent = false;
        for (const auto &call : manager->calls) {
            QCOMPARE(call.url().host(), QString("kyfw.12306.cn"));
            if (call.url().path().contains("submitOrderRequest")) ++submissions;
            if (mode != 4) QVERIFY(!call.url().path().contains("pay"));
            QVERIFY(!call.url().query().contains("FAKE-DOCUMENT-NUMBER"));
        }
        for (const auto &body : manager->bodies) {
            identitySent |= body.contains("FAKE-DOCUMENT-NUMBER");
            QVERIFY(!body.contains("fake-unused-notification-key")); QVERIFY(!body.contains("password"));
        }
        QCOMPARE(submissions, mode == 1 ? 1 : 0); QCOMPARE(identitySent, mode == 1);
        if (mode == 2) { QCOMPARE(manager->calls.size(), 1); QVERIFY(finished.isEmpty()); QVERIFY(rejected.isEmpty()); }
        if (mode == 4) QCOMPARE(manager->calls.size(), 1);
        nh->releaseJourney(); nh->setJourneyAuthenticated(false); nh->nam = original;
        UserData::instance()->notifySetting = oldNotify; UserData::instance()->paySetting = oldPayment;
        delete manager; delete w; w = nullptr;
    }
};

int runJourneyChecks(int argc, char **argv)
{
    JourneyChecks checks; return QTest::qExec(&checks, argc, argv);
}
int runJourneyPreview()
{
    QCoreApplication::setOrganizationName("FenliuJourneyPreview");
    QCoreApplication::setApplicationName("OfflinePreview");
    QTemporaryDir settings; QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    FakeGateway gateway; JourneyWidget widget(nullptr, &gateway);
    widget.setStations(stationFixture());
    auto *from = widget.findChild<QLineEdit *>("journeyFrom"), *to = widget.findChild<QLineEdit *>("journeyTo");
    from->setText(QStringLiteral("太原")); to->setText(QStringLiteral("上海"));
    QMetaObject::invokeMethod(from, "editingFinished"); QMetaObject::invokeMethod(to, "editingFinished");
    widget.setPassengerOptions({{"private-person-ref", QStringLiteral("测试乘客（成人）")}});
    widget.findChild<QListWidget *>("journeyPassengers")->item(0)->setCheckState(Qt::Checked);
    widget.findChild<QDateEdit *>("journeyDate")->setDate(preferences().date);
    widget.searchEngine()->start(preferences(), Journey::parseStations(stationFixture()));
    widget.resize(1200, 800); widget.show();
    QTimer::singleShot(300, &widget, [&widget] {
        widget.grab().save("/tmp/fenliu-journey-desktop.png");
        widget.resize(800, 640);
        QTimer::singleShot(100, &widget, [&widget] { widget.grab().save("/tmp/fenliu-journey-small.png"); qApp->quit(); });
    });
    return qApp->exec();
}

#include "journeychecks.moc"
