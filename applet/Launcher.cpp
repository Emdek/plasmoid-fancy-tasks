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

#include "Launcher.h"
#include "LauncherProperties.h"
#include "Icon.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <KRun>
#include <KShell>
#include <KMessageBox>
#include <KDesktopFile>
#include <KStandardDirs>

#include <KIO/CopyJob>

namespace FancyTasks
{

Launcher::Launcher(const KUrl &url, Applet *parent) : QObject(parent),
    m_applet(parent),
    m_serviceGroup(NULL),
    m_mimeType(NULL),
    m_trashLister(NULL),
    m_trashProcess(NULL),
    m_launcherUrl(url),
    m_isExcluded(false),
    m_isExecutable(false),
    m_isMenu(false)
{
    setUrl(url);

    m_isExcluded = !m_isExecutable;

    if (m_isExecutable)
    {
        const QString command = m_executable.split(QRegExp("(?!\\\\)\\s"), QString::SkipEmptyParts).first().split(QRegExp("(?!\\\\)\\/"), QString::SkipEmptyParts).last();

        m_rules[TaskCommandRule] = LauncherRule(command, PartialMatch, false);
        m_rules[WindowClassRule] = LauncherRule(command, PartialMatch, false);
    }
}

Launcher::~Launcher()
{
    for (int i = 0; i < m_items.count(); ++i)
    {
        Icon *icon = qobject_cast<Icon*>(m_items.at(i));

        if (icon)
        {
            icon->setLauncher(NULL);
        }
    }
}

void Launcher::activate()
{
    new KRun(m_launcherUrl, NULL);
}

void Launcher::dropUrls(const KUrl::List &urls, Qt::KeyboardModifiers modifiers)
{
    if (m_serviceGroup || !urls.count())
    {
        return;
    }

    if (m_mimeType->is("inode/directory"))
    {
        Qt::DropAction dropAction = Qt::CopyAction;

        if ((modifiers & Qt::ShiftModifier && modifiers & Qt::ControlModifier) || modifiers & Qt::AltModifier)
        {
            dropAction = Qt::LinkAction;
        }
        else if (modifiers & Qt::ShiftModifier)
        {
            dropAction = Qt::MoveAction;
        }
        else if (modifiers & Qt::ControlModifier)
        {
            dropAction = Qt::CopyAction;
        }
        else
        {
            KMenu *menu = new KMenu;
            QAction *moveAction = menu->addAction(KIcon("go-jump"), i18nc("@action:inmenu", "&Move Here\t<shortcut>%1</shortcut>", QKeySequence(Qt::ShiftModifier).toString()));
            QAction *copyAction = menu->addAction(KIcon("edit-copy"), i18nc("@action:inmenu", "&Copy Here\t<shortcut>%1</shortcut>", QKeySequence(Qt::ControlModifier).toString()));
            QAction *linkAction = menu->addAction(KIcon("insert-link"), i18nc("@action:inmenu", "&Link Here\t<shortcut>%1</shortcut>", QKeySequence(Qt::ControlModifier + Qt::ShiftModifier).toString()));

            menu->addSeparator();
            menu->addAction(KIcon("process-stop"), i18nc("@action:inmenu", "Cancel"));

            QAction *activatedAction = menu->exec(QCursor::pos());

            delete menu;

            if (activatedAction == moveAction)
            {
                dropAction = Qt::MoveAction;
            }
            else if (activatedAction == copyAction)
            {
                dropAction = Qt::CopyAction;
            }
            else if (activatedAction == linkAction)
            {
                dropAction = Qt::LinkAction;
            }
            else
            {
                return;
            }
        }

        switch (dropAction)
        {
            case Qt::MoveAction:
                KIO::move(urls, m_targetUrl);
            break;
            case Qt::CopyAction:
                KIO::copy(urls, m_targetUrl);
            break;
            case Qt::LinkAction:
                KIO::link(urls, m_targetUrl);
            break;
            default:
                return;
        }
    }
    else if (m_isExecutable)
    {
        QString arguments;
        QString command;

        for (int i = 0; i < urls.count(); ++i)
        {
            arguments += ' ' + KShell::quoteArg(urls[i].isLocalFile()?urls[i].path():urls[i].prettyUrl());
        }

        if (KDesktopFile::isDesktopFile(m_launcherUrl.toLocalFile()))
        {
            KDesktopFile desktopFile(m_launcherUrl.toLocalFile());
            KConfigGroup config = desktopFile.desktopGroup();

            command = config.readPathEntry("Exec", QString());

            if (command.isEmpty())
            {
                command = KShell::quoteArg(m_launcherUrl.path());
            }
        }
        else
        {
            command = KShell::quoteArg(m_launcherUrl.path());
        }

        KRun::runCommand(command + ' ' + arguments, NULL);
    }
}

void Launcher::openUrl(QAction *action)
{
    if (!action->data().isNull())
    {
        new KRun(KUrl(action->data().toString()), NULL);
    }
}

void Launcher::startMenuEditor()
{
    KProcess::execute("kmenuedit");
}

void Launcher::emptyTrash()
{
    QWidget *widget = new QWidget;

    if (KMessageBox::warningContinueCancel(widget, i18nc("@info", "Do you really want to empty the trash? All items will be deleted."), QString(), KGuiItem(i18nc("@action:button", "Empty Trash"), KIcon("user-trash"))) == KMessageBox::Continue)
    {
        m_trashProcess = new KProcess(this);

        connect(m_trashProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(updateTrash()));

        (*m_trashProcess) << KStandardDirs::findExe("ktrash") << "--empty";

        m_trashProcess->start();
    }

    delete widget;
}

void Launcher::updateTrash()
{
    const int amount = m_trashLister->items(KDirLister::AllItems).count();

    m_title = i18n("Trash");
    m_description = (amount?i18np("One item", "%1 items", amount):i18n("Empty"));
    m_icon = KIcon(amount?"user-trash-full":"user-trash");

    ItemChanges changes = TextChanged;
    changes |= IconChanged;

    if (m_trashProcess)
    {
        m_trashProcess->deleteLater();
        m_trashProcess = NULL;
    }

    emit changed(changes);
}

void Launcher::showPropertiesDialog()
{
    LauncherProperties *propertiesDialog = new LauncherProperties(this);
    propertiesDialog->show();

    connect(propertiesDialog, SIGNAL(launcherChanged(Launcher*,KUrl)), this, SIGNAL(launcherChanged(Launcher*,KUrl)));
}

void Launcher::addItem(QObject *object)
{
    if (!m_items.contains(object))
    {
        m_items.append(object);

        connect(object, SIGNAL(destroyed(QObject*)), this, SLOT(removeItem(QObject*)));
    }

    if (m_items.count() == 1)
    {
        emit hide();
    }
}

void Launcher::removeItem(QObject *object)
{
    m_items.removeAll(object);

    if (!m_items.count())
    {
        emit show();
    }
}

void Launcher::setUrl(const KUrl &url)
{
    m_targetUrl = m_launcherUrl = url;
    m_serviceGroup = NULL;
    m_isMenu = false;

    if (url.scheme() == "menu")
    {
        m_serviceGroup = KServiceGroup::group(url.path());

        if (m_serviceGroup && m_serviceGroup->isValid())
        {
            m_executable = QString();
            m_isExecutable = false;
            m_isMenu = true;
            m_title = m_serviceGroup->caption();
            m_description = m_serviceGroup->comment();
            m_icon = KIcon(m_serviceGroup->icon());

            return;
        }
        else
        {
            m_serviceGroup = NULL;
        }
    }

    m_mimeType = KMimeType::findByUrl(m_launcherUrl);

    if (m_launcherUrl.isLocalFile() && KDesktopFile::isDesktopFile(m_launcherUrl.toLocalFile()))
    {
        KDesktopFile desktopFile(m_launcherUrl.toLocalFile());
        KConfigGroup config = desktopFile.desktopGroup();

        m_executable = config.readPathEntry("Exec", QString());
        m_title = (desktopFile.readName().isEmpty()?m_launcherUrl.fileName():desktopFile.readName());
        m_description = (desktopFile.readGenericName().isEmpty()?(desktopFile.readComment().isEmpty()?m_launcherUrl.path():desktopFile.readComment()):desktopFile.readGenericName());

        if (QFile::exists(m_launcherUrl.pathOrUrl()))
        {
            m_icon = KIcon(desktopFile.readIcon());
        }
        else
        {
            m_icon = KIcon("dialog-error");
        }

        if (m_executable.isEmpty())
        {
            QString filePath = desktopFile.readUrl();

            if (filePath.isEmpty())
            {
                filePath = desktopFile.readPath();
            }

            m_targetUrl = KUrl(filePath);
            m_mimeType = KMimeType::findByUrl(KUrl(m_targetUrl));
        }

        if (!m_trashLister && m_targetUrl == KUrl("trash:/"))
        {
            m_trashLister = new KDirLister(this);

            connect(m_trashLister, SIGNAL(completed()), this, SLOT(updateTrash()));
            connect(m_trashLister, SIGNAL(clear()), this, SLOT(updateTrash()));
            connect(m_trashLister, SIGNAL(deleteItem(const KFileItem&)), this, SLOT(updateTrash()));

            m_trashLister->setAutoUpdate(true);
            m_trashLister->openUrl(m_targetUrl);

            updateTrash();
        }
        else if (m_trashLister && m_targetUrl != KUrl("trash:/"))
        {
            delete m_trashLister;

            m_trashLister = NULL;
        }
    }
    else
    {
        m_title = m_launcherUrl.fileName();
        m_description = m_launcherUrl.path();
        m_icon = KIcon(KMimeType::iconNameForUrl(url));

        if (m_title.isEmpty())
        {
            if (m_launcherUrl.isLocalFile())
            {
                m_title = m_launcherUrl.directory();
            }
            else
            {
                m_title = m_launcherUrl.protocol();
            }
        }
    }

    m_isExecutable = (m_launcherUrl.isLocalFile() && (m_mimeType->is("application/x-executable") || m_mimeType->is("application/x-shellscript") || KDesktopFile::isDesktopFile(m_launcherUrl.toLocalFile())));

    ItemChanges changes = TextChanged;
    changes |= IconChanged;

    emit changed(changes);
}

void Launcher::setExcluded(bool excluded)
{
    m_isExcluded = excluded;
}

void Launcher::setRules(const QMap<ConnectionRule, LauncherRule> &rules)
{
    m_rules = rules;
}

void Launcher::setBrowseMenu()
{
    KMenu *menu = qobject_cast<KMenu*>(sender());

    if (menu->actions().count() > 2)
    {
        return;
    }

    QString url = menu->actions()[0]->data().toString();
    QDir dir(url);
    const QStringList entries = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::LocaleAware | QDir::DirsFirst);

