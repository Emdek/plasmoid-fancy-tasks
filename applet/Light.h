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

#ifndef FANCYTASKSLIGHT_HEADER
#define FANCYTASKSLIGHT_HEADER

#include <QtCore/QPointer>
#include <QtCore/QTimerEvent>
#include <QtGui/QGraphicsWidget>
#include <QtGui/QGraphicsSceneHoverEvent>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QGraphicsSceneDragDropEvent>
#include <QtGui/QGraphicsSceneContextMenuEvent>

#include <Plasma/Svg>

namespace FancyTasks
{

class Applet;
class Icon;
class Task;

class Light : public QGraphicsWidget
{
    Q_OBJECT

    public:
        explicit Light(Task *task, Applet *applet, Icon *icon);

    public slots:
        void toolTipAboutToShow();
        void toolTipHidden();
        void setSize(qreal size);

    protected:
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
        void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
        void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
        void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
        void dragLeaveEvent(QGraphicsSceneDragDropEvent *event);
        void mousePressEvent(QGraphicsSceneMouseEvent *event);
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
        void contextMenuEvent(QGraphicsSceneContextMenuEvent *event);
        void timerEvent(QTimerEvent *event);

    private:
        QPointer<Applet> m_applet;
        QPointer<Task> m_task;
        QPointer<Icon> m_icon;
        WId m_window;
        int m_dragTimer;
        int m_highlightTimer;
};

}

#endif
