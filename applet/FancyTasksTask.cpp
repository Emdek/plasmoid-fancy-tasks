/***********************************************************************************
* Fancy Tasks: Plasmoid providing a fancy representation of your tasks and launchers.
* Copyright (C) 2009-2011 Michal Dutkiewicz aka Emdek <emdeck@gmail.com>
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

#include "FancyTasksTask.h"

#include <QTimer>
#include <QVarLengthArray>

#include <KLocale>
#include <NETRootInfo>
#include <KWindowSystem>

namespace FancyTasks
{

Task::Task(TaskManager::AbstractGroupableItem *abstractItem, TaskManager::GroupManager *groupManager) : QObject(groupManager),
    m_abstractItem(NULL),
    m_groupManager(groupManager),
    m_taskType(TypeInvalid)
{
    setTask(abstractItem);
}

void Task::setTask(TaskManager::AbstractGroupableItem *abstractItem)
{
    m_abstractItem = abstractItem;

    if (m_abstractItem->itemType() == TaskManager::GroupItemType)
    {
        m_group = static_cast<TaskManager::TaskGroup*>(abstractItem);
        m_taskType = TypeGroup;

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
        m_taskType = (m_task->task()?TypeTask:TypeStartup);

        if (m_taskType == TypeTask)
        {
            emit windowAdded(windows().at(0));
        }

        connect(m_task, SIGNAL(changed(::TaskManager::TaskChanges)), this, SLOT(taskChanged(::TaskManager::TaskChanges)));
    }

    if (m_taskType == TypeStartup)
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
    if (m_taskType == TypeTask && m_task->task())
    {
        const bool isActive = (Applet::activeWindow() == m_task->task()->window());

        if (!isActive || m_task->task()->isIconified())
        {
            m_task->task()->activate();
        }
        else if (!isActive && !m_task->task()->isOnTop())
        {
            m_task->task()->raise();
        }
        else
        {
            m_task->task()->setIconified(true);
        }
    }
}

void Task::activateWindow()
{
    if (m_taskType == TypeTask && m_task->task())
    {
        m_task->task()->activateRaiseOrIconify();
    }
}

void Task::publishIconGeometry()
{
    if (m_taskType == TypeTask)
    {
        emit publishGeometry(m_task);
    }
}

void Task::dropItems(TaskManager::ItemList items)
{
    if (m_taskType == TypeStartup || m_groupManager->groupingStrategy() != TaskManager::GroupManager::ManualGrouping)
    {
        return;
    }

    if (m_taskType == TypeTask)
    {
        items.append(m_abstractItem);
    }
    else
    {
        items.append(m_group->members());
    }

    m_groupManager->manualGroupingRequest(items);
}

void Task::showPropertiesDialog()
{
    if (m_taskType == TypeGroup && m_groupManager->taskGrouper()->editableGroupProperties() & TaskManager::AbstractGroupingStrategy::Name)
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
    QList<QAction*> actions;

    if (m_taskType == TypeGroup)
    {
        taskMenu = new TaskManager::BasicMenu(NULL, m_group, m_groupManager, actions);
    }
    else
    {
        taskMenu = new TaskManager::BasicMenu(NULL, m_task, m_groupManager, actions);
    }

    menu->addActions(taskMenu->actions());

    connect(menu, SIGNAL(destroyed()), taskMenu, SLOT(deleteLater()));

    return menu;
}

Task::TaskType Task::taskType() const
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
        case TypeStartup:
            return ((m_task && m_task->startup())?KIcon(m_task->startup()->icon()):KIcon());
        break;
        case TypeTask:
            return ((m_task && m_task->task())?KIcon(m_task->task()->icon()):KIcon());
        break;
        case TypeGroup:
            return (m_group?KIcon(m_group->icon()):KIcon());
        break;
        default:
            return KIcon();
        break;
    }
}

QString Task::title() const
{
    QString title = ((m_taskType == TypeGroup && m_group)?m_group->name():((m_taskType == TypeTask && m_task->task())?m_task->task()->visibleName():(m_task->startup()?m_task->startup()->text():QString())));

    if (title.isEmpty())
    {
        if (m_taskType == TypeGroup)
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
    return ((m_taskType == TypeStartup)?i18n("Starting application..."):(m_abstractItem->isOnAllDesktops()?i18n("On all desktops"):i18nc("Which virtual desktop a window is currently on", "On %1", KWindowSystem::desktopName(m_abstractItem->desktop()))));
}

QList<WId> Task::windows()
{
    QList<WId> windows;

    if (m_taskType == TypeTask && m_task->task())
    {
        windows.append(m_task->task()->window());
    }
    else if (m_taskType == TypeGroup && m_group)
    {
        TaskManager::ItemList tasks = m_group->members();

        for (int i = 0; i < tasks.count(); ++i)
        {
            TaskManager::TaskItem *item = static_cast<TaskManager::TaskItem*>(tasks.at(i));

            if (item && item->task())
            {
                windows.append(item->task()->window());
            }
        }
    }

    return windows;
}

bool Task::isActive() const
{
    return m_abstractItem->isActive();
}

bool Task::demandsAttention() const
{
    return m_abstractItem->demandsAttention();
}

}
