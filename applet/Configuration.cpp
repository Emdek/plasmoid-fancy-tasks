/***********************************************************************************
* Fancy Tasks: Plasmoid providing fancy visualization of tasks, launchers and jobs.
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

#include "config-fancytasks.h"

#include "Configuration.h"
#include "ActionDelegate.h"
#include "TriggerDelegate.h"
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
    m_applet(applet),
    m_editedLauncher(NULL)
{
    KConfigGroup configuration = m_applet->config();
    KMenu *addLauncherMenu = new KMenu(parent);
    QWidget *generalWidget = new QWidget;
    QWidget *appearanceWidget = new QWidget;
    QWidget *arrangementWidget = new QWidget;
    QWidget *actionsWidget = new QWidget;
    QWidget *findApplicationWidget = new QWidget;
    QAction *addLauncherApplicationAction = addLauncherMenu->addAction(KIcon("application-x-executable"), i18n("Add Application..."));
    QAction *addLauncherFromFileAction = addLauncherMenu->addAction(KIcon("inode-directory"), i18n("Add File or Directory..."));
    QAction *addMenuLauncher = addLauncherMenu->addAction(KIcon("start-here"), i18n("Add Menu"));
    KMenu *addMenuLauncherMenu = new KMenu(addLauncherMenu);

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
    m_generalUi.showOnlyCurrentActivity->setChecked(configuration.readEntry("showOnlyCurrentActivity", true));
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
    m_appearanceUi.titleLabelMode->addItem(i18n("On mouse-over"), QVariant(MouseOverLabel));
    m_appearanceUi.titleLabelMode->addItem(i18n("For active icon"), QVariant(ActiveIconLabel));
    m_appearanceUi.titleLabelMode->addItem(i18n("Always"), QVariant(AlwaysShowLabel));
    m_appearanceUi.titleLabelMode->setCurrentIndex(m_appearanceUi.titleLabelMode->findData(QVariant(configuration.readEntry("titleLabelMode", static_cast<int>(AlwaysShowLabel)))));

    m_appearanceUi.activeIconIndication->addItem(i18n("None"), QVariant(NoIndication));
    m_appearanceUi.activeIconIndication->addItem(i18n("Zoom"), QVariant(ZoomIndication));
    m_appearanceUi.activeIconIndication->addItem(i18n("Glow"), QVariant(GlowIndication));
    m_appearanceUi.activeIconIndication->addItem(i18n("Fade"), QVariant(FadeIndication));
    m_appearanceUi.activeIconIndication->setCurrentIndex(m_appearanceUi.activeIconIndication->findData(QVariant(configuration.readEntry("activeIconIndication", static_cast<int>(FadeIndication)))));

    m_appearanceUi.useThumbnails->setChecked(configuration.readEntry("useThumbnails", false));
    m_appearanceUi.customBackgroundImage->setUrl(KUrl(configuration.readEntry("customBackgroundImage", QString())));
    m_appearanceUi.customBackgroundImage->setFilter("image/svg+xml image/svg+xml-compressed");
    m_appearanceUi.moveAnimation->setCurrentIndex(moveAnimationIds.indexOf(static_cast<AnimationType>(configuration.readEntry("moveAnimation", static_cast<int>(GlowAnimation)))));
    m_appearanceUi.parabolicMoveAnimation->setChecked(configuration.readEntry("parabolicMoveAnimation", false));
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
        QListWidgetItem *item = NULL;

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

            m_rules[launcher->launcherUrl().pathOrUrl()] = qMakePair(launcher->rules(), launcher->isExcluded());
        }

        m_arrangementUi.currentActionsListWidget->addItem(item);
    }

    QMap<QPair<Qt::MouseButtons, Qt::KeyboardModifiers>, IconAction> iconActions = m_applet->iconActions();
    QMap<QPair<Qt::MouseButtons, Qt::KeyboardModifiers>, IconAction>::iterator iterator;
    int i = 0;

    m_actionsUi.actionsTableWidget->setItemDelegateForColumn(0, new ActionDelegate(this));
    m_actionsUi.actionsTableWidget->setItemDelegateForColumn(1, new TriggerDelegate(this));

    for (iterator = iconActions.begin(); iterator != iconActions.end(); ++iterator)
    {
        if (iterator.key().first == Qt::NoButton)
        {
            continue;
        }

        QStringList action;

        if (iterator.key().first & Qt::LeftButton)
        {
            action.append("left");
        }

        if (iterator.key().first & Qt::MiddleButton)
        {
            action.append("middle");
        }

        if (iterator.key().first & Qt::RightButton)
        {
            action.append("right");
        }

        if (iterator.key().second & Qt::ShiftModifier)
        {
            action.append("shift");
        }

        if (iterator.key().second & Qt::ControlModifier)
        {
            action.append("ctrl");
        }

        if (iterator.key().second & Qt::AltModifier)
        {
            action.append("alt");
        }

        QTableWidgetItem *triggerItem = new QTableWidgetItem(action.join(QChar('+')));
        triggerItem->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        QTableWidgetItem *actionItem = new QTableWidgetItem(QString::number(static_cast<int>(iterator.value())));
        actionItem->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        m_actionsUi.actionsTableWidget->setRowCount(i + 1);
        m_actionsUi.actionsTableWidget->setItem(i, 0, actionItem);
        m_actionsUi.actionsTableWidget->setItem(i, 1, triggerItem);

        ++i;
    }

    moveAnimationTypeChanged(m_appearanceUi.moveAnimation->currentIndex());

    m_appearanceUi.useThumbnails->hide();

#ifdef FANCYTASKS_HAVE_COMPOSITING
    if (KWindowSystem::compositingActive())
    {
        m_appearanceUi.useThumbnails->show();
    }
#endif

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
    connect(m_arrangementUi.editLauncherButton, SIGNAL(clicked()), this, SLOT(editLauncher()));
    connect(m_actionsUi.actionsTableWidget, SIGNAL(clicked(QModelIndex)), this, SLOT(actionClicked(QModelIndex)));
    connect(m_findApplicationUi.query, SIGNAL(textChanged(QString)), this, SLOT(findApplication(QString)));
    connect(addLauncherApplicationAction, SIGNAL(triggered()), m_findApplicationDialog, SLOT(show()));
    connect(addLauncherApplicationAction, SIGNAL(triggered()), m_findApplicationUi.query, SLOT(setFocus()));
    connect(addLauncherFromFileAction, SIGNAL(triggered()), this, SLOT(addLauncher()));
    connect(addMenuLauncherMenu, SIGNAL(aboutToShow()), this, SLOT(populateMenu()));
    connect(addMenuLauncherMenu, SIGNAL(triggered(QAction*)), this, SLOT(addMenu(QAction*)));
    connect(m_findApplicationDialog, SIGNAL(finished()), this, SLOT(closeFindApplicationDialog()));
}

Configuration::~Configuration()
{
    if (m_editedLauncher)
    {
        m_editedLauncher->deleteLater();
    }
}

void Configuration::accepted()
{
    KConfigGroup configuration = m_applet->config();
    QStringList arrangement;

    closeActionEditors();

    for (int i = 0; i < m_arrangementUi.currentActionsListWidget->count(); ++i)
    {
        QListWidgetItem *item = m_arrangementUi.currentActionsListWidget->item(i);

        if (!item->toolTip().isEmpty())
        {
            arrangement.append(item->toolTip());

            const KUrl url(item->toolTip());
            Launcher *launcher = new Launcher(url, m_applet);

            if (m_rules.contains(item->toolTip()) && !launcher->isMenu())
            {
                launcher->setRules(m_rules[item->toolTip()].first);
                launcher->setExcluded(m_rules[item->toolTip()].second);
            }

            m_applet->changeLauncher(launcher, url, true);

            launcher->deleteLater();
        }
        else if (item->text() == i18n("--- tasks area ---"))
        {
            arrangement.append("tasks");
        }
        else if (item->text() == i18n("--- jobs area ---"))
        {
            arrangement.append("jobs");
        }
        else
        {
            arrangement.append("separator");
        }
    }

    configuration.deleteGroup("Actions");

    KConfigGroup actionsConfiguration = configuration.group("Actions");

    for (int i = 0; i < m_actionsUi.actionsTableWidget->rowCount(); ++i)
    {
        QTableWidgetItem *actionItem = m_actionsUi.actionsTableWidget->item(i, 0);
        QTableWidgetItem *triggerItem = m_actionsUi.actionsTableWidget->item(i, 1);

        if (triggerItem->data(Qt::EditRole).toString().isEmpty() || actionItem->data(Qt::EditRole).toInt() == 0)
        {
            continue;
        }

        actionsConfiguration.writeEntry(triggerItem->data(Qt::EditRole).toString(), actionItem->data(Qt::EditRole).toInt());
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
    configuration.writeEntry("showOnlyCurrentActivity", m_generalUi.showOnlyCurrentActivity->isChecked());
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
    m_arrangementUi.editLauncherButton->setEnabled(row >= 0 && !m_arrangementUi.currentActionsListWidget->currentItem()->toolTip().isEmpty() && m_arrangementUi.currentActionsListWidget->currentItem()->toolTip().left(5) != "menu:");

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

        Launcher launcher(KUrl(url), m_applet);
        const int row = (m_arrangementUi.currentActionsListWidget->currentIndex().row() + 1);

        m_arrangementUi.currentActionsListWidget->model()->insertRow(row);

        const QModelIndex index = m_arrangementUi.currentActionsListWidget->model()->index(row, 0);

        m_arrangementUi.currentActionsListWidget->model()->setData(index, launcher.title(), Qt::DisplayRole);
        m_arrangementUi.currentActionsListWidget->model()->setData(index, launcher.icon(), Qt::DecorationRole);
        m_arrangementUi.currentActionsListWidget->model()->setData(index, launcher.launcherUrl().pathOrUrl(), Qt::ToolTipRole);
        m_arrangementUi.currentActionsListWidget->setCurrentRow(row);
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
        addLauncher(dialog.selectedUrl().pathOrUrl());
    }
}

void Configuration::editLauncher()
{
    if (m_editedLauncher)
    {
        m_editedLauncher->deleteLater();
        m_editedLauncher = NULL;
    }

    if (!m_arrangementUi.currentActionsListWidget->currentItem() || m_arrangementUi.currentActionsListWidget->currentItem()->toolTip().isEmpty() || m_arrangementUi.currentActionsListWidget->currentItem()->toolTip().left(5) == "menu:")
    {
        return;
    }

    const QString url = m_arrangementUi.currentActionsListWidget->currentItem()->toolTip();

    m_editedLauncher = new Launcher(KUrl(url), m_applet);

    if (m_rules.contains(url))
    {
        m_editedLauncher->setRules(m_rules[url].first);
        m_editedLauncher->setExcluded(m_rules[url].second);
    }

    connect(m_editedLauncher, SIGNAL(launcherChanged(Launcher*,KUrl)), this, SLOT(changeLauncher(Launcher*,KUrl)));

    m_editedLauncher->showPropertiesDialog();
}

void Configuration::changeLauncher(Launcher *launcher, const KUrl &oldUrl)
{
    Q_UNUSED(oldUrl)

    if (launcher != m_editedLauncher)
    {
        return;
    }

    const QString url = launcher->launcherUrl().pathOrUrl();

///FIXME check for duplicate

    m_rules[url] = qMakePair(launcher->rules(), launcher->isExcluded());
}

void Configuration::addMenu(QAction *action)
{
    if (!action->data().isNull())
    {
        addLauncher("menu:" + action->data().toString());
    }
}

void Configuration::populateMenu()
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

            connect(subMenu, SIGNAL(aboutToShow()), this, SLOT(populateMenu()));
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
                entryWidget->installEventFilter(this);
                entryWidget->setCursor(QCursor(Qt::PointingHandCursor));

                m_findApplicationUi.resultsLayout->addWidget(entryWidget);
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

void Configuration::closeActionEditors()
{
    for (int i = 0; i < m_actionsUi.actionsTableWidget->rowCount(); ++i)
    {
        m_actionsUi.actionsTableWidget->closePersistentEditor(m_actionsUi.actionsTableWidget->item(i, 0));
        m_actionsUi.actionsTableWidget->closePersistentEditor(m_actionsUi.actionsTableWidget->item(i, 1));
    }
}

void Configuration::actionClicked(const QModelIndex &index)
{
    closeActionEditors();

    m_actionsUi.actionsTableWidget->openPersistentEditor(m_actionsUi.actionsTableWidget->item(index.row(), 0));
    m_actionsUi.actionsTableWidget->openPersistentEditor(m_actionsUi.actionsTableWidget->item(index.row(), 1));
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
