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

#include "Task.h"

#include <QtCore/QTimer>
#include <QtCore/QVarLengthArray>

#include <KLocale>
#include <NETRootInfo>
#include <KWindowSystem>

#include <ksysguard/process.h>
#include <ksysguard/processes.h>

namespace FancyTasks
{

Task::Task(TaskManager::AbstractGroupableItem *abstractItem, TaskManager::GroupManager *groupManager) : QObject(groupManager),
    m_abstractItem(NULL),
    m_groupManager(groupManager),
    m_taskType(OtherType)
{
    setTask(abstractItem);
}

void Task::setTask(TaskManager::AbstractGroupableItem *abstractItem)
{
    m_abstractItem = abstractItem;
    m_command = QString();

    if (m_abstractItem->itemType() == TaskManager::GroupItemType)
    {
        m_group = static_cast<TaskManager::TaskGroup*>(abstractItem);
        m_taskType = GroupType;

        if (m_group->name().isEmpty() && m_group->members().count() && m_groupManager->groupingStrategy() != TaskManager::GroupManager::ManualGrouping)
        {
            TaskManager::TaskItem *task = static_cast<TaskManager::TaskItem*>(m_group->members().first());

            if (task && task->task())
            {
                m_group->setName(task->task()->visibleName());
            }
        }

        QList<WId> windowList = windows();

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
        m_task = static_cast<TaskManager::TaskItem*>(abstractItem);
        m_taskType = (m_task->task()?TaskType:StartupType);

        if (m_taskType == TaskType)
        {
            KSysGuard::Processes processes;
            processes.updateAllProcesses();

            KSysGuard::Process *process = processes.getProcess(m_task->task()->pid());

            if (process)
            {
                m_command = process->command;
            }

            emit windowAdded(windows().at(0));
        }
        else
        {
            m_command = m_task->startup()->bin();
        }

        connect(m_task, SIGNAL(changed(::TaskManager::TaskChanges)), this, SLOT(taskChanged(::TaskManager::TaskChanges)));
    }

    if (m_taskType == StartupType)
    {
        connect(m_task, SIGNAL(gotTaskPointer()), this, SLOT(setTaskPointer()));
    }
    else
    {
        QTimer::singleShot(1000, this, SLOT(publishIconGeometry()));
    }

    emit changed(EveythingChanged);
}

void Task::setTaskPointer()
{
    setTask(m_abstractItem);
}

void Task::close()
{
    m_abstractItem->close();
}

void Task::activate()
{
    if (m_taskType == TaskType && m_task->task())
    {
        m_task->task()->activateRaiseOrIconify();
    }
}

void Task::publishIconGeometry()
{
    if (m_taskType == TaskType)
    {
        emit publishGeometry(m_task);
    }
}

void Task::dropTask(Task *task)
{
    if (!task || task->taskType() == StartupType || m_taskType == StartupType || m_groupManager->groupingStrategy() != TaskManager::GroupManager::ManualGrouping)
    {
        return;
    }

    TaskManager::ItemList items;

    if (task->taskType() == TaskType)
    {
        items.append(task->abstractItem());
    }
    else
    {
        items.append(task->group()->members());
    }

    if (m_taskType == TaskType)
    {
        items.append(m_abstractItem);
    }
    else
    {
        items.append(m_group->members());
    }

    m_groupManager->manualGroupingRequest(items);
}

void Task::addMimeData(QMimeData *mimeData)
{
    if (m_abstractItem)
    {
        m_abstractItem->addMimeData(mimeData);
    }
}

void Task::showPropertiesDialog()
{
    if (m_taskType == GroupType && m_groupManager->taskGrouper()->editableGroupProperties() & TaskManager::AbstractGroupingStrategy::Name)
    {
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
}

void Task::setProperties()
{
    m_group->setIcon(KIcon(m_groupUi.icon->icon()));
    m_group->setName(m_groupUi.name->text());
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
        TaskManager::TaskItem *task = static_cast<TaskManager::TaskItem*>(abstractItem);

        if (task->task())
        {
            emit windowAdded(task->task()->window());
        }
    }

    emit changed(WindowsChanged);
}

void Task::removeItem(AbstractGroupableItem *abstractItem)
{
    if (abstractItem->itemType() == TaskManager::GroupItemType)
    {
        TaskManager::TaskItem *task = static_cast<TaskManager::TaskItem*>(abstractItem);

        if (task->task())
        {
            emit windowRemoved(task->task()->window());
        }
    }

    emit changed(WindowsChanged);
}

KMenu* Task::contextMenu()
{
    KMenu *menu = new KMenu;
    TaskManager::BasicMenu *taskMenu;

    if (m_taskType == GroupType)
    {
        taskMenu = new TaskManager::BasicMenu(menu, m_group, m_groupManager);

        for (int i = 0; i < taskMenu->actions().count(); ++i)
        {
            if (!taskMenu->actions().at(i)->menu())
            {
                break;
            }

            taskMenu->actions().at(i)->menu()->actions().at(taskMenu->actions().at(i)->menu()->actions().count() - 4)->setVisible(false);

            if (taskMenu->actions().at(i)->menu()->actions().count() > 7)
            {
                taskMenu->actions().at(i)->menu()->actions().at(taskMenu->actions().at(i)->menu()->actions().count() - 5)->setVisible(false);
            }
        }
    }
    else
    {
        taskMenu = new TaskManager::BasicMenu(menu, m_task, m_groupManager);
        taskMenu->actions().at(taskMenu->actions().count() - 4)->setVisible(false);

        if (taskMenu->actions().count() > 7)
        {
            taskMenu->actions().at(taskMenu->actions().count() - 5)->setVisible(false);
        }
    }

    menu->addActions(taskMenu->actions());

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

TaskItem* Task::task()
{
    return m_task;
}

TaskGroup* Task::group()
{
    return m_group;
}

KIcon Task::icon()
{
    switch (m_taskType)
    {
        case StartupType:
            return ((m_task && m_task->startup())?KIcon(m_task->startup()->icon()):KIcon());
        break;
        case TaskType:
            return ((m_task && m_task->task())?KIcon(m_task->task()->icon()):KIcon());
        break;
        case GroupType:
            return (m_group?KIcon(m_group->icon()):KIcon());
        break;
        default:
            return KIcon();
        break;
    }
}

QString Task::title() const
{
    QString title = ((m_taskType == GroupType && m_group)?m_group->name():(m_abstractItem?m_abstractItem->name():QString()));

    if (title.isEmpty())
    {
        if (m_taskType == GroupType)
        {
            title = static_cast<TaskManager::TaskItem*>(m_group->members().at(0))->task()->visibleName();

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
    return ((m_taskType == StartupType)?i18n("Starting application..."):(m_abstractItem->isOnAllDesktops()?i18n("On all desktops"):i18nc("Which virtual desktop a window is currently on", "On %1", KWindowSystem::desktopName(m_abstractItem->desktop()))));
}

QString Task::command() const
{
    return m_command;
}

QList<WId> Task::windows()
{
    return (m_abstractItem?m_abstractItem->winIds().toList():QList<WId>());
}

bool Task::isActive() const
{
    return (m_abstractItem?m_abstractItem->isActive():false);
}

bool Task::demandsAttention() const
{
    return (m_abstractItem?m_abstractItem->demandsAttention():false);
}

}