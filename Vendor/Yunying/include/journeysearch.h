#ifndef JOURNEYSEARCH_H
#define JOURNEYSEARCH_H
#include "journeygateway.h"
#include <QSet>

class JourneySearch : public QObject {
    Q_OBJECT
public:
    explicit JourneySearch(JourneyGateway *gateway, QObject *parent = nullptr);
    void start(const Journey::Request &request, const QVector<Journey::Station> &stations);
    void stop();
    QVector<Journey::Plan> plans() const { return results; }
    Journey::Request request() const { return preferences; }
    bool running() const { return active; }
signals:
    void changed();
    void progress(int checked, int outstanding, const QString &status);
private:
    void enqueue(const Journey::Query &query);
    void next();
    void receive(quint64 id, const Journey::Query &query, const QVariantMap &data, const QString &error);
    void add(const Journey::Plan &plan);
    void combine();
    struct Split { Journey::Train train; Journey::Query a, b; };
    JourneyGateway *gateway;
    Journey::Request preferences;
    QVector<Journey::Station> stations;
    QVector<Journey::Plan> results;
    QSet<QString> seen, resultKeys;
    QQueue<Journey::Query> queue;
    QMap<QString, QVector<Journey::Train>> tickets;
    QMap<QString, Journey::Train> stopTrains;
    QVector<Split> splits;
    quint64 inFlight = 0;
    int checked = 0;
    bool active = false;
};
#endif
