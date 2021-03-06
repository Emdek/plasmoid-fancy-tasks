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

#include "Task.h"
#include "Applet.h"
#include "FindApplicationDialog.h"

#include <KLocale>
#include <KMessageBox>
#include <NETRootInfo>
#include <KWindowSystem>
#include <KServiceTypeTrader>

#include <ksysguard/process.h>
#include <ksysguard/processes.h>

namespace FancyTasks
{

Task::Task(AbstractGroupableItem *abstractItem, Applet *applet) : QObject(applet),
    m_applet(applet),
    m_abstractItem(NULL),
    m_taskType(OtherType),
    m_validateTimer(0)
{
    setTask(abstractItem);

    connect(this, SIGNAL(destroyed()), m_applet, SLOT(cleanup()));
}

void Task::timerEvent(QTimerEvent *event)
{
    killTimer(event->timerId());

    if (event->timerId() == m_validateTimer && !m_abstractItem)
    {
        deleteLater();
    }
}

void Task::fixMenu(QMenu *menu, Task *task)
{
    if (!menu || menu->actions().count() < 3)
    {
        return;
    }

    const bool group = (task?(task->taskType() == GroupType):false);

    menu->actions().at(menu->actions().count() - 4)->setVisible(false);

    if (menu->actions().count() > 7)
    {
        menu->actions().at(menu->actions().count() - 5)->setVisible(false);
    }

    const QString url = (task?task->launcherUrl().pathOrUrl():QString());

    if (!task || (!url.isEmpty() && m_applet->arrangement().contains(url)) || (group && m_applet->groupManager()->groupingStrategy() != TaskManager::GroupManager::ProgramGrouping))
    {
        return;
    }

    QMenu *moreMenu = menu->actions().at(qMax(0, (menu->actions().count() - 3)))->menu();

    if (!moreMenu)
    {
        return;
    }

    moreMenu->addSeparator();

    QAction *launcherAction = moreMenu->addAction(KIcon("object-locked"), i18n("Pin This Task"), this, SLOT(pinLauncher()));
    launcherAction->setData(url);
}

void Task::validate()
{
    if (!m_abstractItem)
    {
        m_validateTimer = startTimer(500);
    }
}

void Task::activate()
{
    if (m_taskType == TaskType && m_task && m_task->task())
    {
        m_task->task()->activateRaiseOrIconify();
    }
}

void Task::close()
{
    if (m_abstractItem)
    {
        m_abstractItem->close();
    }
}

void Task::kill()
{
    KSysGuard::Processes processes;
    processes.updateAllProcesses();

    if (m_taskType == TaskType && m_task)
    {
        processes.killProcess(m_task->task()->pid());
    }
    else if (m_taskType == GroupType && m_group)
    {
        QList<AbstractGroupableItem*> members = m_group->members();

        for (int i = 0; i < members.count(); ++i)
        {
            TaskItem *task = qobject_cast<TaskItem*>(members.at(i));

            if (task)
            {
                processes.killProcess(task->task()->pid());
            }
        }
    }
}

void Task::resize()
{
    if (m_abstractItem)
    {
        Task *task = qobject_cast<Task*>(m_abstractItem);

        if (task)
        {
            task->resize();
        }
    }
}

void Task::move()
{
    if (m_abstractItem)
    {
        Task *task = qobject_cast<Task*>(m_abstractItem);

        if (task)
        {
            task->move();
        }
    }
}

void Task::moveToDesktop(int desktop)
{
    if (m_abstractItem)
    {
        m_abstractItem->toDesktop((desktop < 0)?KWindowSystem::currentDesktop():desktop);
    }
}

void Task::setMinimized(int mode)
{
    if (m_abstractItem)
    {
        if (mode < 0)
        {
            mode = !m_abstractItem->isMinimized();
        }

        m_abstractItem->setMinimized(mode);
    }
}

void Task::setMaximized(int mode)
{
    if (m_abstractItem)
    {
        if (mode < 0)
        {
            mode = !m_abstractItem->isMaximized();
        }

        m_abstractItem->setMaximized(mode);
    }
}

void Task::setFullscreen(int mode)
{
    if (m_abstractItem)
    {
        if (mode < 0)
        {
            mode = !m_abstractItem->isFullScreen();
        }

        m_abstractItem->setFullScreen(mode);
    }
}

void Task::setShaded(int mode)
{
    if (m_abstractItem)
    {
        if (mode < 0)
        {
            mode = !m_abstractItem->isShaded();
        }

         m_abstractItem->setShaded(mode);
    }
}

void Task::publishIconGeometry(const QRect &geometry)
{
    if (m_task && m_task->task())
    {
        m_task->task()->publishIconGeometry(geometry);
    }
    else if (m_group)
    {
        QList<AbstractGroupableItem*> items = m_group->members();

        for (int i = 0; i < items.count(); ++i)
        {
            if (items.at(i)->itemType() == TaskManager::TaskItemType)
            {
                TaskItem *task = qobject_cast<TaskItem*>(items.at(i));
                task->task()->publishIconGeometry(geometry);
            }
        }
    }
}

void Task::dropTask(Task *task)
{
    if (!task || task->taskType() == StartupType || m_taskType == StartupType || m_applet->groupManager()->groupingStrategy() != TaskManager::GroupManager::ManualGrouping)
    {
        return;
    }

    QList<AbstractGroupableItem*> items;

    if (task->taskType() == TaskType)
    {
        items.append(task->abstractItem());
    }
    else
    {
        items.append(task->members());
    }

    if (m_taskType == TaskType)
    {
        items.append(m_abstractItem);
    }
    else
    {
        items.append(m_group->members());
    }

    m_applet->groupManager()->manualGroupingRequest(items);
}

void Task::addMimeData(QMimeData *mimeData)
{
    if (m_abstractItem)
    {
        m_abstractItem->addMimeData(mimeData);
    }
}

void Task::taskChanged(::TaskManager::TaskChanges changes)
{
    ItemChanges taskChanges;

    if (changes & TaskManager::NameChanged || changes & TaskManager::DesktopChanged)
    {
        taskChanges |= TextChanged;
    }

    if (changes & TaskManager::IconChanged)
    {
        taskChanges |= IconChanged;
    }

    if (changes & TaskManager::StateChanged)
    {
        taskChanges |= StateChanged;
    }

    if (changes & TaskManager::GeometryChanged || changes & TaskManager::WindowTypeChanged || changes & TaskManager::ActionsChanged || changes & TaskManager::TransientsChanged)
    {
        taskChanges |= WindowsChanged;
    }

    emit changed(taskChanges);
}

void Task::addItem(AbstractGroupableItem *abstractItem)
{
    if (abstractItem->itemType() != TaskManager::GroupItemType)
    {
        TaskItem *task = qobject_cast<TaskItem*>(abstractItem);

        if (task->task())
        {
            emit windowAdded(task->task()->window());
        }
    }

    emit changed(WindowsChanged);
}

void Task::removeItem(AbstractGroupableItem *abstractItem)
{
    if (m_group && m_group->members().count() == 1)
    {
        m_taskType = TaskType;
        m_abstractItem = m_group->members().first();
        m_group = NULL;
        m_task = qobject_cast<TaskItem*>(m_abstractItem);
    }

    TaskItem *task = qobject_cast<TaskItem*>(abstractItem);

    if (task && task->task())
    {
        emit windowRemoved(task->task()->window());
    }

    emit changed(EveythingChanged);
}

void Task::pinLauncher()
{
    QAction *launcherAction = qobject_cast<QAction*>(sender());

    if (launcherAction)
    {
        QString url(launcherAction->data().toString());

        if (url.isEmpty())
        {
            FindApplicationDialog dialog(m_applet, qobject_cast<QWidget*>(parent()));
            dialog.exec();

            url = dialog.url();

            if (url.isEmpty())
            {
                return;
            }

            if (m_applet->arrangement().contains(url))
            {
                KMessageBox::sorry(NULL, i18n("Launcher with URL \"%1\" already exists.").arg(url));

                return;
            }
        }

        m_applet->addLauncher(m_applet->launcherForUrl(KUrl(launcherAction->data().toString())));
    }
}

void Task::showPropertiesDialog()
{
    if (m_taskType != GroupType || !(m_applet->groupManager()->taskGrouper()->editableGroupProperties() & TaskManager::AbstractGroupingStrategy::Name))
    {
        return;
    }

    QWidget *groupWidget = new QWidget;

    m_groupUi.setupUi(groupWidget);
    m_groupUi.icon->setIcon(m_group->icon());
    m_groupUi.name->setText(m_group->name());

    KDialog *groupDialog = new KDialog;
    groupDialog->setMainWidget(groupWidget);
    groupDialog->setButtons(KDialog::Cancel | KDialog::Ok);

    connect(groupDialog, SIGNAL(okClicked()), this, SLOT(setProperties()));

    groupDialog->setWindowTitle(i18n("%1 Settings", m_group->name()));
    groupDialog->show();
}

void Task::setProperties()
{
    if (m_group)
    {
        m_group->setIcon(KIcon(m_groupUi.icon->icon()));
        m_group->setName(m_groupUi.name->text());
    }
}

void Task::setTask(AbstractGroupableItem *abstractItem)
{
    if (m_abstractItem)
    {
        disconnect(m_abstractItem, SIGNAL(destroyed()), this, SLOT(validate()));
    }

    m_abstractItem = abstractItem;
    m_command = QString();
    m_launcherUrl = KUrl();

    if (m_abstractItem)
    {
        if (m_validateTimer > 0)
        {
            killTimer(m_validateTimer);

            m_validateTimer = 0;
        }

        connect(m_abstractItem, SIGNAL(destroyed()), this, SLOT(validate()));
    }

    if (m_abstractItem->itemType() == TaskManager::GroupItemType)
    {
        m_group = qobject_cast<TaskGroup*>(abstractItem);
        m_taskType = GroupType;

        if (m_applet->groupManager()->groupingStrategy() != TaskManager::GroupManager::ManualGrouping && m_group->members().count())
        {
            TaskItem *task = qobject_cast<TaskItem*>(m_group->members().first());

            if (task && task->task())
            {
                if (m_group->name().isEmpty())
                {
                    m_group->setName(task->task()->visibleName());
                }

                if (m_applet->groupManager()->groupingStrategy() == TaskManager::GroupManager::ProgramGrouping)
                {
                    m_command = command(task->task()->pid());
                }
            }
        }

        const QList<WId> windowList = windows();

        for (int i = 0; i < windowList.count(); ++i)
        {
            emit windowAdded(windowList.at(i));
        }

        connect(m_group, SIGNAL(changed(::TaskManager::TaskChanges)), this, SLOT(taskChanged(::TaskManager::TaskChanges)));
        connect(m_group, SIGNAL(groupEditRequest()), this, SLOT(showPropertiesDialog()));
        connect(m_group, SIGNAL(itemAdded(AbstractGroupableItem*)), this, SLOT(addItem(AbstractGroupableItem*)));
        connect(m_group, SIGNAL(itemRemoved(AbstractGroupableItem*)), this, SLOT(removeItem(AbstractGroupableItem*)));
    }
    else
    {
        m_task = qobject_cast<TaskItem*>(abstractItem);
        m_taskType = (m_task->task()?TaskType:StartupType);

        if (m_taskType == TaskType)
        {
            m_command = command(m_task->task()->pid());

            emit windowAdded(windows().at(0));
        }
        else
        {
            m_command = m_task->startup()->bin();
        }

        connect(m_task, SIGNAL(changed(::TaskManager::TaskChanges)), this, SLOT(taskChanged(::TaskManager::TaskChanges)));
    }

    if (!m_command.isEmpty())
    {
        KService::List services = KServiceTypeTrader::self()->query("Application", QString("exist Exec and ('%1' ~~ Exec)").arg(m_command.simplified().split(QRegExp("(?!\\\\)\\s"), QString::SkipEmptyParts).first().split(QRegExp("(?!\\\\)\\/"), QString::SkipEmptyParts).last()));

        if (!services.isEmpty())
        {
            m_launcherUrl = services.first()->entryPath();
        }
    }

    if (m_taskType == StartupType)
    {
        connect(m_task, SIGNAL(gotTaskPointer()), this, SLOT(setTaskPointer()));
    }

    emit changed(EveythingChanged);
}

void Task::setTaskPointer()
{
    setTask(m_abstractItem);
}

KMenu* Task::contextMenu()
{
    KMenu *menu = new KMenu;
    BasicMenu *taskMenu = NULL;

    if (m_taskType == GroupType && m_group)
    {
        taskMenu = new BasicMenu(menu, m_group, m_applet->groupManager());

        for (int i = 0; i < taskMenu->actions().count(); ++i)
        {
            Task *task = NULL;

            if (!taskMenu->actions().at(i)->menu())
            {
                break;
            }

            if (i < members().count())
            {
                task = m_applet->taskForWindow(members().at(i)->winIds().toList().value(0, 0));
            }

            fixMenu(taskMenu->actions().at(i)->menu(), task);
        }

        fixMenu(taskMenu, this);
    }
    else if (m_task)
    {
        taskMenu = new BasicMenu(menu, m_task, m_applet->groupManager());

        fixMenu(taskMenu, this);
    }

    if (taskMenu)
    {
        menu->addActions(taskMenu->actions());
    }

    return menu;
}

ItemType Task::taskType() const
{
    return m_taskType;
}

AbstractGroupableItem* Task::abstractItem()
{
    return m_abstractItem;
}

KIcon Task::icon() const
{
    switch (m_taskType)
    {
        case StartupType:
            return ((m_task && m_task->startup())?KIcon(m_task->startup()->icon()):KIcon());
        case TaskType:
            return ((m_task && m_task->task())?KIcon(m_task->task()->icon()):KIcon());
        case GroupType:
            return (m_group?KIcon(m_group->icon()):KIcon());
        default:
            return KIcon();
    }

    return KIcon();
}

KUrl Task::launcherUrl() const
{
    return m_launcherUrl;
}

QString Task::title() const
{
    QString title = ((m_taskType == GroupType && m_group)?m_group->name():(m_abstractItem?m_abstractItem->name():QString()));

    if (title.isEmpty())
    {
        if (m_taskType == GroupType && m_group)
        {
            title = qobject_cast<TaskItem*>(m_group->members().at(0))->task()->visibleName();

            m_group->setName(title);
        }
        else
        {
            title = i18n("Application");
        }
    }

    return title;
}

QString Task::description() const
{
    if (m_taskType == StartupType)
    {
        return i18n("Starting application...");
    }
    else if (m_abstractItem)
    {
        return (m_abstractItem->isOnAllDesktops()?i18n("On all desktops"):i18nc("Which virtual desktop a window is currently on", "On %1", KWindowSystem::desktopName(m_abstractItem->desktop())));
    }

    return QString();
}

QString Task::command() const
{
    return m_command;
}

QString Task::command(int pid) const
{
    KSysGuard::Processes processes;
    processes.updateAllProcesses();

    KSysGuard::Process *process = processes.getProcess(pid);

    if (process)
    {
        return process->command;
    }

    return QString();
}

QList<WId> Task::windows() const
{
    return (m_abstractItem?m_abstractItem->winIds().toList():QList<WId>());
}

QList<AbstractGroupableItem*> Task::members() const
{
    QList<AbstractGroupableItem*> members;

    if (m_group)
    {
        members = m_group->members();
    }
    else if (m_abstractItem)
    {
        members.append(m_abstractItem);
    }

    return members;
}

bool Task::isActive() const
{
    return (m_abstractItem?m_abstractItem->isActive():false);
}

bool Task::isDemandingAttention() const
{
    return (m_abstractItem?m_abstractItem->demandsAttention():false);
}

}