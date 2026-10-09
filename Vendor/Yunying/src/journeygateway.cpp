#include "journeygateway.h"
#include "nethelper.h"
#include "userdata.h"
#include "12306.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QRegularExpression>
#include <limits>

JourneyGateway::JourneyGateway(QObject *parent, int gap) : QObject(parent), interval(qMax(0, gap))
{
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, this, &JourneyGateway::dispatch);
}
quint64 JourneyGateway::request(const Journey::Query &query)
{
    const quint64 id = ++nextId;
    pending.enqueue({id, query});
    if (!current.first && !timer.isActive()) timer.start(0);
    return id;
}
void JourneyGateway::dispatch()
{
    if (current.first || pending.isEmpty()) return;
    const int wait = elapsed.isValid() ? qMax(0, interval - int(elapsed.elapsed())) : 0;
    if (wait) { timer.start(wait); return; }
    current = pending.dequeue();
    elapsed.restart();
    performRequest(current.first, current.second);
}
void JourneyGateway::complete(quint64 id, const QVariantMap &data, const QString &error)
{
    if (current.first != id) return;
    const Journey::Query query = current.second;
    current.first = 0;
    emit received(id, query, data, error);
    if (!current.first && !pending.isEmpty() && !timer.isActive()) timer.start(0);
}
void JourneyGateway::cancel(quint64 id)
{
    for (int i = pending.size() - 1; i >= 0; --i) if (pending[i].first == id) pending.removeAt(i);
    if (current.first == id) {
        current.first = 0;
        abortRequest(id);
    }
    if (!current.first && !pending.isEmpty() && !timer.isActive()) timer.start(0);
}

static QString passengerRef(const PassengerInfo &p)
{
    return QString::fromLatin1(QCryptographicHash::hash(
        (p.passIdTypeCode + '|' + p.passIdNo).toUtf8(), QCryptographicHash::Sha256).toHex());
}

LocalJourneyGateway::LocalJourneyGateway(QObject *parent, QNetworkAccessManager *network)
    : JourneyGateway(parent), transport(network)
{
    if (!transport) connect(NetHelper::instance(), &NetHelper::journeySubmissionFinished,
                            this, &JourneyGateway::submissionFinished);
    if (!transport) connect(NetHelper::instance(), &NetHelper::journeySubmissionRejected,
                            this, &JourneyGateway::submissionRejected);
}

QPair<QNetworkRequest, QByteArray> LocalJourneyGateway::requestSpec(const Journey::Query &query)
{
    QString path;
    QUrlQuery args;
    const bool orders = query.kind == Journey::Query::PendingOrders || query.kind == Journey::Query::PaidOrders;
    if (query.kind == Journey::Query::Tickets) {
        path = QStringLiteral(QUERYTICKETBASEURL);
        args.addQueryItem("leftTicketDTO.train_date", query.date.toString(Qt::ISODate));
        args.addQueryItem("leftTicketDTO.from_station", query.from);
        args.addQueryItem("leftTicketDTO.to_station", query.to);
        args.addQueryItem("purpose_codes", "ADULT");
    } else if (query.kind == Journey::Query::Stops) {
        path = QStringLiteral(QEURYTRAINSTOPSTATION);
        args.addQueryItem("train_no", query.trainNo);
        args.addQueryItem("from_station_telecode", query.from);
        args.addQueryItem("to_station_telecode", query.to);
        args.addQueryItem("depart_date", query.date.toString(Qt::ISODate));
    } else if (query.kind == Journey::Query::PendingOrders) {
        path = QStringLiteral(QUERYNOCOMPLETEORDER);
        args.addQueryItem("_json_att", "");
    } else {
        path = QStringLiteral(BASEURL PUBLICNAME "/queryOrder/queryMyOrder");
        args.addQueryItem("queryType", "2");
        args.addQueryItem("query_where", "G");
        args.addQueryItem("come_from_flag", "my_order");
        args.addQueryItem("queryStartDate", query.date.toString(Qt::ISODate));
        args.addQueryItem("queryEndDate", (query.endDate.isValid() ? query.endDate : query.date).toString(Qt::ISODate));
        args.addQueryItem("pageIndex", QString::number(query.page));
        args.addQueryItem("pageSize", "8");
        args.addQueryItem("sequeue_train_name", "");
    }
    QUrl url(path);
    if (!orders) url.setQuery(args);
    QNetworkRequest request(url);
    request.setTransferTimeout(10000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("User-Agent", USERAGENT);
    request.setRawHeader("Origin", "https://kyfw.12306.cn");
    request.setRawHeader("Referer", "https://kyfw.12306.cn/otn/view/index.html");
    request.setRawHeader("Cache-Control", "no-cache");
    if (orders) request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded; charset=UTF-8");
    return {request, orders ? args.query(QUrl::FullyEncoded).toUtf8() : QByteArray()};
}

void LocalJourneyGateway::performRequest(quint64 id, const Journey::Query &query)
{
    const auto spec = requestSpec(query);
    QNetworkReply *reply = transport ? (spec.second.isEmpty() ? transport->get(spec.first) : transport->post(spec.first, spec.second))
                                   : NetHelper::instance()->journeyRead(spec.first, spec.second);
    replies[id] = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, id, query] {
        replies.remove(id);
        QJsonParseError parseError;
        const QJsonDocument json = QJsonDocument::fromJson(reply->readAll(), &parseError);
        QVariantMap data;
        QString error;
        if (reply->error() != QNetworkReply::NoError
            || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200)
            error = QStringLiteral("请求失败或被限制，已暂停；请检查登录、网络与官方验证要求");
        else if (parseError.error != QJsonParseError::NoError || !json.isObject()
                 || !json.object().value("status").toBool())
            error = QStringLiteral("官方未返回有效结果，已暂停；请检查登录或验证要求");
        else {
            data = json.object().value("data").toObject().toVariantMap();
            if (query.kind == Journey::Query::Tickets) {
                if (data.value("result").metaType().id() != QMetaType::QVariantList) error = QStringLiteral("余票数据不完整，已暂停");
                else ticketData[query.key()] = data;
            } else if (query.kind == Journey::Query::Stops && data.value("data").metaType().id() != QMetaType::QVariantList)
                error = QStringLiteral("经停数据不完整，已暂停");
        }
        reply->deleteLater();
        complete(id, data, error);
    });
}
void LocalJourneyGateway::abortRequest(quint64 id)
{
    const auto reply = replies.take(id);
    if (reply) reply->abort();
}
bool LocalJourneyGateway::reserve(QString &error) { return NetHelper::instance()->reserveJourney(error); }
void LocalJourneyGateway::release() { NetHelper::instance()->releaseJourney(); ticketData.clear(); }

