#include "journeysearch.h"
#include <algorithm>

JourneySearch::JourneySearch(JourneyGateway *network, QObject *parent) : QObject(parent), gateway(network)
{
    connect(gateway, &JourneyGateway::received, this, &JourneySearch::receive);
}
void JourneySearch::start(const Journey::Request &request, const QVector<Journey::Station> &directory)
{
    stop();
    preferences = request; stations = directory;
    results.clear(); seen.clear(); resultKeys.clear(); tickets.clear(); stopTrains.clear(); splits.clear();
    checked = 0; active = true;
    if (request.passengers.isEmpty() || request.seats.isEmpty() || !request.date.isValid()
        || request.earliest > request.latest || request.from.isEmpty() || request.to.isEmpty()) {
        active = false;
        emit progress(0, 0, QStringLiteral("请补全站点、日期、乘客和允许席别"));
        emit changed(); return;
    }
    for (const Journey::Station &a : request.from) for (const Journey::Station &b : request.to) {
        if (a.code == b.code) continue;
        Journey::Query q; q.date = request.date; q.from = a.code; q.to = b.code;
        enqueue(q);
    }
    emit changed(); next();
}
void JourneySearch::stop()
{
    active = false;
    const quint64 id = inFlight;
    inFlight = 0; queue.clear();
    if (id) gateway->cancel(id);
    emit progress(checked, 0, QStringLiteral("已停止；未检查组合不代表无票"));
}
void JourneySearch::enqueue(const Journey::Query &query)
{
    if (seen.contains(query.key())) return;
    seen.insert(query.key()); queue.enqueue(query);
}
void JourneySearch::next()
{
    if (!active || inFlight) return;
    if (queue.isEmpty()) {
        active = false;
        emit progress(checked, 0, results.isEmpty() ? QStringLiteral("本轮已检查组合暂无符合偏好的票")
                                                : QStringLiteral("本轮搜索完成；下单前会重新核实"));
        return;
    }
    inFlight = gateway->request(queue.dequeue());
    emit progress(checked, queue.size() + 1, QStringLiteral("正在检查备选组合"));
}
void JourneySearch::add(const Journey::Plan &plan)
{
    if (!Journey::validPlan(plan, preferences) || resultKeys.contains(plan.key())) return;
    resultKeys.insert(plan.key()); results.append(plan);
}
void JourneySearch::receive(quint64 id, const Journey::Query &query, const QVariantMap &data, const QString &error)
{
    if (!active || id != inFlight) return;
    inFlight = 0;
    if (!error.isEmpty()) {
        active = false; queue.clear();
        emit progress(checked, 0, error); return;
    }
    ++checked;
    if (query.kind == Journey::Query::Tickets) {
        const auto trains = Journey::parseTrains(data, query);
        tickets[query.key()] = trains;
        const bool full = query.date == preferences.date
            && std::any_of(preferences.from.begin(), preferences.from.end(), [&](const Journey::Station &s) { return s.code == query.from; })
            && std::any_of(preferences.to.begin(), preferences.to.end(), [&](const Journey::Station &s) { return s.code == query.to; });
        if (full) {
            for (const Journey::Leg &leg : Journey::availableLegs(trains, preferences))
                add({{leg}, int(preferences.passengers.size())});
            if (preferences.split) for (const Journey::Train &train : trains) {
                Journey::Query stops = query;
                stops.kind = Journey::Query::Stops; stops.trainNo = train.route.trainNo;
                stopTrains[stops.key()] = train;
                enqueue(stops);
            }
        }
        combine();
    } else if (query.kind == Journey::Query::Stops) {
        const Journey::Train train = stopTrains.value(query.key());
        for (const auto &pair : Journey::splitQueries(train, data, stations)) {
            splits.append({train, pair.first, pair.second});
            enqueue(pair.first); enqueue(pair.second);
        }
        combine();
    }
    Journey::sortPlans(results, preferences);
    emit changed(); next();
}
void JourneySearch::combine()
{
    for (const Split &split : splits) {
        if (!tickets.contains(split.a.key()) || !tickets.contains(split.b.key())) continue;
        const auto first = Journey::availableLegs(tickets.value(split.a.key()), preferences);
        const auto second = Journey::availableLegs(tickets.value(split.b.key()), preferences);
        for (const Journey::Leg &a : first) for (const Journey::Leg &b : second) {
            if (a.trainNo != split.train.route.trainNo || a.runDate != split.train.route.runDate
                || b.trainNo != a.trainNo || b.runDate != a.runDate
                || a.departure != split.train.route.departure || b.arrival != split.train.route.arrival) continue;
            add({{a, b}, int(preferences.passengers.size())});
        }
    }
}
