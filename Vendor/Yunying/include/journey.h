#ifndef JOURNEY_H
#define JOURNEY_H

#include <QDateTime>
#include <QMap>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

namespace Journey {
struct Station {
    QString name, code, cityCode, city;
};
QVector<Station> parseStations(const QByteArray &source);

struct Request {
    QVector<Station> from, to;
    QDate date;
    QStringList passengers;
    QStringList seats;
    int budget = 0; // Total fen for all selected passengers; zero means unset.
    QTime earliest = QTime(0, 0), latest = QTime(23, 59);
    bool split = true;
};

struct Query {
    enum Kind { Tickets, Stops, PendingOrders, PaidOrders } kind = Tickets;
    QDate date;
    QString from, to, trainNo;
    QDate endDate;
    int page = 0;
    QString key() const;
};

struct Leg {
    QString trainNo, trainCode, from, to, fromName, toName, seat;
    QDate runDate, date;
    QDateTime departure, arrival, checkedAt;
    int fromIndex = 0, toIndex = 0;
    int remaining = 0, price = -1;
    int paidAmount = -1;
    bool bookable = false;
    QString key() const;
};

struct Train {
    Leg route;
    QMap<QString, QPair<int, int>> offers; // Seat code -> remaining, price in fen.
};
struct Plan {
    QVector<Leg> legs;
    int people = 1;
    int total() const;
    QString key() const;
};

QStringList defaultSeats();
QMap<QString, QString> seatNames();
QVector<Train> parseTrains(const QVariantMap &data, const Query &query);
QVector<Leg> availableLegs(const QVector<Train> &trains, const Request &request);
bool validPlan(const Plan &plan, const Request &request);
void sortPlans(QVector<Plan> &plans, const Request &request);
QVector<QPair<Query, Query>> splitQueries(const Train &train, const QVariantMap &data,
                                        const QVector<Station> &stations);
bool sameQuote(const Leg &expected, const Leg &actual);

struct Order {
    enum Status { Unknown, Pending, Paid, Cancelled } status = Unknown;
    QString id;
    Leg leg;
    QStringList passengers;
    int total = -1;
};
bool matchesOrder(const Order &order, const Leg &leg, const QStringList &passengers);
}
#endif
