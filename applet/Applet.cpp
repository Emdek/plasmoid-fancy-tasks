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

#define PI 3.141592653

#include "config-fancytasks.h"

#include "Applet.h"
#include "Icon.h"
#include "Manager.h"
#include "Task.h"
#include "Launcher.h"
#include "Job.h"
#include "Menu.h"
#include "Separator.h"
#include "DropZone.h"
#include "Configuration.h"

#include <cmath>

#include <QtGui/QPainter>
#include <QtGui/QGraphicsView>
#include <QtGui/QDesktopWidget>

#include <KRun>
#include <KIcon>
#include <KLocale>
#include <NETRootInfo>
#include <KWindowSystem>

#include <Plasma/Theme>
#include <Plasma/Corona>

#ifdef Q_WS_X11
#include <QX11Info>

#include <X11/Xlib.h>
#include <fixx11h.h>
#endif

#ifdef FANCYTASKS_HAVE_COMPOSITING
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/shape.h>
#endif

K_EXPORT_PLASMA_APPLET(fancytasks, FancyTasks::Applet)

namespace FancyTasks
{

WId Applet::m_activeWindow = 0;

Applet::Applet(QObject *parent, const QVariantList &args) : Plasma::Applet(parent, args),
    m_groupManager(new TaskManager::GroupManager(this)),
    m_size(500, 100),
    m_dropZone(new DropZone(this)),
    m_animationTimeLine(new QTimeLine(100, this)),
    m_appletMaximumHeight(100),
    m_initialFactor(0),
    m_focusedItem(-1)
{
    setObjectName("FancyTasksApplet");

    KGlobal::locale()->insertCatalog("fancytasks");

    setBackgroundHints(NoBackground);
    setHasConfigurationInterface(true);
    setAcceptDrops(true);
    setFlag(QGraphicsItem::ItemIsFocusable);

    m_animationTimeLine->setFrameRange(0, 100);
    m_animationTimeLine->setUpdateInterval(40);

    m_theme = new Plasma::FrameSvg(this);
    m_theme->setImagePath("widgets/fancytasks");
    m_theme->setEnabledBorders(Plasma::FrameSvg::AllBorders);

    m_background = m_theme;

    m_layout = new QGraphicsLinearLayout;
    m_layout->setContentsMargins(2, 2, 2, 2);
    m_layout->setSpacing(0);

    setLayout(m_layout);

    resize(100, 100);
}

Applet::~Applet()
{
    qDeleteAll(m_launcherTaskIcons);
    qDeleteAll(m_taskIcons);
    qDeleteAll(m_jobIcons);
    qDeleteAll(m_launchers);
}

void Applet::init()
{
    KConfigGroup configuration = config();

    configChanged();

    if (!configuration.hasKey("arrangement"))
    {
        KConfig kickoffConfiguration("kickoffrc", KConfig::NoGlobals);
        KConfigGroup favoritesGroup(&kickoffConfiguration, "Favorites");

        m_arrangement = favoritesGroup.readEntry("FavoriteURLs", QStringList());

        if (m_arrangement.count())
        {
            m_arrangement.append("separator");
        }

        m_arrangement.append("tasks");

        configuration.writeEntry("arrangement", m_arrangement);

        emit configNeedsSaving();
    }

    if (!m_customBackgroundImage.isEmpty() && KUrl(m_customBackgroundImage).isValid())
    {
        m_background = new Plasma::FrameSvg(this);
        m_background->setImagePath(m_customBackgroundImage);
        m_background->setEnabledBorders(Plasma::FrameSvg::AllBorders);
    }

    connect(this, SIGNAL(activate()), this, SLOT(showMenu()));
    connect(m_groupManager->rootGroup(), SIGNAL(itemAdded(AbstractGroupableItem*)), this, SLOT(addTask(AbstractGroupableItem*)));
    connect(m_groupManager->rootGroup(), SIGNAL(itemRemoved(AbstractGroupableItem*)), this, SLOT(removeTask(AbstractGroupableItem*)));
    connect(m_groupManager->rootGroup(), SIGNAL(itemPositionChanged(AbstractGroupableItem*)), this, SLOT(changeTaskPosition(AbstractGroupableItem*)));

    m_groupManager->reconnect();

    QGraphicsWidget *leftMargin = new QGraphicsWidget(this);
    leftMargin->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    QGraphicsWidget *rightMargin = new QGraphicsWidget(this);
    rightMargin->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    m_layout->addItem(leftMargin);
    m_layout->addItem(rightMargin);
    m_layout->addItem(m_dropZone);

    int index = 1;

    for (int i = 0; i < m_arrangement.count(); ++i)
    {
        if (m_arrangement.at(i) == "separator")
        {
            if (i > 0 && !m_arrangement.at(i - 1).isEmpty())
            {
                Separator *separator = new Separator(m_theme, this);
                separator->setSize(m_itemSize);

                insertItem(index, separator);

                ++index;
            }
        }
        else if (m_arrangement.at(i) != "tasks" && m_arrangement.at(i) != "jobs")
        {
            addLauncher(launcherForUrl(m_arrangement.at(i)), index);

            ++index;
        }
    }

    if (m_arrangement.contains("tasks") || m_showOnlyTasksWithLaunchers)
    {
        foreach (TaskManager::AbstractGroupableItem* abstractItem, m_groupManager->rootGroup()->members())
        {
            addTask(abstractItem);
        }
    }

    if (m_arrangement.contains("jobs"))
    {
        const QStringList jobs = dataEngine("applicationjobs")->sources();

        for (int i = 0; i < jobs.count(); ++i)
        {
            addJob(jobs.at(i));
        }

        connect(dataEngine("applicationjobs"), SIGNAL(sourceAdded(const QString)), this, SLOT(addJob(const QString)));
        connect(dataEngine("applicationjobs"), SIGNAL(sourceRemoved(const QString)), this, SLOT(removeJob(const QString)));
    }

    constraintsEvent(Plasma::LocationConstraint);

    connect(m_animationTimeLine, SIGNAL(frameChanged(int)), this, SLOT(moveAnimation(int)));
}

void Applet::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->fillRect(rect(), Qt::transparent);

    if (config().readEntry("paintBackground", true) && formFactor() != Plasma::Horizontal && formFactor() != Plasma::Vertical)
    {
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setCompositionMode(QPainter::CompositionMode_Source);

        m_background->resizeFrame(boundingRect().size());
        m_background->paintFrame(painter);
    }
}

void Applet::createConfigurationInterface(KConfigDialog *parent)
{
    Configuration *configuration = new Configuration(this, parent);

    connect(configuration, SIGNAL(finished()), this, SLOT(updateConfiguration()));
}

void Applet::constraintsEvent(Plasma::Constraints constraints)
{
    if (constraints & Plasma::LocationConstraint)
    {
        m_layout->setOrientation((location() == Plasma::LeftEdge || location() == Plasma::RightEdge)?Qt::Vertical:Qt::Horizontal);

        m_background->setElementPrefix(location());

        updateSize();

        emit locationChanged();
    }

    setBackgroundHints(NoBackground);
}

