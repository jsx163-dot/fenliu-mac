#include "journeybooking.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

JourneyBooking::JourneyBooking(JourneyGateway *network, QObject *parent, int wait)
    : QObject(parent), gateway(network), timeout(wait)
{
    connect(gateway, &JourneyGateway::received, this, &JourneyBooking::receive);
    connect(gateway, &JourneyGateway::submissionFinished, this, [this] {
        deadline.stop();
        if (currentState == Submitting) requestOrders();
    });
    connect(gateway, &JourneyGateway::submissionRejected, this, [this](const QString &error) {
        if (currentState != Submitting) return;
        deadline.stop(); attemptedIndex = index - 1;
        setState(index ? Partial : Paused, error);
    });
    deadline.setSingleShot(true);
    connect(&deadline, &QTimer::timeout, this, [this] {
        gateway->pauseSubmission();
        setState(index ? Partial : Paused, QStringLiteral("下单结果未明确，请核实官方订单；不会自动重试"));
    });
    restore();
}
void JourneyBooking::setState(State state, const QString &message)
{
    currentState = state; status = message;
    save(); emit changed();
}
bool JourneyBooking::begin(const Journey::Plan &plan, const Journey::Request &request)
{
    if (hasTask() || !Journey::validPlan(plan, request) || plan.total() < 0) return false;
    QString error;
    if (!gateway->reserve(error)) { status = error; emit changed(); return false; }
    reserved = true; restored = false; cancelled = false;
    selected = plan; preferences = request; replacement = {}; index = 0; attemptedIndex = -1;
    ids.clear(); for (int i = 0; i < plan.legs.size(); ++i) ids.append(QString());
    requestOrders(); return true;
}
void JourneyBooking::requestOrders()
{
    if (inFlight) return;
    stage = Pending; pendingOrders.clear(); paidOrders.clear(); paidRows = 0;
    setState(Reconciling, QStringLiteral("正在核对官方未完成订单及已购车票"));
    Journey::Query q; q.kind = Journey::Query::PendingOrders; q.date = selected.legs.first().date;
    inFlight = gateway->request(q);
}
void JourneyBooking::reconcile(const QStringList &passengers)
{
    if (!hasTask() || currentState == Complete || currentState == Submitting || inFlight) return;
    if (passengers.size() != selected.people || passengers.isEmpty()) {
        status = QStringLiteral("请先选择本任务的全部乘客，再核对订单"); emit changed(); return;
    }
    QStringList currentPeople = passengers, confirmedPeople = preferences.passengers;
    currentPeople.sort(); confirmedPeople.sort();
    if (!restored && currentPeople != confirmedPeople) {
        status = QStringLiteral("乘客与已确认方案不一致，不能继续"); emit changed(); return;
    }
    QString error;
    if (!reserved && !gateway->reserve(error)) { status = error; emit changed(); return; }
    reserved = true; preferences.passengers = passengers;
    requestOrders();
}
void JourneyBooking::pause()
{
    if (!hasTask() || currentState == Complete) return;
    const quint64 id = inFlight; inFlight = 0;
    if (id) gateway->cancel(id);
    deadline.stop(); gateway->pauseSubmission();
    setState(index ? Partial : Paused, QStringLiteral("已暂停；已有订单不会取消，请核对后继续"));
}
bool JourneyBooking::reset()
{
    if (inFlight || currentState == Submitting || (attemptedIndex >= index && currentState != Complete && !cancelled)) return false;
    if (reserved) gateway->release();
    reserved = false; restored = false;
    selected = {}; ids.clear(); preferences = {}; replacement = {}; index = 0; attemptedIndex = -1;
    setState(Idle, QStringLiteral("准备就绪")); return true;
}
void JourneyBooking::pay()
{
    if (currentState == AwaitingPayment && !orderId().isEmpty()) gateway->pay(orderId(), selected.legs[index]);
}
void JourneyBooking::verifyQuote()
{
    replacement = {};
    stage = Quote;
    setState(Rechecking, QStringLiteral("正在重新核实余票、车次和报价"));
    const Journey::Leg &leg = selected.legs[quoteIndex];
    Journey::Query q; q.date = leg.date; q.from = leg.from; q.to = leg.to;
    inFlight = gateway->request(q);
}
void JourneyBooking::receive(quint64 id, const Journey::Query &query, const QVariantMap &data, const QString &error)
{
    if (id != inFlight || !hasTask()) return;
    inFlight = 0;
    if (!error.isEmpty()) { setState(index ? Partial : Paused, error); return; }
    if (stage == Pending) {
        if (data.value("orderDBList").metaType().id() != QMetaType::QVariantList) {
            setState(index ? Partial : Paused, QStringLiteral("未完成订单数据不完整，不能继续下单")); return;
        }
        pendingOrders = gateway->orders(data, true);
        stage = Paid;
        Journey::Query q = query; q.kind = Journey::Query::PaidOrders; q.endDate = selected.legs.last().date;
        inFlight = gateway->request(q);
    } else if (stage == Paid) {
        if (data.value("OrderDTODataList").metaType().id() != QMetaType::QVariantList) {
            setState(index ? Partial : Paused, QStringLiteral("已购车票数据不完整，不能确认付款状态")); return;
        }
        paidOrders += gateway->orders(data, false);
        const int rows = data.value("OrderDTODataList").toList().size(); paidRows += rows;
        const int total = data.value("order_total_number", -1).toInt();
        if ((total > paidRows || (total < 0 && rows >= 8)) && query.page < 99) {
            Journey::Query next = query; ++next.page; inFlight = gateway->request(next); return;
        }
        if (total > paidRows || (query.page == 99 && rows >= 8)) {
            setState(index ? Partial : Paused, QStringLiteral("订单列表未完整核实，不能继续下单")); return;
        }
        consumeOrders(paidOrders);
    } else {
        const auto available = Journey::availableLegs(Journey::parseTrains(data, query), preferences);
        bool found = false;
        for (const Journey::Leg &leg : available) if (Journey::sameQuote(selected.legs[quoteIndex], leg)) {
            selected.legs[quoteIndex] = leg; found = true; break;
        }
        if (!found) {
            const auto &expected = selected.legs[quoteIndex];
            for (const auto &leg : available) {
                if (leg.trainNo != expected.trainNo || leg.runDate != expected.runDate
                    || leg.date != expected.date || leg.from != expected.from || leg.to != expected.to
                    || leg.departure != expected.departure || leg.arrival != expected.arrival || leg.price < 0) continue;
                auto proposal = selected; proposal.legs[quoteIndex] = leg;
                if (!Journey::validPlan(proposal, preferences)) continue;
                if (!replacement.bookable || leg.seat == expected.seat) replacement = leg;
                if (leg.seat == expected.seat) break;
            }
            setState(index ? Partial : Paused, QStringLiteral("余票、日期或报价已变化；请重新选择并确认，不自动接受替代票")); return;
        }
        if (quoteIndex + 1 < selected.legs.size()) { ++quoteIndex; verifyQuote(); return; }
        if (!Journey::validPlan(selected, preferences)) {
            setState(index ? Partial : Paused, QStringLiteral("方案不再符合已确认的偏好")); return;
        }
        attemptedIndex = index;
        setState(Submitting, QStringLiteral("正在提交第 %1/%2 段，结果不明确时只核对订单").arg(index + 1).arg(selected.legs.size()));
        deadline.start(timeout);
        gateway->submit(selected.legs[index], preferences.passengers);
    }
}
void JourneyBooking::consumeOrders(const QVector<Journey::Order> &paid)
{
    for (int previous = 0; previous < index; ++previous) {
        int confirmed = 0;
        for (const auto &order : paid) if (order.status == Journey::Order::Paid && order.id == ids.value(previous)
            && Journey::matchesOrder(order, selected.legs[previous], preferences.passengers)) {
            ++confirmed;
            selected.legs[previous].remaining = selected.people;
            selected.legs[previous].paidAmount = order.total;
        }
        if (confirmed != 1) {
            setState(Partial, QStringLiteral("未能确认前段仍是原乘客的已支付车票，不能继续余程下单")); return;
        }
        if (preferences.budget > 0 && selected.legs[previous].paidAmount < 0) {
            setState(Partial, QStringLiteral("无法核实前段实付金额，不能在总预算内自动推进余程")); return;
        }
    }
    if (index > 0) restored = false;
    const Journey::Leg &leg = selected.legs[index];
    if (attemptedIndex < index) for (const auto &order : paid) {
        if (order.leg.trainCode == leg.trainCode && order.leg.date == leg.date
            && order.leg.from == leg.from && order.leg.to == leg.to && order.leg.seat == leg.seat
            && !Journey::matchesOrder(order, leg, preferences.passengers)
            && order.status == Journey::Order::Unknown) {
            setState(index ? Partial : Paused, QStringLiteral("同类订单的乘客或状态无法核验，请先核实，避免重复购买")); return;
        }
    }
    QVector<Journey::Order> matches;
    auto collect = [&](const QVector<Journey::Order> &orders) {
        for (const auto &order : orders) if (Journey::matchesOrder(order, leg, preferences.passengers)
            && (ids.value(index).isEmpty() || ids[index] == order.id)) matches.append(order);
    };
    collect(pendingOrders); collect(paid);
    if (matches.size() > 1) {
        setState(index ? Partial : Paused, QStringLiteral("存在多个相似订单，无法确定本次结果；请在官方订单中核实")); return;
    }
    if (matches.size() == 1) {
        const auto order = matches.first(); ids[index] = order.id;
        if (attemptedIndex < index) {
            attemptedIndex = index;
            setState(index ? Partial : Paused, QStringLiteral("发现已有相同行程订单；请核对后再继续，避免重复购票")); return;
        }
        if (order.status == Journey::Order::Pending) {
            setState(AwaitingPayment, QStringLiteral("第 %1 段待付款；不会提交下一段。付款后请核实订单并继续").arg(index + 1)); return;
        }
        if (order.status == Journey::Order::Cancelled) {
            cancelled = true;
            setState(index ? Partial : Paused, QStringLiteral("本段订单已取消或退票；任务暂停，不自动重新购买")); return;
        }
        if (order.status != Journey::Order::Paid) {
            setState(index ? Partial : Paused, QStringLiteral("官方未明确确认付款成功，不能继续下一段")); return;
        }
        selected.legs[index].paidAmount = order.total;
        restored = false;
        if (++index == selected.legs.size()) {
            if (reserved) gateway->release(); reserved = false;
            setState(Complete, QStringLiteral("全程各段均已由官方确认付款成功")); return;
        }
        requestOrders(); return;
    }
    if (attemptedIndex >= index || restored) {
        setState(index ? Partial : Paused, QStringLiteral("尚未查到明确订单结果；不把订单消失当成付款或取消，也不自动重试")); return;
    }
    if (!pendingOrders.isEmpty()) {
        setState(index ? Partial : Paused, QStringLiteral("账号存在其他未完成订单，请先处理；不会代付或取消其他订单")); return;
    }
    quoteIndex = index; verifyQuote();
}

