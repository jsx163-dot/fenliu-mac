#ifndef ICONDELEGATE_H
#define ICONDELEGATE_H
#include <QTableView>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QApplication>
#include <userdata.h>

#define _ QStringLiteral

class IconDelegate : public QStyledItemDelegate
{
public:
    IconDelegate(QObject* parent = nullptr) :
        QStyledItemDelegate(parent)
    {
        fu.load(_(":/icon/images/fu.ico"));
        zi.load(_(":/icon/images/zi.ico"));
        dong.load(_(":/icon/images/dong.ico"));
        start.load(_(":/icon/images/start.ico"));
        start.scaled(16, 16);
        pass.load(_(":/icon/images/pass.ico"));
        pass.scaled(16, 16);
        end.load(_(":/icon/images/end.ico"));
        end.scaled(16, 16);
    }
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        bool isFu = false;
        bool isZi = false;
        bool isDong = false;
        bool isStart = false, isEnd = false;
        bool handled = false;
        int type = 0;
        int width;
        int spendDays;
        QString text;
        QRect rect;
        QPen pen(QColor("#315ed4"), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        QFont font = opt.font;
        const bool selected = option.state & QStyle::State_Selected;

        if (selected) {
            pen.setColor(opt.palette.color(QPalette::HighlightedText));
        }
        if (index.column() == EFROMSTATIONCOL || index.column() == ETOSTATIONCOL
            || index.column() == EARRIVETIMECOL || index.column() == EOTHERCOL) {
            QStyleOptionViewItem background = opt;
            background.text.clear();
            background.icon = QIcon();
            background.features &= ~(QStyleOptionViewItem::HasDisplay | QStyleOptionViewItem::HasDecoration);
            const QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
            painter->save();
            style->drawControl(QStyle::CE_ItemViewItem, &background, painter, opt.widget);
            painter->restore();
        }

        switch (index.column()) {
        case EFROMSTATIONCOL:
            isStart = index.data(Qt::DecorationRole).toBool();
            text = index.data(Qt::ToolTipRole).toString();
            rect = option.rect;
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);  //抗锯齿
            painter->setRenderHints(QPainter::SmoothPixmapTransform);  //平滑像素图变换
            rect.setWidth(16);
            rect.setHeight(16);
            rect.moveTop(option.rect.top() + (option.rect.height() - 16) / 2);
            if (isStart) {
                painter->drawPixmap(rect, start);
            } else {
                painter->drawPixmap(rect, pass);
            }
            painter->setPen(pen);
            painter->setFont(font);
            width = qMax(0, option.rect.width() - 24);
            text = QFontMetrics(font).elidedText(text, Qt::ElideRight, width);
            rect = option.rect.adjusted(22, 0, -2, 0);
            painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, text);
            painter->restore();
            break;
        case ETOSTATIONCOL:
            isEnd = index.data(Qt::DecorationRole).toBool();
            text = index.data(Qt::ToolTipRole).toString();
            rect = option.rect;
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);  //抗锯齿
            painter->setRenderHints(QPainter::SmoothPixmapTransform);  //平滑像素图变换
            rect.setWidth(16);
            rect.setHeight(16);
            rect.moveTop(option.rect.top() + (option.rect.height() - 16) / 2);
            if (isEnd) {
                painter->drawPixmap(rect, end);
            } else {
                painter->drawPixmap(rect, pass);
            }
            painter->setPen(pen);
            painter->setFont(font);
            width = qMax(0, option.rect.width() - 24);
            text = QFontMetrics(font).elidedText(text, Qt::ElideRight, width);
            rect = option.rect.adjusted(22, 0, -2, 0);
            painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, text);
            painter->restore();
            break;
        case EARRIVETIMECOL:
            text = index.data(Qt::DisplayRole).toString();
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setRenderHints(QPainter::SmoothPixmapTransform);
            if (!selected) {
                pen.setColor(QColor("#a74967"));
            }
            painter->setPen(pen);
            rect = painter->boundingRect(option.rect.x(), option.rect.y(),
                                         option.rect.width(),
                                         option.rect.height(), Qt::AlignCenter, text);
            painter->drawText(rect, text);

            spendDays = index.data(Qt::UserRole).toInt();
            if (spendDays > 0) {
                font.setPixelSize(10);
                painter->setFont(font);
                text = _("+%1").arg(spendDays);
                rect = painter->boundingRect(option.rect.x() + option.rect.width() - 18,
                                             option.rect.y() + 2,
                                             20,
                                             20,
                                             Qt::AlignCenter,
                                             text);
                painter->drawText(rect, text);
            }
            painter->restore();
            break;
        case EOTHERCOL:
            type = index.data(Qt::UserRole).toInt();
            isFu = ISFUXING(type);
            isZi = ISZINENG(type);
            isDong = ISDONGGAN(type);
            if (isFu) {
                painter->save();
                rect = option.rect;
                rect.setLeft(rect.left());
                rect.setTopLeft(QPoint(rect.x(), rect.y() + (option.rect.height() - 16) / 2));
                rect.setWidth(16);
                rect.setHeight(16);
                painter->setRenderHint(QPainter::Antialiasing);  //抗锯齿
                painter->setRenderHints(QPainter::SmoothPixmapTransform);  //平滑像素图变换
                painter->drawPixmap(rect, fu);
                painter->restore();
                handled = true;
            }
            if (isZi) {
                painter->save();
                int l1[2] = { 0, 20 };
                rect = option.rect;
                int left = 0;
                left += rect.left();
                left += l1[isFu];
                rect.setLeft(left);
                rect.setTopLeft(QPoint(rect.x(), rect.y() + (option.rect.height() - 16) / 2));
                rect.setWidth(16);
                rect.setHeight(16);
                painter->setRenderHint(QPainter::Antialiasing);  //抗锯齿
                painter->setRenderHints(QPainter::SmoothPixmapTransform);  //平滑像素图变换
                painter->drawPixmap(rect, zi);
                painter->restore();
                handled = true;
            }
            if (isDong) {
                painter->save();
                rect = option.rect;
                int l1[2] = { 0, 20 };
                int l2[2] = { 0, 20 };
                int left = 0;
                left += rect.left();
                left += l1[isFu];
                left += l2[isZi];
                rect.setLeft(left);
                rect.setTopLeft(QPoint(rect.x(), rect.y() + (option.rect.height() - 16) / 2));
                rect.setWidth(16);
                rect.setHeight(16);
                painter->setRenderHint(QPainter::Antialiasing);  //抗锯齿
                painter->setRenderHints(QPainter::SmoothPixmapTransform);  //平滑像素图变换
                painter->drawPixmap(rect, dong);
                painter->restore();
                handled = true;
            }
            if (!handled) {
                QStyledItemDelegate::paint(painter, option, index);
            }
            break;
        default:
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
    }

private:
    QPixmap fu;
    QPixmap zi;
    QPixmap dong;
    QPixmap start;
    QPixmap pass;
    QPixmap end;
};

#endif // ICONDELEGATE_H
