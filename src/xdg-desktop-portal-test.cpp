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

#include <QCoreApplication>
#include <QDBusConnection>
#include <QLoggingCategory>

#include "desktopportal.h"

Q_LOGGING_CATEGORY(XdgDesktopPortalTest, "xdp-test")

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QDBusConnection sessionBus = QDBusConnection::sessionBus();

    if (sessionBus.registerService(QStringLiteral("org.freedesktop.impl.portal.desktop.test"))) {
        DesktopPortal *desktopPortal = new DesktopPortal(&a);
        if (sessionBus.registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), desktopPortal, QDBusConnection::ExportAdaptors)) {
            qCDebug(XdgDesktopPortalTest) << "Desktop portal registered successfully";
        } else {
            qCDebug(XdgDesktopPortalTest) << "Failed to register desktop portal";
        }
    } else {
        qCDebug(XdgDesktopPortalTest) << "Failed to register org.freedesktop.impl.portal.desktop.test service";
    }

    return a.exec();
}

