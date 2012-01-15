/***********************************************************************************
* Fancy Tasks: Plasmoid providing a fancy representation of your tasks and launchers.
* Copyright (C) 2009-2012 Michal Dutkiewicz aka Emdek <emdeck@gmail.com>
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

#include "Configuration.h"
#include "ActionDelegate.h"
#include "Applet.h"
#include "Launcher.h"

#include <QtGui/QLabel>
#include <QtGui/QHBoxLayout>

#include <KUrl>
#include <KIcon>
#include <KLocale>
#include <KFileDialog>
#include <KMessageBox>
#include <KServiceGroup>
#include <KServiceTypeTrader>

#include <taskmanager/taskmanager.h>
#include <taskmanager/groupmanager.h>

namespace FancyTasks
{

Configuration::Configuration(Applet *applet, KConfigDialog *parent) : QObject(parent),
    m_applet(applet)
{
    KConfigGroup configuration = m_applet->config();
    KMenu* addLauncherMenu = new KMenu(parent);
    QWidget* generalWidget = new QWidget;
    QWidget* appearanceWidget = new QWidget;
    QWidget* arrangementWidget = new QWidget;
    QWidget* actionsWidget = new QWidget;
    QWidget* findApplicationWidget = new QWidget;
    QAction* addLauncherApplicationAction = addLauncherMenu->addAction(KIcon("application-x-executable"), i18n("Add Application..."));
    QAction* addLauncherFromFileAction = addLauncherMenu->addAction(KIcon("inode-directory"), i18n("Add File or Directory..."));
    QAction* addMenuLauncher = addLauncherMenu->addAction(KIcon("start-here"), i18n("Add Menu"));
    KMenu* addMenuLauncherMenu = new KMenu(addLauncherMenu);

    addMenuLauncher->setMenu(addMenuLauncherMenu);

    QAction *action = addMenuLauncherMenu->addAction(QString());
    action->setData("/");
    action->setVisible(false);

    m_generalUi.setupUi(generalWidget);
    m_appearanceUi.setupUi(appearanceWidget);
    m_arrangementUi.setupUi(arrangementWidget);
    m_actionsUi.setupUi(actionsWidget);
    m_findApplicationUi.setupUi(findApplicationWidget);

    m_findApplicationDialog = new KDialog(parent);
    m_findApplicationDialog->setCaption(i18n("Find Application"));
    m_findApplicationDialog->setMainWidget(findApplicationWidget);
    m_findApplicationDialog->setButtons(KDialog::Close);

    m_arrangementUi.addLauncherButton->setMenu(addLauncherMenu);

    m_generalUi.groupingStrategy->addItem(i18n("Do Not Group"), QVariant(TaskManager::GroupManager::NoGrouping));
    m_generalUi.groupingStrategy->addItem(i18n("Manually"), QVariant(TaskManager::GroupManager::ManualGrouping));
    m_generalUi.groupingStrategy->addItem(i18n("By Program Name"), QVariant(TaskManager::GroupManager::ProgramGrouping));
    m_generalUi.groupingStrategy->setCurrentIndex(m_generalUi.groupingStrategy->findData(QVariant(configuration.readEntry("groupingStrategy", static_cast<int>(TaskManager::GroupManager::NoGrouping)))));

    m_generalUi.sortingStrategy->addItem(i18n("Do Not Sort"), QVariant(TaskManager::GroupManager::NoSorting));
    m_generalUi.sortingStrategy->addItem(i18n("Manually"), QVariant(TaskManager::GroupManager::ManualSorting));
    m_generalUi.sortingStrategy->addItem(i18n("Alphabetically"), QVariant(TaskManager::GroupManager::AlphaSorting));
    m_generalUi.sortingStrategy->addItem(i18n("By Desktop"), QVariant(TaskManager::GroupManager::DesktopSorting));
    m_generalUi.sortingStrategy->setCurrentIndex(m_generalUi.sortingStrategy->findData(QVariant(configuration.readEntry("sortingStrategy", static_cast<int>(TaskManager::GroupManager::ManualSorting)))));

    m_generalUi.showOnlyCurrentDesktop->setChecked(configuration.readEntry("showOnlyCurrentDesktop", false));
    m_generalUi.showOnlyCurrentScreen->setChecked(configuration.readEntry("showOnlyCurrentScreen", false));
    m_generalUi.showOnlyMinimized->setChecked(configuration.readEntry("showOnlyMinimized", false));
    m_generalUi.showOnlyTasksWithLaunchers->setChecked(configuration.readEntry("showOnlyTasksWithLaunchers", false));
    m_generalUi.connectJobsWithTasks->setChecked(configuration.readEntry("connectJobsWithTasks", false));
    m_generalUi.groupJobs->setChecked(configuration.readEntry("groupJobs", true));

    m_generalUi.jobCloseMode->addItem(i18n("Instantly"), QVariant(InstantClose));
    m_generalUi.jobCloseMode->addItem(i18n("After delay"), QVariant(DelayedClose));
    m_generalUi.jobCloseMode->addItem(i18n("Manually"), QVariant(ManualClose));
    m_generalUi.jobCloseMode->setCurrentIndex(m_generalUi.jobCloseMode->findData(QVariant(configuration.readEntry("jobCloseMode", static_cast<int>(DelayedClose)))));

    QStringList moveAnimationNames;
    moveAnimationNames << i18n("None") << i18n("Zoom") << i18n("Jump") << i18n("Spotlight") << i18n("Glow") << i18n("Fade");

    QList<AnimationType> moveAnimationIds;
    moveAnimationIds << NoAnimation << ZoomAnimation << JumpAnimation << SpotlightAnimation << GlowAnimation << FadeAnimation;

    for (int i = 0; i < moveAnimationIds.count(); ++i)
    {
        m_appearanceUi.moveAnimation->addItem(moveAnimationNames.at(i), QVariant(moveAnimationIds.at(i)));
    }

    QStringList iconAnimationNames;
    iconAnimationNames << i18n("None") << i18n("Bounce") << i18n("Zoom") << i18n("Blink") << i18n("Spotlight") << i18n("Rotate") << i18n("Glow");

    QList<AnimationType> iconAnimationIds;
    iconAnimationIds << NoAnimation << BounceAnimation << ZoomAnimation << BlinkAnimation << SpotlightAnimation << RotateAnimation << GlowAnimation;

    for (int i = 0; i < iconAnimationIds.count(); ++i)
    {
        m_appearanceUi.demandsAttentionAnimation->addItem(iconAnimationNames.at(i), QVariant(iconAnimationIds.at(i)));
        m_appearanceUi.startupAnimation->addItem(iconAnimationNames.at(i), QVariant(iconAnimationIds.at(i)));
    }

    m_appearanceUi.titleLabelMode->addItem(i18n("Never"), QVariant(NoLabel));
    m_appearanceUi.titleLabelMode->addItem(i18n("On mouse-over"), QVariant(LabelOnMouseOver));
    m_appearanceUi.titleLabelMode->addItem(i18n("For active icon"), QVariant(LabelForActiveIcon));
    m_appearanceUi.titleLabelMode->addItem(i18n("Always"), QVariant(AlwaysShowLabel));
    m_appearanceUi.titleLabelMode->setCurrentIndex(m_appearanceUi.titleLabelMode->findData(QVariant(configuration.readEntry("titleLabelMode", static_cast<int>(NoLabel)))));

    m_appearanceUi.activeIconIndication->addItem(i18n("None"), QVariant(NoIndication));
    m_appearanceUi.activeIconIndication->addItem(i18n("Zoom"), QVariant(ZoomIndication));
    m_appearanceUi.activeIconIndication->addItem(i18n("Glow"), QVariant(GlowIndication));
    m_appearanceUi.activeIconIndication->addItem(i18n("Fade"), QVariant(FadeIndication));
    m_appearanceUi.activeIconIndication->setCurrentIndex(m_appearanceUi.activeIconIndication->findData(QVariant(configuration.readEntry("activeIconIndication", static_cast<int>(FadeIndication)))));

    m_appearanceUi.useThumbnails->setChecked(configuration.readEntry("useThumbnails", false));
    m_appearanceUi.customBackgroundImage->setUrl(KUrl(configuration.readEntry("customBackgroundImage", QString())));
    m_appearanceUi.customBackgroundImage->setFilter("image/svg+xml image/svg+xml-compressed");
    m_appearanceUi.moveAnimation->setCurrentIndex(moveAnimationIds.indexOf(static_cast<AnimationType>(configuration.readEntry("moveAnimation", static_cast<int>(ZoomAnimation)))));
    m_appearanceUi.parabolicMoveAnimation->setChecked(configuration.readEntry("parabolicMoveAnimation", true));
    m_appearanceUi.demandsAttentionAnimation->setCurrentIndex(iconAnimationIds.indexOf(static_cast<AnimationType>(configuration.readEntry("demandsAttentionAnimation", static_cast<int>(BlinkAnimation)))));
    m_appearanceUi.startupAnimation->setCurrentIndex(iconAnimationIds.indexOf(static_cast<AnimationType>(configuration.readEntry("startupAnimation", static_cast<int>(BounceAnimation)))));

    m_arrangementUi.removeButton->setIcon(KIcon("go-previous"));
    m_arrangementUi.addButton->setIcon(KIcon("go-next"));
    m_arrangementUi.moveUpButton->setIcon(KIcon("go-up"));
    m_arrangementUi.moveDownButton->setIcon(KIcon("go-down"));
    m_arrangementUi.availableActionsListWidget->addItem(i18n("--- separator ---"));

    QStringList arrangement = configuration.readEntry("arrangement", QStringList("tasks"));

    if (!arrangement.contains("tasks"))
    {
        m_arrangementUi.availableActionsListWidget->addItem(i18n("--- tasks area ---"));
    }

    if (!arrangement.contains("jobs"))
    {
        m_arrangementUi.availableActionsListWidget->addItem(i18n("--- jobs area ---"));
    }

    for (int i = 0; i < arrangement.count(); ++i)
    {
        QListWidgetItem *item;

        if (arrangement.at(i) == "tasks")
        {
            item = new QListWidgetItem(i18n("--- tasks area ---"), m_arrangementUi.currentActionsListWidget);
        }
        else if (arrangement.at(i) == "jobs")
        {
            item = new QListWidgetItem(i18n("--- jobs area ---"), m_arrangementUi.currentActionsListWidget);
        }
        else if (arrangement.at(i) == "separator")
        {
            item = new QListWidgetItem(i18n("--- separator ---"), m_arrangementUi.currentActionsListWidget);
        }
        else
        {
            Launcher *launcher = m_applet->launcherForUrl(KUrl(arrangement.at(i)));

            if (!launcher)
            {
                continue;
            }

            item = new QListWidgetItem(launcher->icon(), launcher->title(), m_arrangementUi.currentActionsListWidget);
            item->setToolTip(launcher->launcherUrl().pathOrUrl());
        }

        m_arrangementUi.currentActionsListWidget->addItem(item);
    }

    QStringList actionNames;
    actionNames << i18n("Activate Item") << i18n("Activate Task") << i18n("Activate Launcher") << i18n("Show Item Menu") << i18n("Show Item Children List") << i18n("Show Item Windows") << i18n("Close Task");

    QStringList actionOptions;
    actionOptions << "activateItem" << "activateTask" << "activateLauncher" << "showItemMenu" << "showItemChildrenList" << "showItemWindows" << "closeTask";

    QStringList actionDefaults;
    actionDefaults << "left+" << QString('+') << "middle+" << QString('+') << QString('+') << "middle+shift" << "left+shift";

    m_actionsUi.actionsTableWidget->setRowCount(actionNames.count());
    m_actionsUi.actionsTableWidget->setItemDelegate(new ActionDelegate(this));

    for (int i = 0; i < actionOptions.count(); ++i)
    {
        QTableWidgetItem *descriptionItem = new QTableWidgetItem(actionNames.at(i));
        descriptionItem->setToolTip(actionNames.at(i));
        descriptionItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        QTableWidgetItem *editingItem = new QTableWidgetItem(configuration.readEntry((actionOptions.at(i) + "Action"), actionDefaults.at(i)));
        editingItem->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        m_actionsUi.actionsTableWidget->setItem(i, 0, descriptionItem);
        m_actionsUi.actionsTableWidget->setItem(i, 1, editingItem);
    }

    moveAnimationTypeChanged(m_appearanceUi.moveAnimation->currentIndex());

    parent->addPage(generalWidget, i18n("General"), "go-home");
    parent->addPage(appearanceWidget, i18n("Appearance"), "preferences-desktop-theme");
    parent->addPage(arrangementWidget, i18n("Arrangement"), "format-list-unordered");
    parent->addPage(actionsWidget, i18n("Actions"), "configure-shortcuts");

    connect(parent, SIGNAL(okClicked()), this, SLOT(accepted()));
    connect(m_appearanceUi.moveAnimation, SIGNAL(currentIndexChanged(int)), this, SLOT(moveAnimationTypeChanged(int)));
    connect(m_arrangementUi.availableActionsListWidget, SIGNAL(currentRowChanged(int)), this, SLOT(availableActionsCurrentItemChanged(int)));
    connect(m_arrangementUi.currentActionsListWidget, SIGNAL(currentRowChanged(int)), this, SLOT(currentActionsCurrentItemChanged(int)));
    connect(m_arrangementUi.removeButton, SIGNAL(clicked()), this, SLOT(removeItem()));
    connect(m_arrangementUi.addButton, SIGNAL(clicked()), this, SLOT(addItem()));
    connect(m_arrangementUi.moveUpButton, SIGNAL(clicked()), this, SLOT(moveUpItem()));
    connect(m_arrangementUi.moveDownButton, SIGNAL(clicked()), this, SLOT(moveDownItem()));
    connect(m_findApplicationUi.query, SIGNAL(textChanged(QString)), this, SLOT(findApplication(QString)));
    connect(addLauncherApplicationAction, SIGNAL(triggered()), m_findApplicationDialog, SLOT(show()));
    connect(addLauncherApplicationAction, SIGNAL(triggered()), m_findApplicationUi.query, SLOT(setFocus()));
    connect(addLauncherFromFileAction, SIGNAL(triggered()), this, SLOT(addLauncher()));
    connect(addMenuLauncherMenu, SIGNAL(aboutToShow()), this, SLOT(setServiceMenu()));
    connect(addMenuLauncherMenu, SIGNAL(triggered(QAction*)), this, SLOT(addMenu(QAction*)));
    connect(m_findApplicationDialog, SIGNAL(finished()), this, SLOT(closeFindApplicationDialog()));
}

void Configuration::accepted()
{
    KConfigGroup configuration = m_applet->config();
    QStringList arrangement;

    for (int i = 0; i < m_arrangementUi.currentActionsListWidget->count(); ++i)
    {
        if (!m_arrangementUi.currentActionsListWidget->item(i)->toolTip().isEmpty())
        {
            arrangement.append(m_arrangementUi.currentActionsListWidget->item(i)->toolTip());
        }
        else if (m_arrangementUi.currentActionsListWidget->item(i)->text() == i18n("--- tasks area ---"))
        {
            arrangement.append("tasks");
        }
        else if (m_arrangementUi.currentActionsListWidget->item(i)->text() == i18n("--- jobs area ---"))
        {
            arrangement.append("jobs");
        }
        else
        {
            arrangement.append("separator");
        }
    }

    QStringList actionOptions;
    actionOptions << "activateItem" << "activateTask" << "activateLauncher" << "showItemMenu" << "showItemChildrenList" << "showItemWindows" << "closeTask";

    for (int i = 0; i < actionOptions.count(); ++i)
    {
        configuration.writeEntry((actionOptions.at(i) + "Action"), m_actionsUi.actionsTableWidget->item(i, 1)->data(Qt::EditRole).toString());
    }

    configuration.writeEntry("moveAnimation", m_appearanceUi.moveAnimation->itemData(m_appearanceUi.moveAnimation->currentIndex()).toInt());
    configuration.writeEntry("parabolicMoveAnimation", m_appearanceUi.parabolicMoveAnimation->isChecked());
    configuration.writeEntry("demandsAttentionAnimation", m_appearanceUi.demandsAttentionAnimation->itemData(m_appearanceUi.demandsAttentionAnimation->currentIndex()).toInt());
    configuration.writeEntry("startupAnimation", m_appearanceUi.startupAnimation->itemData(m_appearanceUi.startupAnimation->currentIndex()).toInt());
    configuration.writeEntry("useThumbnails", m_appearanceUi.useThumbnails->isChecked());
    configuration.writeEntry("activeIconIndication", m_appearanceUi.activeIconIndication->itemData(m_appearanceUi.activeIconIndication->currentIndex()).toInt());
    configuration.writeEntry("titleLabelMode", m_appearanceUi.titleLabelMode->itemData(m_appearanceUi.titleLabelMode->currentIndex()).toInt());
    configuration.writeEntry("customBackgroundImage", (m_appearanceUi.customBackgroundImage->url().isValid()?m_appearanceUi.customBackgroundImage->url().path():QString()));
    configuration.writeEntry("showOnlyCurrentDesktop", m_generalUi.showOnlyCurrentDesktop->isChecked());
    configuration.writeEntry("showOnlyCurrentScreen", m_generalUi.showOnlyCurrentScreen->isChecked());
    configuration.writeEntry("showOnlyMinimized", m_generalUi.showOnlyMinimized->isChecked());
    configuration.writeEntry("showOnlyTasksWithLaunchers", m_generalUi.showOnlyTasksWithLaunchers->isChecked());
    configuration.writeEntry("connectJobsWithTasks", m_generalUi.connectJobsWithTasks->isChecked());
    configuration.writeEntry("groupJobs", m_generalUi.groupJobs->isChecked());
    configuration.writeEntry("groupingStrategy", m_generalUi.groupingStrategy->itemData(m_generalUi.groupingStrategy->currentIndex()).toInt());
    configuration.writeEntry("sortingStrategy", m_generalUi.sortingStrategy->itemData(m_generalUi.sortingStrategy->currentIndex()).toInt());
    configuration.writeEntry("jobCloseMode", m_generalUi.jobCloseMode->itemData(m_generalUi.jobCloseMode->currentIndex()).toInt());
    configuration.writeEntry("arrangement", arrangement);

    emit finished();
}

void Configuration::moveAnimationTypeChanged(int option)
{
    m_appearanceUi.parabolicMoveAnimation->setEnabled(option != 0 && m_appearanceUi.moveAnimation->itemData(option).toInt() != static_cast<int>(JumpAnimation));
}

void Configuration::availableActionsCurrentItemChanged(int row)
{
    m_arrangementUi.addButton->setEnabled(row >= 0);

    if (row >= 0)
    {
        m_arrangementUi.descriptionLabel->setText(m_arrangementUi.availableActionsListWidget->currentItem()->text());
    }
}

void Configuration::currentActionsCurrentItemChanged(int row)
{
    m_arrangementUi.removeButton->setEnabled(row >= 0);
    m_arrangementUi.moveUpButton->setEnabled(row > 0);
    m_arrangementUi.moveDownButton->setEnabled(row >= 0 && row < (m_arrangementUi.currentActionsListWidget->count() - 1));

    if (row >= 0)
    {
        m_arrangementUi.descriptionLabel->setText(m_arrangementUi.currentActionsListWidget->currentItem()->text());
    }
}

void Configuration::removeItem()
{
    if (m_arrangementUi.currentActionsListWidget->currentRow() >= 0)
    {
        QListWidgetItem *currentItem = m_arrangementUi.currentActionsListWidget->takeItem(m_arrangementUi.currentActionsListWidget->currentRow());

        if (currentItem->text() != i18n("--- separator ---"))
        {
            m_arrangementUi.availableActionsListWidget->addItem(currentItem);
        }
        else
        {
            delete currentItem;
        }

        m_arrangementUi.currentActionsListWidget->setCurrentItem(NULL);
        m_arrangementUi.availableActionsListWidget->setCurrentItem(NULL);
    }
}

void Configuration::addItem()
{
    if (m_arrangementUi.availableActionsListWidget->currentRow() >= 0)
    {
        QListWidgetItem *currentItem = m_arrangementUi.availableActionsListWidget->takeItem(m_arrangementUi.availableActionsListWidget->currentRow());

        if (currentItem->text() == i18n("--- separator ---"))
        {
            m_arrangementUi.availableActionsListWidget->insertItem(0, currentItem->clone());
        }

        m_arrangementUi.currentActionsListWidget->insertItem((m_arrangementUi.currentActionsListWidget->currentRow() + 1), currentItem);
        m_arrangementUi.currentActionsListWidget->setCurrentItem(currentItem);
        m_arrangementUi.availableActionsListWidget->setCurrentItem(NULL);
    }
}

void Configuration::moveUpItem()
{
    const int currentRow = m_arrangementUi.currentActionsListWidget->currentRow();

    if (currentRow > 0)
    {
        QListWidgetItem *currentItem = m_arrangementUi.currentActionsListWidget->takeItem(currentRow);

        m_arrangementUi.currentActionsListWidget->insertItem((currentRow - 1), currentItem);
        m_arrangementUi.currentActionsListWidget->setCurrentItem(currentItem);
    }
}

void Configuration::moveDownItem()
{
    const int currentRow = m_arrangementUi.currentActionsListWidget->currentRow();

    if (currentRow < (m_arrangementUi.currentActionsListWidget->count() - 1))
    {
        QListWidgetItem *currentItem = m_arrangementUi.currentActionsListWidget->takeItem(currentRow);

        m_arrangementUi.currentActionsListWidget->insertItem((currentRow + 1), currentItem);

        m_arrangementUi.currentActionsListWidget->setCurrentItem(currentItem);
    }
}

void Configuration::addLauncher()
{
    KFileDialog dialog(KUrl("~"), "", NULL);
    dialog.setWindowModality(Qt::NonModal);
    dialog.setMode(KFile::File | KFile::Directory);
    dialog.setOperationMode(KFileDialog::Opening);
    dialog.exec();

    if (!dialog.selectedUrl().isEmpty())
    {
        Launcher* launcher = new Launcher(dialog.selectedUrl(), m_applet);

        QListWidgetItem *item = new QListWidgetItem(launcher->icon(), launcher->title(), m_arrangementUi.currentActionsListWidget);
        item->setToolTip(launcher->launcherUrl().pathOrUrl());

        m_arrangementUi.currentActionsListWidget->insertItem((m_arrangementUi.currentActionsListWidget->currentRow() + 1), item);

        delete launcher;
    }
}

void Configuration::addLauncher(const QString &url)
{
    if (!url.isEmpty())
    {
        for (int i = 0; i < m_arrangementUi.currentActionsListWidget->count(); ++i)
        {
            if (m_arrangementUi.currentActionsListWidget->item(i)->toolTip() == url)
            {
                KMessageBox::sorry(static_cast<QWidget*>(parent()), i18n("Launcher with this URL was already added."));

                return;
            }
        }

        Launcher* launcher = new Launcher(KUrl(url), m_applet);

        QListWidgetItem *item = new QListWidgetItem(launcher->icon(), launcher->title(), m_arrangementUi.currentActionsListWidget);
        item->setToolTip(launcher->launcherUrl().pathOrUrl());

        m_arrangementUi.currentActionsListWidget->insertItem((m_arrangementUi.currentActionsListWidget->currentRow() + 1), item);

        delete launcher;
    }
}

void Configuration::addMenu(QAction *action)
{
    if (!action->data().isNull())
    {
        Launcher* launcher = new Launcher(KUrl("menu:" + action->data().toString()), m_applet);

        QListWidgetItem *item = new QListWidgetItem(launcher->icon(), launcher->title(), m_arrangementUi.currentActionsListWidget);
        item->setToolTip(launcher->launcherUrl().pathOrUrl());

        m_arrangementUi.currentActionsListWidget->insertItem((m_arrangementUi.currentActionsListWidget->currentRow() + 1), item);

        delete launcher;
    }
}

void Configuration::setServiceMenu()
{
    KMenu *menu = qobject_cast<KMenu*>(sender());

    if (menu->actions().count() > 1)
    {
        return;
    }

    KServiceGroup::Ptr rootGroup = KServiceGroup::group(menu->actions()[0]->data().toString());

    if (!rootGroup || !rootGroup->isValid() || rootGroup->noDisplay())
    {
        return;
    }

    KServiceGroup::List list = rootGroup->entries(true, true, true, true);

    QAction *action = menu->addAction(KIcon("list-add"), i18n("Add This Menu"));
    action->setData(rootGroup->relPath());

    menu->addSeparator();

    for (int i = 0; i < list.count(); ++i)
    {
        if (list.at(i)->isType(KST_KService))
        {
            const KService::Ptr service = KService::Ptr::staticCast(list.at(i));

            action = menu->addAction(KIcon(service->icon()), service->name());
            action->setEnabled(false);
        }
        else if (list.at(i)->isType(KST_KServiceGroup))
        {
            const KServiceGroup::Ptr group = KServiceGroup::Ptr::staticCast(list.at(i));

            if (group->noDisplay() || group->childCount() == 0)
            {
                continue;
            }

            KMenu *subMenu = new KMenu(menu);

            QAction *action = subMenu->addAction(QString());
            action->setData(group->relPath());
            action->setVisible(false);

            action = menu->addAction(KIcon(group->icon()), group->caption());
            action->setMenu(subMenu);

            connect(subMenu, SIGNAL(aboutToShow()), this, SLOT(setServiceMenu()));
        }
        else if (list.at(i)->isType(KST_KServiceSeparator))
        {
            menu->addSeparator();
        }
    }
}

void Configuration::findApplication(const QString &query)
{
    for (int i = (m_findApplicationUi.resultsLayout->count() - 1); i >= 0; --i)
    {
        m_findApplicationUi.resultsLayout->takeAt(i)->widget()->deleteLater();
        m_findApplicationUi.resultsLayout->removeItem(m_findApplicationUi.resultsLayout->itemAt(i));
    }

    if (query.length() < 3)
    {
        m_findApplicationDialog->adjustSize();

        return;
    }

    KService::List services = KServiceTypeTrader::self()->query("Application", QString("exist Exec and ( (exist Keywords and '%1' ~subin Keywords) or (exist GenericName and '%1' ~~ GenericName) or (exist Name and '%1' ~~ Name) )").arg(query));

    if (!services.isEmpty())
    {
        foreach (const KService::Ptr &service, services)
        {
            if (!service->noDisplay() && service->property("NotShowIn", QVariant::String) != "KDE")
            {
                Launcher* launcher = new Launcher(KUrl(service->entryPath()), m_applet);

                QWidget* entryWidget = new QWidget(static_cast<QWidget*>(parent()));
                QLabel* iconLabel = new QLabel(entryWidget);
                QLabel* textLabel = new QLabel(QString("%1<br /><small>%3</small>").arg(launcher->title()).arg(launcher->description()), entryWidget);

                iconLabel->setPixmap(launcher->icon().pixmap(32, 32));

                textLabel->setFixedSize(240, 40);

                QHBoxLayout* entryWidgetLayout = new QHBoxLayout(entryWidget);
                entryWidgetLayout->addWidget(iconLabel);
                entryWidgetLayout->addWidget(textLabel);
                entryWidgetLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

                entryWidget->setToolTip(QString("<b>%1</b><br /><i>%2</i>").arg(launcher->title()).arg(launcher->description()));
                entryWidget->setLayout(entryWidgetLayout);
                entryWidget->setFixedSize(300, 40);
                entryWidget->setObjectName(service->entryPath());
                entryWidget->installEventFilter(this);
                entryWidget->setCursor(QCursor(Qt::PointingHandCursor));

                m_findApplicationUi.resultsLayout->addWidget(entryWidget);

                delete launcher;
            }
        }
    }

    m_findApplicationDialog->adjustSize();
}

void Configuration::closeFindApplicationDialog()
{
    findApplication(QString());

    m_findApplicationUi.query->setText(QString());
}

bool Configuration::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        addLauncher(object->objectName());
    }

    return QObject::eventFilter(object, event);
}

}
