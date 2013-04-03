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

#ifndef FANCYTASKSFINDAPPLICATIONDIALOG_HEADER
#define FANCYTASKSFINDAPPLICATIONDIALOG_HEADER

#include <KDialog>

#include "ui_findApplication.h"

namespace FancyTasks
{

class Applet;

class FindApplicationDialog : public KDialog
{
    Q_OBJECT

    public:
        FindApplicationDialog(Applet *applet, QWidget *parent);

        bool eventFilter(QObject *object, QEvent *event);

    protected:
        void showEvent(QShowEvent *event);

    protected slots:
        void findApplication(const QString &query = QString());

    private:
        Applet *m_applet;
        Ui::findApplication m_findApplicationUi;

    signals:
        void launcherClicked(QString url);
};

}

#endif
