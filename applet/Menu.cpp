/***********************************************************************************
* Fancy Tasks: Plasmoid providing a fancy representation of your tasks and launchers.
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

#include "Menu.h"
#include "Applet.h"
#include "Task.h"

#include <QtGui/QApplication>

#include <KWindowSystem>

namespace FancyTasks
{

Menu::Menu(Task *task, Applet *applet) : KMenu(),
    m_applet(applet),
    m_currentAction(NULL)
{
    setAcceptDrops(true);

    if (task)
    {
        QList<WId> windows = task->windows();

        for (int i = 0; i < windows.count(); ++i)
        {
            addAction(windows.at(i));
        }
    }
}

void Menu::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void Menu::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();

    QAction *action = actionAt(event->pos());

    if (action && action->data().toULongLong() && action != m_currentAction)
    {
        m_currentAction = action;

        Task *task = m_applet->taskForWindow(action->data().toULongLong());

        if (task)
        {
            task->activate();
        }
    }
}

void Menu::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event)

    close();
}

void Menu::dropEvent(QDropEvent *event)
{
    Q_UNUSED(event)

    close();
}

void Menu::mousePressEvent(QMouseEvent *event)
{
    m_dragStartPosition = event->pos();

    KMenu::mousePressEvent(event);
}

void Menu::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton && (event->pos() - m_dragStartPosition).manhattanLength() >= QApplication::startDragDistance() && activeAction() && activeAction()->data().type() == QVariant::ULongLong)
    {
        QDrag *drag = new QDrag(this);
        QMimeData *mimeData = new QMimeData();
        QByteArray data;
        const WId window = activeAction()->data().toULongLong();

        data.resize(sizeof(WId));

        memcpy(data.data(), &window, sizeof(WId));

        mimeData->setData("windowsystem/winid", data);

        drag->setMimeData(mimeData);
        drag->setPixmap(activeAction()->icon().pixmap(32, 32));

        close();

        drag->exec();
    }

    KMenu::mouseMoveEvent(event);
}

void Menu::mouseReleaseEvent(QMouseEvent *event)
{
    QAction *action = actionAt(event->pos());

    if (action && action->data().type() == QVariant::ULongLong && event->button() == Qt::RightButton)
    {
        Task *task = m_applet->taskForWindow(action->data().toULongLong());

        if (task)
        {
            if (event->button() == Qt::LeftButton)
            {
                task->activate();
            }
            else if (event->button() == Qt::MidButton)
            {
                task->close();
            }
        }
    }

    KMenu::mouseReleaseEvent(event);
}

void Menu::contextMenuEvent(QContextMenuEvent *event)
{
    QAction *action = actionAt(event->pos());

    if (action && action->data().type() == QVariant::ULongLong)
    {
        Task *task = m_applet->taskForWindow(action->data().toULongLong());

        if (task)
        {
            KMenu *menu = task->contextMenu();
            menu->exec(event->globalPos());
            menu->deleteLater();
        }
    }
}

QAction* Menu::addAction(WId window)
{
    return addAction(QIcon(KWindowSystem::icon(window)), KWindowSystem::windowInfo(window, NET::WMVisibleName).visibleName(), window);
}

QAction* Menu::addAction(const QIcon &icon, const QString &text, WId window)
{
    QAction *action = QMenu::addAction(icon, text);

    if (window)
    {
        action->setData(QVariant((qulonglong)window));
    }

    return action;
}

}