    foreach (const QString &entry, entries)
    {
        QString path = url;

        if (!path.endsWith('/'))
        {
            path.append('/');
        }

        path += entry;

        QString iconName = KMimeType::iconNameForUrl(KUrl(path));

        if (QFileInfo(path).isFile())
        {
            QAction *action = menu->addAction(KIcon(iconName), entry);
            action->setData(path);
        }
        else
        {
            KMenu *subMenu = new KMenu(menu);

            QAction *action = subMenu->addAction(KIcon("document-open"), i18n("Open"));
            action->setData(path);

            subMenu->addSeparator();

            action = menu->addAction(KIcon(iconName), entry);
            action->setMenu(subMenu);

            connect(subMenu, SIGNAL(aboutToShow()), this, SLOT(setBrowseMenu()));
        }
    }
}

void Launcher::setServiceMenu()
{
    KMenu *menu = qobject_cast<KMenu*>(sender());

    if (menu->actions().count() > 1)
    {
        return;
    }

    KServiceGroup::Ptr rootGroup = KServiceGroup::group(menu->actions()[0]->data().toString());

    if (!rootGroup || !rootGroup->isValid() || rootGroup->noDisplay())
    {
        return;
    }

    KServiceGroup::List list = rootGroup->entries(true, true, true, true);

    for (int i = 0; i < list.count(); ++i)
    {
        if (list.at(i)->isType(KST_KService))
        {
            const KService::Ptr service = KService::Ptr::staticCast(list.at(i));

            QAction *action = menu->addAction(KIcon(service->icon()), service->name());
            action->setData(service->entryPath());
            action->setToolTip(service->genericName());
        }
        else if (list.at(i)->isType(KST_KServiceGroup))
        {
            const KServiceGroup::Ptr group = KServiceGroup::Ptr::staticCast(list.at(i));

            if (group->noDisplay() || group->childCount() == 0)
            {
                continue;
            }

            KMenu *subMenu = new KMenu(menu);

            QAction *action = subMenu->addAction(QString());
            action->setData(group->relPath());
            action->setVisible(false);

            action = menu->addAction(KIcon(group->icon()), group->caption());
            action->setMenu(subMenu);

            connect(subMenu, SIGNAL(aboutToShow()), this, SLOT(setServiceMenu()));
        }
        else if (list.at(i)->isType(KST_KServiceSeparator))
        {
            menu->addSeparator();
        }
    }
}

