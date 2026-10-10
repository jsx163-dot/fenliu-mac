#include "appstyle.h"
#include <QApplication>
#include <QFile>
#include <QPainter>
#include <QPalette>
#include <QStyleFactory>

void AppStyle::apply(QApplication &app)
{
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#ffffff"));
    palette.setColor(QPalette::WindowText, QColor("#232a36"));
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    palette.setColor(QPalette::AlternateBase, QColor("#f7f8fa"));
    palette.setColor(QPalette::Text, QColor("#232a36"));
    palette.setColor(QPalette::Button, QColor("#ffffff"));
    palette.setColor(QPalette::ButtonText, QColor("#232a36"));
    palette.setColor(QPalette::Highlight, QColor("#eaf0ff"));
    palette.setColor(QPalette::HighlightedText, QColor("#244fbc"));
    palette.setColor(QPalette::Link, QColor("#315ed4"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#9299a5"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#9299a5"));
    app.setPalette(palette);
    QFont font = app.font();
    font.setPixelSize(13);
    app.setFont(font);
    QFile stylesheet(QStringLiteral(":/res/main.qss"));
    if (stylesheet.open(QFile::ReadOnly)) app.setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
}

QIcon AppStyle::icon(const QString &name, const QColor &color)
{
    const QIcon source(QStringLiteral(":/ui/icons/%1.svg").arg(name));
    QIcon result;
    for (int size : {16, 20, 24, 32}) {
        for (int scale : {1, 2}) {
            QPixmap pixmap = source.pixmap(size * scale, size * scale);
            if (pixmap.isNull()) continue;
            QPainter painter(&pixmap);
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(pixmap.rect(), color);
            painter.end();
            pixmap.setDevicePixelRatio(scale);
            result.addPixmap(pixmap);
        }
    }
    return result;
}
