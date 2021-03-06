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

    connect(m_dropZone, SIGNAL(visibilityChanged(bool)), this, SLOT(updateSize()));
    connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), this, SLOT(updateTheme()));
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

    updateTheme();

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

    connect(configuration, SIGNAL(accepted()), this, SIGNAL(configNeedsSaving()));
    connect(configuration, SIGNAL(accepted()), this, SLOT(configChanged()));
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

    const KUrl::List droppedUrls = KUrl::List::fromMimeData(event->mimeData());

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

    const QString customBackgroundImage = configuration.readEntry("customBackgroundImage", QString());

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

    const QStringList arrangement = configuration.readEntry("arrangement", QStringList("tasks"));
    const TaskManager::GroupManager::TaskGroupingStrategy groupingStrategy = static_cast<TaskManager::GroupManager::TaskGroupingStrategy>(configuration.readEntry("groupingStrategy", static_cast<int>(TaskManager::GroupManager::NoGrouping)));
    const TaskManager::GroupManager::TaskSortingStrategy sortingStrategy = static_cast<TaskManager::GroupManager::TaskSortingStrategy>(configuration.readEntry("sortingStrategy", static_cast<int>(TaskManager::GroupManager::ManualSorting)));
    const bool showOnlyTasksWithLaunchers = configuration.readEntry("showOnlyTasksWithLaunchers", false);
    const bool needsReload = (groupingStrategy != m_groupManager->groupingStrategy() || sortingStrategy != m_groupManager->sortingStrategy() || showOnlyTasksWithLaunchers != m_showOnlyTasksWithLaunchers || arrangement != m_arrangement);

    m_jobCloseMode = static_cast<CloseJobMode>(configuration.readEntry("jobCloseMode", static_cast<int>(DelayedClose)));
    m_activeIconIndication = static_cast<ActiveIconIndication>(configuration.readEntry("activeIconIndication", static_cast<int>(FadeIndication)));
    m_moveAnimation = static_cast<AnimationType>(configuration.readEntry("moveAnimation", static_cast<int>(GlowAnimation)));
    m_parabolicMoveAnimation = configuration.readEntry("parabolicMoveAnimation", false);
    m_demandsAttentionAnimation = static_cast<AnimationType>(configuration.readEntry("demandsAttentionAnimation", static_cast<int>(BlinkAnimation)));
    m_startupAnimation = static_cast<AnimationType>(configuration.readEntry("startupAnimation", static_cast<int>(BounceAnimation)));
    m_titleLabelMode = static_cast<TitleLabelMode>(configuration.readEntry("titleLabelMode", static_cast<int>(AlwaysShowLabel)));
    m_customBackgroundImage = customBackgroundImage;
    m_showOnlyTasksWithLaunchers = showOnlyTasksWithLaunchers;
    m_connectJobsWithTasks = configuration.readEntry("connectJobsWithTasks", false);
    m_groupJobs = configuration.readEntry("groupJobs", true);
    m_arrangement = configuration.readEntry("arrangement", QStringList("tasks"));
    m_initialFactor = ((m_moveAnimation == ZoomAnimation)?configuration.readEntry("initialZoomLevel", 0.7):((m_moveAnimation == JumpAnimation)?0.7:0));
    m_paintReflections = configuration.readEntry("paintReflections", true);

    m_groupManager->setGroupingStrategy(groupingStrategy);
    m_groupManager->setSortingStrategy(sortingStrategy);
    m_groupManager->setShowOnlyCurrentDesktop(configuration.readEntry("showOnlyCurrentDesktop", false));
    m_groupManager->setShowOnlyCurrentActivity(configuration.readEntry("showOnlyCurrentActivity", true));
    m_groupManager->setShowOnlyCurrentScreen(configuration.readEntry("showOnlyCurrentScreen", false));
    m_groupManager->setShowOnlyMinimized(configuration.readEntry("showOnlyMinimized", false));
    m_groupManager->reconnect();

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

        reload();

        connect(this, SIGNAL(activate()), this, SLOT(showMenu()));
    }
    else if (needsReload)
    {
        reload();
    }
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

    updateSize();
}

