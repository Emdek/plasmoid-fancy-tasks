/***********************************************************************************
* Fancy Tasks: Plasmoid providing fancy visualization of tasks, launchers and jobs.
* Copyright (C) 2009-2013 Michal Dutkiewicz aka Emdek <emdeck@gmail.com>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
***********************************************************************************/

#include "FindApplicationDialog.h"
#include "Applet.h"
#include "Launcher.h"

#include <QtGui/QLabel>
#include <QtGui/QHBoxLayout>

#include <KLocale>
#include <KServiceGroup>
#include <KServiceTypeTrader>

namespace FancyTasks
{

FindApplicationDialog::FindApplicationDialog(Applet *applet, QWidget *parent) : KDialog(parent),
    m_applet(applet)
{
    QWidget *findApplicationWidget = new QWidget;

    m_findApplicationUi.setupUi(findApplicationWidget);

    setCaption(i18n("Find Application"));
    setMainWidget(findApplicationWidget);
    setButtons(KDialog::Close);

    connect(m_findApplicationUi.query, SIGNAL(textChanged(QString)), this, SLOT(findApplication(QString)));
    connect(this, SIGNAL(finished()), m_findApplicationUi.query, SLOT(clear()));
    connect(this, SIGNAL(finished()), this, SLOT(findApplication()));
}

void FindApplicationDialog::showEvent(QShowEvent *event)
{
    KDialog::showEvent(event);

    m_findApplicationUi.query->setFocus();
}

void FindApplicationDialog::findApplication(const QString &query)
{
    for (int i = (m_findApplicationUi.resultsLayout->count() - 1); i >= 0; --i)
    {
        m_findApplicationUi.resultsLayout->takeAt(i)->widget()->deleteLater();
        m_findApplicationUi.resultsLayout->removeItem(m_findApplicationUi.resultsLayout->itemAt(i));
    }

    if (query.length() < 3)
    {
        adjustSize();

        return;
    }

    KService::List services = KServiceTypeTrader::self()->query("Application", QString("exist Exec and ( (exist Keywords and '%1' ~subin Keywords) or (exist GenericName and '%1' ~~ GenericName) or (exist Name and '%1' ~~ Name) )").arg(query));

    if (!services.isEmpty())
    {
        foreach (const KService::Ptr &service, services)
        {
            if (!service->noDisplay() && service->property("NotShowIn", QVariant::String) != "KDE")
            {
                Launcher launcher(KUrl(service->entryPath()), m_applet);
                QWidget* entryWidget = new QWidget(static_cast<QWidget*>(parent()));
                QLabel* iconLabel = new QLabel(entryWidget);
                QLabel* textLabel = new QLabel(QString("%1<br /><small>%3</small>").arg(launcher.title()).arg(launcher.description()), entryWidget);

                iconLabel->setPixmap(launcher.icon().pixmap(32, 32));

                textLabel->setFixedSize(240, 40);

                QHBoxLayout* entryWidgetLayout = new QHBoxLayout(entryWidget);
                entryWidgetLayout->addWidget(iconLabel);
                entryWidgetLayout->addWidget(textLabel);
                entryWidgetLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

                entryWidget->setToolTip(QString("<b>%1</b><br /><i>%2</i>").arg(launcher.title()).arg(launcher.description()));
                entryWidget->setLayout(entryWidgetLayout);
                entryWidget->setFixedSize(300, 40);
                entryWidget->setObjectName(service->entryPath());
                entryWidget->setCursor(QCursor(Qt::PointingHandCursor));
                entryWidget->installEventFilter(this);

                m_findApplicationUi.resultsLayout->addWidget(entryWidget);
            }
        }
    }

    adjustSize();
}

bool FindApplicationDialog::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        emit launcherClicked(object->objectName());
    }

    return QObject::eventFilter(object, event);
}

}
