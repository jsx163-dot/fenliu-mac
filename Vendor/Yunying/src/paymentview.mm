#include "paymentview.h"
#include <QDialog>
#include <QLabel>
#include <QPointer>
#include <QScreen>
#include <QVBoxLayout>
#import <AppKit/AppKit.h>
#import <WebKit/WebKit.h>

bool isAllowedPaymentUrl(const QUrl &url)
{
    if (url.scheme() != QStringLiteral("https") || !url.userInfo().isEmpty()
        || (url.port(-1) != -1 && url.port() != 443))
        return false;
    const QString host = url.host().toLower();
    const QStringList domains = {
        QStringLiteral("12306.cn"), QStringLiteral("alipay.com"),
        QStringLiteral("alipayobjects.com"), QStringLiteral("tenpay.com"),
        QStringLiteral("weixin.qq.com"), QStringLiteral("unionpay.com"),
        QStringLiteral("unionpaysecure.com"), QStringLiteral("95516.com"),
        QStringLiteral("icbc.com.cn"), QStringLiteral("abchina.com"),
        QStringLiteral("boc.cn"), QStringLiteral("ccb.com"),
        QStringLiteral("cmbchina.com"), QStringLiteral("psbc.com"),
        QStringLiteral("bankcomm.com")
    };
    for (const QString &domain : domains) {
        if (host == domain || host.endsWith(QStringLiteral(".") + domain))
            return true;
    }
    return false;
}

namespace {
QString fromString(NSString *string)
{
    return string ? QString::fromUtf8(string.UTF8String) : QString();
}

NSURL *toURL(const QUrl &url)
{
    return [NSURL URLWithString:[NSString stringWithUTF8String:url.toEncoded().constData()]];
}
}

@interface FenliuPaymentDelegate : NSObject <WKNavigationDelegate, WKUIDelegate> {
@public
    QPointer<QLabel> statusLabel;
}
@end

@implementation FenliuPaymentDelegate
- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)action
    decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    QUrl url(fromString(action.request.URL.absoluteString));
    if (url == QUrl(QStringLiteral("about:blank"))) {
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
    }
    if (!isAllowedPaymentUrl(url)) {
        if (statusLabel)
            statusLabel->setText(QStringLiteral("已阻止非受信任地址或外部应用跳转：%1").arg(url.host()));
        decisionHandler(WKNavigationActionPolicyCancel);
        return;
    }
    if (statusLabel && (action.targetFrame == nil || action.targetFrame.isMainFrame))
        statusLabel->setText(QStringLiteral("https://%1").arg(url.host()));
    if (action.targetFrame == nil) {
        // Payment popups stay in this application window.
        [webView loadRequest:action.request];
        decisionHandler(WKNavigationActionPolicyCancel);
        return;
    }
    decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation
    withError:(NSError *)error
{
    Q_UNUSED(webView);
    Q_UNUSED(navigation);
    if (statusLabel && error.code != NSURLErrorCancelled)
        statusLabel->setText(QStringLiteral("支付页面加载失败 (%1)").arg(error.code));
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message
    initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    Q_UNUSED(webView);
    Q_UNUSED(frame);
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"支付页面";
    alert.informativeText = message;
    [alert runModal];
    completionHandler();
}

- (void)webView:(WKWebView *)webView runJavaScriptConfirmPanelWithMessage:(NSString *)message
    initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL))completionHandler
{
    Q_UNUSED(webView);
    Q_UNUSED(frame);
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"支付页面";
    alert.informativeText = message;
    [alert addButtonWithTitle:@"确认"];
    [alert addButtonWithTitle:@"取消"];
    completionHandler([alert runModal] == NSAlertFirstButtonReturn);
}
@end

class PaymentDialog final : public QDialog {
public:
    PaymentDialog(QWidget *parent, const QByteArray &html, const QUrl &baseUrl,
                  const QList<QNetworkCookie> &cookies) : QDialog(parent)
    {
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle(QStringLiteral("分流云映 · 支付"));
        auto *layout = new QVBoxLayout(this);
        auto *status = new QLabel(QStringLiteral("https://%1").arg(baseUrl.host()), this);
        status->setWordWrap(true);
        layout->addWidget(status);
        auto *container = new QWidget(this);
        container->setAttribute(Qt::WA_NativeWindow);
        layout->addWidget(container, 1);
        QSize available = screen()->availableGeometry().size();
        resize(qMin(1000, available.width() - 40), qMin(720, available.height() - 80));
        NSView *nativeView = (__bridge NSView *)reinterpret_cast<void *>(container->winId());
        WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
        configuration.websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
        webView = [[WKWebView alloc] initWithFrame:nativeView.bounds configuration:configuration];
        webView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        delegate = [[FenliuPaymentDelegate alloc] init];
        delegate->statusLabel = status;
        webView.navigationDelegate = delegate;
        webView.UIDelegate = delegate;
        [nativeView addSubview:webView];

        // Transfer only cookies applicable to this official payment URL.
        dispatch_group_t pending = dispatch_group_create();
        for (const QNetworkCookie &cookie : cookies) {
            NSMutableDictionary *properties = [NSMutableDictionary dictionary];
            properties[NSHTTPCookieName] = [NSString stringWithUTF8String:cookie.name().constData()];
            properties[NSHTTPCookieValue] = [NSString stringWithUTF8String:cookie.value().constData()];
            properties[NSHTTPCookieDomain] = [NSString stringWithUTF8String:cookie.domain().toUtf8().constData()];
            properties[NSHTTPCookiePath] = [NSString stringWithUTF8String:cookie.path().toUtf8().constData()];
            if (cookie.isSecure())
                properties[NSHTTPCookieSecure] = @"TRUE";
            if (cookie.isHttpOnly())
                properties[@"HttpOnly"] = @"TRUE";
            NSHTTPCookie *nativeCookie = [NSHTTPCookie cookieWithProperties:properties];
            if (nativeCookie) {
                dispatch_group_enter(pending);
                [configuration.websiteDataStore.httpCookieStore setCookie:nativeCookie completionHandler:^{
                    dispatch_group_leave(pending);
                }];
            }
        }
        NSString *content = [[NSString alloc] initWithBytes:html.constData()
            length:html.size() encoding:NSUTF8StringEncoding];
        NSURL *base = toURL(baseUrl);
        __weak WKWebView *weakView = webView;
        dispatch_group_notify(pending, dispatch_get_main_queue(), ^{
            [weakView loadHTMLString:content baseURL:base];
        });
    }

    ~PaymentDialog() override
    {
        [webView stopLoading];
        webView.navigationDelegate = nil;
        webView.UIDelegate = nil;
        [webView removeFromSuperview];
    }

private:
    __strong WKWebView *webView;
    __strong FenliuPaymentDelegate *delegate;
};

void showPaymentPage(QWidget *parent, const QByteArray &html, const QUrl &baseUrl,
                     const QList<QNetworkCookie> &cookies)
{
    if (!isAllowedPaymentUrl(baseUrl) || html.isEmpty())
        return;
    auto *dialog = new PaymentDialog(parent, html, baseUrl, cookies);
    dialog->show();
}
