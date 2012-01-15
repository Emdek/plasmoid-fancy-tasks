/***********************************************************************************
* Fancy Tasks: Plasmoid providing a fancy representation of your tasks and launchers.
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

#include "Separator.h"
#include "Applet.h"

namespace FancyTasks
{

Separator::Separator(Plasma::Svg *svg, Applet *applet) : Plasma::SvgWidget(svg, "separator", applet),
    m_applet(applet),
    m_isVisible(true)
{
    setObjectName("FancyTasksSeparator");

    setAcceptsHoverEvents(true);

    setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

    updateOrientation();

    connect(this, SIGNAL(hoverMoved(QGraphicsWidget*, qreal)), m_applet, SLOT(itemHoverMoved(QGraphicsWidget*, qreal)));
    connect(this, SIGNAL(hoverLeft()), m_applet, SLOT(hoverLeft()));
    connect(m_applet, SIGNAL(sizeChanged(qreal)), this, SLOT(setSize(qreal)));
    connect(m_applet, SIGNAL(locationChanged()), this, SLOT(updateOrientation()));
}

void Separator::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    emit hoverMoved(this, (qreal) (((m_applet->location() == Plasma::LeftEdge || m_applet->location() == Plasma::RightEdge)?event->pos().y():event->pos().x()) / m_size));
}

void Separator::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)

    emit hoverLeft();
}

void Separator::setSize(qreal size)
{
    m_size = size;

    if (m_isVisible)
    {
        if (m_applet->location() == Plasma::LeftEdge || m_applet->location() == Plasma::RightEdge)
        {
            setPreferredSize(m_size, (m_size / 4));
        }
        else
        {
            setPreferredSize((m_size / 4), m_size);
        }
    }
}

void Separator::updateOrientation()
{
    setElementID((m_applet->location() == Plasma::LeftEdge)?"separator-west":((m_applet->location() == Plasma::RightEdge)?"separator-east":((m_applet->location() == Plasma::TopEdge)?"separator-north":"separator")));
}

void Separator::show()
{
    m_isVisible = true;

    setSize(m_size);
}

void Separator::hide()
{
    m_isVisible = false;

    setPreferredSize(0, 0);
}

QPainterPath Separator::shape() const
{
    QPainterPath path;
    QRectF rectangle;

    switch (m_applet->location())
    {
        case Plasma::LeftEdge:
            rectangle = QRectF((boundingRect().width() * 0.2), 0, boundingRect().width(), boundingRect().height());
        break;
        case Plasma::RightEdge:
            rectangle = QRectF(0, 0, (boundingRect().width() * 0.8), boundingRect().height());
        break;
        case Plasma::TopEdge:
            rectangle = QRectF(0, (boundingRect().height() * 0.2), boundingRect().width(), boundingRect().height());
        break;
        default:
            rectangle = QRectF(0, 0, boundingRect().width(), (boundingRect().height() * 0.8));
        break;
    }

    path.addRect(rectangle);

    return path;
}

bool Separator::isVisible() const
{
    return m_isVisible;
}

}