void Applet::resizeEvent(QGraphicsSceneResizeEvent *event)
{
    qreal height = 0;

    if (location() == Plasma::LeftEdge || location() == Plasma::RightEdge)
    {
        height = event->newSize().width();
    }
    else
    {
        height = event->newSize().height();

        if (location() != Plasma::TopEdge && location() != Plasma::BottomEdge)
        {
            setPos(QPointF((pos().x() + ((event->oldSize().width() - event->newSize().width()) / 2)), pos().y()));
        }
    }

    if (height < m_appletMaximumHeight || height > (m_appletMaximumHeight * 1.3))
    {
        m_appletMaximumHeight = height;

        QTimer::singleShot(100, this, SLOT(updateSize()));
    }
}

void Applet::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    if (immutability() == Plasma::Mutable && KUrl::List::canDecode(event->mimeData()))
    {
        int index = 0;

        if (location() == Plasma::LeftEdge || location() == Plasma::RightEdge)
        {
            if (event->pos().y() > (boundingRect().height() / 2))
            {
                index = (m_layout->count() - 1);
            }
        }
        else if (event->pos().x() > (boundingRect().width() / 2))
        {
            index = (m_layout->count() - 1);
        }

        if (m_dropZone->index() != index || !m_dropZone->isVisible())
        {
            m_dropZone->show(index);
        }
    }
}

void Applet::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)

    QTimer::singleShot(500, m_dropZone, SLOT(hide()));
}

void Applet::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    if (!KUrl::List::canDecode(event->mimeData()))
    {
        event->ignore();

        return;
    }

    KUrl::List droppedUrls = KUrl::List::fromMimeData(event->mimeData());

    for (int i = 0; i < droppedUrls.count(); ++i)
    {
        new KRun(droppedUrls[i], NULL);
    }
}

void Applet::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)

    hoverLeft();
}

void Applet::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)

    if (!hasFocus())
    {
        requestFocus();
    }
}

void Applet::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    int steps = (event->delta() / 120);
    const bool next = ((steps >= 0)?true:false);

    steps = abs(steps);

    for (int i = 0; i < steps; ++i)
    {
        focusIcon(next, !(event->modifiers() & Qt::ControlModifier));
    }

    event->accept();
}

void Applet::focusInEvent(QFocusEvent *event)
{
    Q_UNUSED(event)

    m_focusedItem = -1;

    int itemNumber = -1;

    for (int i = 0; i < m_visibleItems.count(); ++i)
    {
        if (m_visibleItems.at(i)->objectName() != "FancyTasksIcon")
        {
            continue;
        }

        Icon *icon = static_cast<Icon*>(m_visibleItems.at(i));

        if (!m_visibleItems.at(i) || !m_visibleItems.at(i)->isVisible())
        {
            continue;
        }

        if (icon && icon->task() && icon->task()->isActive())
        {
            m_focusedItem = itemNumber;

            break;
        }

        ++itemNumber;
    }

    focusNextPrevChild(true);
}

void Applet::configChanged()
{
    KConfigGroup configuration = config();

    m_iconActions.clear();

    QList<IconAction> actionIds;
    actionIds << ActivateItem << ActivateTask << ActivateLauncher << ShowMenu << ShowChildrenList << ShowWindows << CloseTask;

    QStringList actionOptions;
    actionOptions << "activateItem" << "activateTask" << "activateLauncher" << "showItemMenu" << "showItemChildrenList" << "showItemWindows" << "closeTask";

    QStringList actionDefaults;
    actionDefaults << "left+" << QString('+') << "middle+" << QString('+') << QString('+') << "middle+shift" << "left+shift";

    for (int i = 0; i < actionOptions.count(); ++i)
    {
        QStringList action = configuration.readEntry((actionOptions.at(i) + "Action"), actionDefaults.at(i)).split('+', QString::KeepEmptyParts);
        QPair<Qt::MouseButton, Qt::Modifier> actionPair;

        actionPair.first = Qt::NoButton;
        actionPair.second = Qt::UNICODE_ACCEL;

        if (action.count() > 0 && !action.at(0).isEmpty())
        {
            if (action.at(0) == "left")
            {
                actionPair.first = Qt::LeftButton;
            }
            else if (action.at(0) == "middle")
            {
                actionPair.first = Qt::MidButton;
            }
            else if (action.at(0) == "right")
            {
                actionPair.first = Qt::RightButton;
            }

            if (action.count() > 1)
            {
                if (action.at(1) == "ctrl")
                {
                    actionPair.second = Qt::CTRL;
                }
                else if (action.at(1) == "shift")
                {
                    actionPair.second = Qt::SHIFT;
                }
                else if (action.at(1) == "alt")
                {
                    actionPair.second = Qt::ALT;
                }
            }
        }

        m_iconActions[actionIds.at(i)] = actionPair;
    }

    m_jobCloseMode = static_cast<CloseJobMode>(configuration.readEntry("jobCloseMode", static_cast<int>(DelayedClose)));
    m_activeIconIndication = static_cast<ActiveIconIndication>(configuration.readEntry("activeIconIndication", static_cast<int>(FadeIndication)));
    m_moveAnimation = static_cast<AnimationType>(configuration.readEntry("moveAnimation", static_cast<int>(ZoomAnimation)));
    m_parabolicMoveAnimation = configuration.readEntry("parabolicMoveAnimation", true);
    m_demandsAttentionAnimation = static_cast<AnimationType>(configuration.readEntry("demandsAttentionAnimation", static_cast<int>(BlinkAnimation)));
    m_startupAnimation = static_cast<AnimationType>(configuration.readEntry("startupAnimation", static_cast<int>(BounceAnimation)));
    m_useThumbnails = configuration.readEntry("useThumbnails", false);
    m_titleLabelMode = static_cast<TitleLabelMode>(configuration.readEntry("titleLabelMode", static_cast<int>(NoLabel)));
    m_customBackgroundImage = configuration.readEntry("customBackgroundImage", QString());
    m_showOnlyCurrentDesktop = configuration.readEntry("showOnlyCurrentDesktop", false);
    m_showOnlyCurrentScreen = configuration.readEntry("showOnlyCurrentScreen", false);
    m_showOnlyMinimized = configuration.readEntry("showOnlyMinimized", false);
    m_showOnlyTasksWithLaunchers = configuration.readEntry("showOnlyTasksWithLaunchers", false);
    m_connectJobsWithTasks = configuration.readEntry("connectJobsWithTasks", false);
    m_groupJobs = configuration.readEntry("groupJobs", true);
    m_groupingStrategy = static_cast<TaskManager::GroupManager::TaskGroupingStrategy>(configuration.readEntry("groupingStrategy", static_cast<int>(TaskManager::GroupManager::NoGrouping)));
    m_sortingStrategy = static_cast<TaskManager::GroupManager::TaskSortingStrategy>(configuration.readEntry("sortingStrategy", static_cast<int>(TaskManager::GroupManager::ManualSorting)));
    m_arrangement = configuration.readEntry("arrangement", QStringList("tasks"));
    m_initialFactor = ((m_moveAnimation == ZoomAnimation)?configuration.readEntry("initialZoomLevel", 0.7):((m_moveAnimation == JumpAnimation)?0.7:0));
    m_paintReflections = configuration.readEntry("paintReflections", true);

    m_groupManager->setGroupingStrategy(m_groupingStrategy);
    m_groupManager->setSortingStrategy(m_sortingStrategy);
    m_groupManager->setShowOnlyCurrentDesktop(m_showOnlyCurrentDesktop);
    m_groupManager->setShowOnlyCurrentScreen(m_showOnlyCurrentScreen);
    m_groupManager->setShowOnlyMinimized(m_showOnlyMinimized);

    if (containment())
    {
        m_groupManager->setScreen(containment()->screen());
    }

    m_groupManager->reconnect();
}

