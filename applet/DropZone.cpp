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

#include "DropZone.h"
#include "Applet.h"
#include "Icon.h"

#include <QtCore/QTimer>

#include <KUrl>

#include <Plasma/Theme>
#include <Plasma/PaintUtils>

namespace FancyTasks
{

DropZone::DropZone(Applet *applet) : QGraphicsWidget(applet),
    m_applet(applet),
    m_index(-1),
    m_visible(false)
{
    setObjectName("FancyTasksDropZone");

    setAcceptDrops(true);

    setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

    setPreferredSize(0, 0);

    connect(m_applet, SIGNAL(sizeChanged(qreal)), this, SLOT(setSize(qreal)));
}

void DropZone::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (!m_visible)
    {
        return;
    }

    QPainterPath path = Plasma::PaintUtils::roundedRectangle(contentsRect().adjusted(1, 1, -2, -2), 4);
    QColor color = Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor);
    color.setAlphaF(0.3);

    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillPath(path, color);
}

void DropZone::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)

    QTimer::singleShot(1500, this, SLOT(hide()));
}

void DropZone::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    Icon *icon = m_applet->iconForMimeData(event->mimeData());

    if (icon)
    {
        m_applet->itemDropped(icon, m_index);

        event->accept();
    }
    else if (KUrl::List::canDecode(event->mimeData()) && m_applet->immutability() == Plasma::Mutable)
    {
        KUrl::List droppedUrls = KUrl::List::fromMimeData(event->mimeData());

        for (int i = (droppedUrls.count() - 1); i >= 0; --i)
        {
            m_applet->addLauncher(m_applet->launcherForUrl(droppedUrls[i]), m_index);
        }

         event->accept();
    }
    else
    {
        event->ignore();
    }

    hide(true);
}

void DropZone::setSize(qreal size)
{
    m_size = size;
}

void DropZone::show(int index)
{
    m_index = index;

    QTimer::singleShot(3000, this, SLOT(hide()));

    if (m_visible)
    {
        return;
    }

    if (m_applet->formFactor() == Plasma::Vertical)
    {
        setPreferredSize(m_size, (m_size / 2));
    }
    else
    {
        setPreferredSize((m_size / 2), m_size);
    }

    m_visible = true;

    emit visibilityChanged(true);
}

void DropZone::hide(bool force)
{
    if (!m_visible)
    {
        return;
    }

    if (!force && isUnderMouse())
    {
        QTimer::singleShot(1000, this, SLOT(hide()));

        return;
    }

    m_visible = false;

    setPreferredSize(0, 0);

    emit visibilityChanged(false);
}

int DropZone::index() const
{
    return (m_visible?m_index:-1);
}

bool DropZone::isVisible() const
{
    return m_visible;
}

}
