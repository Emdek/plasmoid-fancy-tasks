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

#ifndef FANCYTASKSCONFIGURATION_HEADER
#define FANCYTASKSCONFIGURATION_HEADER

#include <QtCore/QPointer>

#include <KConfigDialog>

#include "ui_general.h"
#include "ui_appearance.h"
#include "ui_arrangement.h"
#include "ui_actions.h"
#include "ui_findApplication.h"

namespace FancyTasks
{

class Applet;

class Configuration : public QObject
{
    Q_OBJECT

    public:
        Configuration(Applet *applet, KConfigDialog *parent);

        bool eventFilter(QObject *object, QEvent *event);

    public slots:
        void accepted();
        void moveAnimationTypeChanged(int option);
        void availableActionsCurrentItemChanged(int row);
        void currentActionsCurrentItemChanged(int row);
        void removeItem();
        void addItem();
        void moveUpItem();
        void moveDownItem();
        void addLauncher();
        void addLauncher(const QString &url);
        void addMenu(QAction *action);
        void setServiceMenu();
        void findApplication(const QString &query);
        void closeFindApplicationDialog();

    private:
        QPointer<Applet> m_applet;
        KDialog *m_findApplicationDialog;
        Ui::general m_generalUi;
        Ui::appearance m_appearanceUi;
        Ui::arrangement m_arrangementUi;
        Ui::actions m_actionsUi;
        Ui::findApplication m_findApplicationUi;

    signals:
        void finished();
};

}

#endif
