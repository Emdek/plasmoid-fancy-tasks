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

#ifndef FANCYTASKSDROPZONE_HEADER
#define FANCYTASKSDROPZONE_HEADER

#include <QPainter>
#include <QPointer>
#include <QGraphicsWidget>

namespace FancyTasks
{

class Applet;

class DropZone : public QGraphicsWidget
{
    Q_OBJECT

    public:
        DropZone(Applet *applet);

        int index() const;
        bool isVisible() const;

    protected:
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
        void dragLeaveEvent(QGraphicsSceneDragDropEvent *event);
        void dropEvent(QGraphicsSceneDragDropEvent *event);

    public slots:
        void setSize(qreal size);
        void show(int index);
        void hide(bool force = false);

    private:
        QPointer<Applet> m_applet;
        qreal m_size;
        int m_index;
        bool m_visible;
};

}

#endif
