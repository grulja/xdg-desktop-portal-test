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

#include "screencast.h"
#include "screencaststream.h"
#include "session.h"

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QEventLoop>
#include <QLoggingCategory>
#include <QImage>
#include <QSize>
#include <QTimer>

Q_LOGGING_CATEGORY(XdgDesktopPortalTestScreenCast, "xdp-test-screencast")

const QDBusArgument &operator >> (const QDBusArgument &arg, ScreenCastPortal::Stream &stream)
{
    arg.beginStructure();
    arg >> stream.nodeId;

    arg.beginMap();
    while (!arg.atEnd()) {
        QString key;
        QVariant map;
        arg.beginMapEntry();
        arg >> key >> map;
        arg.endMapEntry();
        stream.map.insert(key, map);
    }
    arg.endMap();
    arg.endStructure();

    return arg;
}

const QDBusArgument &operator << (QDBusArgument &arg, const ScreenCastPortal::Stream &stream)
{
    arg.beginStructure();
    arg << stream.nodeId;
    arg << stream.map;
    arg.endStructure();

    return arg;
}

Q_DECLARE_METATYPE(ScreenCastPortal::Stream)
Q_DECLARE_METATYPE(ScreenCastPortal::Streams)

ScreenCastPortal::ScreenCastPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    qDBusRegisterMetaType<ScreenCastPortal::Stream>();
    qDBusRegisterMetaType<ScreenCastPortal::Streams>();
}

ScreenCastPortal::~ScreenCastPortal()
{
    if (m_timer)
        delete m_timer;

    if (m_stream)
        delete m_stream;
}

uint ScreenCastPortal::CreateSession(const QDBusObjectPath &handle,
                                     const QDBusObjectPath &session_handle,
                                     const QString &app_id,
                                     const QVariantMap &options,
                                     QVariantMap &results)
{
    Q_UNUSED(results)

    qCDebug(XdgDesktopPortalTestScreenCast) << "CreateSession called with parameters:";
    qCDebug(XdgDesktopPortalTestScreenCast) << "    handle: " << handle.path();
    qCDebug(XdgDesktopPortalTestScreenCast) << "    session_handle: " << session_handle.path();
    qCDebug(XdgDesktopPortalTestScreenCast) << "    app_id: " << app_id;
    qCDebug(XdgDesktopPortalTestScreenCast) << "    options: " << options;

    Session *session = Session::createSession(this, Session::ScreenCast, app_id, session_handle.path());

    if (!session) {
        return 2;
    }

    connect(session, &Session::closed, [this] () {
        stopStreaming();
    });

    return 0;
}

uint ScreenCastPortal::SelectSources(const QDBusObjectPath &handle,
                                     const QDBusObjectPath &session_handle,
                                     const QString &app_id,
                                     const QVariantMap &options,
                                     QVariantMap &results)
{
    Q_UNUSED(results)

    qCDebug(XdgDesktopPortalTestScreenCast) << "SelectSource called with parameters:";
    qCDebug(XdgDesktopPortalTestScreenCast) << "    handle: " << handle.path();
    qCDebug(XdgDesktopPortalTestScreenCast) << "    session_handle: " << session_handle.path();
    qCDebug(XdgDesktopPortalTestScreenCast) << "    app_id: " << app_id;
    qCDebug(XdgDesktopPortalTestScreenCast) << "    options: " << options;

    uint types = Monitor;

    ScreenCastSession *session = qobject_cast<ScreenCastSession*>(Session::getSession(session_handle.path()));

    if (!session) {
        qCWarning(XdgDesktopPortalTestScreenCast) << "Tried to select sources on non-existing session " << session_handle.path();
        return 2;
    }

    if (options.contains(QStringLiteral("multiple"))) {
        session->setMultipleSources(options.value(QStringLiteral("multiple")).toBool());
    }

    if (options.contains(QStringLiteral("types"))) {
        types = (SourceType)(options.value(QStringLiteral("types")).toUInt());
    }

    return 0;
}

uint ScreenCastPortal::Start(const QDBusObjectPath &handle,
                             const QDBusObjectPath &session_handle,
                             const QString &app_id,
                             const QString &parent_window,
                             const QVariantMap &options,
                             QVariantMap &results)
{
    Q_UNUSED(results)

    qCDebug(XdgDesktopPortalTestScreenCast) << "Start called with parameters:";
    qCDebug(XdgDesktopPortalTestScreenCast) << "    handle: " << handle.path();
    qCDebug(XdgDesktopPortalTestScreenCast) << "    session_handle: " << session_handle.path();
    qCDebug(XdgDesktopPortalTestScreenCast) << "    app_id: " << app_id;
    qCDebug(XdgDesktopPortalTestScreenCast) << "    parent_window: " << parent_window;
    qCDebug(XdgDesktopPortalTestScreenCast) << "    options: " << options;

    ScreenCastSession *session = qobject_cast<ScreenCastSession*>(Session::getSession(session_handle.path()));

    if (!session) {
        qCWarning(XdgDesktopPortalTestScreenCast) << "Tried to call start on non-existing session " << session_handle.path();
        return 2;
    }

    m_stream = new ScreenCastStream(QSize(8, 8));
    m_stream->init();

    connect(m_stream, &ScreenCastStream::startStreaming, this, [this] () {
        m_streamingEnabled = true;
        if (m_timer)
            m_timer->start();
    });

    connect(m_stream, &ScreenCastStream::stopStreaming, this, &ScreenCastPortal::stopStreaming);

    bool streamReady = false;
    QEventLoop loop;
    connect(m_stream, &ScreenCastStream::streamReady, this, [&loop, &streamReady] {
        loop.quit();
        streamReady = true;
    });

    // HACK wait for stream to be ready
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    loop.exec();

    disconnect(m_stream, &ScreenCastStream::streamReady, this, nullptr);

    if (!streamReady) {
        if (m_stream) {
            delete m_stream;
            m_stream = nullptr;
        }
        return 2;
    }

    m_timer = new QTimer(this);
    m_timer->setInterval(2000);
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, [this] () {
        QImage capture = QImage(QSize(8, 8), QImage::Format_RGBA8888);
        switch (m_frameCounter) {
            case 0:
                capture.fill(QColor("red"));
                break;
            case 1:
                capture.fill(QColor("green"));
                break;
            case 2:
                capture.fill(QColor("blue"));
                break;
            default:
                capture.fill(QColor("black"));
        }
        m_frameCounter++;
        if (!m_stream->writeFrame(capture.bits()))
            qCWarning(XdgDesktopPortalTestScreenCast) << "Failed to write frame";
    });

    Stream stream;
    stream.nodeId = m_stream->nodeId();
    stream.map = QVariantMap({{QLatin1String("size"), QSize(8, 8)}});
    QVariant streams = QVariant::fromValue<Streams>({stream});

    if (!streams.isValid()) {
        qCWarning(XdgDesktopPortalTestScreenCast) << "Pipewire stream is not ready to be streamed";
        return 2;
    }

    results.insert(QStringLiteral("streams"), streams);

    return 0;
}

void ScreenCastPortal::stopStreaming()
{
    if (m_streamingEnabled) {
        m_streamingEnabled = false;

        if (m_timer) {
            m_timer->stop();
            delete m_timer;
            m_timer = nullptr;
        }

        if (m_stream) {
            delete m_stream;
            m_stream = nullptr;
        }

        m_frameCounter = 0;
    }
}
