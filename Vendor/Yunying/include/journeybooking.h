#ifndef JOURNEYBOOKING_H
#define JOURNEYBOOKING_H
#include "journeygateway.h"

class JourneyBooking : public QObject {
    Q_OBJECT
public:
    enum State { Idle, Rechecking, Submitting, Reconciling, AwaitingPayment, Paused, Partial, Complete };
    explicit JourneyBooking(JourneyGateway *gateway, QObject *parent = nullptr, int timeout = 90000);
    bool begin(const Journey::Plan &plan, const Journey::Request &request);
    void reconcile(const QStringList &passengers);
    void pause();
    bool reset();
    void pay();
    bool hasNewQuote() const { return replacement.bookable; }
    Journey::Plan proposedPlan() const;
    void acceptNewQuote();
    State state() const { return currentState; }
    QString message() const { return status; }
    Journey::Plan plan() const { return selected; }
    bool hasTask() const { return currentState != Idle; }
    int legIndex() const { return index; }
    QString orderId() const { return ids.value(index); }
signals:
    void changed();
private:
    enum Stage { Pending, Paid, Quote } stage = Pending;
    void setState(State state, const QString &message);
    void requestOrders();
    void verifyQuote();
    void receive(quint64 id, const Journey::Query &query, const QVariantMap &data, const QString &error);
    void consumeOrders(const QVector<Journey::Order> &paid);
    void save();
    void restore();
    JourneyGateway *gateway;
    Journey::Plan selected;
    Journey::Request preferences;
    Journey::Leg replacement;
    QVector<Journey::Order> pendingOrders;
    QVector<Journey::Order> paidOrders;
    int paidRows = 0;
    QStringList ids;
    State currentState = Idle;
    QString status;
    int index = 0, quoteIndex = 0, attemptedIndex = -1;
    quint64 inFlight = 0;
    bool reserved = false, restored = false, cancelled = false;
    QTimer deadline;
    int timeout;
};
#endif
