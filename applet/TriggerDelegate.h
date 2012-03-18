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

#ifndef FANCYTASKSTRIGGERDELEGATE_HEADER
#define FANCYTASKSTRIGGERDELEGATE_HEADER

#include <QtGui/QStyledItemDelegate>

namespace FancyTasks
{

class Configuration;

class TriggerDelegate : public QStyledItemDelegate
{
    Q_OBJECT

    public:
        TriggerDelegate(Configuration *parent);

        void setEditorData(QWidget *editor, const QModelIndex &index) const;
        void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
        QString displayText(const QVariant &value, const QLocale &locale = QLocale()) const;
        QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
        bool eventFilter(QObject *editor, QEvent *event);

    private:
        Configuration *m_parent;

    signals:
        void assigned(QString trigger, QString description);
};

}

#endif
