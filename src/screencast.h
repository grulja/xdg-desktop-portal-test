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

#ifndef XDG_DESKTOP_PORTAL_TEST_SCREENCAST_H
#define XDG_DESKTOP_PORTAL_TEST_SCREENCAST_H

#include <QDBusAbstractAdaptor>

class QDBusObjectPath;
class ScreenCastStream;
class QSize;
class QTimer;

class ScreenCastPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.ScreenCast")
    Q_PROPERTY(uint version READ version)
    Q_PROPERTY(uint AvailableSourceTypes READ AvailableSourceTypes)
public:
    enum SourceType {
        Any = 0,
        Monitor,
        Window
    };

    typedef struct {
        uint nodeId;
        QVariantMap map;
    } Stream;
    typedef QList<Stream> Streams;

    explicit ScreenCastPortal(QObject *parent);
    ~ScreenCastPortal();

    uint version() const { return 1; }
    uint AvailableSourceTypes() const { return Any; };

public Q_SLOTS:
    uint CreateSession(const QDBusObjectPath &handle,
                       const QDBusObjectPath &session_handle,
                       const QString &app_id,
                       const QVariantMap &options,
                       QVariantMap &results);

    uint SelectSources(const QDBusObjectPath &handle,
                       const QDBusObjectPath &session_handle,
                       const QString &app_id,
                       const QVariantMap &options,
                       QVariantMap &results);

    uint Start(const QDBusObjectPath &handle,
               const QDBusObjectPath &session_handle,
               const QString &app_id,
               const QString &parent_window,
               const QVariantMap &options,
               QVariantMap &results);

private Q_SLOTS:
    void stopStreaming();

private:
    ScreenCastStream *m_stream;
    QTimer *m_timer;
    int m_frameCounter = 0;

    bool m_streamingEnabled;

};

#endif // XDG_DESKTOP_PORTAL_TEST_SCREENCAST_H


