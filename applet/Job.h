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

#ifndef FANCYTASKSJOB_HEADER
#define FANCYTASKSJOB_HEADER

#include "Constants.h"

#include <KIcon>
#include <KMenu>

#include <Plasma/Service>
#include <Plasma/DataEngine>

namespace FancyTasks
{

class Applet;

class Job : public QObject
{
    Q_OBJECT

    public:
        Job(const QString &job, Applet *applet);

        JobState state() const;
        KMenu* contextMenu();
        KIcon icon();
        QString title() const;
        QString description() const;
        QString application() const;
        QString information() const;
        int percentage() const;
        bool closeOnFinish() const;

    public slots:
        void dataUpdated(const QString &name, const Plasma::DataEngine::Data &data);
        void setFinished(bool finished);
        void setCloseOnFinish(bool close);
        void suspend();
        void resume();
        void stop();
        void close();
        void destroy();

    private:
        QPointer<Applet> m_applet;
        QString m_job;
        QString m_title;
        QString m_description;
        QString m_information;
        QString m_application;
        QString m_iconName;
        JobState m_state;
        int m_percentage;
        bool m_closeOnFinish;
        bool m_killable;
        bool m_suspendable;

    signals:
        void changed(ItemChanges changes);
        void demandsAttention();
        void close(Job*);
};

}

#endif
