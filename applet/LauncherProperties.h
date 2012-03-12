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

#ifndef FANCYTASKSLAUNCHERPROPERTIES_HEADER
#define FANCYTASKSLAUNCHERPROPERTIES_HEADER

#include "Applet.h"

#include <KPropertiesDialog>

#include "ui_launcherRules.h"

namespace FancyTasks
{

class Launcher;

class LauncherProperties : public KPropertiesDialog
{
    Q_OBJECT

    public:
        LauncherProperties(Launcher *launcher);

        bool eventFilter(QObject *object, QEvent *event);

    protected:
        void setRules(const QMap<ConnectionRule, LauncherRule> &rules);

    protected slots:
        void accepted();
        void detectWindowProperties();
        void ruleClicked(const QModelIndex &index);

    private:
        Launcher *m_launcher;
        QDialog *m_selector;
        Ui::launcherRules m_launcherRulesUi;

    signals:
        void launcherChanged(Launcher *launcher, KUrl oldUrl);
};

}

#endif
