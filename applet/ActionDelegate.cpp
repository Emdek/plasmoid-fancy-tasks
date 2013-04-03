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

#include "ActionDelegate.h"

#include <KLocale>
#include <KComboBox>

namespace FancyTasks
{

QMap<IconAction, QString> ActionDelegate::m_actions;

ActionDelegate::ActionDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
    m_actions[NoAction] = i18n("No Action");
    m_actions[ActivateItemAction] = i18n("Activate Item");
    m_actions[ActivateTaskAction] = i18n("Activate Task");
    m_actions[ActivateLauncherAction] = i18n("Activate Launcher");
    m_actions[ShowItemMenuAction] = i18n("Show Item Menu");
    m_actions[ShowItemChildrenListAction] = i18n("Show Item Children List");
    m_actions[ShowItemWindowsAction] = i18n("Show Item Windows");
    m_actions[CloseTaskAction] = i18n("Close Task");
    m_actions[KillTaskAction] = i18n("Kill Task");
    m_actions[MinimizeTaskAction] = i18n("Minimize Task");
    m_actions[MaximizeTaskAction] = i18n("Maximize Task");
    m_actions[FullscreenTaskAction] = i18n("Toggle Fullscreen State Of Task");
    m_actions[ShadeTaskAction] = i18n("Toggle Shade State Of Task");
    m_actions[ResizeTaskAction] = i18n("Resize Task");
    m_actions[MoveTaskAction] = i18n("Move Task");
    m_actions[MoveTaskToCurrentDesktopAction] = i18n("Move Task To Current Desktop");
    m_actions[MoveTaskToAllDesktopsAction] = i18n("Move Task To All Desktops");
}

void ActionDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    KComboBox *actionComboBox = static_cast<KComboBox*>(editor);
    int item = actionComboBox->findData(index.data(Qt::EditRole), Qt::UserRole);

    if (item < 0)
    {
        item = 0;
    }

    actionComboBox->setCurrentIndex(item);
}

void ActionDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    KComboBox *actionComboBox = static_cast<KComboBox*>(editor);

    if (actionComboBox->currentIndex() > 0)
    {
        model->setData(index, actionComboBox->itemData(actionComboBox->currentIndex()), Qt::EditRole);
    }
    else
    {
        model->setData(index, QString(), Qt::EditRole);
    }
}

QString ActionDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
    Q_UNUSED(locale)

    IconAction action = static_cast<IconAction>(value.toInt());

    if (action < 1)
    {
        return QString();
    }

    return m_actions[action];
}

QWidget* ActionDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option)

    KComboBox *actionComboBox = new KComboBox(parent);
    actionComboBox->setToolTip(i18n("Action"));

    QMap<IconAction, QString>::iterator iterator;

    for (iterator = m_actions.begin(); iterator != m_actions.end(); ++iterator)
    {
        actionComboBox->addItem(iterator.value(), QString::number(iterator.key()));
    }

    setEditorData(actionComboBox, index);

    return actionComboBox;
}

}
