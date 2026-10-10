#ifndef APPSTYLE_H
#define APPSTYLE_H

#include <QColor>
#include <QIcon>

class QApplication;

namespace AppStyle {
void apply(QApplication &app);
QIcon icon(const QString &name, const QColor &color = QColor("#515b6d"));
}

#endif
