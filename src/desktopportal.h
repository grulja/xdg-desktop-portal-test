/*
 * Copyright © 2019 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Jan Grulich <jgrulich@redhat.com>
 */

#ifndef XDG_DESKTOP_PORTAL_TEST_DESKTOP_PORTAL_H
#define XDG_DESKTOP_PORTAL_TEST_DESKTOP_PORTAL_H

#include <QObject>
#include <QDBusVirtualObject>
#include <QDBusContext>

#include "screencast.h"

class DesktopPortal : public QObject, public QDBusContext
{
    Q_OBJECT
public:
    explicit DesktopPortal(QObject *parent = nullptr);
    ~DesktopPortal();

private:
    ScreenCastPortal *m_screenCast;

};

#endif // XDG_DESKTOP_PORTAL_KDE_DESKTOP_PORTAL_H