void Applet::updateConfiguration()
{
    KConfigGroup configuration = config();
    QStringList arrangement = configuration.readEntry("arrangement", QStringList("tasks"));
    QString customBackgroundImage = configuration.readEntry("customBackgroundImage", "");
    TaskManager::GroupManager::TaskSortingStrategy sortingStrategy = static_cast<TaskManager::GroupManager::TaskSortingStrategy>(configuration.readEntry("sortingStrategy", static_cast<int>(TaskManager::GroupManager::ManualSorting)));
    const bool showOnlyTasksWithLaunchers = configuration.readEntry("showOnlyTasksWithLaunchers", false);
    const bool addTasks = ((!m_arrangement.contains("tasks") && arrangement.contains("tasks")) || m_showOnlyTasksWithLaunchers != showOnlyTasksWithLaunchers);
    const bool addJobs = (!m_arrangement.contains("jobs") && arrangement.contains("jobs"));
    const bool disconnectlauncherTaskIcons = (m_sortingStrategy == TaskManager::GroupManager::NoSorting && sortingStrategy != m_sortingStrategy);

    if (!m_customBackgroundImage.isEmpty() && (customBackgroundImage.isEmpty() || !KUrl(customBackgroundImage).isValid()))
    {
        m_background = m_theme;

        update();
    }
    else if (!customBackgroundImage.isEmpty() && KUrl(customBackgroundImage).isValid())
    {
        if (m_background == m_theme)
        {
            m_background = new Plasma::FrameSvg(this);
            m_background->setEnabledBorders(Plasma::FrameSvg::AllBorders);
        }

        m_background->setImagePath(customBackgroundImage);

        update();
    }

    if (arrangement != m_arrangement || m_showOnlyTasksWithLaunchers != showOnlyTasksWithLaunchers)
    {
        if ((m_arrangement.contains("tasks") && !arrangement.contains("tasks")) || showOnlyTasksWithLaunchers)
        {
            QHash<TaskManager::AbstractGroupableItem*, QPointer<Icon> >::iterator tasksIterator;

            for (tasksIterator = m_taskIcons.begin(); tasksIterator != m_taskIcons.end(); ++tasksIterator)
            {
                m_layout->removeItem(tasksIterator.value().data());

                tasksIterator.value().data()->deleteLater();
            }

            m_taskIcons.clear();

            QHash<AbstractGroupableItem*, QPointer<Icon> >::iterator launcherTaskIconsIterator;

            for (launcherTaskIconsIterator = m_launcherTaskIcons.begin(); launcherTaskIconsIterator != m_launcherTaskIcons.end(); ++launcherTaskIconsIterator)
            {
                if (launcherTaskIconsIterator.value())
                {
                    launcherTaskIconsIterator.value()->setTask(NULL);
                }
            }

            m_launcherTaskIcons.clear();
        }

        if (m_arrangement.contains("jobs") && !arrangement.contains("jobs"))
        {
            disconnect(dataEngine("applicationjobs"), SIGNAL(sourceAdded(const QString)), this, SLOT(addJob(const QString)));
            disconnect(dataEngine("applicationjobs"), SIGNAL(sourceRemoved(const QString)), this, SLOT(removeJob(const QString)));
        }

        m_arrangement = arrangement;

        for (int i = 0; i < m_layout->count(); ++i)
        {
            QObject *object = dynamic_cast<QObject*>(m_layout->itemAt(i)->graphicsItem());

            if (object && (object->objectName() == "FancyTasksIcon" || object->objectName() == "FancyTasksSeparator"))
            {
                if (object->objectName() == "FancyTasksIcon")
                {
                    QPointer<Icon> icon = dynamic_cast<Icon*>(m_layout->itemAt(i)->graphicsItem());

                    if (icon && icon->itemType() != TypeLauncher)
                    {
                        continue;
                    }

                    m_launcherIcons.remove(icon->launcher());
                }

                m_layout->removeItem(m_layout->itemAt(i));

                delete object;

                --i;
            }
        }

        int index = 1;

        for (int i = 0; i < arrangement.count(); ++i)
        {
            if (arrangement.at(i) == "separator")
            {
                Separator *separator = new Separator(m_theme, this);
                separator->setSize(m_itemSize);

                insertItem((index + ((m_arrangement.contains("tasks") && i >= m_arrangement.indexOf("tasks"))?m_taskIcons.count():0) + ((m_arrangement.contains("jobs") && i >= m_arrangement.indexOf("jobs"))?m_jobs.count():0)), separator);

                ++index;
            }
            else if (arrangement.at(i) != "tasks" && arrangement.at(i) != "jobs")
            {
                addLauncher(launcherForUrl(arrangement.at(i)), index);

                ++index;
            }
        }

        if (addTasks)
        {
            foreach (TaskManager::AbstractGroupableItem *abstractItem, m_groupManager->rootGroup()->members())
            {
                addTask(abstractItem);
            }
        }

        if (addJobs)
        {
            QStringList jobs = dataEngine("applicationjobs")->sources();

            for (int i = 0; i < jobs.count(); ++i)
            {
                addJob(jobs.at(i));
            }

            connect(dataEngine("applicationjobs"), SIGNAL(sourceAdded(const QString)), this, SLOT(addJob(const QString)));
            connect(dataEngine("applicationjobs"), SIGNAL(sourceRemoved(const QString)), this, SLOT(removeJob(const QString)));
        }
    }

    configChanged();

    if (disconnectlauncherTaskIcons)
    {
        QList<QPointer<Icon> > launcherTaskIcons = m_launcherTaskIcons.values();

        m_launcherTaskIcons.clear();

        for (int i = 0; i < launcherTaskIcons.count(); ++i)
        {
            if (launcherTaskIcons.at(i) && launcherTaskIcons.at(i)->task())
            {
                addTask(launcherTaskIcons.at(i)->task()->abstractItem());

                launcherTaskIcons.at(i)->setTask(NULL);
            }
        }
    }

    updateSize();

    emit configNeedsSaving();
}

