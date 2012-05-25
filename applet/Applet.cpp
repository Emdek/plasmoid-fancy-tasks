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

#include "Applet.h"
#include "Icon.h"
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

LauncherRule::LauncherRule() : match(NoMatch),
    required(false)
{
}

LauncherRule::LauncherRule(QString expressionI, RuleMatch matchI, bool requiredI) : expression(expressionI),
    match(matchI),
    required(requiredI)
{
}

Applet::Applet(QObject *parent, const QVariantList &args) : Plasma::Applet(parent, args),
    m_groupManager(new TaskManager::GroupManager(this)),
    m_size(500, 100),
    m_dropZone(new DropZone(this)),
    m_entriesAction(NULL),
    m_animationTimeLine(new QTimeLine(100, this)),
    m_appletMaximumHeight(100),
    m_initialFactor(0),
    m_focusedItem(-1),
    m_initialized(false)
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
    QMap<Launcher*, QPointer<Icon> >::iterator launcherIconsIterator;

    for (launcherIconsIterator = m_launcherIcons.begin(); launcherIconsIterator != m_launcherIcons.end(); ++launcherIconsIterator)
    {
        if (launcherIconsIterator.key())
        {
            launcherIconsIterator.key()->blockSignals(true);
            launcherIconsIterator.key()->deleteLater();
        }
    }
}

