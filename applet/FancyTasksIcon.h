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

#ifndef FANCYTASKSICON_HEADER
#define FANCYTASKSICON_HEADER

#include "FancyTasksApplet.h"

#include <QPixmap>
#include <QPainter>
#include <QPointer>
#include <QTimeLine>
#include <QKeyEvent>
#include <QTimerEvent>
#include <QFocusEvent>
#include <QGraphicsWidget>
#include <QGraphicsLinearLayout>
#include <QGraphicsDropShadowEffect>

#include <KIcon>
#include <KFileItem>

#include <taskmanager/task.h>
#include <taskmanager/taskitem.h>
#include <taskmanager/taskactions.h>
#include <taskmanager/taskmanager.h>
#include <taskmanager/groupmanager.h>
#include <taskmanager/abstractgroupingstrategy.h>

namespace FancyTasks
{

using TaskManager::GroupPtr;
using TaskManager::TaskPtr;
using TaskManager::StartupPtr;
using TaskManager::AbstractGroupableItem;
using TaskManager::GroupManager;
using TaskManager::TaskItem;
using TaskManager::TaskGroup;

class Applet;
class Task;
class Launcher;
class Job;
class Light;

class Icon : public QGraphicsWidget
{
    Q_OBJECT

    public:
        Icon(AbstractGroupableItem *abstractItem, Launcher *launcher,Job *job, Applet *parent);
        ~Icon();

        ItemType itemType() const;
        QPointer<Task> task();
        QPointer<Launcher> launcher();
        QList<QPointer<Job> > jobs();
        QString title() const;
        QString description() const;
        QPainterPath shape() const;
        KIcon icon();
        qreal factor() const;
        bool isVisible() const;
        bool demandsAttention() const;

    public slots:
        void show();
        void hide();
        void activate();
        void updateSize();
        void updateIcon();
        void setFactor(qreal factor);
        void setSize(qreal size);
        void setThumbnail(const KFileItem &item = KFileItem(), const QPixmap thumbnail = QPixmap());
        void startAnimation(AnimationType animationType, int duration, bool repeat);
        void stopAnimation();
        void progressAnimation(int progress);
        void changeGlow(bool enable, qreal radius);
        void publishGeometry(TaskManager::TaskItem *task);
        void taskChanged(ItemChanges changes);
        void launcherChanged(ItemChanges changes);
        void jobChanged(ItemChanges changes);
        void jobDemandsAttention();
        void setTask(AbstractGroupableItem *abstractItem);
        void setLauncher(Launcher *launcher);
        void addJob(Job *job);
        void removeJob(Job *job);
        void addWindow(WId window);
        void removeWindow(WId window);
        void windowPreviewActivated(WId window, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, const QPoint &point);
        void performAction(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, Task *task = NULL);
        void performAction(IconAction action, Task *task = NULL);
        void toolTipAboutToShow();
        void toolTipHidden();
        void updateToolTip();

    protected:
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
        void focusInEvent(QFocusEvent *event);
        void focusOutEvent(QFocusEvent *event);
        void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
        void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
        void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
        void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
        void dragMoveEvent(QGraphicsSceneDragDropEvent *event);
        void dragLeaveEvent(QGraphicsSceneDragDropEvent *event);
        void dropEvent(QGraphicsSceneDragDropEvent *event);
        void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
        void mousePressEvent(QGraphicsSceneMouseEvent *event);
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
        void keyPressEvent(QKeyEvent *event);
        void contextMenuEvent(QGraphicsSceneContextMenuEvent *event);
        void timerEvent(QTimerEvent *event);

    private:
        QPointer<Applet> m_applet;
        QPointer<Task> m_task;
        QPointer<Launcher> m_launcher;
        QPointer<QGraphicsDropShadowEffect> m_glowEffect;
        QList<QPointer<Job> > m_jobs;
        QHash<WId, QPointer<Light> > m_windowLights;
        QGraphicsLinearLayout *m_layout;
        QTimeLine *m_animationTimeLine;
        QTimeLine *m_jobAnimationTimeLine;
        ItemType m_itemType;
        QPixmap m_visualizationPixmap;
        QPixmap m_thumbnailPixmap;
        AnimationType m_animationType;
        qreal m_size;
        qreal m_factor;
        qreal m_animationProgress;
        int m_jobsProgress;
        int m_jobsAnimationProgress;
        int m_dragTimer;
        int m_highlightTimer;
        bool m_menuVisible;
        bool m_demandsAttention;
        bool m_jobsRunning;
        bool m_isVisible;
        bool m_isPressed;

    signals:
        void sizeChanged(qreal size);
        void hoverMoved(QGraphicsWidget *item, qreal across);
        void hoverLeft();
};

}

#endif