void Applet::insertItem(int index, QGraphicsLayoutItem *item)
{

    if (index >= (m_layout->count() - 1))
    {
        index = (m_layout->count() - 2);
    }

    if (index < 1)
    {
        index = 1;
    }

    m_layout->insertItem(index, item);
}

void Applet::addTask(AbstractGroupableItem *abstractItem)
{
    if ((!m_arrangement.contains("tasks") && !m_showOnlyTasksWithLaunchers) || !abstractItem || m_groupManager->rootGroup()->members().indexOf(abstractItem) < 0)
    {
        return;
    }

    QPointer<Icon> icon = NULL;
    TaskManager::TaskItem *task = NULL;
    TaskManager::TaskGroup *group = NULL;

    if (abstractItem->itemType() == TaskManager::GroupItemType && m_groupManager->groupingStrategy() != TaskManager::GroupManager::ManualGrouping)
    {
        group = static_cast<TaskManager::TaskGroup*>(abstractItem);

        if (group->name().isEmpty() && group->members().count())
        {
            TaskManager::TaskItem *firstTask = static_cast<TaskManager::TaskItem*>(group->members().first());

            if (firstTask && firstTask->task())
            {
                group->setName(firstTask->task()->visibleName());
            }
        }
    }
    else if (abstractItem->itemType() != TaskManager::GroupItemType)
    {
        task = static_cast<TaskManager::TaskItem*>(abstractItem);
    }

    QString title = (task?(task->task()?task->task()->visibleName():task->startup()->text()):(group?group->name():abstractItem->name()));

    if (m_groupManager->sortingStrategy() == TaskManager::GroupManager::NoSorting || m_showOnlyTasksWithLaunchers)
    {
        if (m_groupManager->groupingStrategy() == TaskManager::GroupManager::ProgramGrouping && task && task->task())
        {
            QHash<AbstractGroupableItem*, QPointer<Icon> >::iterator launcherTaskIconsIterator;

            for (launcherTaskIconsIterator = m_launcherTaskIcons.begin(); launcherTaskIconsIterator != m_launcherTaskIcons.end(); ++launcherTaskIconsIterator)
            {
                if (launcherTaskIconsIterator.value()->task() && launcherTaskIconsIterator.value()->task()->group() && launcherTaskIconsIterator.value()->task()->group()->members().indexOf(abstractItem))
                {
                    launcherTaskIconsIterator.value()->setTask(abstractItem);

                    m_launcherTaskIcons[abstractItem] = launcherTaskIconsIterator.value();
                    m_launcherTaskIcons.remove(launcherTaskIconsIterator.key());

                    return;
                }
            }
        }

        QString className;
        WId window = ((!task || !task->task())?0:task->task()->window());

        if (window)
        {
            className = KWindowSystem::windowInfo(window, 0, NET::WM2WindowClass).windowClassName();
        }

        QHash<Launcher*, QPointer<Icon> >::iterator launcherIconsIterator;

        for (launcherIconsIterator = m_launcherIcons.begin(); launcherIconsIterator != m_launcherIcons.end(); ++launcherIconsIterator)
        {
            if (!launcherIconsIterator.key()->isExecutable() || m_launcherTaskIcons.key(launcherIconsIterator.value(), NULL) || (launcherIconsIterator.value()->task() && m_groupManager->groupingStrategy() == TaskManager::GroupManager::NoGrouping))
            {
                continue;
            }

            if ((!className.isEmpty() && (launcherIconsIterator.key()->title().contains(className, Qt::CaseInsensitive) || launcherIconsIterator.key()->executable().contains(className, Qt::CaseInsensitive))) || title.contains(launcherIconsIterator.key()->title(), Qt::CaseInsensitive) || launcherIconsIterator.key()->executable().contains(title, Qt::CaseInsensitive))
            {
                if (group && launcherIconsIterator.value()->task())
                {
                    m_launcherTaskIcons.remove(launcherIconsIterator.value()->task()->abstractItem());
                    m_launcherTaskIcons[abstractItem] = launcherIconsIterator.value();
                }
                else if (task && !launcherIconsIterator.value()->task())
                {
                    m_launcherTaskIcons[abstractItem] = launcherIconsIterator.value();
                }

                launcherIconsIterator.value()->setTask(abstractItem);

                return;
            }
        }

        if (m_showOnlyTasksWithLaunchers)
        {
            return;
        }
    }

    if (!abstractItem->name().isEmpty())
    {
        QHash<Icon*, QDateTime>::iterator removedStartupsIterator;

        for (removedStartupsIterator = m_removedStartups.begin(); removedStartupsIterator != m_removedStartups.end(); ++removedStartupsIterator)
        {
            if (title.contains(removedStartupsIterator.key()->title(), Qt::CaseInsensitive) && removedStartupsIterator.key()->itemType() == TypeStartup)
            {
                icon = removedStartupsIterator.key();
                icon->setTask(abstractItem);

                m_removedStartups.erase(removedStartupsIterator);

                break;
            }
        }
    }

    if (!icon)
    {
        int index = (((m_groupManager->sortingStrategy() == TaskManager::GroupManager::NoSorting)?m_taskIcons.count():m_groupManager->rootGroup()->members().indexOf(abstractItem)) + m_arrangement.indexOf("tasks") + 1);

        icon = new Icon(abstractItem, launcherForTask(task), NULL, this);
        icon->setSize(m_itemSize);
        icon->setFactor(m_initialFactor);

        if (m_arrangement.contains("jobs") && m_arrangement.indexOf("tasks") > m_arrangement.indexOf("jobs"))
        {
            index += m_jobs.count();
        }

        insertItem(index, icon);

        updateSize();
    }

    m_taskIcons[abstractItem] = icon;
}

void Applet::removeTask(AbstractGroupableItem *abstractItem)
{
    if (m_launcherTaskIcons.contains(abstractItem))
    {
        if (m_launcherTaskIcons[abstractItem])
        {
            m_launcherTaskIcons[abstractItem]->setTask(NULL);
        }

        m_launcherTaskIcons.remove(abstractItem);

        return;
    }

    if (!m_taskIcons.contains(abstractItem))
    {
        return;
    }

    QPointer<Icon> icon = m_taskIcons[abstractItem];

    if (icon && icon->itemType() == TypeStartup)
    {
        m_removedStartups[icon] = QDateTime::currentDateTime();

        QTimer::singleShot(2000, this, SLOT(cleanupRemovedStartups()));

        return;
    }

    m_taskIcons.remove(abstractItem);

    if (!icon)
    {
       return;
    }

    m_layout->removeItem(icon);

    delete icon;
}

