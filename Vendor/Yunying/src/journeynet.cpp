#include "nethelper.h"
#include "userdata.h"
#include "12306.h"
#include <QCryptographicHash>
#include <QSet>
#include <QRegularExpression>

QNetworkReply *NetHelper::journeyRead(const QNetworkRequest &request, const QByteArray &body)
{
    QNetworkRequest actual = request;
    const QUrl configured(queryLeftTicketUrl);
    if (body.isEmpty() && request.url().path().startsWith("/otn/leftTicket/query")
        && configured.scheme() == "https" && configured.host() == "kyfw.12306.cn"
        && configured.userInfo().isEmpty() && (configured.port(-1) == -1 || configured.port() == 443)
        && QRegularExpression("^/otn/leftTicket/query[A-Za-z]?$").match(configured.path()).hasMatch()) {
        QUrl url = configured; url.setQuery(request.url().query()); actual.setUrl(url);
    }
    return body.isEmpty() ? nam->get(actual) : nam->post(actual, body);
}
bool NetHelper::reserveJourney(QString &error)
{
    if (journeyOwned || !replyMap.isEmpty() || UserData::instance()->runStatus != EIDLE
        || (queryOrderTimer && queryOrderTimer->isActive()) || (queryCandidateTimer && queryCandidateTimer->isActive())) {
        error = QStringLiteral("已有请求或购票流程正在运行，请等待处理结束再继续"); return false;
    }
    if (!journeyAuthenticated) { error = QStringLiteral("请先登录 12306 并加载乘客"); return false; }
    journeyOwned = true; return true;
}
void NetHelper::pauseJourneySubmission()
{
    journeySubmitting = false;
    if (queryOrderTimer) { queryOrderTimer->stop(); queryOrderTimer->deleteLater(); queryOrderTimer = nullptr; }
}
void NetHelper::releaseJourney()
{
    pauseJourneySubmission(); journeyOwned = false; journeyPayment = false;
    UserData::instance()->runStatus = EIDLE;
    UserData::instance()->submitTicketInfo = {};
}
bool NetHelper::submitJourneyLeg(const Journey::Leg &leg, const QStringList &row,
                                const QStringList &references, QString &error)
{
    if (!journeyOwned || journeySubmitting || !journeyAuthenticated || references.isEmpty()
        || leg.departure <= QDateTime::currentDateTimeUtc()
        || row.size() <= EBEDLEVELINFO || row[ESECRETSTR].isEmpty() || row[ECANWEBBUY] != "Y"
        || row[ETRAINNO] != leg.trainNo || row[EFROMSTATIONTELECODE] != leg.from
        || row[ETOSTATIONTELECODE] != leg.to
        || QDate::fromString(row[ESTARTTRAINDATE], "yyyyMMdd") != leg.runDate) {
        error = QStringLiteral("登录、余票或下单上下文已变化，请重新核对"); return false;
    }
    const QMap<QString, int> columns = {{"4", ERWNUM}, {"3", EYWNUM}, {"F", ESRRBNUM}, {"6", EGRNUM},
        {"O", EZENUM}, {"M", EZYNUM}, {"9", ESWZNUM}, {"2", ERZNUM}, {"1", EYZNUM}};
    if (!columns.contains(leg.seat)) { error = QStringLiteral("暂不支持此席别"); return false; }
    const QString remaining = row[columns.value(leg.seat)];
    if (remaining != QStringLiteral("有") && remaining.toInt() < references.size()) {
        error = QStringLiteral("余票不足全部乘客人数"); return false;
    }
    UserData *ud = UserData::instance();
    SubmitTicketInfo info{};
    QStringList ticketStrings, oldStrings;
    QSet<QString> seen;
    for (const QString &reference : references) {
        const PassengerInfo *passenger = nullptr;
        for (const auto &p : ud->passenger) {
            const QString ref = QString::fromLatin1(QCryptographicHash::hash(
                (p.passIdTypeCode + '|' + p.passIdNo).toUtf8(), QCryptographicHash::Sha256).toHex());
            if (ref == reference) { passenger = &p; break; }
        }
        if (!passenger || seen.contains(reference)) { error = QStringLiteral("乘客已变化，请重新加载并确认"); return false; }
        seen.insert(reference);
        const PassengerInfo &p = *passenger;
        const QString type = p.passType == "3" ? QStringLiteral(PASSENGERADULT) : p.passType;
        ticketStrings.append(QStringLiteral("%1,0,%2,%3,%4,%5,%6,N,%7")
            .arg(leg.seat, type, QString::fromLatin1(p.passName.toUtf8().toPercentEncoding()),
                 p.passIdTypeCode, p.passIdNo, p.mobile, p.allEncStr));
        oldStrings.append(QStringLiteral("%1,%2,%3,%4_")
            .arg(QString::fromLatin1(p.passName.toUtf8().toPercentEncoding()), p.passIdTypeCode, p.passIdNo, type));
        info.submitSeatType.append({p.passName, leg.seat[0]});
    }
    info.trainCode = leg.trainCode; info.trainNo = leg.trainNo; info.date = leg.date.toString(Qt::ISODate);
    info.fromStationCode = leg.from; info.toStationCode = leg.to;
    info.fromStationName = leg.fromName; info.toStationName = leg.toName;
    info.fromTime = leg.departure.time().toString("HH:mm"); info.toTime = leg.arrival.time().toString("HH:mm");
    info.secretStr = row[ESECRETSTR]; info.ypDetailInfo = row[EYPINFO];
    info.bedLevelInfo = row[EBEDLEVELINFO]; info.seatDiscountInfo = row.value(ESEATDISCOUNTINFO);
    info.passengerTicketInfo = ticketStrings.join('_').replace(',', "%2C");
    info.oldPassengerTicketInfo = oldStrings.join(QString()).replace(',', "%2C");
    ud->submitTicketInfo = info; ud->runStatus = ESUBMITORDER;
    waitOrderInfo.clear(); ++journeyGeneration; journeyMutationSent = false; journeySubmitting = true;
    checkUser(); return true;
}
void NetHelper::payJourneyOrder(const QString &id, const QString &arriveTime)
{
    if (!journeyOwned || journeySubmitting || journeyPayment || id.isEmpty()) return;
    journeyPayment = true;
    payNoCompleteOrder(id, arriveTime);
}
