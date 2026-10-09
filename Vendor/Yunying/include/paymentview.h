#ifndef PAYMENTVIEW_H
#define PAYMENTVIEW_H

#include <QByteArray>
#include <QList>
#include <QNetworkCookie>
#include <QUrl>

class QWidget;

bool isAllowedPaymentUrl(const QUrl &url);
void showPaymentPage(QWidget *parent, const QByteArray &html, const QUrl &baseUrl,
                     const QList<QNetworkCookie> &cookies);

#endif
