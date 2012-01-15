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

#ifndef FANCYTASKSSEPARATOR_HEADER
#define FANCYTASKSSEPARATOR_HEADER

#include <QPointer>
#include <QGraphicsSceneMouseEvent>

#include <Plasma/SvgWidget>

namespace FancyTasks
{

class Applet;

class Separator : public Plasma::SvgWidget
{
    Q_OBJECT

    public:
        Separator(Plasma::Svg *svg, Applet *applet);

        QPainterPath shape() const;
        bool isVisible() const;

    protected:
        void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
        void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

    public slots:
        void setSize(qreal size);
        void updateOrientation();
        void show();
        void hide();

    private:
        QPointer<Applet> m_applet;
        qreal m_size;
        bool m_isVisible;

    signals:
        void hoverMoved(QGraphicsWidget *item, qreal across);
        void hoverLeft();
};

}

#endif
