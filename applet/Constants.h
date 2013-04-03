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

#ifndef FANCYTASKSCONSTANTS_HEADER
#define FANCYTASKSCONSTANTS_HEADER

#include "config-fancytasks.h"

#include <QtCore/QString>

namespace FancyTasks
{

enum RuleMatch
{
    NoMatch = 0,
    RegExpMatch,
    PartialMatch,
    ExactMatch
};

enum ConnectionRule
{
    NoRule = 0,
    TaskCommandRule,
    TaskTitleRule,
    WindowClassRule,
    WindowRoleRule
};

enum TitleLabelMode
{
    NoLabel = 0,
    MouseOverLabel,
    ActiveIconLabel,
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
    NoAction = 0,
    ActivateItemAction,
    ActivateTaskAction,
    ActivateLauncherAction,
    ShowItemMenuAction,
    ShowItemChildrenListAction,
    ShowItemWindowsAction,
    CloseTaskAction,
    MinimizeTaskAction,
    MaximizeTaskAction,
    FullscreenTaskAction,
    ShadeTaskAction,
    ResizeTaskAction,
    MoveTaskAction,
    MoveTaskToCurrentDesktopAction,
    MoveTaskToAllDesktopsAction,
    KillTaskAction
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

enum JobState
{
    UnknownState = 0,
    RunningState,
    SuspendedState,
    FinishedState,
    ErrorState
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
    OtherType = 0,
    LauncherType,
    JobType,
    StartupType,
    TaskType,
    GroupType
};

Q_DECLARE_FLAGS(ItemChanges, ItemChange)

struct LauncherRule
{
    QString expression;
    RuleMatch match;
    bool required;

    LauncherRule();
    LauncherRule(QString expressionI, RuleMatch matchI = ExactMatch, bool requiredI = false);
};

}

#endif
