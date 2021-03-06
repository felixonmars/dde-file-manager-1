#include "dfmvaultactivestartview.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <DIconButton>
#include <DLabel>

DWIDGET_USE_NAMESPACE

DFMVaultActiveStartView::DFMVaultActiveStartView(QWidget *parent)
    : QWidget(parent)
    , m_pStartBtn(nullptr)
{
    // 标题
    DLabel *pLabel1 = new DLabel(tr("File Vault"), this);
    QFont font = pLabel1->font();
    font.setPixelSize(18);
    pLabel1->setFont(font);
    pLabel1->setAlignment(Qt::AlignHCenter);

    DLabel *pLabel2 = new DLabel(tr("Create your secure private space") + '\n' +
                                 tr("Advanced encryption technology") + '\n' +
                                 tr("Convenient and easy to use"), this);
    pLabel2->setAlignment(Qt::AlignHCenter);

    DLabel *pLabel3 = new DLabel();
    pLabel3->setPixmap(QIcon::fromTheme("dfm_vault_active_start").pixmap(88, 100));
    pLabel3->setAlignment(Qt::AlignHCenter);

    m_pStartBtn = new QPushButton(tr("Create"), this);
    m_pStartBtn->setFixedSize(452, 30);
    connect(m_pStartBtn, &QPushButton::clicked,
            this, &DFMVaultActiveStartView::slotStartBtnClicked);

    // 布局
    QVBoxLayout *play = new QVBoxLayout(this);
    play->setMargin(0);
    play->addWidget(pLabel1);
    play->addSpacing(5);
    play->addWidget(pLabel2);
    play->addSpacing(15);
    play->addWidget(pLabel3);
    play->addStretch();
    play->addWidget(m_pStartBtn);
}

void DFMVaultActiveStartView::slotStartBtnClicked()
{
    emit sigAccepted();
}