void Applet::changeTaskPosition(AbstractGroupableItem *abstractItem)
{
    if (m_groupManager->sortingStrategy() == TaskManager::GroupManager::NoSorting || !m_arrangement.contains("tasks"))
    {
        return;
    }

    Icon *icon = m_taskIcons[abstractItem];

    if (!icon || !icon->task() || !icon->task()->abstractItem())
    {
        return;
    }

    int index = (m_groupManager->rootGroup()->members().indexOf(abstractItem) + m_arrangement.indexOf("tasks") + 1);

    m_layout->removeItem(icon);

    insertItem(index, icon);
}

void Applet::addLauncher(Launcher *launcher, int index)
{
    if (!launcher || m_launcherIcons.contains(launcher))
    {
        return;
    }

    if (m_arrangement.contains("tasks") && index >= m_arrangement.indexOf("tasks"))
    {
        index += m_taskIcons.count();
    }

    if (m_arrangement.contains("jobs") && index >= m_arrangement.indexOf("jobs"))
    {
        index += m_jobs.count();
    }

    Icon *icon = new Icon(NULL, launcher, NULL, this);
    icon->setSize(m_itemSize);
    icon->setFactor(m_initialFactor);

    m_launcherIcons[launcher] = icon;

    insertItem(index, icon);

    updateSize();
}

void Applet::removeLauncher(Launcher *launcher)
{
    if (!m_launcherIcons.contains(launcher))
    {
        return;
    }

    if (launcher)
    {
        m_launchers.removeAll(launcher);
    }

    Icon *icon = m_launcherIcons[launcher];

    m_launcherIcons.remove(launcher);

    if (!icon)
    {
       return;
    }

    m_layout->removeItem(icon);

    icon->deleteLater();
}

void Applet::addJob(const QString &source)
{
    if (!m_arrangement.contains("jobs") || m_jobs.contains(source))
    {
        return;
    }

    Job *job = new Job(source, this);

    m_jobsQueue.enqueue(job);
    m_jobs[source] = job;

    QTimer::singleShot(1500, this, SLOT(showJob()));
}

void Applet::removeJob(const QString &source, bool force)
{
    QPointer<Job> job = m_jobs[source];

    if (!job)
    {
        m_jobIcons.remove(m_jobs[source]);
        m_jobs.remove(source);

        return;
    }

    if (!force && !dataEngine("applicationjobs")->sources().contains(source))
    {
        job->setFinished(true);
    }

    if (job->closeOnFinish() || m_jobCloseMode == InstantClose)
    {
        if (m_jobCloseMode == DelayedClose && !force)
        {
            QTimer::singleShot(5000, job, SLOT(close()));
        }
        else if (job->state() != Job::Error)
        {
            force = true;
        }
    }

    if (!force)
    {
        return;
    }

    m_jobIcons.remove(m_jobs[source]);
    m_jobs.remove(source);

    if (!job)
    {
       return;
    }

    job->destroy();
}

void Applet::showJob()
{
    QPointer<Job> job = m_jobsQueue.dequeue();

    if (!job)
    {
        return;
    }

    if (job->state() == Job::Finished)
    {
        job->close();

        return;
    }

    QString className;

    if (m_jobCloseMode != ManualClose)
    {
        job->setCloseOnFinish(true);
    }

    if (m_connectJobsWithTasks)
    {
        QHash<TaskManager::AbstractGroupableItem*, QPointer<Icon> >::iterator tasksIterator;

        for (tasksIterator = m_taskIcons.begin(); tasksIterator != m_taskIcons.end(); ++tasksIterator)
        {
            if (!tasksIterator.value())
            {
                continue;
            }

            if (tasksIterator.key()->itemType() == TaskManager::GroupItemType && m_groupManager->groupingStrategy() != TaskManager::GroupManager::ManualGrouping)
            {
                TaskManager::TaskGroup *group = static_cast<TaskManager::TaskGroup*>(tasksIterator.key());

                if (!group || !group->members().count())
                {
                    continue;
                }

                if (group->name().isEmpty())
                {
                    className = KWindowSystem::windowInfo(static_cast<TaskManager::TaskItem*>(group->members().at(0))->task()->window(), 0, NET::WM2WindowClass).windowClassName();
                }
            }
            else if (tasksIterator.key()->itemType() != TaskManager::GroupItemType)
            {
                className = KWindowSystem::windowInfo(static_cast<TaskManager::TaskItem*>(tasksIterator.key())->task()->window(), 0, NET::WM2WindowClass).windowClassName();
            }

            if (job->application().contains(className, Qt::CaseInsensitive))
            {
                tasksIterator.value()->addJob(job);

                return;
            }
        }
    }

    if (m_groupJobs)
    {
        QHash<Job*, QPointer<Icon> >::iterator jobsIterator;

        for (jobsIterator = m_jobIcons.begin(); jobsIterator != m_jobIcons.end(); ++jobsIterator)
        {
            if (!jobsIterator.value())
            {
                continue;
            }

            if (job->application() == jobsIterator.key()->application())
            {
                jobsIterator.value()->addJob(job);

                return;
            }
        }

    }

    Icon *icon = new Icon(NULL, NULL, job, this);
    icon->setSize(m_itemSize);
    icon->setFactor(m_initialFactor);

    int index = (m_arrangement.indexOf("jobs") + m_jobs.count() - 1);

    if (m_arrangement.contains("tasks") && m_arrangement.indexOf("jobs") > m_arrangement.indexOf("tasks"))
    {
        index += m_taskIcons.count();
    }

    insertItem(index, icon);

    updateSize();

    m_jobIcons[job] = icon;
}

void Applet::cleanupRemovedStartups()
{
    QMutableHashIterator<Icon*, QDateTime> iterator(m_removedStartups);

    while (iterator.hasNext())
    {
        iterator.next();

        if (iterator.value().secsTo(QDateTime::currentDateTime()) > 1 && iterator.key()->itemType() == TypeStartup)
        {
            iterator.key()->deleteLater();

            iterator.remove();
        }
    }
}

void Applet::itemHoverMoved(QGraphicsWidget *item, qreal across)
{
    m_activeItem = m_visibleItems.indexOf(item);

    m_across = across;

    moveAnimation(100);
}

void Applet::hoverLeft()
{
    m_animationTimeLine->stop();

    m_activeItem = -1;

    m_animationTimeLine->start();
}

void Applet::moveAnimation(int progress)
{
    qreal factor;
    qreal animationProgres = ((qreal) progress / 100);

    for (int i = 0; i < m_visibleItems.count(); ++i)
    {
        if (m_visibleItems.at(i)->objectName() != "FancyTasksIcon")
        {
            continue;
        }

        Icon *icon = static_cast<Icon*>(m_visibleItems.at(i));

        if (!icon || !icon->isVisible())
        {
            continue;
        }

        if (i == m_activeItem)
        {
            factor = 1;
        }
        else if (!m_parabolicMoveAnimation || m_activeItem < 0 || i < (m_activeItem - 3) || i > (m_activeItem + 3))
        {
            factor = m_initialFactor;
        }
        else if (i < m_activeItem)
        {
            factor = (m_initialFactor + ((1 - m_initialFactor) * (cos(((i - m_activeItem - m_across + 1) / 3) * PI) + 1)) / 2);
        }
        else
        {
            factor = (m_initialFactor + ((1 - m_initialFactor) * (cos(((i - m_activeItem - m_across) / 3) * PI) + 1)) / 2);
        }

        if (icon->factor() != factor)
        {
            icon->setFactor(icon->factor() + ((factor - icon->factor()) * animationProgres));
        }
    }
}

