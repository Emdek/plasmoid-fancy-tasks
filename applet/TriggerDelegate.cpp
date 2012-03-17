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

#include "TriggerDelegate.h"

#include <QtGui/QPushButton>

#include <KLocale>

namespace FancyTasks
{

TriggerDelegate::TriggerDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
}

void TriggerDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QPushButton *triggerButton = static_cast<QPushButton*>(editor);
    triggerButton->setToolTip(displayText(index.data(Qt::EditRole)));
    triggerButton->setWindowTitle(index.data(Qt::EditRole).toString());
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

    if (!value.toString().contains('+'))
    {
        return value.toString();
    }

    QStringList action = value.toString().split('+');
    QString text;

    if (action.count() > 0 && !action.at(0).isEmpty())
    {
        if (action.at(0) == "left")
        {
            text = i18n("Left mouse button");
        }
        else if (action.at(0) == "middle")
        {
            text = i18n("Middle mouse button");
        }
        else if (action.at(0) == "right")
        {
            text = i18n("Right mouse button");
        }

        if (action.count() > 1 && !action.at(1).isEmpty())
        {
            text.append(" + ");

            if (action.at(1) == "ctrl")
            {
                text.append(i18n("Ctrl modifier"));
            }
            else if (action.at(1) == "shift")
            {
                text.append(i18n("Shift modifier"));
            }
            else if (action.at(1) == "alt")
            {
                text.append(i18n("Alt modifier"));
            }
       }
    }

    return text;
}

QWidget* TriggerDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option)

    QPushButton *triggerButton = new QPushButton(parent);
    triggerButton->setText(i18n("Click..."));

    setEditorData(triggerButton, index);

    return triggerButton;
}

}
