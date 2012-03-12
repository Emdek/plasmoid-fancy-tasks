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

#ifndef FANCYTASKSTASK_HEADER
#define FANCYTASKSTASK_HEADER

#include "Applet.h"

#include <QtCore/QPointer>

#include <KMenu>
#include <KIcon>

#include <taskmanager/task.h>
#include <taskmanager/taskitem.h>
#include <taskmanager/taskmanager.h>
#include <taskmanager/groupmanager.h>
#include <taskmanager/abstractgroupingstrategy.h>

#include "ui_group.h"

using TaskManager::AbstractGroupableItem;
using TaskManager::GroupManager;
using TaskManager::TaskItem;
using TaskManager::TaskGroup;

namespace FancyTasks
{

class Task : public QObject
{
    Q_OBJECT

    public:
        Task(TaskManager::AbstractGroupableItem *abstractItem, TaskManager::GroupManager *groupManager);

        void dropTask(Task *task);
        void addMimeData(QMimeData *mimeData);
        void publishIconGeometry(const QRect &geometry);
        ItemType taskType() const;
        TaskGroup* group();
        KMenu* contextMenu();
        KIcon icon();
        QString title() const;
        QString description() const;
        QString command() const;
        QList<WId> windows();
        bool isActive() const;
        bool demandsAttention() const;

    public slots:
        void setTask(TaskManager::AbstractGroupableItem *abstractItem);
        void setTaskPointer();
        void close();
        void activate();
        void showPropertiesDialog();
        void setProperties();
        void taskChanged(::TaskManager::TaskChanges changes);
        void addItem(AbstractGroupableItem *abstractItem);
        void removeItem(AbstractGroupableItem *abstractItem);

    protected:
        AbstractGroupableItem* abstractItem();

    private:
        QPointer<TaskManager::AbstractGroupableItem> m_abstractItem;
        QPointer<TaskManager::TaskItem> m_task;
        QPointer<TaskManager::TaskGroup> m_group;
        TaskManager::GroupManager *m_groupManager;
        QString m_command;
        ItemType m_taskType;
        Ui::group m_groupUi;

    signals:
        void changed(ItemChanges changes);
        void windowAdded(WId window);
        void windowRemoved(WId window);

    friend class Applet;
};

}

#endif