KMimeType::Ptr Launcher::mimeType()
{
    return m_mimeType;
}

KMenu* Launcher::contextMenu()
{
    KMenu *menu = new KMenu;

    if (m_isMenu)
    {
        if (KService::serviceByStorageId("kde4-kmenuedit.desktop"))
        {
            menu->addAction(i18n("Menu Editor"), this, SLOT(startMenuEditor()));
            menu->setTitle(i18n("Menu"));
        }
    }
    else
    {
        menu->addAction(KIcon("system-run"), i18n("Run"), this, SLOT(activate()));

        if (m_mimeType->is("inode/directory"))
        {
            KMenu *browseMenu = new KMenu(menu);

            QAction *action = browseMenu->addAction(KIcon("document-open"), i18n("Open"));
            action->setData(m_launcherUrl.path());

            browseMenu->addSeparator();

            action = menu->addAction(KIcon("document-preview"), i18n("Browse"));
            action->setMenu(browseMenu);

            connect(browseMenu, SIGNAL(aboutToShow()), this, SLOT(setBrowseMenu()));
            connect(browseMenu, SIGNAL(triggered(QAction*)), this, SLOT(openUrl(QAction*)));
        }

        if (m_targetUrl == KUrl("trash:/"))
        {
            menu->addAction(KIcon("trash-empty"), i18n("&Empty Trashcan"), this, SLOT(emptyTrash()))->setEnabled(m_trashLister->items(KDirLister::AllItems).count() && !m_trashProcess);
        }

        menu->addSeparator();
        menu->addAction(KIcon("document-edit"), i18n("Edit"), this, SLOT(showPropertiesDialog()));
        menu->setTitle(i18n("Launcher"));
    }

    return menu;
}

