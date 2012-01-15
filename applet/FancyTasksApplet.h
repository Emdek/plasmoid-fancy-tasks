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

#ifndef FANCYTASKSAPPLET_HEADER
#define FANCYTASKSAPPLET_HEADER

#include <QHash>
#include <QQueue>
#include <QPointer>
#include <QMimeData>
#include <QDateTime>
#include <QTimeLine>
#include <QFocusEvent>
#include <QGraphicsLinearLayout>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneResizeEvent>
#include <QGraphicsSceneDragDropEvent>

#include <KUrl>
#include <KMenu>

#include <taskmanager/task.h>
#include <taskmanager/taskitem.h>
#include <taskmanager/taskactions.h>
#include <taskmanager/taskmanager.h>
#include <taskmanager/groupmanager.h>

#include <Plasma/Applet>
#include <Plasma/FrameSvg>
#include <Plasma/DataEngine>
#include <Plasma/Containment>

using TaskManager::GroupPtr;
using TaskManager::TaskPtr;
using TaskManager::StartupPtr;
using TaskManager::AbstractGroupableItem;
using TaskManager::GroupManager;
using TaskManager::TaskItem;
using TaskManager::TaskGroup;

namespace FancyTasks
{

enum TitleLabelMode
{
    NoLabel = 0,
    LabelOnMouseOver,
    LabelForActiveIcon,
    AlwaysShowLabel
};

enum CloseJobMode
{
    InstantClose,
    DelayedClose,
    ManualClose
};

enum IconAction
{
    ActivateItem,
    ActivateTask,
    ActivateLauncher,
    ShowMenu,
    ShowChildrenList,
    ShowWindows,
    CloseTask
};

enum AnimationType
{
    NoAnimation = 0,
    ZoomAnimation,
    JumpAnimation,
    RotateAnimation,
    BounceAnimation,
    BlinkAnimation,
    GlowAnimation,
    SpotlightAnimation,
    FadeAnimation
};

enum ActiveIconIndication
{
    NoIndication = 0,
    ZoomIndication,
    GlowIndication,
    FadeIndication
};

enum ItemChange
{
    NoChanges = 0,
    TextChanged,
    IconChanged,
    WindowsChanged,
    StateChanged,
    OtherChanges,
    EveythingChanged = (TextChanged | IconChanged | WindowsChanged | StateChanged | OtherChanges)
};

enum ItemType
{
    TypeOther = 0,
    TypeLauncher,
    TypeJob,
    TypeStartup,
    TypeTask,
    TypeGroup
};

Q_DECLARE_FLAGS(ItemChanges, ItemChange)

class Icon;
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
        Launcher* launcherForUrl(KUrl url);
        Launcher* launcherForTask(TaskManager::TaskItem *task);
        Icon* iconForMimeData(const QMimeData *mimeData);
        TaskManager::GroupManager* groupManager();
        Plasma::Svg* theme();
        QStringList arrangement() const;
        TitleLabelMode titleLabelMode() const;
        ActiveIconIndication activeIconIndication() const;
        AnimationType moveAnimation() const;
        AnimationType demandsAttentionAnimation() const;
        AnimationType startupAnimation() const;
        QHash<IconAction, QPair<Qt::MouseButton, Qt::Modifier> > iconActions() const;
        QList<QAction*> contextualActions();
        QPixmap lightPixmap();
        WId window() const;
        qreal initialFactor() const;
        qreal itemSize() const;
        bool parabolicMoveAnimation() const;
        bool useThumbnails() const;
        bool paintReflections() const;
        static void setActiveWindow(WId window);
        static WId activeWindow();
        static QPixmap windowPreview(WId window, int size);

    public slots:
        void configChanged();
        void updateConfiguration();
        void insertItem(int index, QGraphicsLayoutItem *item);
        void addTask(AbstractGroupableItem *abstractItem);
        void removeTask(AbstractGroupableItem *abstractItem);
        void changeTaskPosition(AbstractGroupableItem *abstractItem);
        void addLauncher(Launcher *launcher, int index);
        void removeLauncher(Launcher *launcher);
        void addJob(const QString &source);
        void removeJob(const QString &source, bool force = false);
        void showJob();
        void cleanupRemovedStartups();
        void itemHoverMoved(QGraphicsWidget *item, qreal across);
        void hoverLeft();
        void moveAnimation(int progress);
        void focusIcon(bool next, bool activateWindow = false);
        void needsVisualFocus();
        void showMenu();
        void showDropZone(int index);
        void hideDropZone();
        void updateSize();
        void urlChanged(const KUrl &oldUrl, KUrl &newUrl);
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
        bool focusNextPrevChild(bool next);

    private:
        QGraphicsLinearLayout *m_layout;
        GroupManager *m_groupManager;
        QQueue<QPointer<Job> > m_jobsQueue;
        QList<QGraphicsWidget*> m_visibleItems;
        QList<QPointer<Launcher> > m_launchers;
        QHash<QString, QPointer<Job> > m_jobs;
        QHash<Launcher*, QPointer<Icon> > m_launcherIcons;
        QHash<Job*, QPointer<Icon> > m_jobIcons;
        QHash<AbstractGroupableItem*, QPointer<Icon> > m_taskIcons;
        QHash<AbstractGroupableItem*, QPointer<Icon> > m_launcherTaskIcons;
        QHash<Icon*, QDateTime> m_removedStartups;
        QHash<IconAction, QPair<Qt::MouseButton, Qt::Modifier> > m_iconActions;
        QDateTime m_lastAttentionDemand;
        QPixmap m_lightPixmap;
        QSize m_size;
        Plasma::FrameSvg *m_theme;
        Plasma::FrameSvg *m_background;
        DropZone *m_dropZone;
        TaskManager::GroupManager::TaskGroupingStrategy m_groupingStrategy;
        TaskManager::GroupManager::TaskSortingStrategy m_sortingStrategy;
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
        bool m_useThumbnails;
        bool m_parabolicMoveAnimation;
        bool m_showOnlyCurrentDesktop;
        bool m_showOnlyCurrentScreen;
        bool m_showOnlyMinimized;
        bool m_showOnlyTasksWithLaunchers;
        bool m_connectJobsWithTasks;
        bool m_groupJobs;
        bool m_paintReflections;
        static WId m_activeWindow;

    signals:
        void sizeChanged(qreal size);
        void locationChanged();
};

}

#endif
