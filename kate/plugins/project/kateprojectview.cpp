/*  This file is part of the Kate project.
 *
 *  Copyright (C) 2012 Christoph Cullmann <cullmann@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "kateprojectview.h"
#include "kateprojectpluginview.h"

KateProjectView::KateProjectView (KateProjectPluginView *pluginView, KateProject *project)
  : QTreeView ()
  , m_pluginView (pluginView)
  , m_project (project)
{
  /**
   * default style
   */
  setHeaderHidden (true);
  setEditTriggers (QAbstractItemView::NoEditTriggers);
   
   /**
    * attach view => project
    */
  setModel (m_project->model ());
  
  /**
   * connect needed signals
   */
  connect (this, SIGNAL(activated (const QModelIndex &)), this, SLOT(slotActivated (const QModelIndex &)));
}

KateProjectView::~KateProjectView ()
{
}

void KateProjectView::slotActivated (const QModelIndex &index)
{
  /**
   * open document, if any usable user data
   */
  QString filePath = index.data (Qt::UserRole).toString();
  if (!filePath.isEmpty())
    m_pluginView->mainWindow()->openUrl (KUrl::fromPath (filePath));
}

// kate: space-indent on; indent-width 2; replace-tabs on;
