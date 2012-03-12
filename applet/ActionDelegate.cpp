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

#include <QtGui/QHBoxLayout>
#include <QtGui/QApplication>
#include <QtGui/QTableWidgetItem>

#include <KLocale>
#include <KComboBox>

namespace FancyTasks
{

ActionDelegate::ActionDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
}

void ActionDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QStringList action = index.data(Qt::EditRole).toString().split('+', QString::KeepEmptyParts);
    KComboBox *buttonComboBox = static_cast<KComboBox*>(editor->layout()->itemAt(0)->widget());
    buttonComboBox->setCurrentIndex(0);

    KComboBox *modifierComboBox = static_cast<KComboBox*>(editor->layout()->itemAt(1)->widget());
    modifierComboBox->setCurrentIndex(0);

    if (action.count() > 0 && !action.at(0).isEmpty())
    {
        if (action.at(0) == "left")
        {
            buttonComboBox->setCurrentIndex(1);
        }
        else if (action.at(0) == "middle")
        {
            buttonComboBox->setCurrentIndex(2);
        }
        else if (action.at(0) == "right")
        {
            buttonComboBox->setCurrentIndex(3);
        }

        if (action.count() > 1)
        {
            if (action.at(1) == "ctrl")
            {
                modifierComboBox->setCurrentIndex(1);
            }
            else if (action.at(1) == "shift")
            {
                modifierComboBox->setCurrentIndex(2);
            }
            else if (action.at(1) == "alt")
            {
                modifierComboBox->setCurrentIndex(3);
            }
        }
    }
}

void ActionDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    KComboBox *buttonComboBox = static_cast<KComboBox*>(editor->layout()->itemAt(0)->widget());
    KComboBox *modifierComboBox = static_cast<KComboBox*>(editor->layout()->itemAt(1)->widget());
    QStringList buttons;
    buttons << QString() << "left" << "middle" << "right";

    QStringList modifiers;
    modifiers << QString() << "ctrl" << "shift" << "alt";

    if (buttonComboBox->currentIndex() > 0)
    {
        QString action = buttons.at(buttonComboBox->currentIndex());
        action.append('+');

        if (modifierComboBox->currentIndex() > 0)
        {
            action.append(modifiers.at(modifierComboBox->currentIndex()));
        }

        model->setData(index, action, Qt::EditRole);

    }
    else
    {
        model->setData(index, QString('+'), Qt::EditRole);
    }
}

QString ActionDelegate::displayText(const QVariant &value, const QLocale &locale) const
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

QWidget* ActionDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option)

    QWidget *editor = new QWidget(parent);
    KComboBox *buttonComboBox = new KComboBox(editor);
    buttonComboBox->setToolTip(i18n("Mouse Button"));
    buttonComboBox->addItem(i18n("Disabled"));
    buttonComboBox->addItem(i18n("Left"));
    buttonComboBox->addItem(i18n("Middle"));
    buttonComboBox->addItem(i18n("Right"));

    KComboBox *modifierComboBox = new KComboBox(editor);
    modifierComboBox->setToolTip(i18n("Modifier Key"));
    modifierComboBox->addItem(i18n("None"));
    modifierComboBox->addItem("Ctrl");
    modifierComboBox->addItem("Shift");
    modifierComboBox->addItem("Alt");

    QHBoxLayout *layout = new QHBoxLayout(editor);
    layout->addWidget(buttonComboBox);
    layout->addWidget(modifierComboBox);
    layout->setMargin(0);

    setEditorData(editor, index);

    return editor;
}

}