void Applet::checkStartup()
{
    if (m_startupsQueue.isEmpty())
    {
        return;
    }

    QPointer<Task> task = m_startupsQueue.dequeue();

    if (task)
    {
        addTask(task->abstractItem(), true);

        task->deleteLater();
    }
}

void Applet::addTask(AbstractGroupableItem *abstractItem, bool force)
{
    if (!abstractItem || (!m_arrangement.contains("tasks") && !m_showOnlyTasksWithLaunchers) || m_groupManager->rootGroup()->members().indexOf(abstractItem) < 0)
    {
        return;
    }

    Task *task = NULL;

    if (abstractItem->itemType() == TaskManager::TaskItemType)
    {
        task = taskForWindow(*abstractItem->winIds().begin());
    }

    if (!task)
    {
        task = new Task(abstractItem, this);
    }

    if (task->taskType() == StartupType && !force)
    {
        m_startupsQueue.enqueue(task);

        QTimer::singleShot(250, this, SLOT(checkStartup()));

        return;
    }

    Icon *icon = NULL;

    if (m_groupManager->sortingStrategy() == TaskManager::GroupManager::NoSorting || m_groupManager->sortingStrategy() == TaskManager::GroupManager::ManualSorting || m_showOnlyTasksWithLaunchers)
    {
        Launcher *launcher = launcherForTask(task);

        if (m_groupManager->groupingStrategy() == TaskManager::GroupManager::ProgramGrouping)
        {
            QMap<AbstractGroupableItem*, QPointer<Icon> >::iterator launcherTaskIconsIterator;

            for (launcherTaskIconsIterator = m_launcherTaskIcons.begin(); launcherTaskIconsIterator != m_launcherTaskIcons.end(); ++launcherTaskIconsIterator)
            {
                if (launcherTaskIconsIterator.value()->launcher() == launcher && launcherTaskIconsIterator.value()->task() && launcherTaskIconsIterator.value()->task()->taskType() == GroupType && launcherTaskIconsIterator.value()->task()->members().indexOf(abstractItem))
                {
                    launcherTaskIconsIterator.value()->setTask(task);

                    m_launcherTaskIcons[abstractItem] = launcherTaskIconsIterator.value();
                    m_launcherTaskIcons.remove(launcherTaskIconsIterator.key());

                    return;
                }
            }
        }

        if (launcher && m_launcherIcons.contains(launcher))
        {
            icon = m_launcherIcons[launcher];

            if (icon && (!icon->task() || task->taskType() == GroupType))
            {
                if (task->taskType() == GroupType && icon->task())
                {
                    m_launcherTaskIcons.remove(icon->task()->abstractItem());
                }

                m_launcherTaskIcons[abstractItem] = icon;

                icon->setTask(task);

                return;
            }

            icon = NULL;
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
        QMap<AbstractGroupableItem*, QPointer<Icon> >::iterator taskIconsIterator;

        for (taskIconsIterator = m_taskIcons.begin(); taskIconsIterator != m_taskIcons.end(); ++taskIconsIterator)
        {
            if (!taskIconsIterator.value())
            {
                m_taskIcons.erase(taskIconsIterator);

                continue;
            }

            if (taskIconsIterator.value()->task() && taskIconsIterator.value()->task()->taskType() == StartupType && ((!title.isEmpty() && title.contains(taskIconsIterator.value()->task()->title(), Qt::CaseInsensitive)) || (command.isEmpty() && command.contains(taskIconsIterator.value()->task()->command(), Qt::CaseInsensitive))))
            {
                taskIconsIterator.value()->setTask(task);

                break;
            }
        }
    }

    if (!icon)
    {
        int index = (((m_groupManager->sortingStrategy() == TaskManager::GroupManager::NoSorting || m_groupManager->sortingStrategy() == TaskManager::GroupManager::ManualSorting)?m_taskIcons.count():m_groupManager->rootGroup()->members().indexOf(abstractItem)) + m_arrangement.indexOf("tasks") + 1);

        icon = createIcon(task, launcherForTask(task), NULL);

        if (m_arrangement.contains("jobs") && m_arrangement.indexOf("tasks") > m_arrangement.indexOf("jobs"))
        {
            index += m_jobIcons.count();
        }

        insertItem(index, icon);
    }

    m_taskIcons[abstractItem] = icon;
}

void Applet::removeTask(AbstractGroupableItem *abstractItem)
{
    if (m_launcherTaskIcons.contains(abstractItem))
    {
        if (m_launcherTaskIcons[abstractItem])
        {
            if (m_launcherTaskIcons[abstractItem]->task() && m_launcherTaskIcons[abstractItem]->task()->abstractItem() && abstractItem != m_launcherTaskIcons[abstractItem]->task()->abstractItem())
            {
                if (m_groupManager->groupingStrategy() == TaskManager::GroupManager::NoGrouping)
                {
                    QMap<AbstractGroupableItem*, QPointer<Icon> >::iterator taskIconsIterator;

                    for (taskIconsIterator = m_taskIcons.begin(); taskIconsIterator != m_taskIcons.end(); ++taskIconsIterator)
                    {
                        if (taskIconsIterator.value()->launcher() && taskIconsIterator.value()->launcher() == m_launcherTaskIcons[abstractItem]->launcher())
                        {
                            m_launcherTaskIcons[abstractItem]->setTask(taskIconsIterator.value()->task());
                            m_launcherTaskIcons[taskIconsIterator.value()->task()->abstractItem()] = m_launcherTaskIcons[abstractItem];

                            m_taskIcons[taskIconsIterator.key()]->deleteLater();

                            m_taskIcons.remove(taskIconsIterator.key());

                            break;
                        }
                    }
                }
                else
                {
                    m_launcherTaskIcons[m_launcherTaskIcons[abstractItem]->task()->abstractItem()] = m_launcherTaskIcons[abstractItem];
                }
            }
            else
            {
                m_launcherTaskIcons[abstractItem]->setTask(NULL);
            }
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
    if (m_groupManager->sortingStrategy() == TaskManager::GroupManager::NoSorting || m_groupManager->sortingStrategy() == TaskManager::GroupManager::ManualSorting || !m_arrangement.contains("tasks"))
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
        icon = createIcon(NULL, launcher, NULL);
    }

    m_launcherIcons[launcher] = icon;

    if (!m_arrangement.contains(url))
    {
        m_arrangement.insert(index, url);
    }

    const int originalIndex = index;

    if (m_arrangement.contains("tasks") && originalIndex >= m_arrangement.indexOf("tasks"))
    {
        index += m_taskIcons.count();
    }

    if (m_arrangement.contains("jobs") && originalIndex >= m_arrangement.indexOf("jobs"))
    {
        index += m_jobIcons.count();
    }

    insertItem(index, icon);

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

    Icon *icon = createIcon(NULL, NULL, job);

    int index = (m_arrangement.indexOf("jobs") + m_jobIcons.count() - 1);

    if (m_arrangement.contains("tasks") && m_arrangement.indexOf("jobs") > m_arrangement.indexOf("tasks"))
    {
        index += m_taskIcons.count();
    }

    insertItem(index, icon);

    m_jobIcons[job] = icon;
}

void Applet::cleanup()
{
    QMap<AbstractGroupableItem*, QPointer<Icon> >::iterator taskIconsIterator;
    QMap<Launcher*, QPointer<Icon> >::iterator launcherIconsIterator;
    QMap<Job*, QPointer<Icon> >::iterator jobIconsIterator;

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

void Applet::reload()
{
    disconnect(m_groupManager->rootGroup(), SIGNAL(itemAdded(AbstractGroupableItem*)), this, SLOT(addTask(AbstractGroupableItem*)));
    disconnect(m_groupManager->rootGroup(), SIGNAL(itemRemoved(AbstractGroupableItem*)), this, SLOT(removeTask(AbstractGroupableItem*)));
    disconnect(m_groupManager->rootGroup(), SIGNAL(itemPositionChanged(AbstractGroupableItem*)), this, SLOT(changeTaskPosition(AbstractGroupableItem*)));
    disconnect(dataEngine("applicationjobs"), SIGNAL(sourceAdded(const QString)), this, SLOT(addJob(const QString)));
    disconnect(dataEngine("applicationjobs"), SIGNAL(sourceRemoved(const QString)), this, SLOT(removeJob(const QString)));

    m_visibleItems.clear();

    qDeleteAll(m_jobsQueue);
    qDeleteAll(m_startupsQueue);
    qDeleteAll(m_launchers);
    qDeleteAll(m_tasks);
    qDeleteAll(m_jobs);
    qDeleteAll(m_launchers);

    for (int i = 0; i < m_layout->count(); ++i)
    {
        QObject *object = dynamic_cast<QObject*>(m_layout->itemAt(i));

        if (!object)
        {
            continue;
        }

        if (object->objectName() == "FancyTasksIcon" || object->objectName() == "FancyTasksSeparator")
        {
            object->deleteLater();
        }
    }

    m_jobsQueue.clear();
    m_startupsQueue.clear();
    m_launchers.clear();
    m_tasks.clear();
    m_jobs.clear();
    m_launchers.clear();

    int index = 1;

    for (int i = 0; i < m_arrangement.count(); ++i)
    {
        if (m_arrangement.at(i) == "separator")
        {
            if (i > 0 && !m_arrangement.at(i - 1).isEmpty())
            {
                Separator *separator = new Separator(m_theme, this);
                separator->setSize(m_itemSize);

                connect(separator, SIGNAL(hoverMoved(QGraphicsWidget*,qreal)), this, SLOT(itemHoverMoved(QGraphicsWidget*,qreal)));
                connect(separator, SIGNAL(hoverLeft()), this, SLOT(hoverLeft()));

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
        connect(m_groupManager->rootGroup(), SIGNAL(itemAdded(AbstractGroupableItem*)), this, SLOT(addTask(AbstractGroupableItem*)));
        connect(m_groupManager->rootGroup(), SIGNAL(itemRemoved(AbstractGroupableItem*)), this, SLOT(removeTask(AbstractGroupableItem*)));
        connect(m_groupManager->rootGroup(), SIGNAL(itemPositionChanged(AbstractGroupableItem*)), this, SLOT(changeTaskPosition(AbstractGroupableItem*)));

        foreach (TaskManager::AbstractGroupableItem* abstractItem, m_groupManager->rootGroup()->members())
        {
            addTask(abstractItem);
        }
    }

    if (m_arrangement.contains("jobs"))
    {
        connect(dataEngine("applicationjobs"), SIGNAL(sourceAdded(const QString)), this, SLOT(addJob(const QString)));
        connect(dataEngine("applicationjobs"), SIGNAL(sourceRemoved(const QString)), this, SLOT(removeJob(const QString)));

        const QStringList jobs = dataEngine("applicationjobs")->sources();

        for (int i = 0; i < jobs.count(); ++i)
        {
            addJob(jobs.at(i));
        }
    }

    updateSize();
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
                separator->hide();
            }
            else
            {
                separator->show();

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

    if (location() != Plasma::Floating)
    {
        emit sizeHintChanged(Qt::PreferredSize);
        emit sizeHintChanged(Qt::MinimumSize);
    }

    resize(size);

    update();

    emit sizeChanged(m_itemSize);
}

void Applet::updateTheme()
{
    m_lightPixmap = QPixmap(m_theme->elementSize("task"));
    m_lightPixmap.fill(Qt::transparent);

    QPainter pixmapPainter(&m_lightPixmap);
    pixmapPainter.setRenderHints(QPainter::SmoothPixmapTransform);

    m_theme->paint(&pixmapPainter, m_lightPixmap.rect(), "task");

    pixmapPainter.end();
}
void Applet::itemDropped(Icon *icon, int index)
{
    if (!icon)
    {
        return;
    }

    if ((icon->itemType() == TaskType || icon->itemType() == GroupType) && icon->task() && icon->task()->abstractItem())
    {
        m_layout->removeItem(icon);

        insertItem(index, icon);

        return;
    }

    if (immutability() != Plasma::Mutable || !icon->launcher())
    {
        return;
    }

    m_layout->removeItem(icon);

    insertItem(index, icon);

    const int originalIndex = index;

    if (m_arrangement.contains("tasks") && originalIndex >= m_arrangement.indexOf("tasks"))
    {
        index -= m_taskIcons.count();
    }

    if (m_arrangement.contains("jobs") && originalIndex >= m_arrangement.indexOf("jobs"))
    {
        index -= m_jobIcons.count();
    }

    m_arrangement.move(m_arrangement.indexOf(icon->launcher()->launcherUrl().pathOrUrl()), qMin((index - 1), (m_arrangement.count() - 1)));

    config().writeEntry("arrangement", m_arrangement);

    emit configNeedsSaving();
}

void Applet::itemDragged(Icon *icon, const QPointF &position, const QMimeData *mimeData)
{
    if (!icon)
    {
        return;
    }

    const bool hasWindows = (mimeData->hasFormat("windowsystem/winid") || mimeData->hasFormat("windowsystem/multiple-winids"));
    const bool isPossibleTask = (hasWindows && icon->task() && m_groupManager->sortingStrategy() == TaskManager::GroupManager::ManualSorting);
    const bool isPossibleLauncher = (!hasWindows && KUrl::List::canDecode(mimeData) && immutability() == Plasma::Mutable);

    if (!isPossibleTask && !isPossibleLauncher)
    {
        return;
    }

    QList<int> items;

    for (int i = 0; i < m_layout->count(); ++i)
    {
        QObject *object = dynamic_cast<QObject*>(m_layout->itemAt(i)->graphicsItem());

        if (!object)
        {
            continue;
        }

        if (object->objectName() == "FancyTasksIcon")
        {
            Icon *currentIcon = dynamic_cast<Icon*>(object);

            if (currentIcon)
            {
                items.append(currentIcon->id());
            }
        }
        else if (object->objectName() == "FancyTasksSeparator")
        {
            items.append(-1);
        }
    }

    int index = items.indexOf(icon->id());
    const int draggedIndex = items.indexOf(mimeData->hasFormat("plasmoid-fancytasks/iconid")?QString(mimeData->data("plasmoid-fancytasks/iconid")).toInt():-1);

    if (index < 0 || icon->id() < 0 || (index >= (draggedIndex - 1) && index <= (draggedIndex + 1)))
    {
        return;
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

    ++index;

    if (m_dropZone->index() != index)
    {
        m_layout->removeItem(m_dropZone);
        m_layout->insertItem(index, m_dropZone);

        m_dropZone->show(index);
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
            if (!values[iterator.key()].isEmpty() && matchRule(iterator.value().expression, values[iterator.key()], iterator.value().match))
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
    if (mimeData->hasFormat("plasmoid-fancytasks/iconid"))
    {
        const int id = QString(mimeData->data("plasmoid-fancytasks/iconid")).toInt();

        if (m_icons.contains(id))
        {
            return m_icons[id];
        }
    }

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

Icon* Applet::createIcon(Task *task, Launcher *launcher, Job *job)
{
    int id = qrand();

    while (m_icons.contains(id))
    {
        id = qrand();
    }

    m_icons[id] = NULL;

    Icon *icon = new Icon(id, task, launcher, job, this);
    icon->setSize(m_itemSize);
    icon->setFactor(m_initialFactor);

    connect(icon, SIGNAL(hoverMoved(QGraphicsWidget*,qreal)), this, SLOT(itemHoverMoved(QGraphicsWidget*,qreal)));
    connect(icon, SIGNAL(hoverLeft()), this, SLOT(hoverLeft()));
    connect(icon, SIGNAL(visibilityChanged(bool)), this, SLOT(updateSize()));
    connect(icon, SIGNAL(destroyed()), this, SLOT(updateSize()));
    connect(icon, SIGNAL(destroyed()), this, SLOT(cleanup()));

    m_icons[id] = icon;

    return icon;
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

bool Applet::paintReflections() const
{
    return m_paintReflections;
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