void Applet::needsVisualFocus()
{
    m_lastAttentionDemand = QDateTime::currentDateTime();

    emit activate();
}

void Applet::focusIcon(bool next, bool activateWindow)
{
    Icon *firstIcon = NULL;
    Icon *currentIcon = NULL;
    int currentItem = -1;
    bool found = false;

    if (next)
    {
        m_focusedItem = ((m_focusedItem < 0)?0:(m_focusedItem + 1));
    }
    else
    {
        --m_focusedItem;

        if (m_focusedItem < 0)
        {
            m_focusedItem = m_layout->count();
        }
    }

    for (int i = 0; i < m_visibleItems.count(); ++i)
    {
        if (m_visibleItems.at(i)->objectName() == "FancyTasksIcon")
        {
            currentIcon = static_cast<Icon*>(m_visibleItems.at(i));

            if (!currentIcon || !currentIcon->isVisible())
            {
                continue;
            }

            if (!firstIcon)
            {
                firstIcon = currentIcon;
            }

            ++currentItem;

            if (currentItem == m_focusedItem)
            {
                found = true;

                break;
            }
        }
    }

    if (!found)
    {
        if (next)
        {
            currentItem = 0;

            if (firstIcon)
            {
                currentIcon = firstIcon;

                found = true;
            }
        }
        else if (currentIcon)
        {
            found = true;
        }
    }

    m_focusedItem = currentItem;

    if (found && currentIcon)
    {
        currentIcon->setFocus();

        if (activateWindow && currentIcon->task())
        {
            currentIcon->task()->activateWindow();
        }
    }
}

void Applet::showMenu()
{
    KMenu* menu = contextMenu();

    if (menu->actions().count())
    {
        menu->exec(QCursor::pos());
    }

    delete menu;
}

void Applet::showDropZone(int index)
{
    if (m_dropZone->index() != index)
    {
        m_layout->removeItem(m_dropZone);
        m_layout->insertItem(index, m_dropZone);

        m_dropZone->show(index);
    }
}

void Applet::hideDropZone()
{
    if (!m_dropZone->isUnderMouse())
    {
        m_dropZone->hide(true);

        m_layout->removeItem(m_dropZone);
        m_layout->insertItem((m_layout->count() - 1), m_dropZone);
    }
}

void Applet::updateSize()
{
    QList<QGraphicsWidget*> items;
    QPointer<Separator> lastSeparator = NULL;
    QSize size;
    int separatorsGap = -1;
    int iconNumber = 0;

    m_itemSize = m_appletMaximumHeight;

    if (location() == Plasma::Floating || (containment() && containment()->objectName() == "FancyPanel"))
    {
        m_appletMaximumWidth = (m_background->marginSize(Plasma::LeftMargin) + m_background->marginSize(Plasma::RightMargin));
    }
    else
    {
        m_appletMaximumWidth = 0;
    }

    for (int i = 0; i < m_layout->count(); ++i)
    {
        QObject *object = dynamic_cast<QObject*>(m_layout->itemAt(i));

        if (!object)
        {
            continue;
        }

        if (object->objectName() == "FancyTasksDropZone")
        {
            if (m_dropZone->isVisible())
            {
                m_appletMaximumWidth += (m_itemSize / 2);
            }

            continue;
        }

        if (object->objectName() == "FancyTasksSeparator")
        {
            Separator *separator = dynamic_cast<Separator*>(m_layout->itemAt(i)->graphicsItem());

            if (!separator)
            {
                continue;
            }

            items.append(separator);

            lastSeparator = separator;

            if (separatorsGap == 0)
            {
                if (separator->isVisible())
                {
                    separator->hide();
                }
            }
            else
            {
                if (!separator->isVisible())
                {
                    separator->show();
                }

                m_appletMaximumWidth += (m_itemSize / 4);

                separatorsGap = 0;
            }

            continue;
        }

        if (object->objectName() == "FancyTasksIcon")
        {
            Icon *icon = static_cast<Icon*>(m_layout->itemAt(i)->graphicsItem());

            if (!icon || !icon->isVisible())
            {
                continue;
            }

            items.append(icon);

            if (separatorsGap >= 0)
            {
                ++separatorsGap;
            }

            if (m_moveAnimation != JumpAnimation && (m_moveAnimation != ZoomAnimation || (!m_parabolicMoveAnimation && iconNumber == 0) || (m_parabolicMoveAnimation && iconNumber < 6)))
            {
                m_appletMaximumWidth += m_itemSize;
            }
            else
            {
                m_appletMaximumWidth += (m_initialFactor * m_itemSize);
            }

            ++iconNumber;
        }
    }

    if (!separatorsGap && lastSeparator && lastSeparator->isVisible())
    {
        lastSeparator->hide();
    }

    m_visibleItems = items;

    m_appletMaximumWidth *= 0.9;

    if (location() == Plasma::LeftEdge || location() == Plasma::RightEdge)
    {
        size = QSize(m_appletMaximumHeight, m_appletMaximumWidth);

        setPreferredHeight(m_appletMaximumWidth);

        setMinimumHeight(m_appletMaximumWidth);
    }
    else
    {
        size = QSize(m_appletMaximumWidth, m_appletMaximumHeight);

        setPreferredWidth(m_appletMaximumWidth);

        setMinimumWidth(m_appletMaximumWidth);
    }

    if (size == m_size)
    {
        return;
    }

    m_size = size;

    m_lightPixmap = QPixmap(m_theme->elementSize("task"));
    m_lightPixmap.fill(Qt::transparent);

    QPainter pixmapPainter(&m_lightPixmap);
    pixmapPainter.setRenderHints(QPainter::SmoothPixmapTransform);

    m_theme->paint(&pixmapPainter, m_lightPixmap.rect(), "task");

    pixmapPainter.end();

    if (location() != Plasma::Floating)
    {
        emit sizeHintChanged(Qt::PreferredSize);
        emit sizeHintChanged(Qt::MinimumSize);
    }

    resize(size);

    update();

    emit sizeChanged(m_itemSize);
}

void Applet::urlChanged(const KUrl &oldUrl, KUrl &newUrl)
{
    KConfigGroup configuration = config();

    if (newUrl.url().isEmpty())
    {
        if (m_arrangement.contains(oldUrl.url()))
        {
            m_arrangement.removeAll(oldUrl.url());

            removeLauncher(launcherForUrl(oldUrl));
        }
    }
    else if (oldUrl.url().isEmpty())
    {
        if (!m_arrangement.contains(newUrl.url()))
        {
            m_arrangement.append(newUrl.url());

            addLauncher(launcherForUrl(newUrl), 0);
        }
    }
    else if (m_arrangement.contains(oldUrl.url()))
    {
        m_arrangement.replace(m_arrangement.indexOf(oldUrl.url()), newUrl.url());

        for (int i = 0; i < m_launchers.count(); ++i)
        {
            if (m_launchers.at(i)->launcherUrl() == oldUrl)
            {
                m_launchers.at(i)->setUrl(newUrl);

                break;
            }
        }
    }

    configuration.writeEntry("arrangement", m_arrangement);

    emit configNeedsSaving();
}

