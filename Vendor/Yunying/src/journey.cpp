#include "journey.h"
#include "userdata.h"
#include <QTimeZone>
#include <QSet>
#include <algorithm>
#include <limits>
#include <tuple>

namespace Journey {
static QDateTime at(const QDate &date, const QTime &time)
{
    return QDateTime(date, time, QTimeZone("Asia/Shanghai"));
}

QVector<Station> parseStations(const QByteArray &source)
{
    QVector<Station> result;
    const QString text = QString::fromUtf8(source);
    const int first = text.indexOf('\''), last = text.lastIndexOf('\'');
    if (first < 0 || last <= first) return result;
    QSet<QString> seen;
    for (const QString &record : text.mid(first + 1, last - first - 1).split('@')) {
        const QStringList f = record.split('|', Qt::KeepEmptyParts);
        if (f.size() < 5 || f[1].isEmpty() || f[2].size() != 3 || seen.contains(f[2])) continue;
        seen.insert(f[2]);
        result.append({f[1], f[2], f.value(6), f.value(7)});
    }
    return result;
}

QString Query::key() const
{
    return QString::number(kind) + '|' + date.toString(Qt::ISODate) + '|' + from + '|' + to + '|' + trainNo
        + '|' + QString::number(page);
}
QString Leg::key() const
{
    return trainNo + '|' + runDate.toString(Qt::ISODate) + '|' + date.toString(Qt::ISODate)
           + '|' + from + '|' + to + '|' + seat;
}
int Plan::total() const
{
    qint64 sum = 0;
    for (const Leg &leg : legs) {
        if (leg.price < 0 && leg.paidAmount < 0) return -1;
        sum += leg.paidAmount >= 0 ? leg.paidAmount : qint64(leg.price) * people;
    }
    return sum <= std::numeric_limits<int>::max() ? int(sum) : -1;
}
QString Plan::key() const
{
    QStringList parts;
    for (const Leg &leg : legs) parts.append(leg.key());
    return parts.join(';');
}
QStringList defaultSeats() { return {QStringLiteral("4"), QStringLiteral("3")}; }
QMap<QString, QString> seatNames()
{
    return {{"4", QStringLiteral("软卧")}, {"3", QStringLiteral("硬卧")},
            {"F", QStringLiteral("动卧")}, {"6", QStringLiteral("高级软卧")},
            {"O", QStringLiteral("二等座")}, {"M", QStringLiteral("一等座")},
            {"9", QStringLiteral("商务座")}, {"2", QStringLiteral("软座")},
            {"1", QStringLiteral("硬座")}};
}

QVector<Train> parseTrains(const QVariantMap &data, const Query &query)
{
    QVector<Train> result;
    const QVariantMap names = data.value("map").toMap();
    const QMap<QString, int> columns = {{"4", ERWNUM}, {"3", EYWNUM}, {"F", ESRRBNUM},
        {"6", EGRNUM}, {"O", EZENUM}, {"M", EZYNUM}, {"9", ESWZNUM}, {"2", ERZNUM}, {"1", EYZNUM}};
    for (const QVariant &row : data.value("result").toList()) {
        const QStringList f = row.toString().split('|', Qt::KeepEmptyParts);
        if (f.size() <= EYPINFONEW || f[ETRAINNO].isEmpty()
            || f[EFROMSTATIONTELECODE] != query.from || f[ETOSTATIONTELECODE] != query.to) continue;
        Train train;
        Leg &leg = train.route;
        leg.trainNo = f[ETRAINNO]; leg.trainCode = f[ESTATIONTRAINCODE];
        leg.runDate = QDate::fromString(f[ESTARTTRAINDATE], "yyyyMMdd");
        leg.date = query.date; leg.from = query.from; leg.to = query.to;
        leg.fromName = names.value(leg.from, leg.from).toString();
        leg.toName = names.value(leg.to, leg.to).toString();
        leg.fromIndex = f[EFROMSTATIONNO].toInt(); leg.toIndex = f[ETOSTATIONNO].toInt();
        leg.departure = at(query.date, QTime::fromString(f[ESTARTTIME], "HH:mm"));
        const QStringList duration = f[ESPENDTIME].split(':');
        bool hoursOk = false, minutesOk = false;
        const int hours = duration.value(0).toInt(&hoursOk), minutes = duration.value(1).toInt(&minutesOk);
        if (!hoursOk || !minutesOk || hours < 0 || hours > 240 || minutes < 0 || minutes > 59) continue;
        leg.arrival = leg.departure.addSecs((hours * 60 + minutes) * 60);
        if (!leg.runDate.isValid() || !leg.departure.isValid() || leg.fromIndex >= leg.toIndex
            || leg.arrival.time() != QTime::fromString(f[EARRIVETIME], "HH:mm")) continue;
        leg.checkedAt = QDateTime::currentDateTimeUtc();
        leg.bookable = f[ECANWEBBUY] == "Y" && !f[ESECRETSTR].isEmpty();
        QMap<QString, int> prices;
        const QString encoded = f[EYPINFONEW];
        for (int pos = 0; pos + 10 <= encoded.size(); pos += 10) {
            bool ok = false;
            const int price = encoded.mid(pos + 1, 5).toInt(&ok);
            bool countOk = false;
            const int count = encoded.mid(pos + 6, 4).toInt(&countOk);
            if (ok && countOk && price > 0 && count >= 0 && count < 3000) {
                const QString code = encoded.mid(pos, 1);
                // Different berth levels may carry different prices; use the highest quote.
                prices[code] = qMax(prices.value(code), price * 10);
            }
        }
        for (auto it = columns.cbegin(); it != columns.cend(); ++it) {
            const QString count = f[it.value()];
            bool ok = false;
            int remaining = count.toInt(&ok);
            if (count == QStringLiteral("有")) remaining = std::numeric_limits<int>::max();
            else if (!ok || remaining < 0) remaining = 0;
            train.offers[it.key()] = {remaining, prices.value(it.key(), -1)};
        }
        result.append(train);
    }
    return result;
}

QVector<Leg> availableLegs(const QVector<Train> &trains, const Request &request)
{
    QVector<Leg> result;
    for (const Train &train : trains) for (const QString &seat : request.seats) {
        const auto offer = train.offers.value(seat, {0, -1});
        if (!train.route.bookable || offer.first < request.passengers.size()) continue;
        Leg leg = train.route;
        leg.seat = seat; leg.remaining = offer.first; leg.price = offer.second;
        result.append(leg);
    }
    return result;
}

bool validPlan(const Plan &plan, const Request &request)
{
    if (plan.legs.isEmpty() || plan.legs.size() > 2 || plan.people != request.passengers.size()
        || request.passengers.isEmpty() || QSet<QString>(request.passengers.begin(), request.passengers.end()).size() != request.passengers.size()
        || (plan.legs.size() == 2 && !request.split)) return false;
    auto contains = [](const QVector<Station> &stations, const QString &code) {
        return std::any_of(stations.begin(), stations.end(), [&](const Station &s) { return s.code == code; });
    };
    if (!contains(request.from, plan.legs.first().from) || !contains(request.to, plan.legs.last().to)
        || plan.legs.first().date != request.date) return false;
    const QTime time = plan.legs.first().departure.time();
    if (time < request.earliest || time > request.latest) return false;
    for (const Leg &leg : plan.legs) {
        if (!request.seats.contains(leg.seat) || leg.remaining < plan.people || !leg.bookable
            || !leg.runDate.isValid() || !leg.departure.isValid() || !leg.arrival.isValid()
            || leg.arrival <= leg.departure || leg.date != leg.departure.date()
            || leg.from.isEmpty() || leg.to.isEmpty() || leg.from == leg.to || leg.fromIndex >= leg.toIndex
            || leg.trainNo.isEmpty() || leg.trainCode.isEmpty()) return false;
    }
    if (plan.legs.size() == 2) {
        const Leg &a = plan.legs[0], &b = plan.legs[1];
        if (a.trainNo != b.trainNo || a.runDate != b.runDate || a.to != b.from
            || a.toIndex != b.fromIndex || a.arrival > b.departure
            || a.arrival.secsTo(b.departure) > 24 * 3600) return false;
    }
    return request.budget == 0 || (plan.total() >= 0 && plan.total() <= request.budget);
}

void sortPlans(QVector<Plan> &plans, const Request &request)
{
    auto score = [&](const Plan &p) {
        int rank = 0;
        for (const Leg &leg : p.legs) rank = qMax(rank, request.seats.indexOf(leg.seat));
        return std::make_tuple(p.legs.size(), rank, p.total() < 0 ? std::numeric_limits<int>::max() : p.total(),
            p.legs.first().departure.secsTo(p.legs.last().arrival),
            p.legs.first().departure.toMSecsSinceEpoch(), p.key());
    };
    std::sort(plans.begin(), plans.end(), [&](const Plan &a, const Plan &b) { return score(a) < score(b); });
}

QVector<QPair<Query, Query>> splitQueries(const Train &train, const QVariantMap &data,
                                        const QVector<Station> &stations)
{
    QVector<QPair<Query, Query>> result;
    const Leg &route = train.route;
    const QVariantList stops = data.value("data").toList();
    QMap<QString, QString> codes;
    for (const Station &station : stations) codes[station.name] = station.code;
    bool inside = false, finished = false;
    int previousIndex = route.fromIndex;
    QDateTime previous;
    for (const QVariant &value : stops) {
        const QVariantMap stop = value.toMap();
        const QString code = codes.value(stop.value("station_name").toString());
        const int index = stop.value("station_no").toString().toInt();
        if (code == route.from && index == route.fromIndex) {
            if (QTime::fromString(stop.value("start_time").toString(), "HH:mm") != route.departure.time()) return {};
            inside = true; previous = route.departure; continue;
        }
        if (!inside) continue;
        const QTime arrivalTime = QTime::fromString(stop.value("arrive_time").toString(), "HH:mm");
        if (!arrivalTime.isValid() || index <= previousIndex || index > route.toIndex) return {};
        previousIndex = index;
        QDateTime arrival = at(previous.date(), arrivalTime);
        if (arrival < previous) arrival = arrival.addDays(1);
        if (code == route.to && index == route.toIndex) {
            if (arrival != route.arrival) return {}; // Reject ambiguous multi-day timetables.
            finished = true; break;
        }
        const QTime departureTime = QTime::fromString(stop.value("start_time").toString(), "HH:mm");
        if (!departureTime.isValid()) return {};
        QDateTime departure = at(arrival.date(), departureTime);
        if (departure < arrival) departure = departure.addDays(1);
        if (!code.isEmpty()) {
            Query a; a.date = route.date; a.from = route.from; a.to = code;
            Query b; b.date = departure.date(); b.from = code; b.to = route.to;
            result.append({a, b});
        }
        previous = departure;
    }
    return finished ? result : QVector<QPair<Query, Query>>{};
}

bool sameQuote(const Leg &expected, const Leg &actual)
{
    return expected.key() == actual.key() && expected.departure == actual.departure
           && expected.arrival == actual.arrival && expected.price >= 0 && expected.price == actual.price;
}
bool matchesOrder(const Order &order, const Leg &leg, const QStringList &passengers)
{
    QStringList a = order.passengers, b = passengers;
    a.sort(); b.sort();
    return !order.id.isEmpty() && order.leg.trainCode == leg.trainCode && order.leg.date == leg.date
           && (order.leg.trainNo.isEmpty() || order.leg.trainNo == leg.trainNo)
           && order.leg.from == leg.from && order.leg.to == leg.to && order.leg.seat == leg.seat
           && a == b && !b.isEmpty();
}
}
