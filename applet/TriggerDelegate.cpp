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

#include "TriggerDelegate.h"
#include "Configuration.h"

#include <QtGui/QMouseEvent>
#include <QtGui/QPushButton>

#include <KLocale>

namespace FancyTasks
{

TriggerDelegate::TriggerDelegate(Configuration *parent) : QStyledItemDelegate(parent),
    m_parent(parent)
{
}

void TriggerDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QPushButton *triggerButton = static_cast<QPushButton*>(editor);
    triggerButton->setToolTip(displayText(index.data(Qt::EditRole)));
    triggerButton->setWindowTitle(index.data(Qt::EditRole).toString());

    if (m_parent->hasTrigger(triggerButton->windowTitle()))
    {
        triggerButton->setText(i18n("Already used..."));
    }
}

void TriggerDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QPushButton *triggerButton = static_cast<QPushButton*>(editor);

    if (triggerButton->windowTitle().isEmpty())
    {
        model->setData(index, QString('+'), Qt::EditRole);
    }
    else
    {
        model->setData(index, triggerButton->windowTitle(), Qt::EditRole);
    }
}

QString TriggerDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
    Q_UNUSED(locale)

    const QStringList action = value.toString().split('+', QString::SkipEmptyParts);
    QStringList text;

    if (action.isEmpty())
    {
        return i18n("No trigger");
    }

    if (action.contains("left"))
    {
        text.append(i18nc("As mouse button", "Left button"));
    }

    if (action.contains("middle"))
    {
        text.append(i18nc("As mouse button", "Middle button"));
    }

    if (action.contains("right"))
    {
        text.append(i18nc("As mouse button", "Right button"));
    }

    if (text.isEmpty())
    {
        return i18n("No trigger");
    }

    if (action.contains("shift"))
    {
        text.append(i18nc("As keyboard modifier", "Shift modifier"));
    }

    if (action.contains("ctrl"))
    {
        text.append(i18nc("As keyboard modifier", "Ctrl modifier"));
    }

    if (action.contains("alt"))
    {
        text.append(i18nc("As keyboard modifier", "Alt modifier"));
    }

    return text.join(" + ");
}

QWidget* TriggerDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option)

    QPushButton *triggerButton = new QPushButton(parent);
    triggerButton->setText(i18n("Click..."));

    setEditorData(triggerButton, index);

    return triggerButton;
}

bool TriggerDelegate::eventFilter(QObject *editor, QEvent *event)
{
    QPushButton *button = qobject_cast<QPushButton*>(editor);

    if (button && event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        QStringList action;

        if (mouseEvent->buttons() & Qt::LeftButton)
        {
            action.append("left");
        }

        if (mouseEvent->buttons() & Qt::MiddleButton)
        {
            action.append("middle");
        }

        if (mouseEvent->buttons() & Qt::RightButton)
        {
            action.append("right");
        }

        if (mouseEvent->modifiers() & Qt::ShiftModifier)
        {
            action.append("shift");
        }

        if (mouseEvent->modifiers() & Qt::ControlModifier)
        {
            action.append("ctrl");
        }

        if (mouseEvent->modifiers() & Qt::AltModifier)
        {
            action.append("alt");
        }

        button->setWindowTitle(action.join(QChar('+')));
        button->setToolTip(displayText(button->windowTitle()));

        if (m_parent->hasTrigger(button->windowTitle()))
        {
            button->setText(i18n("Already used..."));
        }

        return true;
    }

    return QStyledItemDelegate::eventFilter(editor, event);
}

}