void Applet::itemDropped(Icon *icon, int index)
{
    if (!icon)
    {
        return;
    }

    if ((icon->itemType() == TypeTask || icon->itemType() == TypeGroup) && icon->task() && icon->task()->abstractItem())
    {
        index -= (m_arrangement.indexOf("tasks") + 1);

        if (index < 0)
        {
            index = 0;
        }

        m_groupManager->manualSortingRequest(icon->task()->abstractItem(), index);

        return;
    }

    if (immutability() != Plasma::Mutable)
    {
        return;
    }

    KConfigGroup configuration = config();
    QStringList arrangement;

    if (m_arrangement.contains("tasks") && index >= m_arrangement.indexOf("tasks") && index <= (m_arrangement.indexOf("tasks") + m_taskIcons.count()))
    {
        index = 1;
    }

    m_layout->removeItem(icon);

    insertItem(index, icon);

    for (int i = 0; i < m_layout->count(); ++i)
    {
        QString objectName = dynamic_cast<QObject*>(m_layout->itemAt(i)->graphicsItem())->objectName();

        if (objectName == "FancyTasksIcon")
        {
            Icon *icon = static_cast<Icon*>(m_layout->itemAt(i)->graphicsItem());

            if (icon && icon->itemType() == TypeLauncher)
            {
                arrangement.append(icon->launcher()->launcherUrl().pathOrUrl());
            }
            else if (!arrangement.contains("tasks"))
            {
                arrangement.append("tasks");
            }
        }
        else if (objectName == "FancyTasksSeparator")
        {
            arrangement.append("separator");
        }
    }

    m_arrangement = arrangement;

    configuration.writeEntry("arrangement", m_arrangement);

    emit configNeedsSaving();
}

void Applet::itemDragged(Icon *icon, const QPointF &position, const QMimeData *mimeData)
{
    if (((mimeData->hasFormat("windowsystem/winid") || mimeData->hasFormat("windowsystem/multiple-winids")) && m_groupManager->sortingStrategy() == TaskManager::GroupManager::ManualSorting && icon && icon->task()) || (KUrl::List::canDecode(mimeData) && immutability() == Plasma::Mutable && !mimeData->hasFormat("windowsystem/winid") && !mimeData->hasFormat("windowsystem/multiple-winids")))
    {
        int index = 0;

        for (int i = 0; i < m_layout->count(); ++i)
        {
            QObject *object = dynamic_cast<QObject*>(m_layout->itemAt(i)->graphicsItem());

            if (!object || (object->objectName() != "FancyTasksIcon" && object->objectName() != "FancyTasksSeparator"))
            {
                continue;
            }

            if (object == icon)
            {
                break;
            }

            ++index;
        }

        if (location() == Plasma::LeftEdge || location() == Plasma::RightEdge)
        {
            if (position.y() > (icon->boundingRect().height() / 2))
            {
                ++index;
            }
        }
        else if (position.x() > (icon->boundingRect().width() / 2))
        {
            ++index;
        }

        showDropZone(index + 1);
    }
}

void Applet::requestFocus()
{
    KWindowSystem::forceActiveWindow(window());

    setFocus();

    QTimer::singleShot(250, this, SLOT(setFocus()));
}

KMenu* Applet::contextMenu()
{
    KMenu *menu = new KMenu;

    if (m_lastAttentionDemand.isValid() && m_lastAttentionDemand.secsTo(QDateTime::currentDateTime()) < 1)
    {
        return menu;
    }

    for (int i = 0; i < m_visibleItems.count(); ++i)
    {
        if (m_visibleItems.at(i)->objectName() == "FancyTasksSeparator")
        {
            if (m_visibleItems.at(i) && m_visibleItems.at(i)->isVisible())
            {
                menu->addSeparator();
            }

            continue;
        }

        if (m_visibleItems.at(i)->objectName() != "FancyTasksIcon")
        {
            continue;
        }

        Icon *icon = static_cast<Icon*>(m_visibleItems.at(i));

        if (!m_visibleItems.at(i) || !m_visibleItems.at(i)->isVisible())
        {
            continue;
        }

        QAction *action = menu->addAction(icon->icon(), icon->title());
        QFont font = QFont(action->font());

        if (icon->itemType() == TypeLauncher)
        {
            font.setItalic(true);
        }
        else if (icon->demandsAttention())
        {
            font.setBold(true);
        }

        if (icon->itemType() == TypeGroup)
        {
            Menu *groupMenu = new Menu(icon->task()->windows());

            action->setMenu(groupMenu);

            connect(menu, SIGNAL(destroyed()), groupMenu, SLOT(deleteLater()));
        }
        else if (icon->itemType() == TypeLauncher && icon->launcher()->isServiceGroup())
        {
            KMenu *launcherMenu = icon->launcher()->serviceMenu();

            action->setMenu(launcherMenu);

            connect(menu, SIGNAL(destroyed()), launcherMenu, SLOT(deleteLater()));
        }
        else if (icon->itemType() == TypeJob)
        {
            KMenu *jobMenu = icon->jobs().at(0)->contextMenu();

            action->setMenu(jobMenu);

            connect(menu, SIGNAL(destroyed()), jobMenu, SLOT(deleteLater()));
        }
        else
        {
            connect(action, SIGNAL(triggered()), icon, SLOT(activate()));
        }

        action->setFont(font);
    }

    return menu;
}

Launcher* Applet::launcherForUrl(KUrl url)
{
    Launcher *launcher = NULL;

    if (!url.isValid())
    {
        return launcher;
    }

    for (int i = 0; i < m_launchers.count(); ++i)
    {
        if (m_launchers.at(i)->launcherUrl() == url)
        {
            launcher = m_launchers.at(i);

            break;
        }
    }

    if (!launcher)
    {
        launcher = new Launcher(url, this);

        m_launchers.append(launcher);
    }

    return launcher;
}

Launcher* Applet::launcherForTask(TaskManager::TaskItem *task)
{
    if (!task || !task->task())
    {
        return (NULL);
    }

    QString className;
    WId window = task->task()->window();

    if (window)
    {
        className = KWindowSystem::windowInfo(window, 0, NET::WM2WindowClass).windowClassName();
    }

    for (int i = 0; i < m_launchers.count(); ++i)
    {
        if (!m_launchers.at(i)->isExecutable())
        {
            continue;
        }

        if ((!className.isEmpty() && (m_launchers.at(i)->title().contains(className, Qt::CaseInsensitive) || m_launchers.at(i)->executable().contains(className, Qt::CaseInsensitive))) || task->name().contains(m_launchers.at(i)->title(), Qt::CaseInsensitive))
        {
            return (m_launchers.at(i));
        }
    }

    return (NULL);
}

