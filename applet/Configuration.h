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

#ifndef FANCYTASKSCONFIGURATION_HEADER
#define FANCYTASKSCONFIGURATION_HEADER

#include "Constants.h"

#include <KConfigDialog>

#include "ui_general.h"
#include "ui_appearance.h"
#include "ui_arrangement.h"
#include "ui_actions.h"

namespace FancyTasks
{

class Applet;
class Launcher;

class Configuration : public QObject
{
    Q_OBJECT

    public:
        explicit Configuration(Applet *applet, KConfigDialog *parent);
        ~Configuration();

        bool hasTrigger(const QString &trigger);

    protected:
        void connectWidgets(QWidget *widget);
        bool hasEntry(const QString &entry, bool warn = true);

    protected slots:
        void save();
        void modify();
        void moveAnimationTypeChanged(int option);
        void availableEntriesCurrentItemChanged(int row);
        void currentEntriesCurrentItemChanged(int row);
        void addItem();
        void removeItem();
        void moveUpItem();
        void moveDownItem();
        void findLauncher();
        void addLauncher(const QString &url);
        void addLauncher();
        void editLauncher();
        void changeLauncher(Launcher *launcher, const KUrl &oldUrl);
        void addMenu(QAction *action);
        void populateMenu();
        void closeActionEditors();
        void actionClicked(const QModelIndex &index);
        void actionSelectionChanged();
        void addAction();
        void removeAction();

    private:
        QPointer<Applet> m_applet;
        Launcher *m_editedLauncher;
        QMap<QString, QPair<QMap<ConnectionRule, LauncherRule>, bool> > m_rules;
        Ui::general m_generalUi;
        Ui::appearance m_appearanceUi;
        Ui::arrangement m_arrangementUi;
        Ui::actions m_actionsUi;

    signals:
        void accepted();
};

}

#endif
