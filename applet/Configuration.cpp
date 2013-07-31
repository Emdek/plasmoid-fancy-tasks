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

#include "Configuration.h"
#include "Applet.h"
#include "ActionDelegate.h"
#include "TriggerDelegate.h"
#include "FindApplicationDialog.h"
#include "Launcher.h"

#include <KUrl>
#include <KIcon>
#include <KLocale>
#include <KFileDialog>
#include <KMessageBox>
#include <KServiceGroup>

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

    connectWidgets(generalWidget);
    connectWidgets(appearanceWidget);

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

    m_appearanceUi.customBackgroundImage->setUrl(KUrl(configuration.readEntry("customBackgroundImage", QString())));
    m_appearanceUi.customBackgroundImage->setFilter("image/svg+xml image/svg+xml-compressed");

    if (m_applet->location() != Plasma::Floating && (!m_applet->containment() || m_applet->containment()->objectName() != "FancyPanel")) {
        m_appearanceUi.customBackgroundImageLabel->hide();
        m_appearanceUi.customBackgroundImage->hide();
    }

    m_appearanceUi.moveAnimation->setCurrentIndex(moveAnimationIds.indexOf(static_cast<AnimationType>(configuration.readEntry("moveAnimation", static_cast<int>(GlowAnimation)))));
    m_appearanceUi.parabolicMoveAnimation->setChecked(configuration.readEntry("parabolicMoveAnimation", false));
    m_appearanceUi.demandsAttentionAnimation->setCurrentIndex(iconAnimationIds.indexOf(static_cast<AnimationType>(configuration.readEntry("demandsAttentionAnimation", static_cast<int>(BlinkAnimation)))));
    m_appearanceUi.startupAnimation->setCurrentIndex(iconAnimationIds.indexOf(static_cast<AnimationType>(configuration.readEntry("startupAnimation", static_cast<int>(BounceAnimation)))));

    m_arrangementUi.removeButton->setIcon(KIcon("go-previous"));
    m_arrangementUi.addButton->setIcon(KIcon("go-next"));
    m_arrangementUi.moveUpButton->setIcon(KIcon("go-up"));
    m_arrangementUi.moveDownButton->setIcon(KIcon("go-down"));

    KConfig kickoffConfiguration("kickoffrc", KConfig::NoGlobals);
    KConfigGroup favoritesGroup(&kickoffConfiguration, "Favorites");
    const QStringList currentEntries = configuration.readEntry("arrangement", QStringList("tasks"));
    QStringList availableEntries;
    availableEntries << i18n("--- separator ---") << i18n("--- tasks area ---") << i18n("--- jobs area ---") << "menu:/";
    availableEntries.append(favoritesGroup.readEntry("FavoriteURLs", QStringList()));

    for (int i = 0; i < currentEntries.count(); ++i)
    {
        QListWidgetItem *item = NULL;

        if (currentEntries.at(i) == "tasks")
        {
            item = new QListWidgetItem(i18n("--- tasks area ---"), m_arrangementUi.currentEntriesListWidget);
        }
        else if (currentEntries.at(i) == "jobs")
        {
            item = new QListWidgetItem(i18n("--- jobs area ---"), m_arrangementUi.currentEntriesListWidget);
        }
        else if (currentEntries.at(i) == "separator")
        {
            item = new QListWidgetItem(i18n("--- separator ---"), m_arrangementUi.currentEntriesListWidget);
        }
        else
        {
            if (hasEntry(currentEntries.at(i), false))
            {
                continue;
            }

            Launcher *launcher = m_applet->launcherForUrl(KUrl(currentEntries.at(i)));

            if (!launcher)
            {
                continue;
            }

            item = new QListWidgetItem(launcher->icon(), launcher->title(), m_arrangementUi.currentEntriesListWidget);
            item->setToolTip(launcher->launcherUrl().pathOrUrl());

            m_rules[launcher->launcherUrl().pathOrUrl()] = qMakePair(launcher->rules(), launcher->isExcluded());
        }

        m_arrangementUi.currentEntriesListWidget->addItem(item);
    }

    for (int i = 0; i < availableEntries.count(); ++i)
    {
        if (i > 0 && hasEntry(availableEntries.at(i), false))
        {
            continue;
        }

        QListWidgetItem *item = NULL;

        if (availableEntries.at(i).startsWith("--- "))
        {
            item = new QListWidgetItem(availableEntries.at(i), m_arrangementUi.availableEntriesListWidget);
        }
        else
        {
            Launcher *launcher = m_applet->launcherForUrl(KUrl(availableEntries.at(i)));

            if (!launcher)
            {
                continue;
            }

            item = new QListWidgetItem(launcher->icon(), launcher->title(), m_arrangementUi.availableEntriesListWidget);
            item->setToolTip(launcher->launcherUrl().pathOrUrl());

            m_rules[launcher->launcherUrl().pathOrUrl()] = qMakePair(launcher->rules(), launcher->isExcluded());
        }

        m_arrangementUi.availableEntriesListWidget->addItem(item);
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

    parent->addPage(generalWidget, i18n("General"), "go-home");
    parent->addPage(appearanceWidget, i18n("Appearance"), "preferences-desktop-theme");
    parent->addPage(arrangementWidget, i18n("Arrangement"), "format-list-unordered");
    parent->addPage(actionsWidget, i18n("Actions"), "configure-shortcuts");

    connect(parent, SIGNAL(applyClicked()), this, SLOT(save()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(save()));
    connect(m_appearanceUi.moveAnimation, SIGNAL(currentIndexChanged(int)), this, SLOT(moveAnimationTypeChanged(int)));
    connect(m_appearanceUi.customBackgroundImage, SIGNAL(textChanged(QString)), this, SLOT(modify()));
    connect(m_arrangementUi.availableEntriesListWidget, SIGNAL(currentRowChanged(int)), this, SLOT(availableEntriesCurrentItemChanged(int)));
    connect(m_arrangementUi.currentEntriesListWidget, SIGNAL(currentRowChanged(int)), this, SLOT(currentEntriesCurrentItemChanged(int)));
    connect(m_arrangementUi.removeButton, SIGNAL(clicked()), this, SLOT(removeItem()));
    connect(m_arrangementUi.addButton, SIGNAL(clicked()), this, SLOT(addItem()));
    connect(m_arrangementUi.moveUpButton, SIGNAL(clicked()), this, SLOT(moveUpItem()));
    connect(m_arrangementUi.moveDownButton, SIGNAL(clicked()), this, SLOT(moveDownItem()));
    connect(m_arrangementUi.editLauncherButton, SIGNAL(clicked()), this, SLOT(editLauncher()));
    connect(m_actionsUi.actionsTableWidget, SIGNAL(clicked(QModelIndex)), this, SLOT(actionClicked(QModelIndex)));
    connect(m_actionsUi.actionsTableWidget->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(actionSelectionChanged()));
    connect(m_actionsUi.addButton, SIGNAL(clicked()), this, SLOT(addAction()));
    connect(m_actionsUi.removeButton, SIGNAL(clicked()), this, SLOT(removeAction()));
    connect(addLauncherApplicationAction, SIGNAL(triggered()), this, SLOT(findLauncher()));
    connect(addLauncherFromFileAction, SIGNAL(triggered()), this, SLOT(addLauncher()));
    connect(addMenuLauncherMenu, SIGNAL(aboutToShow()), this, SLOT(populateMenu()));
    connect(addMenuLauncherMenu, SIGNAL(triggered(QAction*)), this, SLOT(addMenu(QAction*)));
}

Configuration::~Configuration()
{
    if (m_editedLauncher)
    {
        m_editedLauncher->deleteLater();
    }
}

void Configuration::save()
{
    KConfigGroup configuration = m_applet->config();
    QStringList arrangement;

    closeActionEditors();

    for (int i = 0; i < m_arrangementUi.currentEntriesListWidget->count(); ++i)
    {
        QListWidgetItem *item = m_arrangementUi.currentEntriesListWidget->item(i);

        if (!item->toolTip().isEmpty())
        {
            if (arrangement.contains(item->toolTip()))
            {
                continue;
            }

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

    static_cast<KConfigDialog*>(parent())->enableButtonApply(false);

    emit accepted();
}

void Configuration::modify()
{
    static_cast<KConfigDialog*>(parent())->enableButtonApply(true);
}

void Configuration::connectWidgets(QWidget *widget)
{
    QList<QAbstractButton*> buttons = widget->findChildren<QAbstractButton*>();

    for (int i = 0; i < buttons.count(); ++i)
    {
        connect(buttons.at(i), SIGNAL(toggled(bool)), this, SLOT(modify()));
    }

    QList<QComboBox*> comboBoxes = widget->findChildren<QComboBox*>();

    for (int i = 0; i < comboBoxes.count(); ++i)
    {
        connect(comboBoxes.at(i), SIGNAL(currentIndexChanged(int)), this, SLOT(modify()));
    }
}

void Configuration::moveAnimationTypeChanged(int option)
{
    m_appearanceUi.parabolicMoveAnimation->setEnabled(option != 0 && m_appearanceUi.moveAnimation->itemData(option).toInt() != static_cast<int>(JumpAnimation));
}

void Configuration::availableEntriesCurrentItemChanged(int row)
{
    m_arrangementUi.addButton->setEnabled(row >= 0);

    if (row >= 0)
    {
        m_arrangementUi.descriptionLabel->setText(m_arrangementUi.availableEntriesListWidget->currentItem()->text());
    }
}

void Configuration::currentEntriesCurrentItemChanged(int row)
{
    m_arrangementUi.removeButton->setEnabled(row >= 0);
    m_arrangementUi.moveUpButton->setEnabled(row > 0);
    m_arrangementUi.moveDownButton->setEnabled(row >= 0 && row < (m_arrangementUi.currentEntriesListWidget->count() - 1));
    m_arrangementUi.editLauncherButton->setEnabled(row >= 0 && !m_arrangementUi.currentEntriesListWidget->currentItem()->toolTip().isEmpty() && m_arrangementUi.currentEntriesListWidget->currentItem()->toolTip().left(5) != "menu:");

    if (row >= 0)
    {
        m_arrangementUi.descriptionLabel->setText(m_arrangementUi.currentEntriesListWidget->currentItem()->text());
    }
}

void Configuration::addItem()
{
    if (m_arrangementUi.availableEntriesListWidget->currentRow() < 0)
    {
        return;
    }

    QListWidgetItem *currentItem = m_arrangementUi.availableEntriesListWidget->takeItem(m_arrangementUi.availableEntriesListWidget->currentRow());

    if (currentItem->text() == i18n("--- separator ---"))
    {
        m_arrangementUi.availableEntriesListWidget->insertItem(0, currentItem->clone());
    }

    m_arrangementUi.currentEntriesListWidget->insertItem((m_arrangementUi.currentEntriesListWidget->currentRow() + 1), currentItem);
    m_arrangementUi.currentEntriesListWidget->setCurrentItem(currentItem);
    m_arrangementUi.availableEntriesListWidget->setCurrentItem(NULL);

    modify();
}

void Configuration::removeItem()
{
    if (m_arrangementUi.currentEntriesListWidget->currentRow() < 0)
    {
        return;
    }

    QListWidgetItem *currentItem = m_arrangementUi.currentEntriesListWidget->takeItem(m_arrangementUi.currentEntriesListWidget->currentRow());

    if (currentItem->text() != i18n("--- separator ---"))
    {
        m_arrangementUi.availableEntriesListWidget->addItem(currentItem);
    }
    else
    {
        delete currentItem;
    }

    modify();
}

void Configuration::moveUpItem()
{
    const int currentRow = m_arrangementUi.currentEntriesListWidget->currentRow();

    if (currentRow < 1)
    {
        return;
    }

    QListWidgetItem *currentItem = m_arrangementUi.currentEntriesListWidget->takeItem(currentRow);

    m_arrangementUi.currentEntriesListWidget->insertItem((currentRow - 1), currentItem);
    m_arrangementUi.currentEntriesListWidget->setCurrentItem(currentItem);

    modify();
}

void Configuration::moveDownItem()
{
    const int currentRow = m_arrangementUi.currentEntriesListWidget->currentRow();

    if (currentRow >= (m_arrangementUi.currentEntriesListWidget->count() - 1))
    {
        return;
    }

    QListWidgetItem *currentItem = m_arrangementUi.currentEntriesListWidget->takeItem(currentRow);

    m_arrangementUi.currentEntriesListWidget->insertItem((currentRow + 1), currentItem);

    m_arrangementUi.currentEntriesListWidget->setCurrentItem(currentItem);

    modify();
}

void Configuration::findLauncher()
{
    FindApplicationDialog dialog(m_applet, qobject_cast<QWidget*>(parent()));
    dialog.exec();

    addLauncher(dialog.url());
}

void Configuration::addLauncher(const QString &url)
{
    if (url.isEmpty() || hasEntry(url))
    {
        return;
    }

    Launcher launcher(KUrl(url), m_applet);
    const int row = (m_arrangementUi.currentEntriesListWidget->currentIndex().row() + 1);

    m_arrangementUi.currentEntriesListWidget->model()->insertRow(row);

    const QModelIndex index = m_arrangementUi.currentEntriesListWidget->model()->index(row, 0);

    m_arrangementUi.currentEntriesListWidget->model()->setData(index, launcher.title(), Qt::DisplayRole);
    m_arrangementUi.currentEntriesListWidget->model()->setData(index, launcher.icon(), Qt::DecorationRole);
    m_arrangementUi.currentEntriesListWidget->model()->setData(index, launcher.launcherUrl().pathOrUrl(), Qt::ToolTipRole);
    m_arrangementUi.currentEntriesListWidget->setCurrentRow(row);

    modify();
}

void Configuration::addLauncher()
{
    KFileDialog dialog(KUrl("~"), QString(), NULL);
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

    if (!m_arrangementUi.currentEntriesListWidget->currentItem() || m_arrangementUi.currentEntriesListWidget->currentItem()->toolTip().isEmpty() || m_arrangementUi.currentEntriesListWidget->currentItem()->toolTip().left(5) == "menu:")
    {
        return;
    }

    const QString url = m_arrangementUi.currentEntriesListWidget->currentItem()->toolTip();

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

    if (hasEntry(url))
    {
        return;
    }

    m_rules[url] = qMakePair(launcher->rules(), launcher->isExcluded());

    modify();
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

    modify();
}

void Configuration::actionSelectionChanged()
{
    m_actionsUi.removeButton->setEnabled(m_actionsUi.actionsTableWidget->selectionModel()->hasSelection());
}

void Configuration::addAction()
{
    QTableWidgetItem *triggerItem = new QTableWidgetItem();
    triggerItem->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *actionItem = new QTableWidgetItem("0");
    actionItem->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    const int rowCount = m_actionsUi.actionsTableWidget->rowCount();

    m_actionsUi.actionsTableWidget->setRowCount(rowCount + 1);
    m_actionsUi.actionsTableWidget->setItem(rowCount, 0, actionItem);
    m_actionsUi.actionsTableWidget->setItem(rowCount, 1, triggerItem);

    modify();
}

void Configuration::removeAction()
{
    m_actionsUi.actionsTableWidget->removeRow(m_actionsUi.actionsTableWidget->currentRow());

    modify();
}

bool Configuration::hasTrigger(const QString &trigger)
{
    if (trigger.isEmpty())
    {
        return false;
    }

    for (int i = 0; i < m_actionsUi.actionsTableWidget->rowCount(); ++i)
    {
        if (i != m_actionsUi.actionsTableWidget->currentItem()->row() && trigger == m_actionsUi.actionsTableWidget->item(i, 1)->data(Qt::EditRole).toString())
        {
            return true;
        }
    }

    return false;
}

bool Configuration::hasEntry(const QString &entry, bool warn)
{
    for (int i = 0; i < m_arrangementUi.currentEntriesListWidget->count(); ++i)
    {
        QListWidgetItem *item = m_arrangementUi.currentEntriesListWidget->item(i);

        if (entry == item->toolTip() || (item->toolTip().isEmpty() && entry == item->text()))
        {
            if (warn)
            {
                KMessageBox::sorry(static_cast<QWidget*>(parent()), i18n("Launcher with URL \"%1\" already exists.").arg(entry));
            }

            return true;
        }
    }

    return false;
}

}
