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

#include "LauncherProperties.h"
#include "Launcher.h"
#include "RuleDelegate.h"

#include <KLocale>
#include <NETWinInfo>
#include <KWindowInfo>
#include <KWindowSystem>

#include <ksysguard/process.h>
#include <ksysguard/processes.h>

#ifdef Q_WS_X11
#include <QX11Info>

#include <X11/Xlib.h>
#include <fixx11h.h>
#endif

namespace FancyTasks
{

LauncherProperties::LauncherProperties(Launcher *launcher) : KPropertiesDialog(launcher->launcherUrl()),
    m_launcher(launcher),
    m_selector(NULL)
{
    setModal(true);
    setWindowTitle(i18n("%1 Settings", m_launcher->title()));

    QWidget *launcherRulesWidget = new QWidget(this);

    m_launcherRulesUi.setupUi(launcherRulesWidget);
    m_launcherRulesUi.connectCheckBox->setChecked(!m_launcher->isExcluded());
    m_launcherRulesUi.rulesTableWidget->setEnabled(!m_launcher->isExcluded());
    m_launcherRulesUi.detectPropertiesButton->setEnabled(!m_launcher->isExcluded());

    QList<QPair<QString, ConnectionRule> > rules;
    rules << qMakePair(i18n("Task command"), TaskCommandRule) << qMakePair(i18n("Task title"), TaskTitleRule) << qMakePair(i18n("Window class"), WindowClassRule) << qMakePair(i18n("Window role"), WindowRoleRule);

    m_launcherRulesUi.rulesTableWidget->setRowCount(rules.count());
    m_launcherRulesUi.rulesTableWidget->setItemDelegateForColumn(1, new RuleDelegate(this));

    for (int i = 0; i < rules.count(); ++i)
    {
        QTableWidgetItem *descriptionItem = new QTableWidgetItem(rules.at(i).first);
        descriptionItem->setToolTip(rules.at(i).first);
        descriptionItem->setData(Qt::UserRole, rules.at(i).second);
        descriptionItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        QTableWidgetItem *editingItem = new QTableWidgetItem();
        editingItem->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        m_launcherRulesUi.rulesTableWidget->setItem(i, 0, descriptionItem);
        m_launcherRulesUi.rulesTableWidget->setItem(i, 1, editingItem);
    }

    setRules(m_launcher->rules());
    addPage(launcherRulesWidget, i18n("Rules"));

    connect(m_launcherRulesUi.connectCheckBox, SIGNAL(toggled(bool)), m_launcherRulesUi.rulesTableWidget, SLOT(setEnabled(bool)));
    connect(m_launcherRulesUi.connectCheckBox, SIGNAL(toggled(bool)), m_launcherRulesUi.detectPropertiesButton, SLOT(setEnabled(bool)));
    connect(m_launcherRulesUi.rulesTableWidget, SIGNAL(clicked(QModelIndex)), this, SLOT(ruleClicked(QModelIndex)));
    connect(m_launcherRulesUi.detectPropertiesButton, SIGNAL(clicked()), this, SLOT(detectWindowProperties()));
    connect(this, SIGNAL(applied()), this, SLOT(accepted()));
}

void LauncherProperties::accepted()
{
    const KUrl url = m_launcher->launcherUrl();

    if (kurl() != url)
    {
        m_launcher->setUrl(kurl());
    }

    if (m_launcherRulesUi.rulesTableWidget->currentItem())
    {
        m_launcherRulesUi.rulesTableWidget->closePersistentEditor(m_launcherRulesUi.rulesTableWidget->currentItem());
    }

    QMap<ConnectionRule, LauncherRule> rules;
    QList<ConnectionRule> ruleOptions;
    ruleOptions << TaskCommandRule << TaskTitleRule << WindowClassRule << WindowRoleRule;

    for (int i = 0; i < ruleOptions.count(); ++i)
    {
        const QString rule = m_launcherRulesUi.rulesTableWidget->item(i, 1)->data(Qt::EditRole).toString();

        if (rule.isEmpty() || rule.mid(2, (rule.indexOf('+', 3) - 2)).toInt() == 0)
        {
            continue;
        }

        rules[ruleOptions.at(i)] = LauncherRule(rule.mid(rule.indexOf('+', 3) + 1), static_cast<RuleMatch>(rule.mid(2, (rule.indexOf('+', 3) - 2)).toInt()), (rule.at(0) == '1'));
    }

    m_launcher->setRules(rules);

    emit launcherChanged(m_launcher, url);
}

void LauncherProperties::ruleClicked(const QModelIndex &index)
{
    m_launcherRulesUi.rulesTableWidget->edit(m_launcherRulesUi.rulesTableWidget->model()->index(index.row(), 1));
}

void LauncherProperties::detectWindowProperties()
{
    m_selector = new QDialog(this, Qt::X11BypassWindowManagerHint);
    m_selector->installEventFilter(this);
    m_selector->move(-1000, -1000);
    m_selector->setModal(true);
    m_selector->show();
    m_selector->grabMouse(Qt::CrossCursor);
}

void LauncherProperties::setRules(const QMap<ConnectionRule, LauncherRule> &rules)
{
    QList<ConnectionRule> ruleOptions;
    ruleOptions << TaskCommandRule << TaskTitleRule << WindowClassRule << WindowRoleRule;

    for (int i = 0; i < ruleOptions.count(); ++i)
    {
        m_launcherRulesUi.rulesTableWidget->item(i, 1)->setData(Qt::EditRole, (rules.contains(ruleOptions.at(i))?QString("%1+%2+%3").arg(rules[ruleOptions.at(i)].required?'1':'0').arg(static_cast<int>(rules[ruleOptions.at(i)].match)).arg(rules[ruleOptions.at(i)].expression):QString()));
    }
}

bool LauncherProperties::eventFilter(QObject *object, QEvent *event)
{
    if (object != m_selector || event->type() != QEvent::MouseButtonRelease || static_cast<QMouseEvent*>(event)->button() != Qt::LeftButton)
    {
        return QObject::eventFilter(object, event);
    }

    m_selector->deleteLater();
    m_selector = NULL;

    Atom state = XInternAtom(QX11Info::display(), "WM_STATE", False);
    WId parent = QX11Info::appRootWindow();
    WId target = 0;
    WId root;
    WId child;
    uint mask;
    const int windows = KWindowSystem::windows().count();
    int rootX;
    int rootY;
    int x;
    int y;

    for (int i = 0; i < windows; ++i)
    {
        XQueryPointer(QX11Info::display(), parent, &root, &child, &rootX, &rootY, &x, &y, &mask);

        if (child == None)
        {
            break;
        }

        Atom type;
        int format;
        unsigned long itemsNumber;
        unsigned long bytesAfter;
        unsigned char* property;

        if (XGetWindowProperty(QX11Info::display(), child, state, 0, 0, False, AnyPropertyType, &type, &format, &itemsNumber, &bytesAfter, &property) == Success)
        {
            if (property != NULL)
            {
                XFree(property);
            }

            if (type != None)
            {
                target = child;

                break;
            }
        }

        parent = child;
    }

    if (target)
    {
        KWindowInfo window = KWindowSystem::windowInfo(target, NET::WMName, NET::WM2WindowClass);

        if (window.valid())
        {
            QString command;
            const int pid = NETWinInfo(QX11Info::display(), target, root, NET::WMPid).pid();

            KSysGuard::Processes processes;
            processes.updateAllProcesses();

            KSysGuard::Process *process = processes.getProcess(pid);

            if (process)
            {
                command = process->command;
            }

            QMap<ConnectionRule, LauncherRule> rules;

            if (!command.isEmpty())
            {
                rules[TaskCommandRule] = LauncherRule(command, ExactMatch, false);
            }

            if (!window.name().isEmpty())
            {
                rules[TaskTitleRule] = LauncherRule(window.name(), ExactMatch, false);
            }

            if (!window.windowClassClass().isEmpty())
            {
                rules[WindowClassRule] = LauncherRule(window.windowClassClass(), ExactMatch, false);
            }

            if (!window.windowClassName().isEmpty())
            {
                rules[WindowRoleRule] = LauncherRule(window.windowClassName(), ExactMatch, false);
            }

            setRules(rules);
        }
    }

    return true;
}

}