Icon* Applet::iconForMimeData(const QMimeData *mimeData)
{
    Icon *icon = NULL;

    if (!mimeData->hasFormat("windowsystem/winid") && !mimeData->hasFormat("windowsystem/multiple-winids"))
    {
        return icon;
    }

    QList<WId> sourceWindows;
    QByteArray data;
    WId window;

    if (mimeData->hasFormat("windowsystem/winid"))
    {
        data = mimeData->data("windowsystem/winid");

        if (data.size() != sizeof(WId))
        {
            return icon;
        }

        memcpy(&window, data.data(), sizeof(WId));

        sourceWindows.append(window);
    }
    else
    {
        data = mimeData->data("windowsystem/multiple-winids");

        if ((unsigned int) data.size() < (sizeof(int) + sizeof(WId)))
        {
            return icon;
        }

        int count = 0;

        memcpy(&count, data.data(), sizeof(int));

        if (count < 1 || (unsigned int) data.size() < (sizeof(int) + (sizeof(WId) * count)))
        {
            return icon;
        }

        for (int i = 0; i < count; ++i)
        {
            memcpy(&window, (data.data() + sizeof(int) + (sizeof(WId) * i)), sizeof(WId));

            sourceWindows.append(window);
        }
    }

    qSort(sourceWindows);

    QHash<TaskManager::AbstractGroupableItem*, QPointer<Icon> >::iterator iterator;

    for (iterator = m_taskIcons.begin(); iterator != m_taskIcons.end(); ++iterator)
    {
        if (!iterator.value() || (iterator.value()->itemType() != TypeTask && iterator.value()->itemType() != TypeGroup))
        {
            continue;
        }

        QList<WId> targetWindows = iterator.value()->task()->windows();

        qSort(targetWindows);

        if (sourceWindows == targetWindows)
        {
            return iterator.value();
        }
    }

    return icon;
}

TaskManager::GroupManager* Applet::groupManager()
{
    return m_groupManager;
}

Plasma::Svg* Applet::theme()
{
    return m_theme;
}

QStringList Applet::arrangement() const
{
    return m_arrangement;
}

TitleLabelMode Applet::titleLabelMode() const
{
    return m_titleLabelMode;
}

ActiveIconIndication Applet::activeIconIndication() const
{
    return m_activeIconIndication;
}

AnimationType Applet::moveAnimation() const
{
    return m_moveAnimation;
}

AnimationType Applet::demandsAttentionAnimation() const
{
    return m_demandsAttentionAnimation;
}

AnimationType Applet::startupAnimation() const
{
    return m_startupAnimation;
}

QHash<IconAction, QPair<Qt::MouseButton, Qt::Modifier> > Applet::iconActions() const
{
    return m_iconActions;
}

QList<QAction*> Applet::contextualActions()
{
    QList<QAction*> actions;
    QAction *action = new QAction(KIcon("system-run"), i18n("Entries"), this);
    action->setMenu(contextMenu());

    actions.append(action);

    return actions;
}

QPixmap Applet::lightPixmap()
{
    return m_lightPixmap;
}

WId Applet::window() const
{
    if (scene())
    {
        QGraphicsView *parentView = NULL;
        QGraphicsView *possibleParentView = NULL;

        foreach (QGraphicsView *view, scene()->views())
        {
            if (view->sceneRect().intersects(sceneBoundingRect()) || view->sceneRect().contains(scenePos()))
            {
                if (view->isActiveWindow())
                {
                    parentView = view;

                    break;
                }
                else
                {
                    possibleParentView = view;
                }
            }
        }

        if (!parentView)
        {
            parentView = possibleParentView;
        }

        if (parentView)
        {
            return parentView->winId();
        }
    }

    return 0;
}

qreal Applet::initialFactor() const
{
    return m_initialFactor;
}

qreal Applet::itemSize() const
{
    return m_itemSize;
}

bool Applet::focusNextPrevChild(bool next)
{
    focusIcon(next);

    return true;
}

bool Applet::parabolicMoveAnimation() const
{
    return m_parabolicMoveAnimation;
}

bool Applet::useThumbnails() const
{
    return m_useThumbnails;
}

bool Applet::paintReflections() const
{
    return m_paintReflections;
}

void Applet::setActiveWindow(WId window)
{
    m_activeWindow = window;
}

WId Applet::activeWindow()
{
    return m_activeWindow;
}

QPixmap Applet::windowPreview(WId window, int size)
{
    QPixmap thumbnail;

#ifdef FANCYTASKS_HAVE_COMPOSITING
    if (KWindowSystem::compositingActive())
    {
        Display *display = QX11Info::display();
        XWindowAttributes attributes;

        XCompositeRedirectWindow(display, window, CompositeRedirectAutomatic);
        XGetWindowAttributes(display, window, &attributes);

        XRenderPictFormat *format = XRenderFindVisualFormat(display, attributes.visual);

        if (format)
        {
            bool hasAlpha  = (format->type == PictTypeDirect && format->direct.alphaMask);
            int x = attributes.x;
            int y = attributes.y;
            int width = attributes.width;
            int height = attributes.height;

            XRenderPictureAttributes pictureAttributes;
            pictureAttributes.subwindow_mode = IncludeInferiors;

            Picture picture = XRenderCreatePicture(display, window, format, CPSubwindowMode, &pictureAttributes);
            XserverRegion region = XFixesCreateRegionFromWindow(display, window, WindowRegionBounding);

            XFixesTranslateRegion(display, region, -x, -y);
            XFixesSetPictureClipRegion(display, picture, 0, 0, region);
            XFixesDestroyRegion(display, region);
            XShapeSelectInput(display, window, ShapeNotifyMask);

            thumbnail = QPixmap(width, height);
            thumbnail.fill(Qt::transparent);

            XRenderComposite(display, (hasAlpha?PictOpOver:PictOpSrc), picture, None, thumbnail.x11PictureHandle(), 0, 0, 0, 0, 0, 0, width, height);
        }
    }
#endif

    if (thumbnail.isNull() && KWindowSystem::windowInfo(window, NET::XAWMState).mappingState() == NET::Visible)
    {
        thumbnail = QPixmap::grabWindow(QApplication::desktop()->winId()).copy(KWindowSystem::windowInfo(window, NET::WMGeometry | NET::WMFrameExtents).frameGeometry());
    }

    if (!thumbnail.isNull())
    {
        if (thumbnail.width() > thumbnail.height())
        {
            thumbnail = thumbnail.scaledToWidth(size, Qt::SmoothTransformation);
        }
        else
        {
            thumbnail = thumbnail.scaledToHeight(size, Qt::SmoothTransformation);
        }
    }

    return thumbnail;
}

}
