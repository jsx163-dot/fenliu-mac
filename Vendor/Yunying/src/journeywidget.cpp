#include "journeywidget.h"
#include "analysis.h"
#include "userdata.h"
#include "appstyle.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QShowEvent>
#include <QStyle>
#include <QTableWidget>
#include <QTimeEdit>
#include <QVBoxLayout>
#include <limits>

static QListWidget *checkList(QWidget *parent)
{
    auto *list = new QListWidget(parent);
    list->setFixedHeight(78);
    list->setSelectionMode(QAbstractItemView::NoSelection);
    return list;
}
static void addCheck(QListWidget *list, const QString &label, const QString &value, bool checked)
{
    auto *item = new QListWidgetItem(label, list);
    item->setData(Qt::UserRole, value);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}
static QStringList checkedValues(const QListWidget *list)
{
    QStringList result;
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->checkState() == Qt::Checked) result.append(list->item(i)->data(Qt::UserRole).toString());
    return result;
}

JourneyWidget::JourneyWidget(QWidget *parent, JourneyGateway *network) : QWidget(parent), localGateway(!network)
{
    gateway = network ? network : new LocalJourneyGateway(this);
    search = new JourneySearch(gateway, this);
    booking = new JourneyBooking(gateway, this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);
    auto *form = new QWidget(this);
    form->setObjectName("journeyForm");
    auto *grid = new QGridLayout(form);
    grid->setContentsMargins(0, 0, 8, 0);
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(1, 1); grid->setColumnStretch(3, 1);
    fromMode = new QComboBox; toMode = new QComboBox;
    for (auto *mode : {fromMode, toMode}) {
        mode->addItems({tr("城市"), tr("指定站点")});
        mode->setFixedWidth(110);
    }
    from = new QLineEdit; to = new QLineEdit;
    from->setPlaceholderText(tr("出发城市或车站")); to->setPlaceholderText(tr("到达城市或车站"));
    from->setObjectName("journeyFrom"); to->setObjectName("journeyTo");
    auto *fromRow = new QHBoxLayout; fromRow->addWidget(fromMode); fromRow->addWidget(from);
    auto *toRow = new QHBoxLayout; toRow->addWidget(toMode); toRow->addWidget(to);
    fromRow->setSpacing(8); toRow->setSpacing(8);
    grid->addWidget(new QLabel(tr("出发地")), 0, 0); grid->addLayout(fromRow, 0, 1);
    grid->addWidget(new QLabel(tr("目的地")), 0, 2); grid->addLayout(toRow, 0, 3);
    fromStations = checkList(this); toStations = checkList(this);
    fromStations->setObjectName("journeyFromStations"); toStations->setObjectName("journeyToStations");
    grid->addWidget(new QLabel(tr("出发站范围")), 1, 0); grid->addWidget(fromStations, 1, 1);
    grid->addWidget(new QLabel(tr("到达站范围")), 1, 2); grid->addWidget(toStations, 1, 3);
    date = new QDateEdit(QDate::currentDate()); date->setCalendarPopup(true); date->setDisplayFormat("yyyy-MM-dd");
    date->setObjectName("journeyDate");
    budget = new QDoubleSpinBox; budget->setDecimals(2); budget->setRange(0, 1000000);
    budget->setSpecialValueText(tr("不限预算")); budget->setSuffix(tr(" 元 / 全部乘客"));
    grid->addWidget(new QLabel(tr("出发日期")), 2, 0); grid->addWidget(date, 2, 1);
    grid->addWidget(new QLabel(tr("总预算")), 2, 2); grid->addWidget(budget, 2, 3);
    passengers = checkList(this); passengers->setObjectName("journeyPassengers");
    seats = checkList(this); seats->setObjectName("journeySeats");
    seats->setDragDropMode(QAbstractItemView::InternalMove);
    seats->setSelectionMode(QAbstractItemView::SingleSelection);
    grid->addWidget(new QLabel(tr("乘车人")), 3, 0); grid->addWidget(passengers, 3, 1);
    grid->addWidget(new QLabel(tr("允许席别 / 优先顺序")), 3, 2); grid->addWidget(seats, 3, 3);
    auto *timeRow = new QHBoxLayout;
    timeLimit = new QCheckBox(tr("出发时间"));
    earliest = new QTimeEdit(QTime(0, 0)); latest = new QTimeEdit(QTime(23, 59));
    earliest->setFixedWidth(108); latest->setFixedWidth(108);
    earliest->setDisplayFormat("HH:mm"); latest->setDisplayFormat("HH:mm");
    earliest->setEnabled(false); latest->setEnabled(false);
    timeRow->addWidget(timeLimit); timeRow->addWidget(earliest); timeRow->addWidget(new QLabel(tr("至"))); timeRow->addWidget(latest);
    timeRow->addStretch();
    split = new QCheckBox(tr("允许同车拆两段")); split->setChecked(true);
    grid->addLayout(timeRow, 4, 0, 1, 2); grid->addWidget(split, 4, 2, 1, 2);
    auto *scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setWidget(form);
    scroll->setObjectName("journeyFormScroll");
    scroll->setFrameShape(QFrame::NoFrame); scroll->setMinimumHeight(180); scroll->setMaximumHeight(328);
    layout->addWidget(scroll);
    auto *commands = new QHBoxLayout;
    auto button = [&](const QString &text, const QString &icon) {
        auto *b = new QPushButton(AppStyle::icon(icon), text); b->setToolTip(text); commands->addWidget(b); return b;
    };
    searchButton = button(tr("搜索备选"), "search");
    searchButton->setIcon(AppStyle::icon("search", Qt::white));
    searchButton->setProperty("role", "primary");
    searchButton->setObjectName("journeySearchButton");
    stopButton = button(tr("停止"), "square");
    buyButton = button(tr("确认并下单"), "check");
    buyButton->setProperty("role", "accent");
    commands->addStretch();
    progress = new QLabel(tr("准备就绪")); progress->setWordWrap(true); commands->addWidget(progress, 2);
    progress->setProperty("muted", true);
    layout->addLayout(commands);
    table = new QTableWidget(0, 8); table->setObjectName("journeyPlans");
    table->setAlternatingRowColors(true); table->setShowGrid(false);
    table->setMinimumHeight(130);
    table->setHorizontalHeaderLabels({tr("方案"), tr("实际站点"), tr("车次 / 席别"), tr("出发"), tr("到达"),
                                     tr("总价"), tr("最少余票"), tr("换铺 / 查询时间")});
    table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->verticalHeader()->setMinimumSectionSize(42);
    for (int col = 0; col < 8; ++col) table->setColumnWidth(col, col == 1 ? 190 : col == 2 ? 150 : 115);
    table->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(table, 1);
    orderStatus = new QLabel; orderStatus->setObjectName("journeyOrderStatus"); orderStatus->setWordWrap(true);
    orderStatus->setTextFormat(Qt::PlainText); progress->setTextFormat(Qt::PlainText);
    layout->addWidget(orderStatus);
    auto *orderCommands = new QHBoxLayout;
    reconcileButton = new QPushButton(AppStyle::icon("refresh-cw"), tr("核实订单并继续"));
    payButton = new QPushButton(AppStyle::icon("credit-card"), tr("支付本段订单"));
    newQuoteButton = new QPushButton(AppStyle::icon("check"), tr("重新确认余程"));
    resetButton = new QPushButton(tr("结束已核实任务"));
    for (auto *b : {reconcileButton, payButton, newQuoteButton, resetButton}) { b->setToolTip(b->text()); orderCommands->addWidget(b); }
    orderCommands->addStretch(); layout->addLayout(orderCommands);

    QSettings settings;
    from->setText(settings.value("journey/from", UserData::instance()->userConfig.staFromName).toString());
    to->setText(settings.value("journey/to", UserData::instance()->userConfig.staToName).toString());
    fromMode->setCurrentIndex(settings.value("journey/fromMode", 0).toInt());
    toMode->setCurrentIndex(settings.value("journey/toMode", 0).toInt());
    budget->setValue(settings.value("journey/budget", 0).toDouble());
    split->setChecked(settings.value("journey/split", true).toBool());
    const QDate savedDate = settings.value("journey/date").toDate();
    if (savedDate.isValid()) date->setDate(savedDate);
    earliest->setTime(settings.value("journey/earliest", QTime(0, 0)).toTime());
    latest->setTime(settings.value("journey/latest", QTime(23, 59)).toTime());
    QStringList allowed = settings.value("journey/seats", Journey::defaultSeats()).toStringList();
    QStringList all = allowed;
    for (const QString &code : Journey::seatNames().keys()) if (!all.contains(code)) all.append(code);
    for (const QString &code : all) if (Journey::seatNames().contains(code))
        addCheck(seats, Journey::seatNames().value(code), code, allowed.contains(code));
    connect(from, &QLineEdit::editingFinished, this, [this] { updateStations(true); });
    connect(to, &QLineEdit::editingFinished, this, [this] { updateStations(false); });
    connect(fromMode, &QComboBox::currentIndexChanged, this, [this] { updateStations(true); });
    connect(toMode, &QComboBox::currentIndexChanged, this, [this] { updateStations(false); });
    connect(timeLimit, &QCheckBox::toggled, earliest, &QWidget::setEnabled);
    connect(timeLimit, &QCheckBox::toggled, latest, &QWidget::setEnabled);
    timeLimit->setChecked(settings.value("journey/timeLimit", false).toBool());
    connect(searchButton, &QPushButton::clicked, this, &JourneyWidget::startSearch);
    connect(stopButton, &QPushButton::clicked, this, &JourneyWidget::pause);
    connect(buyButton, &QPushButton::clicked, this, &JourneyWidget::confirmPlan);
    connect(search, &JourneySearch::changed, this, &JourneyWidget::renderPlans);
    connect(search, &JourneySearch::progress, this, [this](int checked, int outstanding, const QString &message) {
        progress->setText(tr("%1 · 已查 %2 / 待查 %3").arg(message).arg(checked).arg(outstanding));
    });
    connect(booking, &JourneyBooking::changed, this, &JourneyWidget::renderBooking);
    connect(reconcileButton, &QPushButton::clicked, this, [this] { booking->reconcile(passengerRefs()); });
    connect(payButton, &QPushButton::clicked, booking, &JourneyBooking::pay);
    connect(newQuoteButton, &QPushButton::clicked, this, [this] {
        const auto plan = booking->proposedPlan();
        QStringList lines;
        for (const auto &leg : plan.legs) lines.append(tr("%1 %2 → %3 %4 %5 元 / 人")
            .arg(leg.trainCode, leg.fromName, leg.toName, Journey::seatNames().value(leg.seat))
            .arg(leg.price / 100.0, 0, 'f', 2));
        lines.append(tr("合计 %1 元。已支付的前段不会重新购买；余程仍可能失票，不自动付款。").arg(plan.total() / 100.0, 0, 'f', 2));
        QMessageBox box(QMessageBox::Question, tr("重新确认完整行程"), lines.join('\n'), QMessageBox::Yes | QMessageBox::No, this);
        box.setTextFormat(Qt::PlainText); box.setDefaultButton(QMessageBox::No);
        if (box.exec() == QMessageBox::Yes) booking->acceptNewQuote();
    });
    connect(resetButton, &QPushButton::clicked, this, [this] {
        if (booking->legIndex() > 0 && booking->state() != JourneyBooking::Complete
            && QMessageBox::question(this, tr("保留已购车票并结束任务"),
                tr("当前行程尚未全部买好。结束本地任务不会退掉已购车票，也不会购买余程。确认结束？"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        if (!booking->reset()) QMessageBox::warning(this, tr("订单尚未明确"), tr("请先核实官方订单；不会自动取消已有订单或重复购买。"));
    });
    renderBooking();
}
void JourneyWidget::setStations(const QByteArray &data)
{
    directory = Journey::parseStations(data);
    updateStations(true); updateStations(false);
}
void JourneyWidget::updateStations(bool departure)
{
    auto *field = departure ? from : to;
    auto *mode = departure ? fromMode : toMode;
    auto *list = departure ? fromStations : toStations;
    const QString scope = QString::number(mode->currentIndex()) + '|' + field->text().trimmed();
    QStringList excludedNow;
    const bool sameScope = list->property("scope").toString() == scope;
    if (sameScope) for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->checkState() != Qt::Checked) excludedNow.append(list->item(i)->data(Qt::UserRole).toString());
    QStringList options;
    for (const auto &station : directory) {
        const QString name = mode->currentIndex() == 0 ? station.city : station.name;
        if (!name.isEmpty() && !options.contains(name)) options.append(name);
    }
    options.sort();
    QPointer<QCompleter> old = field->completer();
    auto *completer = new QCompleter(options, field); completer->setFilterMode(Qt::MatchContains);
    field->setCompleter(completer); if (old) old->deleteLater();
    list->clear();
    const QString value = field->text().trimmed();
    QSettings settings;
    const QStringList excluded = sameScope ? excludedNow
        : settings.value(departure ? "journey/excludedFrom" : "journey/excludedTo").toStringList();
    list->setProperty("scope", scope);
    for (const auto &station : directory) {
        if ((mode->currentIndex() == 0 && !station.city.isEmpty() && station.city == value)
            || (mode->currentIndex() == 1 && station.name == value))
            addCheck(list, station.name, station.code, !excluded.contains(station.code));
    }
}
void JourneyWidget::setPassengerOptions(const QVector<QPair<QString, QString>> &options)
{
    const QStringList selected = passengerRefs();
    passengers->clear();
    for (const auto &option : options) addCheck(passengers, option.second, option.first, selected.contains(option.first));
}
void JourneyWidget::importSeats(const QStringList &names)
{
    if (QSettings().contains("journey/seats") || names.isEmpty()) return;
    QStringList codes;
    for (const QString &name : names) {
        const QString code(seatTypeNameToCode(name));
        if (Journey::seatNames().contains(code) && !codes.contains(code)) codes.append(code);
    }
    if (codes.isEmpty()) return;
    seats->clear();
    for (const QString &code : codes) addCheck(seats, Journey::seatNames().value(code), code, true);
    for (const QString &code : Journey::seatNames().keys()) if (!codes.contains(code)) addCheck(seats, Journey::seatNames().value(code), code, false);
}
void JourneyWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (localGateway) setPassengerOptions(LocalJourneyGateway::passengerOptions());
}
QStringList JourneyWidget::passengerRefs() const { return checkedValues(passengers); }
Journey::Request JourneyWidget::input() const
{
    Journey::Request r;
    const auto origins = checkedValues(fromStations), destinations = checkedValues(toStations);
    for (const auto &s : directory) { if (origins.contains(s.code)) r.from.append(s); if (destinations.contains(s.code)) r.to.append(s); }
    r.date = date->date(); r.passengers = passengerRefs(); r.seats = checkedValues(seats);
    r.budget = qRound(budget->value() * 100); r.split = split->isChecked();
    if (timeLimit->isChecked()) { r.earliest = earliest->time(); r.latest = latest->time(); }
    return r;
}
void JourneyWidget::savePreferences()
{
    QSettings s;
    s.setValue("journey/from", from->text()); s.setValue("journey/to", to->text());
    s.setValue("journey/fromMode", fromMode->currentIndex()); s.setValue("journey/toMode", toMode->currentIndex());
    s.setValue("journey/budget", budget->value()); s.setValue("journey/split", split->isChecked());
    s.setValue("journey/date", date->date()); s.setValue("journey/timeLimit", timeLimit->isChecked());
    s.setValue("journey/earliest", earliest->time()); s.setValue("journey/latest", latest->time());
    s.setValue("journey/seats", checkedValues(seats));
    for (const auto &pair : {qMakePair(fromStations, QString("journey/excludedFrom")), qMakePair(toStations, QString("journey/excludedTo"))}) {
        QStringList excluded;
        for (int i = 0; i < pair.first->count(); ++i)
            if (pair.first->item(i)->checkState() != Qt::Checked) excluded.append(pair.first->item(i)->data(Qt::UserRole).toString());
        s.setValue(pair.second, excluded);
    }
}
void JourneyWidget::startSearch()
{
    if (booking->hasTask() && booking->state() != JourneyBooking::Complete) {
        progress->setText(tr("请先核实或结束已有任务")); return;
    }
    updateStations(true); updateStations(false);
    savePreferences(); search->start(input(), directory);
}
void JourneyWidget::renderPlans()
{
    const QString selected = table->currentRow() >= 0 ? table->item(table->currentRow(), 0)->data(Qt::UserRole).toString() : QString();
    const auto plans = search->plans(); table->setRowCount(plans.size());
    for (int row = 0; row < plans.size(); ++row) {
        const auto &plan = plans[row]; QStringList routes, trains, departures, arrivals; int remaining = std::numeric_limits<int>::max();
        QDateTime checked;
        for (const auto &leg : plan.legs) {
            routes.append(leg.fromName + QStringLiteral(" → ") + leg.toName);
            trains.append(leg.trainCode + ' ' + Journey::seatNames().value(leg.seat));
            departures.append(leg.departure.toString("MM-dd HH:mm")); arrivals.append(leg.arrival.toString("MM-dd HH:mm"));
            remaining = qMin(remaining, leg.remaining);
            if (!checked.isValid() || leg.checkedAt < checked) checked = leg.checkedAt;
        }
        const QStringList cells = {plan.legs.size() == 1 ? tr("整程") : tr("同车两段"), routes.join('\n'), trains.join('\n'),
            departures.join('\n'), arrivals.join('\n'),
            plan.total() < 0 ? tr("报价缺失") : tr("%1 元").arg(plan.total() / 100.0, 0, 'f', 2),
            remaining == std::numeric_limits<int>::max() ? tr("有") : QString::number(remaining),
            (plan.legs.size() == 1 ? tr("无需换铺") : tr("可能换铺")) + '\n' + checked.toLocalTime().toString("HH:mm:ss")};
        for (int col = 0; col < cells.size(); ++col) {
            auto *item = new QTableWidgetItem(cells[col]); item->setToolTip(cells[col]); table->setItem(row, col, item);
        }
        table->item(row, 0)->setData(Qt::UserRole, plan.key());
        if (plan.key() == selected) table->selectRow(row);
    }
    table->resizeRowsToContents();
    if (selected.isEmpty() && !plans.isEmpty()) table->selectRow(0);
}
void JourneyWidget::confirmPlan()
{
    const auto plans = search->plans(); const int row = table->currentRow();
    if (row < 0 || row >= plans.size()) return;
    const auto plan = plans[row];
    updateStations(true); updateStations(false);
    const auto request = input();
    QStringList currentPeople = request.passengers, searchedPeople = search->request().passengers;
    currentPeople.sort(); searchedPeople.sort();
    if (currentPeople != searchedPeople) {
        QMessageBox::warning(this, tr("乘客已变化"), tr("当前乘客与搜索时不一致，请重新搜索后确认。")); return;
    }
    if (!Journey::validPlan(plan, request)) {
        QMessageBox::warning(this, tr("偏好已变化"), tr("此方案不再符合当前站点、日期、席别、预算或时间范围，请重新搜索。")); return;
    }
    if (plan.total() < 0) { QMessageBox::warning(this, tr("报价缺失"), tr("报价完整后才能确认下单。")); return; }
    QStringList names;
    for (int i = 0; i < passengers->count(); ++i)
        if (passengers->item(i)->checkState() == Qt::Checked) names.append(passengers->item(i)->text());
    QStringList lines{tr("乘车人：%1").arg(names.join(QStringLiteral("、")))};
    for (const auto &leg : plan.legs) lines.append(tr("%1  %2 → %3  %4\n%5 → %6  %7 元 / 人")
        .arg(leg.trainCode, leg.fromName, leg.toName, Journey::seatNames().value(leg.seat),
             leg.departure.toString("yyyy-MM-dd HH:mm"), leg.arrival.toString("yyyy-MM-dd HH:mm"))
        .arg(leg.price / 100.0, 0, 'f', 2));
    lines.append(tr("全部 %1 位乘客报价合计：%2 元").arg(plan.people).arg(plan.total() / 100.0, 0, 'f', 2));
    if (plan.legs.size() == 2) lines.append(tr("两段不能同时锁票，可能需要换铺，也可能只买到一段。第一段付款后才会核实并购买第二段，不会自动付款、退票或改买其他路线。"));
    auto *box = new QMessageBox(QMessageBox::Question, tr("确认完整行程"), lines.join('\n'), QMessageBox::Yes | QMessageBox::No, this);
    box->setTextFormat(Qt::PlainText); box->setDefaultButton(QMessageBox::No);
    const int answer = box->exec(); box->deleteLater();
    if (answer != QMessageBox::Yes) return;
    search->stop();
    if (booking->state() == JourneyBooking::Complete) booking->reset();
    savePreferences();
    if (!booking->begin(plan, request)) progress->setText(booking->message().isEmpty() ? tr("方案未通过校验或已有任务") : booking->message());
}
void JourneyWidget::renderBooking()
{
    QString text = booking->message();
    if (booking->hasTask()) {
        QStringList routes;
        for (const auto &leg : booking->plan().legs) routes.append(leg.fromName + QStringLiteral(" → ") + leg.toName);
        text = routes.join(QStringLiteral(" / ")) + '\n' + text;
        if (!booking->orderId().isEmpty()) text += tr("\n本段订单：%1").arg(booking->orderId());
    }
    orderStatus->setText(text);
    orderStatus->setVisible(!text.isEmpty());
    const bool busy = booking->state() == JourneyBooking::Submitting || booking->state() == JourneyBooking::Rechecking
                      || booking->state() == JourneyBooking::Reconciling;
    reconcileButton->setEnabled(booking->hasTask() && !busy && booking->state() != JourneyBooking::Complete);
    payButton->setEnabled(booking->state() == JourneyBooking::AwaitingPayment);
    newQuoteButton->setEnabled(booking->hasNewQuote() && !busy);
    resetButton->setEnabled(booking->hasTask() && !busy);
    buyButton->setEnabled(!booking->hasTask() || booking->state() == JourneyBooking::Complete);
}
void JourneyWidget::pause() { search->stop(); booking->pause(); }
void JourneyWidget::loggedOut() { pause(); passengers->clear(); }
