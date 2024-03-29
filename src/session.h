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

#ifndef XDG_DESKTOP_PORTAL_TEST_SESSION_H
#define XDG_DESKTOP_PORTAL_TEST_SESSION_H

#include <QDBusVirtualObject>

class Session : public QDBusVirtualObject
{
    Q_OBJECT
public:
    explicit Session(QObject *parent = nullptr, const QString &appId = QString(), const QString &path = QString());
    ~Session();

    enum SessionType {
        ScreenCast = 0,
        RemoteDesktop = 1
    };

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override;
    QString introspect(const QString &path) const override;

    bool close();
    virtual SessionType type() const = 0;

    static Session *createSession(QObject *parent, SessionType type, const QString &appId, const QString &path);
    static Session *getSession(const QString &sessionHandle);

Q_SIGNALS:
    void closed();

private:
    const QString m_appId;
    const QString m_path;
};

class ScreenCastSession : public Session
{
    Q_OBJECT
public:
    explicit ScreenCastSession(QObject *parent = nullptr, const QString &appId = QString(), const QString &path = QString());
    ~ScreenCastSession();

    bool multipleSources() const;
    void setMultipleSources(bool multipleSources);

    SessionType type() const override { return SessionType::ScreenCast; }

private:
    bool m_multipleSources;
    // TODO type
};

#endif // XDG_DESKTOP_PORTAL_TEST_SESSION_H

