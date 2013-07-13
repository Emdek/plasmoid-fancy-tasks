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

#ifndef FANCYTASKSAPPLET_HEADER
#define FANCYTASKSAPPLET_HEADER

#include "Constants.h"

#include <QtCore/QHash>
#include <QtCore/QQueue>
#include <QtCore/QPointer>
#include <QtCore/QDateTime>
#include <QtCore/QMimeData>
#include <QtCore/QTimeLine>
#include <QtGui/QFocusEvent>
#include <QtGui/QGraphicsLinearLayout>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QGraphicsSceneResizeEvent>
#include <QtGui/QGraphicsSceneDragDropEvent>

#include <KUrl>
#include <KMenu>

#include <taskmanager/task.h>
#include <taskmanager/taskitem.h>
#include <taskmanager/taskmanager.h>
#include <taskmanager/groupmanager.h>

#include <Plasma/Applet>
#include <Plasma/FrameSvg>
#include <Plasma/DataEngine>
#include <Plasma/Containment>

using TaskManager::AbstractGroupableItem;
using TaskManager::GroupManager;
using TaskManager::TaskItem;
using TaskManager::TaskGroup;

namespace FancyTasks
{

class Icon;
class Task;
class Job;
class Launcher;
class DropZone;

class Applet : public Plasma::Applet
{
    Q_OBJECT

    public:
        Applet(QObject *parent, const QVariantList &args);
        ~Applet();

        void init();
        void itemDropped(Icon *icon, int index);
        void itemDragged(Icon *icon, const QPointF &position, const QMimeData *mimeData);
        KMenu* contextMenu();
        Task* taskForWindow(WId window);
        Launcher* launcherForUrl(KUrl url);
        Launcher* launcherForTask(Task *task);
        Icon* iconForMimeData(const QMimeData *mimeData);
        TaskManager::GroupManager* groupManager();
        Plasma::Svg* theme();
        QStringList arrangement() const;
        TitleLabelMode titleLabelMode() const;
        ActiveIconIndication activeIconIndication() const;
        AnimationType moveAnimation() const;
        AnimationType demandsAttentionAnimation() const;
        AnimationType startupAnimation() const;
        QMap<QPair<Qt::MouseButtons, Qt::KeyboardModifiers>, IconAction> iconActions() const;
        QList<QAction*> contextualActions();
        QPixmap lightPixmap();
        WId window() const;
        qreal initialFactor() const;
        qreal itemSize() const;
        bool parabolicMoveAnimation() const;
        bool paintReflections() const;
        static bool matchRule(const QString &expression, const QString &value, RuleMatch match);

    public slots:
        void configChanged();
        void addLauncher(Launcher *launcher, int index = -1);
        void removeLauncher(Launcher *launcher);
        void changeLauncher(Launcher *launcher, const KUrl &oldUrl, bool force = false);
        void updateLauncher(Launcher *launcher);
        void addJob(const QString &source);
        void removeJob(const QString &source, bool force = false);
        void needsVisualFocus();
        void hideDropZone();
        void requestFocus();

    protected:
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
        void createConfigurationInterface(KConfigDialog *parent);
        void constraintsEvent(Plasma::Constraints constraints);
        void resizeEvent(QGraphicsSceneResizeEvent *event);
        void dragMoveEvent(QGraphicsSceneDragDropEvent *event);
        void dragLeaveEvent(QGraphicsSceneDragDropEvent *event);
        void dropEvent(QGraphicsSceneDragDropEvent *event);
        void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
        void mousePressEvent(QGraphicsSceneMouseEvent *event);
        void wheelEvent(QGraphicsSceneWheelEvent *event);
        void focusInEvent(QFocusEvent *event);
        void focusIcon(bool next, bool activateWindow = false);
        Icon* createIcon(Task *task, Launcher *launcher, Job *job);
        bool focusNextPrevChild(bool next);

    protected slots:
        void insertItem(int index, QGraphicsLayoutItem *item);
        void checkStartup();
        void addTask(AbstractGroupableItem *abstractItem, bool force = false);
        void removeTask(AbstractGroupableItem *abstractItem);
        void changeTaskPosition(AbstractGroupableItem *abstractItem);
        void showJob();
        void cleanup();
        void reload();
        void itemHoverMoved(QGraphicsWidget *item, qreal across);
        void hoverLeft();
        void moveAnimation(int progress);
        void showMenu();
        void updateSize();
        void updateTheme();

    private:
        QGraphicsLinearLayout *m_layout;
        GroupManager *m_groupManager;
        QQueue<QPointer<Task> > m_startupsQueue;
        QQueue<QPointer<Job> > m_jobsQueue;
        QList<QGraphicsWidget*> m_visibleItems;
        QList<QPointer<Launcher> > m_launchers;
        QMap<WId, QPointer<Task> > m_tasks;
        QMap<QString, QPointer<Job> > m_jobs;
        QMap<int, QPointer<Icon> > m_icons;
        QMap<AbstractGroupableItem*, QPointer<Icon> > m_taskIcons;
        QMap<AbstractGroupableItem*, QPointer<Icon> > m_launcherTaskIcons;
        QMap<Launcher*, QPointer<Icon> > m_launcherIcons;
        QMap<Job*, QPointer<Icon> > m_jobIcons;
        QMap<QPair<Qt::MouseButtons, Qt::KeyboardModifiers>, IconAction> m_iconActions;
        QDateTime m_lastAttentionDemand;
        QPixmap m_lightPixmap;
        QSize m_size;
        Plasma::FrameSvg *m_theme;
        Plasma::FrameSvg *m_background;
        DropZone *m_dropZone;
        QAction *m_entriesAction;
        QString m_customBackgroundImage;
        QStringList m_arrangement;
        QTimeLine *m_animationTimeLine;
        TitleLabelMode m_titleLabelMode;
        CloseJobMode m_jobCloseMode;
        ActiveIconIndication m_activeIconIndication;
        AnimationType m_moveAnimation;
        AnimationType m_demandsAttentionAnimation;
        AnimationType m_startupAnimation;
        qreal m_appletMaximumWidth;
        qreal m_appletMaximumHeight;
        qreal m_initialFactor;
        qreal m_across;
        qreal m_itemSize;
        int m_activeItem;
        int m_focusedItem;
        bool m_initialized;
        bool m_parabolicMoveAnimation;
        bool m_showOnlyTasksWithLaunchers;
        bool m_connectJobsWithTasks;
        bool m_groupJobs;
        bool m_paintReflections;

    signals:
        void sizeChanged(qreal size);
        void locationChanged();
};

}

#endif
