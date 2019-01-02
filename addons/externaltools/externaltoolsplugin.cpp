/* This file is part of the KDE project
   Copyright (C) 2001 Christoph Cullmann <cullmann@kde.org>
   Copyright (C) 2002 Joseph Wenninger <jowenn@kde.org>
   Copyright (C) 2002 Anders Lund <anders.lund@lund.tdcadsl.dk>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "externaltoolsplugin.h"

#include <QIcon>
#include <QDebug>
#include <KTextEditor/Application>
#include <KIconLoader>
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include <QAction>
#include <KActionCollection>
#include <kparts/part.h>

#include <kmessagebox.h>

#include <KAboutData>
#include <KAuthorized>
#include <KPluginFactory>
#include <KXMLGUIFactory>

K_PLUGIN_FACTORY_WITH_JSON(KateExternalToolsFactory, "externaltoolsplugin.json", registerPlugin<KateExternalToolsPlugin>();)

KateExternalToolsPlugin::KateExternalToolsPlugin(QObject* parent, const QList<QVariant>&)
    : KTextEditor::Plugin(parent)
{
    if (KAuthorized::authorizeAction(QStringLiteral("shell_access"))) {
        m_command = new KateExternalToolsCommand(this);
    }
}

KateExternalToolsPlugin::~KateExternalToolsPlugin()
{
    if (KAuthorized::authorizeAction(QStringLiteral("shell_access"))) {
        if (m_command) {
            delete m_command;
        }
    }
}

QObject* KateExternalToolsPlugin::createView(KTextEditor::MainWindow* mainWindow)
{
    KateExternalToolsPluginView* view = new KateExternalToolsPluginView(mainWindow);
    connect(view, SIGNAL(destroyed(QObject*)), this, SLOT(viewDestroyed(QObject*)));
    m_views.append(view);
    return view;
}

KateExternalToolsPluginView* KateExternalToolsPlugin::extView(QWidget* widget)
{
    foreach (KateExternalToolsPluginView* view, m_views) {
        if (view->mainWindow()->window() == widget)
            return view;
    }
    return nullptr;
}

void KateExternalToolsPlugin::viewDestroyed(QObject* view)
{
    m_views.removeAll(dynamic_cast<KateExternalToolsPluginView*>(view));
}

void KateExternalToolsPlugin::reload()
{
    if (KAuthorized::authorizeAction(QStringLiteral("shell_access"))) {
        if (m_command)
            m_command->reload();
    }
    foreach (KateExternalToolsPluginView* view, m_views) {
        view->rebuildMenu();
    }
}

int KateExternalToolsPlugin::configPages() const
{
    return 1;
}

KTextEditor::ConfigPage* KateExternalToolsPlugin::configPage(uint number, QWidget* parent, const char* name)
{
    if (number == 0) {
        return new KateExternalToolsConfigWidget(parent, this, name);
    }
    return nullptr;
}

QString KateExternalToolsPlugin::configPageName(uint number) const
{
    if (number == 0) {
        return i18n("External Tools");
    }
    return QString();
}

QString KateExternalToolsPlugin::configPageFullName(uint number) const
{
    if (number == 0) {
        return i18n("External Tools");
    }
    return QString();
}

QIcon KateExternalToolsPlugin::configPageIcon(uint number) const
{
    if (number == 0) {
        return QIcon();
    }
    return QIcon();
}

KateExternalToolsPluginView::KateExternalToolsPluginView(KTextEditor::MainWindow* mainWindow)
    : QObject(mainWindow)
    , KXMLGUIClient(KateExternalToolsFactory::componentData())
    , m_mainWindow(mainWindow)
{
    if (KAuthorized::authorizeAction(QStringLiteral("shell_access"))) {
        externalTools = new KateExternalToolsMenuAction(i18n("External Tools"), actionCollection(), mainWindow, mainWindow);
        actionCollection()->addAction(QStringLiteral("tools_external"), externalTools);
        externalTools->setWhatsThis(i18n("Launch external helper applications"));
    }

    mainWindow->guiFactory()->addClient(this);
}

void KateExternalToolsPluginView::rebuildMenu()
{
    if (externalTools) {
        KXMLGUIFactory* f = factory();
        f->removeClient(this);
        reloadXML();
        externalTools->reload();
        qDebug() << "has just returned from externalTools->reload()";
        f->addClient(this);
    }
}

KateExternalToolsPluginView::~KateExternalToolsPluginView()
{
    m_mainWindow->guiFactory()->removeClient(this);

    delete externalTools;
    externalTools = nullptr;
}

#include "externaltoolsplugin.moc"

// kate: space-indent on; indent-width 2; replace-tabs on;