void Applet::init()
{
    QGraphicsWidget *leftMargin = new QGraphicsWidget(this);
    leftMargin->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    QGraphicsWidget *rightMargin = new QGraphicsWidget(this);
    rightMargin->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    m_layout->addItem(leftMargin);
    m_layout->addItem(rightMargin);
    m_layout->addItem(m_dropZone);

    constraintsEvent(Plasma::LocationConstraint);

    QTimer::singleShot(100, this, SLOT(configChanged()));

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

    connect(configuration, SIGNAL(accepted()), this, SLOT(updateConfiguration()));
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

    if (configuration.hasGroup("Actions"))
    {
        KConfigGroup actionsConfiguration = configuration.group("Actions");
        QStringList actions = actionsConfiguration.keyList();

        for (int i = 0; i < actions.count(); ++i)
        {
            IconAction action = static_cast<IconAction>(actionsConfiguration.readEntry(actions.at(i), 0));

            if (action == NoAction)
            {
                continue;
            }

            QStringList trigger = actions.at(i).split('+', QString::KeepEmptyParts);
            QPair<Qt::MouseButtons, Qt::KeyboardModifiers> triggerPair;

            triggerPair.first = Qt::NoButton;
            triggerPair.second = Qt::NoModifier;

            if (trigger.contains("left"))
            {
                triggerPair.first |= Qt::LeftButton;
            }

            if (trigger.contains("middle"))
            {
                triggerPair.first |= Qt::MidButton;
            }

            if (trigger.contains("right"))
            {
                triggerPair.first |= Qt::RightButton;
            }

            if (triggerPair.first == Qt::NoButton)
            {
                continue;
            }

            if (trigger.contains("ctrl"))
            {
                triggerPair.second |= Qt::ControlModifier;
            }

            if (trigger.contains("shift"))
            {
                triggerPair.second |= Qt::ShiftModifier;
            }

            if (trigger.contains("alt"))
            {
                triggerPair.second |= Qt::AltModifier;
            }

            m_iconActions[triggerPair] = action;
        }
    }
    else
    {
        QPair<Qt::MouseButtons, Qt::KeyboardModifiers> triggerPair;
        triggerPair.first = Qt::LeftButton;
        triggerPair.second = Qt::NoModifier;

        m_iconActions[triggerPair] = ActivateItemAction;

        triggerPair.first = Qt::MidButton;
        triggerPair.second = Qt::NoModifier;

        m_iconActions[triggerPair] = ActivateLauncherAction;

        triggerPair.first = Qt::RightButton;
        triggerPair.second = Qt::NoModifier;

        m_iconActions[triggerPair] = ShowItemMenuAction;

        triggerPair.first = Qt::MidButton;
        triggerPair.second = Qt::ShiftModifier;

        m_iconActions[triggerPair] = ShowItemWindowsAction;

        triggerPair.first = Qt::LeftButton;
        triggerPair.second = Qt::ShiftModifier;

        m_iconActions[triggerPair] = CloseTaskAction;
    }

    m_jobCloseMode = static_cast<CloseJobMode>(configuration.readEntry("jobCloseMode", static_cast<int>(DelayedClose)));
    m_activeIconIndication = static_cast<ActiveIconIndication>(configuration.readEntry("activeIconIndication", static_cast<int>(FadeIndication)));
    m_moveAnimation = static_cast<AnimationType>(configuration.readEntry("moveAnimation", static_cast<int>(GlowAnimation)));
    m_parabolicMoveAnimation = configuration.readEntry("parabolicMoveAnimation", false);
    m_demandsAttentionAnimation = static_cast<AnimationType>(configuration.readEntry("demandsAttentionAnimation", static_cast<int>(BlinkAnimation)));
    m_startupAnimation = static_cast<AnimationType>(configuration.readEntry("startupAnimation", static_cast<int>(BounceAnimation)));
    m_useThumbnails = configuration.readEntry("useThumbnails", false);
    m_titleLabelMode = static_cast<TitleLabelMode>(configuration.readEntry("titleLabelMode", static_cast<int>(AlwaysShowLabel)));
    m_customBackgroundImage = configuration.readEntry("customBackgroundImage", QString());
    m_showOnlyCurrentDesktop = configuration.readEntry("showOnlyCurrentDesktop", false);
    m_showOnlyCurrentActivity = configuration.readEntry("showOnlyCurrentActivity", true);
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
    m_groupManager->setShowOnlyCurrentActivity(m_showOnlyCurrentActivity);
    m_groupManager->setShowOnlyCurrentScreen(m_showOnlyCurrentScreen);
    m_groupManager->setShowOnlyMinimized(m_showOnlyMinimized);

    if (containment())
    {
        m_groupManager->setScreen(containment()->screen());
    }

    if (!m_initialized)
    {
        m_initialized = true;

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
            QMap<TaskManager::AbstractGroupableItem*, QPointer<Icon> >::iterator tasksIterator;

            for (tasksIterator = m_taskIcons.begin(); tasksIterator != m_taskIcons.end(); ++tasksIterator)
            {
                m_layout->removeItem(tasksIterator.value().data());

                tasksIterator.value().data()->deleteLater();
            }

            m_taskIcons.clear();

            QMap<AbstractGroupableItem*, QPointer<Icon> >::iterator launcherTaskIconsIterator;

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

                    if (icon && icon->itemType() != LauncherType)
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

    for (int i = 0; i < m_launchers.count(); ++i)
    {
        updateLauncher(m_launchers.at(i));
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
    if (!abstractItem || (!m_arrangement.contains("tasks") && !m_showOnlyTasksWithLaunchers) || m_groupManager->rootGroup()->members().indexOf(abstractItem) < 0)
    {
        return;
    }

    Icon *icon = NULL;
    Task *task = NULL;

    if (abstractItem->itemType() == TaskManager::TaskItemType)
    {
        task = taskForWindow(*abstractItem->winIds().begin());
    }

    if (!task)
    {
        task = new Task(abstractItem, this);
    }

    if (m_groupManager->sortingStrategy() == TaskManager::GroupManager::NoSorting || m_showOnlyTasksWithLaunchers)
    {
        if (m_groupManager->groupingStrategy() == TaskManager::GroupManager::ProgramGrouping)
        {
            QMap<AbstractGroupableItem*, QPointer<Icon> >::iterator launcherTaskIconsIterator;

            for (launcherTaskIconsIterator = m_launcherTaskIcons.begin(); launcherTaskIconsIterator != m_launcherTaskIcons.end(); ++launcherTaskIconsIterator)
            {
                if (launcherTaskIconsIterator.value()->task() && launcherTaskIconsIterator.value()->task()->taskType() == GroupType && launcherTaskIconsIterator.value()->task()->members().indexOf(abstractItem))
                {
                    launcherTaskIconsIterator.value()->setTask(task);

                    m_launcherTaskIcons[abstractItem] = launcherTaskIconsIterator.value();
                    m_launcherTaskIcons.remove(launcherTaskIconsIterator.key());

                    return;
                }
            }
        }

        Launcher *launcher = launcherForTask(task);

        if (launcher && m_launcherIcons.contains(launcher))
        {
            icon = m_launcherIcons[launcher];

            if (task->taskType() == GroupType && icon->task())
            {
                m_launcherTaskIcons.remove(icon->task()->abstractItem());

                m_launcherTaskIcons[abstractItem] = icon;
            }
            else if (task->taskType() == TaskType && !icon->task())
            {
                m_launcherTaskIcons[abstractItem] = icon;
            }

            icon->setTask(task);

            return;
        }

        if (m_showOnlyTasksWithLaunchers)
        {
            task->deleteLater();

            return;
        }
    }

    if ((task->taskType() == TaskType || task->taskType() == GroupType) && (!task->title().isEmpty() || !task->command().isEmpty()))
    {
        const QString title = task->title();
        const QString command = task->command();
        QList<QPair<QPointer<Icon>, QDateTime> >::iterator iterator;

        for (iterator = m_startups.begin(); iterator != m_startups.end(); ++iterator)
        {
            QPair<QPointer<Icon>, QDateTime> pair = * iterator;

            if (!pair.first || !pair.first->task())
            {
                m_startups.erase(iterator);

                continue;
            }

            if (((!title.isEmpty() && title.contains(pair.first->task()->title(), Qt::CaseInsensitive)) || (command.isEmpty() && command.contains(pair.first->task()->command(), Qt::CaseInsensitive))) && pair.first->itemType() == StartupType)
            {
                icon = pair.first;
                icon->setTask(task);

                m_startups.erase(iterator);

                break;
            }
        }
    }

    if (!icon)
    {
        int index = (((m_groupManager->sortingStrategy() == TaskManager::GroupManager::NoSorting)?m_taskIcons.count():m_groupManager->rootGroup()->members().indexOf(abstractItem)) + m_arrangement.indexOf("tasks") + 1);

        icon = new Icon(task, launcherForTask(task), NULL, this);
        icon->setSize(m_itemSize);
        icon->setFactor(m_initialFactor);

        if (m_arrangement.contains("jobs") && m_arrangement.indexOf("tasks") > m_arrangement.indexOf("jobs"))
        {
            index += m_jobIcons.count();
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

    if (icon && icon->itemType() == StartupType)
    {
        m_startups.append(qMakePair(icon, QDateTime::currentDateTime()));

        QTimer::singleShot(2000, this, SLOT(cleanup()));

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
    if (!launcher)
    {
        return;
    }

    const QString url = launcher->launcherUrl().pathOrUrl();

    if (index < 0)
    {
        if (m_arrangement.contains(url))
        {
            index = m_arrangement.indexOf(url);
        }
        else
        {
            index = m_arrangement.indexOf("tasks");
        }
    }

    Icon *icon = NULL;

    if (m_launcherIcons.contains(launcher) && m_launcherIcons[launcher])
    {
        const int currentIndex = m_arrangement.indexOf(url);

        if (currentIndex == index)
        {
            return;
        }

        if (currentIndex < index)
        {
            --index;
        }

        m_layout->removeItem(m_launcherIcons[launcher]);

        icon = m_launcherIcons[launcher];

        m_arrangement.removeAll(url);
    }
    else
    {
        icon = new Icon(NULL, launcher, NULL, this);
        icon->setSize(m_itemSize);
        icon->setFactor(m_initialFactor);
    }

    m_launcherIcons[launcher] = icon;

    m_arrangement.insert(index, url);

    if (m_arrangement.contains("tasks") && index >= m_arrangement.indexOf("tasks"))
    {
        index += m_taskIcons.count();
    }

    if (m_arrangement.contains("jobs") && index >= m_arrangement.indexOf("jobs"))
    {
        index += m_jobIcons.count();
    }

    insertItem(index, icon);

    updateSize();

    config().writeEntry("arrangement", m_arrangement);

    emit configNeedsSaving();
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

    delete icon;
}

void Applet::changeLauncher(Launcher *launcher, const KUrl &oldUrl, bool force)
{
    if (!launcher || (!m_arrangement.contains(oldUrl.pathOrUrl()) && !force))
    {
        return;
    }

    config().group("Launchers").deleteGroup(oldUrl.pathOrUrl());

    if (launcher->launcherUrl() != oldUrl)
    {
        m_arrangement.replace(m_arrangement.indexOf(oldUrl.pathOrUrl()), launcher->launcherUrl().pathOrUrl());

        config().writeEntry("arrangement", m_arrangement);
    }

    KConfigGroup configuration = config().group("Launchers").group(launcher->launcherUrl().pathOrUrl());
    configuration.writeEntry("exclude", launcher->isExcluded());

    QMap<ConnectionRule, QString> ruleKeys;
    ruleKeys[TaskCommandRule] = "taskCommand";
    ruleKeys[TaskTitleRule] = "taskTitle";
    ruleKeys[WindowClassRule] = "windowClass";
    ruleKeys[WindowRoleRule] = "windowRole";

    QMap<ConnectionRule, LauncherRule> rules = launcher->rules();
    QMap<ConnectionRule, LauncherRule>::iterator iterator;

    for (iterator = rules.begin(); iterator != rules.end(); ++iterator)
    {
        if (!ruleKeys.contains(iterator.key()))
        {
            continue;
        }

        configuration.writeEntry((ruleKeys[iterator.key()] + "Expression"), iterator.value().expression);
        configuration.writeEntry((ruleKeys[iterator.key()] + "Match"), static_cast<int>(iterator.value().match));
        configuration.writeEntry((ruleKeys[iterator.key()] + "Required"), iterator.value().required);
    }

    emit configNeedsSaving();
}

void Applet::updateLauncher(Launcher *launcher)
{
    if (!launcher)
    {
        return;
    }

    KConfigGroup configuration = config().group("Launchers").group(launcher->launcherUrl().pathOrUrl());

    if (!configuration.exists())
    {
        return;
    }

    QMap<ConnectionRule, LauncherRule> rules;
    QList<QPair<QString, ConnectionRule> > ruleKeys;
    ruleKeys << qMakePair(QString("taskCommand"), TaskCommandRule) << qMakePair(QString("taskTitle"), TaskTitleRule) << qMakePair(QString("windowClass"), WindowClassRule) << qMakePair(QString("windowRole"), WindowRoleRule);

    for (int i = 0; i < ruleKeys.count(); ++i)
    {
        if (configuration.hasKey(ruleKeys.at(i).first + "Expression"))
        {
            rules[ruleKeys.at(i).second] = LauncherRule(configuration.readEntry((ruleKeys.at(i).first + "Expression"), QString()), static_cast<RuleMatch>(configuration.readEntry((ruleKeys.at(i).first + "Match"), static_cast<int>(NoMatch))), configuration.readEntry((ruleKeys.at(i).first + "Required"), false));
        }
    }

    launcher->setRules(rules);
    launcher->setExcluded(configuration.readEntry("exclude", false));
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
        else if (job->state() != ErrorState)
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

    if (job->state() == FinishedState)
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
        QMap<TaskManager::AbstractGroupableItem*, QPointer<Icon> >::iterator tasksIterator;

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
        QMap<Job*, QPointer<Icon> >::iterator jobsIterator;

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

    int index = (m_arrangement.indexOf("jobs") + m_jobIcons.count() - 1);

    if (m_arrangement.contains("tasks") && m_arrangement.indexOf("jobs") > m_arrangement.indexOf("tasks"))
    {
        index += m_taskIcons.count();
    }

    insertItem(index, icon);

    updateSize();

    m_jobIcons[job] = icon;
}

void Applet::cleanup()
{
    QList<QPair<QPointer<Icon>, QDateTime> >::iterator startupsIterator;
    QMap<AbstractGroupableItem*, QPointer<Icon> >::iterator taskIconsIterator;
    QMap<Launcher*, QPointer<Icon> >::iterator launcherIconsIterator;
    QMap<Job*, QPointer<Icon> >::iterator jobIconsIterator;

    for (startupsIterator = m_startups.begin(); startupsIterator != m_startups.end(); ++startupsIterator)
    {
        QPair<QPointer<Icon>, QDateTime> pair = * startupsIterator;

        if (pair.second.secsTo(QDateTime::currentDateTime()) > 1 && (!pair.first || pair.first->itemType() == StartupType))
        {
            if (pair.first)
            {
                pair.first->deleteLater();
            }

            m_startups.erase(startupsIterator);
        }
    }

    for (taskIconsIterator = m_taskIcons.begin(); taskIconsIterator != m_taskIcons.end(); ++taskIconsIterator)
    {
        if (!taskIconsIterator.value())
        {
            m_taskIcons.erase(taskIconsIterator);
        }
    }

    for (taskIconsIterator = m_launcherTaskIcons.begin(); taskIconsIterator != m_launcherTaskIcons.end(); ++taskIconsIterator)
    {
        if (!taskIconsIterator.value())
        {
            m_launcherTaskIcons.erase(taskIconsIterator);
        }
    }

    for (launcherIconsIterator = m_launcherIcons.begin(); launcherIconsIterator != m_launcherIcons.end(); ++launcherIconsIterator)
    {
        if (!launcherIconsIterator.value())
        {
            m_launcherIcons.erase(launcherIconsIterator);
        }
    }

    for (jobIconsIterator = m_jobIcons.begin(); jobIconsIterator != m_jobIcons.end(); ++jobIconsIterator)
    {
        if (!jobIconsIterator.value())
        {
            m_jobIcons.erase(jobIconsIterator);
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
            currentIcon->task()->activate();
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

    menu->deleteLater();
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

void Applet::itemDropped(Icon *icon, int index)
{
    if (!icon)
    {
        return;
    }

    if ((icon->itemType() == TaskType || icon->itemType() == GroupType) && icon->task() && icon->task()->abstractItem())
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

            if (icon && icon->itemType() == LauncherType)
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
    Menu *menu = new Menu(NULL, this);

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

        QAction *action = NULL;

        if (icon->task() && icon->task()->taskType() == TaskType && !icon->task()->windows().isEmpty())
        {
            action = menu->addAction(icon->icon(), icon->title(), icon->task()->windows().first());
        }
        else
        {
            action = menu->addAction(icon->icon(), icon->title());
        }

        QFont font = QFont(action->font());

        if (icon->itemType() == LauncherType)
        {
            font.setItalic(true);
        }
        else if (icon->isDemandingAttention())
        {
            font.setBold(true);
        }

        if (icon->itemType() == GroupType)
        {
            Menu *groupMenu = new Menu(icon->task(), this);

            action->setMenu(groupMenu);

            connect(menu, SIGNAL(destroyed()), groupMenu, SLOT(deleteLater()));
        }
        else if (icon->itemType() == LauncherType && icon->launcher()->isMenu())
        {
            KMenu *launcherMenu = icon->launcher()->serviceMenu();

            action->setMenu(launcherMenu);

            connect(menu, SIGNAL(destroyed()), launcherMenu, SLOT(deleteLater()));
        }
        else if (icon->itemType() == JobType)
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

Task* Applet::taskForWindow(WId window)
{
    if (m_tasks.contains(window))
    {
        if (m_tasks[window] && m_tasks[window]->taskType() == TaskType)
        {
            return m_tasks[window];
        }
        else
        {
            m_tasks.remove(window);
        }
    }

    TaskManager::Task *task = TaskManager::TaskManager::self()->findTask(window);

    if (!task)
    {
        return NULL;
    }

    m_tasks[window] = new Task(new TaskManager::TaskItem(this, task), this);

    return m_tasks[window];
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

        updateLauncher(launcher);

        connect(launcher, SIGNAL(launcherChanged(Launcher*,KUrl)), this, SLOT(changeLauncher(Launcher*,KUrl)));

        m_launchers.append(launcher);
    }

    return launcher;
}

Launcher* Applet::launcherForTask(Task *task)
{
    if (!task)
    {
        return NULL;
    }

    QMap<ConnectionRule, QString> values;
    values[TaskCommandRule] = task->command();
    values[TaskTitleRule] = task->title();

    if (task->windows().count() > 0)
    {
        const WId window = task->windows().first();

        values[WindowClassRule] = KWindowSystem::windowInfo(window, 0, NET::WM2WindowClass).windowClassName();
        values[WindowRoleRule] = KWindowSystem::windowInfo(window, 0, NET::WM2WindowClass).windowClassClass();
    }
    else
    {
        values[WindowClassRule] = QString();
        values[WindowRoleRule] = QString();
    }

    for (int i = 0; i < m_launchers.count(); ++i)
    {
        if (m_launchers.at(i)->isMenu() || m_launchers.at(i)->isExcluded() || m_launchers.at(i)->rules().isEmpty())
        {
            continue;
        }

        QMap<ConnectionRule, LauncherRule> rules = m_launchers.at(i)->rules();
        QMap<ConnectionRule, LauncherRule>::iterator iterator;
        int matched = 0;

        for (iterator = rules.begin(); iterator != rules.end(); ++iterator)
        {
            if (matchRule(iterator.value().expression, values[iterator.key()], iterator.value().match))
            {
                ++matched;
            }
            else if (iterator.value().required)
            {
                matched = 0;

                break;
            }
        }

        if (matched > 0)
        {
            return m_launchers.at(i);
        }
    }

    return NULL;
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

    QMap<TaskManager::AbstractGroupableItem*, QPointer<Icon> >::iterator iterator;

    for (iterator = m_taskIcons.begin(); iterator != m_taskIcons.end(); ++iterator)
    {
        if (!iterator.value() || (iterator.value()->itemType() != TaskType && iterator.value()->itemType() != GroupType))
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

QMap<QPair<Qt::MouseButtons, Qt::KeyboardModifiers>, IconAction> Applet::iconActions() const
{
    return m_iconActions;
}

QList<QAction*> Applet::contextualActions()
{
    QList<QAction*> actions;

    if (m_entriesAction)
    {
        m_entriesAction->menu()->deleteLater();
    }
    else
    {
        m_entriesAction = new QAction(KIcon("system-run"), i18n("Entries"), this);
    }

    m_entriesAction->setMenu(contextMenu());

    actions.append(m_entriesAction);

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

QPixmap Applet::windowPreview(WId window, int size)
{
    QPixmap thumbnail;

#ifdef FANCYTASKS_HAVE_COMPOSITING
    if (!KWindowSystem::compositingActive())
    {
        return thumbnail;
    }

    Display *display = QX11Info::display();
    XWindowAttributes attributes;

    XGetWindowAttributes(display, window, &attributes);

    const int x = attributes.x;
    const int y = attributes.y;
    const int width = attributes.width;
    const int height = attributes.height;

    XImage *image = XGetImage(display, window, x, y, width, height, AllPlanes, ZPixmap);

    if (!image)
    {
        return thumbnail;
    }

    thumbnail = QPixmap::fromImage(QImage((const uchar*) image->data, width, height, image->bytes_per_line, QImage::Format_ARGB32));

    XDestroyImage(image);

    if (thumbnail.width() > thumbnail.height())
    {
        thumbnail = thumbnail.scaledToWidth(size, Qt::SmoothTransformation);
    }
    else
    {
        thumbnail = thumbnail.scaledToHeight(size, Qt::SmoothTransformation);
    }
#endif

    return thumbnail;
}

bool Applet::matchRule(const QString &expression, const QString &value, RuleMatch match)
{
    switch (match)
    {
        case ExactMatch:
            return (expression == value);
        case PartialMatch:
            return value.contains(expression);
        case RegExpMatch:
            return QRegExp(expression, Qt::CaseInsensitive).exactMatch(value);
        default:
            return false;
    }

    return false;
}

}
