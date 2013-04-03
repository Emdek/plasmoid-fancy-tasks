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

#include "Job.h"
#include "Applet.h"

namespace FancyTasks
{

Job::Job(const QString &job, Applet *applet) : QObject(applet),
    m_applet(applet),
    m_job(job),
    m_state(UnknownState),
    m_percentage(-1),
    m_closeOnFinish(false)
{
    m_applet->dataEngine("applicationjobs")->connectSource(m_job, this, 250, Plasma::NoAlignment);

    dataUpdated("", m_applet->dataEngine("applicationjobs")->query(m_job));
}

void Job::dataUpdated(const QString &source, const Plasma::DataEngine::Data &data)
{
    Q_UNUSED(source)

    ItemChanges changes = TextChanged;

    if (m_iconName.isEmpty())
    {
        m_iconName = data["appIconName"].toString();

        changes |= IconChanged;
    }

    m_title = data["infoMessage"].toString();
    m_application = data["appName"].toString();
    m_percentage = (data.contains("percentage")?data["percentage"].toInt():-1);
    m_suspendable = data["suspendable"].toBool();
    m_killable = data["killable"].toBool();

    QString state = data["state"].toString();
    JobState previousState = m_state;

    if (!data["error"].toString().isEmpty())
    {
        m_title = data["error"].toString();

        if (m_state != ErrorState)
        {
            emit demandsAttention();
        }

        m_state = ErrorState;
    }
    else if (state == "running" && m_percentage > 0)
    {
        m_state = RunningState;
    }
    else if (state == "suspended")
    {
        m_state = SuspendedState;
    }
    else if (state == "stopped")
    {
        m_state = FinishedState;
    }

    QString description;

    if (m_state == RunningState || m_state == SuspendedState || m_state == UnknownState)
    {
        m_title = ((m_state == SuspendedState)?i18n("%1 [Paused]", m_title):m_title);

        if (data["eta"].toUInt() > 0)
        {
            description.append(i18n("%1 (%2 remaining) ", data["speed"].toString(), KGlobal::locale()->prettyFormatDuration(data["eta"].toUInt())) + "<br />");
        }
    }
    else
    {
        setFinished(true);
    }

    m_description.clear();
    m_description.append(description);

    int i = 0;

    while (data.contains(QString("label%1").arg(i)))
    {
        m_description.append(data[QString("labelName%1").arg(i)].toString() + ": <i>" + data[QString("label%1").arg(i)].toString() + "</i><br>");

        ++i;
    }

    m_description = m_description.left(m_description.size() - 4);
    m_information = QString("<b>%1</b> %2").arg(m_percentage?QString("%1 %2%").arg(m_title).arg(m_percentage):m_title).arg(description);

    if (previousState != m_state)
    {
        changes |= StateChanged;
    }

    emit changed(changes);
}

void Job::setFinished(bool finished)
{
    if (finished)
    {
        m_title = i18n("%1 [Finished]", m_title);
        m_information = QString("<b>%1</b>").arg(m_title);
        m_state = FinishedState;
    }

    emit demandsAttention();
}

void Job::setCloseOnFinish(bool close)
{
    m_closeOnFinish = close;
}

void Job::suspend()
{
    Plasma::Service *service = m_applet->dataEngine("applicationjobs")->serviceForSource(m_job);
    service->startOperationCall(service->operationDescription("suspend"));
}

void Job::resume()
{
    Plasma::Service *service = m_applet->dataEngine("applicationjobs")->serviceForSource(m_job);
    service->startOperationCall(service->operationDescription("resume"));
}

void Job::stop()
{
    Plasma::Service *service = m_applet->dataEngine("applicationjobs")->serviceForSource(m_job);
    service->startOperationCall(service->operationDescription("stop"));
}

void Job::close()
{
    m_applet->removeJob(m_job, true);
}

void Job::destroy()
{
    emit close(this);

    deleteLater();
}

KMenu* Job::contextMenu()
{
    KMenu *menu = new KMenu;

    if (m_state == FinishedState || m_state == ErrorState)
    {
        menu->addAction(KIcon("window-close"), i18n("Close Job"), this, SLOT(close()));
    }
    else
    {
        if (m_suspendable && m_state != UnknownState)
        {
            if (m_state == RunningState)
            {
                menu->addAction(KIcon("media-playback-pause"), i18n("Pause Job"), this, SLOT(suspend()));
            }
            else
            {
                menu->addAction(KIcon("media-playback-start"), i18n("Resume Job"), this, SLOT(resume()));
            }
        }

        if (m_killable)
        {
            if (m_state != UnknownState)
            {
                menu->addSeparator();
            }

            menu->addAction(KIcon("media-playback-stop"), i18n("Cancel Job"), this, SLOT(stop()))->setEnabled(m_state != FinishedState && m_state != ErrorState);
        }
    }

    menu->addSeparator();

    QAction *closeOnFinishAction = menu->addAction(i18n("Close on Finish"));
    closeOnFinishAction->setCheckable(true);
    closeOnFinishAction->setChecked(m_closeOnFinish && m_state != ErrorState);
    closeOnFinishAction->setEnabled(m_state != FinishedState && m_state != ErrorState);

    connect(closeOnFinishAction, SIGNAL(toggled(bool)), this, SLOT(setCloseOnFinish(bool)));

    return menu;
}

JobState Job::state() const
{
    return m_state;
}

KIcon Job::icon()
{
    return KIcon(m_iconName);
}

QString Job::title() const
{
    return m_title;
}

QString Job::description() const
{
    return m_description;
}

QString Job::information() const
{
    return m_information;
}

QString Job::application() const
{
    return m_application;
}

int Job::percentage() const
{
    return m_percentage;
}

bool Job::closeOnFinish() const
{
    return m_closeOnFinish;
}

}