KMenu* Launcher::serviceMenu()
{
    KMenu *menu = new KMenu;

    if (!m_serviceGroup || !m_serviceGroup->isValid() || m_serviceGroup->noDisplay())
    {
        return menu;
    }

    KServiceGroup::List list = m_serviceGroup->entries(true, true, true, true);

    for (int i = 0; i < list.count(); ++i)
    {
        if (list.at(i)->isType(KST_KService))
        {
            const KService::Ptr service = KService::Ptr::staticCast(list.at(i));

            QAction *action = menu->addAction(KIcon(service->icon()), service->name());
            action->setData(service->entryPath());
            action->setToolTip(service->genericName());
        }
        else if (list.at(i)->isType(KST_KServiceGroup))
        {
            const KServiceGroup::Ptr group = KServiceGroup::Ptr::staticCast(list.at(i));

            if (group->noDisplay() || group->childCount() == 0)
            {
                continue;
            }

            KMenu *subMenu = new KMenu(menu);

            QAction *action = subMenu->addAction(QString());
            action->setData(group->relPath());
            action->setVisible(false);

            action = menu->addAction(KIcon(group->icon()), group->caption());
            action->setMenu(subMenu);

            connect(subMenu, SIGNAL(aboutToShow()), this, SLOT(setServiceMenu()));
        }
        else if (list.at(i)->isType(KST_KServiceSeparator))
        {
            menu->addSeparator();
        }
    }

    connect(menu, SIGNAL(triggered(QAction*)), this, SLOT(openUrl(QAction*)));

    return menu;
}

KIcon Launcher::icon()
{
    return m_icon;
}

KUrl Launcher::launcherUrl() const
{
    return m_launcherUrl;
}

KUrl Launcher::targetUrl() const
{
    return m_targetUrl;
}

QString Launcher::title() const
{
    return m_title;
}

QString Launcher::description() const
{
    return m_description;
}

QString Launcher::executable() const
{
    return m_executable;
}

QMap<ConnectionRule, LauncherRule> Launcher::rules() const
{
    return m_rules;
}

int Launcher::itemsAmount() const
{
    return m_items.count();
}

bool Launcher::isExecutable() const
{
    return m_isExecutable;
}

bool Launcher::isExcluded() const
{
    return m_isExcluded;
}

bool Launcher::isMenu() const
{
    return m_isMenu;
}

}
