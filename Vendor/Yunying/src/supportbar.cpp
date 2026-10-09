#include "supportbar.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

static QLabel *plainLabel(const QString &text, const QString &name, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(name);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    return label;
}

SupportBar::SupportBar(QWidget *parent) : QFrame(parent)
{
    setObjectName("supportBar");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(96);
    setStyleSheet(QStringLiteral(
        "QFrame#supportBar { background: #f5f7f6; border-top: 1px solid #d8ddda; }"
        "QLabel#sponsorPlaceholder { color: #6a726d; border: 1px dashed #9ca8a0; border-radius: 4px; }"
        "QLabel#sponsorStatus, QLabel#advertisingStatus { color: #6a726d; }"
        "QLabel#advertisingBadge { color: #666666; }"
        "QFrame#advertisingSlot { background: #ffffff; border: 1px dashed #b4bcb7; border-radius: 4px; }"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(16);

    auto *sponsor = new QWidget(this);
    sponsor->setObjectName("sponsorSlot");
    sponsor->setFixedWidth(244);
    auto *sponsorLayout = new QHBoxLayout(sponsor);
    sponsorLayout->setContentsMargins(0, 0, 0, 0);
    sponsorLayout->setSpacing(12);
    auto *code = plainLabel(tr("收款码"), "sponsorPlaceholder", sponsor);
    code->setAlignment(Qt::AlignCenter);
    code->setFixedSize(64, 64);
    sponsorLayout->addWidget(code);
    auto *sponsorText = new QVBoxLayout;
    sponsorText->setSpacing(4);
    sponsorText->addStretch();
    auto *sponsorTitle = plainLabel(tr("赞助开发"), "sponsorTitle", sponsor);
    QFont titleFont = sponsorTitle->font(); titleFont.setBold(true);
    sponsorTitle->setFont(titleFont);
    sponsorText->addWidget(sponsorTitle);
    sponsorText->addWidget(plainLabel(tr("赞助信息待配置"), "sponsorStatus", sponsor));
    sponsorText->addStretch();
    sponsorLayout->addLayout(sponsorText, 1);
    layout->addWidget(sponsor);

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Plain);
    layout->addWidget(separator);

    auto *advertisement = new QFrame(this);
    advertisement->setObjectName("advertisingSlot");
    advertisement->setMinimumWidth(200);
    advertisement->setFixedHeight(64);
    auto *adLayout = new QHBoxLayout(advertisement);
    adLayout->setContentsMargins(12, 8, 12, 8);
    adLayout->setSpacing(12);
    auto *badge = plainLabel(tr("广告"), "advertisingBadge", advertisement);
    badge->setFixedWidth(40);
    badge->setAlignment(Qt::AlignCenter);
    adLayout->addWidget(badge);
    auto *adText = new QVBoxLayout;
    adText->setSpacing(2);
    auto *adTitle = plainLabel(tr("广告展示位"), "advertisingTitle", advertisement);
    adTitle->setFont(titleFont);
    adText->addWidget(adTitle);
    adText->addWidget(plainLabel(tr("暂无投放"), "advertisingStatus", advertisement));
    adLayout->addLayout(adText, 1);
    layout->addWidget(advertisement, 1);
}