Journey::Plan JourneyBooking::proposedPlan() const
{
    auto result = selected;
    if (replacement.bookable) result.legs[quoteIndex] = replacement;
    return result;
}
void JourneyBooking::acceptNewQuote()
{
    if (!replacement.bookable || inFlight || attemptedIndex >= index
        || (currentState != Paused && currentState != Partial)) return;
    const auto proposed = proposedPlan();
    if (!Journey::validPlan(proposed, preferences) || proposed.total() < 0) return;
    selected = proposed; replacement = {}; requestOrders();
}

void JourneyBooking::save()
{
    QJsonArray legs;
    for (const auto &l : selected.legs) legs.append(QJsonObject{
        {"trainNo", l.trainNo}, {"trainCode", l.trainCode}, {"from", l.from}, {"to", l.to},
        {"fromName", l.fromName}, {"toName", l.toName}, {"seat", l.seat},
        {"runDate", l.runDate.toString(Qt::ISODate)}, {"date", l.date.toString(Qt::ISODate)},
        {"departure", l.departure.toString(Qt::ISODate)}, {"arrival", l.arrival.toString(Qt::ISODate)},
        {"fromIndex", l.fromIndex}, {"toIndex", l.toIndex}, {"price", l.price}, {"paidAmount", l.paidAmount}});
    const QJsonObject state{{"version", 1}, {"legs", legs}, {"people", selected.people}, {"index", index},
        {"attempted", attemptedIndex}, {"orders", QJsonArray::fromStringList(ids)}, {"budget", preferences.budget},
        {"seats", QJsonArray::fromStringList(preferences.seats)}, {"complete", currentState == Complete}};
    QSettings().setValue("journey/progress", QJsonDocument(state).toJson(QJsonDocument::Compact));
}
void JourneyBooking::restore()
{
    const auto obj = QJsonDocument::fromJson(QSettings().value("journey/progress").toByteArray()).object();
    if (obj.value("version").toInt() != 1) return;
    for (const QJsonValue &v : obj.value("legs").toArray()) {
        const auto o = v.toObject(); Journey::Leg l;
        l.trainNo = o.value("trainNo").toString(); l.trainCode = o.value("trainCode").toString();
        l.from = o.value("from").toString(); l.to = o.value("to").toString();
        l.fromName = o.value("fromName").toString(); l.toName = o.value("toName").toString(); l.seat = o.value("seat").toString();
        l.date = QDate::fromString(o.value("date").toString(), Qt::ISODate);
        l.runDate = QDate::fromString(o.value("runDate").toString(), Qt::ISODate);
        l.departure = QDateTime::fromString(o.value("departure").toString(), Qt::ISODate);
        l.arrival = QDateTime::fromString(o.value("arrival").toString(), Qt::ISODate);
        l.fromIndex = o.value("fromIndex").toInt(); l.toIndex = o.value("toIndex").toInt(); l.price = o.value("price").toInt(-1);
        l.paidAmount = o.value("paidAmount").toInt(-1);
        l.bookable = true; selected.legs.append(l);
    }
    if (selected.legs.isEmpty() || selected.legs.size() > 2) { selected = {}; return; }
    selected.people = obj.value("people").toInt(); index = obj.value("index").toInt();
    attemptedIndex = obj.value("attempted").toInt(-1);
    for (const auto &id : obj.value("orders").toArray()) ids.append(id.toString());
    if (selected.people < 1 || ids.size() != selected.legs.size() || index < 0 || index > selected.legs.size()) { selected = {}; return; }
    preferences.date = selected.legs.first().date; preferences.budget = obj.value("budget").toInt();
    for (const auto &seat : obj.value("seats").toArray()) preferences.seats.append(seat.toString());
    preferences.from = {{selected.legs.first().fromName, selected.legs.first().from, {}, {}}};
    preferences.to = {{selected.legs.last().toName, selected.legs.last().to, {}, {}}};
    restored = true;
    currentState = obj.value("complete").toBool() && index == selected.legs.size() ? Complete : Paused;
    if (currentState != Complete && index == selected.legs.size()) { selected = {}; currentState = Idle; return; }
    status = currentState == Complete ? QStringLiteral("上次行程已完成")
        : QStringLiteral("发现本地未结束任务；请选择原乘客并核对官方订单，不会自动恢复下单");
}