void LocalJourneyGateway::submit(const Journey::Leg &leg, const QStringList &passengers)
{
    Journey::Query query; query.date = leg.date; query.from = leg.from; query.to = leg.to;
    const QVariantMap data = ticketData.value(query.key());
    QStringList raw;
    for (const QVariant &row : data.value("result").toList()) {
        const QStringList f = row.toString().split('|', Qt::KeepEmptyParts);
        if (f.size() > EYPINFONEW && f[ETRAINNO] == leg.trainNo
            && QDate::fromString(f[ESTARTTRAINDATE], "yyyyMMdd") == leg.runDate) { raw = f; break; }
    }
    QString error;
    if (!NetHelper::instance()->submitJourneyLeg(leg, raw, passengers, error)) emit submissionRejected(error);
}

QVector<QPair<QString, QString>> LocalJourneyGateway::passengerOptions()
{
    QVector<QPair<QString, QString>> result;
    for (const PassengerInfo &p : UserData::instance()->passenger)
        result.append({passengerRef(p), p.passName + '(' + p.passTypeName + ')'});
    return result;
}

QVector<Journey::Order> LocalJourneyGateway::orders(const QVariantMap &data, bool pending) const
{
    return decodeOrders(data, pending);
}
QVector<Journey::Order> LocalJourneyGateway::decodeOrders(const QVariantMap &data, bool pending)
{
    QVector<Journey::Order> result;
    const QVariantList list = data.value(pending ? "orderDBList" : "OrderDTODataList").toList();
    for (const QVariant &value : list) {
        const QVariantMap order = value.toMap();
        Journey::Order parsed;
        parsed.id = order.value("sequence_no").toString();
        const QVariantList tickets = order.value("tickets").toList();
        bool consistent = !tickets.isEmpty(), allPaid = true, allCancelled = true;
        qint64 total = 0;
        bool knownPrice = !tickets.isEmpty();
        QString identity;
        for (const QVariant &v : tickets) {
            const QVariantMap t = v.toMap(), train = t.value("stationTrainDTO").toMap();
            Journey::Leg leg;
            leg.trainCode = train.value("station_train_code").toString();
            leg.trainNo = train.value("train_no").toString();
            leg.from = train.value("from_station_telecode").toString();
            leg.to = train.value("to_station_telecode").toString();
            leg.date = QDate::fromString(t.value("start_train_date_page").toString().left(10), Qt::ISODate);
            if (!leg.date.isValid()) leg.date = QDate::fromString(t.value("start_train_date").toString().left(10), Qt::ISODate);
            leg.seat = t.value("seat_type_code").toString();
            const QString key = leg.trainCode + leg.from + leg.to + leg.date.toString(Qt::ISODate) + leg.seat;
            if (!identity.isEmpty() && identity != key) consistent = false;
            identity = key; parsed.leg = leg;
            const QVariantMap person = t.value("passengerDTO").toMap();
            PassengerInfo p;
            p.passIdTypeCode = person.value("passenger_id_type_code").toString();
            p.passIdNo = person.value("passenger_id_no").toString();
            if (p.passIdNo.isEmpty() || p.passIdTypeCode.isEmpty()) consistent = false;
            parsed.passengers.append(passengerRef(p));
            const QString status = t.value("ticket_status_name").toString();
            const QString price = t.value("str_ticket_price_page").toString().trimmed();
            if (QRegularExpression(QStringLiteral("^[0-9]{1,7}(\\.[0-9]{1,2})?$")).match(price).hasMatch()) {
                const auto parts = price.split('.');
                total += parts[0].toLongLong() * 100 + parts.value(1).leftJustified(2, '0').toInt();
            } else knownPrice = false;
            allPaid &= status == QStringLiteral("已支付") || status == QStringLiteral("已出票");
            allCancelled &= status == QStringLiteral("已取消") || status.startsWith(QStringLiteral("已退票"));
        }
        if (consistent) parsed.status = pending ? Journey::Order::Pending
            : allPaid ? Journey::Order::Paid : allCancelled ? Journey::Order::Cancelled : Journey::Order::Unknown;
        if (knownPrice && total <= std::numeric_limits<int>::max()) parsed.total = int(total);
        result.append(parsed);
    }
    return result;
}
void LocalJourneyGateway::pay(const QString &orderId, const Journey::Leg &leg)
{
    NetHelper::instance()->payJourneyOrder(orderId, leg.trainCode + ',' + leg.arrival.time().toString("HH:mm") + ';');
}
void LocalJourneyGateway::pauseSubmission() { NetHelper::instance()->pauseJourneySubmission(); }
