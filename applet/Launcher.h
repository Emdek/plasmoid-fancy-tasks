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

#ifndef FANCYTASKSLAUNCHER_HEADER
#define FANCYTASKSLAUNCHER_HEADER

#include "Applet.h"

#include <QtCore/QPointer>
#include <QtGui/QGraphicsSceneDragDropEvent>

#include <KUrl>
#include <KMenu>
#include <KIcon>
#include <KProcess>
#include <KDirLister>
#include <KServiceGroup>

namespace FancyTasks
{

class Applet;

class Launcher : public QObject
{
    Q_OBJECT

    public:
        Launcher(const KUrl &url, Applet *applet);
        ~Launcher();

        void dropUrls(const KUrl::List &urls, Qt::KeyboardModifiers modifiers);
        void addItem(QObject *object);
        KMimeType::Ptr mimeType();
        KMenu* contextMenu();
        KMenu* serviceMenu();
        KIcon icon();
        KUrl launcherUrl() const;
        KUrl targetUrl() const;
        QString title() const;
        QString description() const;
        QString executable() const;
        QMap<ConnectionRule, LauncherRule> rules() const;
        int itemsAmount() const;
        bool isExcluded() const;
        bool isExecutable() const;
        bool isMenu() const;

    public slots:
        void setUrl(const KUrl &url);
        void setExcluded(bool excluded);
        void setRules(const QMap<ConnectionRule, LauncherRule> &rules);
        void setBrowseMenu();
        void setServiceMenu();
        void activate();
        void openUrl(QAction *action);
        void startMenuEditor();
        void emptyTrash();
        void updateTrash();
        void removeItem(QObject *object);
        void showPropertiesDialog();

    private:
        QPointer<Applet> m_applet;
        KServiceGroup::Ptr m_serviceGroup;
        KMimeType::Ptr m_mimeType;
        KDirLister *m_trashLister;
        KProcess *m_trashProcess;
        KUrl m_launcherUrl;
        KUrl m_targetUrl;
        KIcon m_icon;
        QString m_title;
        QString m_description;
        QString m_executable;
        QMap<ConnectionRule, LauncherRule> m_rules;
        QList<QObject*> m_items;
        bool m_isExcluded;
        bool m_isExecutable;
        bool m_isMenu;

    signals:
        void changed(ItemChanges changes);
        void launcherChanged(Launcher *launcher, KUrl oldUrl);
        void hide();
        void show();
};

}

#endif
