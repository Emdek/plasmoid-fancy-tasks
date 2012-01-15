/***********************************************************************************
* Fancy Tasks: Plasmoid providing a fancy representation of your tasks and launchers.
* Copyright (C) 2009-2010 Michal Dutkiewicz aka Emdek <emdeck@gmail.com>
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

#ifndef FANCYPANEL_HEADER
#define FANCYPANEL_HEADER

#include <QGraphicsSceneContextMenuEvent>

#include <Plasma/Containment>

class FancyPanel : public Plasma::Containment
{
    Q_OBJECT

    public:
        FancyPanel(QObject *parent, const QVariantList &args);
        ~FancyPanel();

        void init();
        Plasma::Applet* addApplet(const QString &name, const QVariantList &args = QVariantList(), const QRectF &geometry = QRectF(-1, -1, -1, -1));

    public slots:
        void setSize(QSize size);

    protected:
        void constraintsEvent(Plasma::Constraints constraints);
        void contextMenuEvent(QGraphicsSceneContextMenuEvent *event);

    private:
        Plasma::Applet *m_applet;
};

#endif
