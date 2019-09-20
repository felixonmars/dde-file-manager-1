/*
 * Copyright (C) 2019 Deepin Technology Co., Ltd.
 *
 * Author:     Gary Wang <wzc782970009@gmail.com>
 *
 * Maintainer: Gary Wang <wangzichong@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "dfmvaultsetuppages.h"

#include <QLabel>
#include <dpasswordedit.h>
#include <DSecureString>

DFM_BEGIN_NAMESPACE

VaultSetupWelcomePage::VaultSetupWelcomePage(QWidget *parent)
    : QWidget (parent)
{
    m_createNewButton = new QPushButton(tr("Create New Vault"), this);
    m_importButton = new QPushButton(tr("Import From File"), this);
    m_importButton->setVisible(false);

    QPushButton * icon = new QPushButton(this);
    icon->setDisabled(true);
    icon->setFlat(true);
    icon->setIcon(QIcon::fromTheme("dfm_safebox"));
    icon->setIconSize(QSize(64, 64));
    icon->setMinimumHeight(64);

    QLabel * description = new QLabel("Sample text, sample text.\nSample text.", this);
    description->setAlignment(Qt::AlignHCenter);

    QVBoxLayout * layout = new QVBoxLayout(this);
    layout->addStretch();
    layout->addWidget(icon);
    layout->addWidget(description);
    layout->addStretch();
    layout->addWidget(m_createNewButton);
    layout->addWidget(m_importButton);

    connect(m_createNewButton, &QPushButton::clicked, this, [ = ](){
        emit requestRedirect(VaultController::makeVaultUrl("set_password", "setup"));
    });
}

VaultSetupWelcomePage::~VaultSetupWelcomePage()
{

}

// --------------------------------------

VaultSetupSetPasswordPage::VaultSetupSetPasswordPage(QWidget *parent)
    : QWidget (parent)
    , m_enterPassword(new DPasswordEdit(this))
    , m_confirmPassword(new DPasswordEdit(this))
{
    m_finishButton = new QPushButton(tr("Finished"), this);

    QPushButton * icon = new QPushButton(this);
    icon->setDisabled(true);
    icon->setFlat(true);
    icon->setIcon(QIcon::fromTheme("dfm_safebox"));
    icon->setIconSize(QSize(64, 64));
    icon->setMinimumHeight(64);

    QLabel * description = new QLabel("Sample text, sample text.", this);
    description->setAlignment(Qt::AlignHCenter);

    QVBoxLayout * layout = new QVBoxLayout(this);
    layout->addStretch();
    layout->addWidget(icon);
    layout->addWidget(description);
    layout->addWidget(m_enterPassword);
    layout->addWidget(m_confirmPassword);
    layout->addStretch();
    layout->addWidget(m_finishButton);

    connect(m_finishButton, &QPushButton::clicked, this, [ = ](){
        // TODO: create vault,
    });
}

VaultSetupSetPasswordPage::~VaultSetupSetPasswordPage()
{
    m_enterPassword->clear();
    m_confirmPassword->clear();
}

// --------------------------------------

DFMVaultSetupPages::DFMVaultSetupPages(QWidget *parent)
    : DFMVaultPages(parent)
{
    VaultSetupWelcomePage * welcomePage = new VaultSetupWelcomePage(this);
    VaultSetupSetPasswordPage * passwordPage = new VaultSetupSetPasswordPage(this);

    connect(welcomePage, &VaultSetupWelcomePage::requestRedirect, this, &DFMVaultSetupPages::requestRedirect);
    connect(passwordPage, &VaultSetupSetPasswordPage::requestRedirect, this, &DFMVaultSetupPages::requestRedirect);

    insertPage("welcome", welcomePage);
    insertPage("set_password", passwordPage);
}

QPair<DUrl, bool> DFMVaultSetupPages::requireRedirect(VaultController::VaultState state)
{
    switch (state) {
    case VaultController::Unlocked:
        return {VaultController::makeVaultUrl(), true};
    case VaultController::Encrypted:
//        return {VaultController::makeVaultUrl("/", "unlock"), true};
    default:
        break;
    }
    return DFMVaultPages::requireRedirect(state);
}

QString DFMVaultSetupPages::pageString(const DUrl &url)
{
    // welcome -> set_password
    if (url.path() == "/set_password") {
        return "set_password";
    }
    return "welcome";
}

DFM_END_NAMESPACE