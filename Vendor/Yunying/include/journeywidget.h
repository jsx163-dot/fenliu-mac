#ifndef JOURNEYWIDGET_H
#define JOURNEYWIDGET_H
#include "journeybooking.h"
#include "journeysearch.h"
#include <QWidget>

class QComboBox;
class QLineEdit;
class QDateEdit;
class QListWidget;
class QDoubleSpinBox;
class QTimeEdit;
class QCheckBox;
class QLabel;
class QPushButton;
class QTableWidget;

class JourneyWidget : public QWidget {
    Q_OBJECT
public:
    explicit JourneyWidget(QWidget *parent = nullptr, JourneyGateway *gateway = nullptr);
    void setStations(const QByteArray &data);
    void setPassengerOptions(const QVector<QPair<QString, QString>> &options);
    void importSeats(const QStringList &names);
    void loggedOut();
    void pause();
    JourneySearch *searchEngine() const { return search; }
protected:
    void showEvent(QShowEvent *event) override;
private:
    void updateStations(bool departure);
    void startSearch();
    void renderPlans();
    void renderBooking();
    void confirmPlan();
    QStringList passengerRefs() const;
    Journey::Request input() const;
    void savePreferences();
    JourneyGateway *gateway;
    JourneySearch *search;
    JourneyBooking *booking;
    bool localGateway;
    QVector<Journey::Station> directory;
    QComboBox *fromMode, *toMode;
    QLineEdit *from, *to;
    QDateEdit *date;
    QListWidget *fromStations, *toStations, *passengers, *seats;
    QDoubleSpinBox *budget;
    QTimeEdit *earliest, *latest;
    QCheckBox *timeLimit, *split;
    QTableWidget *table;
    QLabel *progress, *orderStatus;
    QPushButton *searchButton, *stopButton, *buyButton, *reconcileButton, *payButton, *resetButton, *newQuoteButton;
};
#endif
