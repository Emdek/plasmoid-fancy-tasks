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

#include "Panel.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QGraphicsLinearLayout>

#include <KIcon>
#include <KMenu>
#include <KConfigGroup>

K_EXPORT_PLASMA_APPLET(fancypanel, FancyPanel)

FancyPanel::FancyPanel(QObject *parent, const QVariantList &args) : Containment(parent, args),
    m_applet(NULL)
{
    KGlobal::locale()->insertCatalog("fancypanel");

    setBackgroundHints(NoBackground);

    setZValue(150);

    setObjectName("FancyPanel");

    setSize(QSize(100, 100));
}

FancyPanel::~FancyPanel()
{
    if (m_applet)
    {
        KConfigGroup panelConfiguration = config();
        KConfigGroup appletConfiguration(&panelConfiguration, "Applet");

        m_applet->save(appletConfiguration);
    }
}

void FancyPanel::init()
{
    setContainmentType(Containment::PanelContainment);

    Containment::init();

    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    setLayout(layout);

    setContentsMargins(0, 0, 0, 0);

    constraintsEvent(Plasma::LocationConstraint);

    m_applet = Plasma::Applet::load("fancytasks");

    if (m_applet)
    {
        KConfigGroup panelConfiguration = config();
        KConfigGroup appletConfiguration(&panelConfiguration, "Applet");

        layout->addItem(m_applet);

        m_applet->init();
        m_applet->restore(appletConfiguration);

        connect(m_applet, SIGNAL(sizeChanged(QSize)), this, SLOT(setSize(QSize)));
    }

    setDrawWallpaper(false);
}

void FancyPanel::constraintsEvent(Plasma::Constraints constraints)
{
    if (constraints & Plasma::LocationConstraint)
    {
        setFormFactor(Plasma::Horizontal);

        setLocation(Plasma::BottomEdge);
    }

    setBackgroundHints(NoBackground);

    enableAction("add widgets", false);
    enableAction("add space", false);
}

void FancyPanel::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    Q_UNUSED(event)

    KMenu *menu = new KMenu;

    if (m_applet)
    {
        menu->addAction(KIcon("configure"), i18n("Configure"), m_applet, SLOT(showConfigurationInterface()));
    }

    if (immutability() == Plasma::Mutable)
    {
        menu->addAction(KIcon("configure"), i18n("Configure Panel"), this, SIGNAL(toolBoxToggled()));
        menu->addSeparator();
        menu->addAction(KIcon("edit-delete"), i18n("Remove Panel"), this, SLOT(destroy()));
    }

    if (menu->actions().count())
    {
        menu->exec(QCursor::pos());
    }

    delete menu;
}

void FancyPanel::setSize(QSize size)
{
    if (size.width() > QApplication::desktop()->screenGeometry().size().width() || size.height() > QApplication::desktop()->screenGeometry().size().height())
    {
        if (formFactor() == Plasma::Vertical)
        {
            size = QSize((QApplication::desktop()->screenGeometry().size().width() / 10), (QApplication::desktop()->screenGeometry().size().height() / 3));
        }
        else
        {
            size = QSize((QApplication::desktop()->screenGeometry().size().width() / 3), (QApplication::desktop()->screenGeometry().size().height() / 10));
        }
    }

    setMinimumSize(size);
    setMaximumSize(size);
}

Plasma::Applet* FancyPanel::addApplet(const QString &name, const QVariantList &args, const QRectF &geometry)
{
    Q_UNUSED(name)
    Q_UNUSED(args)
    Q_UNUSED(geometry)

    return NULL;
}
