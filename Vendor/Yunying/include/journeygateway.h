#ifndef JOURNEYGATEWAY_H
#define JOURNEYGATEWAY_H
#include "journey.h"
#include <QElapsedTimer>
#include <QNetworkRequest>
#include <QPointer>
#include <QQueue>
#include <QTimer>

class QNetworkReply;
class QNetworkAccessManager;
class JourneyGateway : public QObject {
    Q_OBJECT
public:
    explicit JourneyGateway(QObject *parent = nullptr, int interval = 3000);
    quint64 request(const Journey::Query &query);
    void cancel(quint64 id);
    virtual bool reserve(QString &error) = 0;
    virtual void release() = 0;
    virtual void submit(const Journey::Leg &leg, const QStringList &passengers) = 0;
    virtual QVector<Journey::Order> orders(const QVariantMap &data, bool pending) const = 0;
    virtual void pay(const QString &orderId, const Journey::Leg &leg) = 0;
    virtual void pauseSubmission() = 0;
signals:
    void received(quint64 id, const Journey::Query &query, const QVariantMap &data, const QString &error);
    void submissionFinished();
    void submissionRejected(const QString &error);
protected:
    virtual void performRequest(quint64 id, const Journey::Query &query) = 0;
    virtual void abortRequest(quint64 id) { Q_UNUSED(id); }
    void complete(quint64 id, const QVariantMap &data, const QString &error = {});
private:
    void dispatch();
    QQueue<QPair<quint64, Journey::Query>> pending;
    QPair<quint64, Journey::Query> current;
    QTimer timer;
    QElapsedTimer elapsed;
    int interval;
    quint64 nextId = 0;
};

class LocalJourneyGateway : public JourneyGateway {
    Q_OBJECT
public:
    explicit LocalJourneyGateway(QObject *parent = nullptr, QNetworkAccessManager *transport = nullptr);
    bool reserve(QString &error) override;
    void release() override;
    void submit(const Journey::Leg &leg, const QStringList &passengers) override;
    QVector<Journey::Order> orders(const QVariantMap &data, bool pending) const override;
    void pay(const QString &orderId, const Journey::Leg &leg) override;
    void pauseSubmission() override;
    static QPair<QNetworkRequest, QByteArray> requestSpec(const Journey::Query &query);
    static QVector<Journey::Order> decodeOrders(const QVariantMap &data, bool pending);
    static QVector<QPair<QString, QString>> passengerOptions();
protected:
    void performRequest(quint64 id, const Journey::Query &query) override;
    void abortRequest(quint64 id) override;
private:
    QNetworkAccessManager *transport;
    QMap<quint64, QPointer<QNetworkReply>> replies;
    QMap<QString, QVariantMap> ticketData;
};
#endif
