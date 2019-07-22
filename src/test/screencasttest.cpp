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

#include <QTest>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDBusUnixFileDescriptor>

#include "../screencaststream.h"

#include <QSignalSpy>

#define DBUS_SERVICE_NAME "org.freedesktop.portal.Desktop"
#define DBUS_PATH "/org/freedesktop/portal/desktop"
#define DBUS_SCREENCAST_INTERFACE_NAME "org.freedesktop.portal.ScreenCast"
#define DBUS_REQUEST_INTERFACE_NAME "org.freedesktop.portal.Request"
#define DBUS_PROPERTIES_INTERFACE_NAME "org.freedesktop.DBus.Properties"

class ScreenCastTest : public QObject
{
    Q_OBJECT
public:
    typedef struct {
        uint node_id;
        QVariantMap map;
    } Stream;
    typedef QList<Stream> Streams;

    ScreenCastTest();

private Q_SLOTS:
    void testPortalRunning();
    void testCreateSession();
    void testSelectSources();
    void testStart();
    void testOpenPipeWireRemote();

Q_SIGNALS:
    void createSessionResponse(uint response, const QVariantMap &map);
    void selectSourcesResponse(uint response, const QVariantMap &map);
    void startResponse(uint response, const QVariantMap &map);

private:
    QString getSessionToken()
    {
        m_sessionTokenCounter += 1;
        return QString("test%1").arg(m_sessionTokenCounter);
    }

    QString getRequestToken()
    {
        m_requestTokenCounter += 1;
        return QString("test%1").arg(m_requestTokenCounter);
    }

    int m_sessionTokenCounter = 0;
    int m_requestTokenCounter = 0;

    QSize m_resolution;
    QString m_sessionPath;
    uint m_streamNodeId;
};

Q_DECLARE_METATYPE(ScreenCastTest::Stream);
Q_DECLARE_METATYPE(ScreenCastTest::Streams);

const QDBusArgument &operator >> (const QDBusArgument &arg, ScreenCastTest::Stream &stream)
{
    arg.beginStructure();
    arg >> stream.node_id;

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

ScreenCastTest::ScreenCastTest()
{
}

void ScreenCastTest::testPortalRunning()
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral(DBUS_SERVICE_NAME),
                                                          QStringLiteral(DBUS_PATH),
                                                          QStringLiteral(DBUS_PROPERTIES_INTERFACE_NAME),
                                                          QStringLiteral("Get"));
    message << QStringLiteral(DBUS_SCREENCAST_INTERFACE_NAME) << QStringLiteral("AvailableSourceTypes");
    QDBusMessage reply = QDBusConnection::sessionBus().call(message);
    QCOMPARE(reply.type(), QDBusMessage::ReplyMessage);
    QCOMPARE(reply.arguments().count(), 1);

    QDBusVariant dbusVariant = qvariant_cast<QDBusVariant>(reply.arguments().at(0));
    // retrieve the actual value stored in the D-Bus variant
    QVariant result = dbusVariant.variant();
    QCOMPARE(result.toUInt(), 0);
}

void ScreenCastTest::testCreateSession()
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral(DBUS_SERVICE_NAME),
                                                          QStringLiteral(DBUS_PATH),
                                                          QStringLiteral(DBUS_SCREENCAST_INTERFACE_NAME),
                                                          QStringLiteral("CreateSession"));
    message << QVariantMap { { QLatin1String("session_handle_token"), getSessionToken() }, { QLatin1String("handle_token"), getRequestToken() } };
    QDBusReply<QDBusObjectPath> reply = QDBusConnection::sessionBus().asyncCall(message);
    QCOMPARE(reply.isValid(), true);
    QSignalSpy responseSpy(this, SIGNAL(createSessionResponse(uint,QVariantMap)));
    QDBusConnection::sessionBus().connect(QString(), reply.value().path(), QStringLiteral(DBUS_REQUEST_INTERFACE_NAME),
                                          QStringLiteral("Response"), this, SIGNAL(createSessionResponse(uint,QVariantMap)));
    QVERIFY(responseSpy.wait());
    QList<QVariant> arguments = responseSpy.takeFirst();
    QCOMPARE(arguments.count(), 2);
    QCOMPARE(arguments.at(0).toInt(), 0);

    m_sessionPath = arguments.at(1).toMap().value(QStringLiteral("session_handle")).toString();
    QVERIFY(!m_sessionPath.isEmpty());
}

void ScreenCastTest::testSelectSources()
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral(DBUS_SERVICE_NAME),
                                                          QStringLiteral(DBUS_PATH),
                                                          QStringLiteral(DBUS_SCREENCAST_INTERFACE_NAME),
                                                          QStringLiteral("SelectSources"));

    message << QVariant::fromValue(QDBusObjectPath(m_sessionPath))
            << QVariantMap { { QLatin1String("multiple"), false},
                             { QLatin1String("types"), (uint)1},
                             { QLatin1String("handle_token"), getRequestToken() } };

    QDBusReply<QDBusObjectPath> reply = QDBusConnection::sessionBus().asyncCall(message);
    QCOMPARE(reply.isValid(), true);
    QSignalSpy responseSpy(this, SIGNAL(selectSourcesResponse(uint,QVariantMap)));
    QDBusConnection::sessionBus().connect(QString(), reply.value().path(), QStringLiteral(DBUS_REQUEST_INTERFACE_NAME),
                                          QStringLiteral("Response"), this, SIGNAL(selectSourcesResponse(uint,QVariantMap)));
    QVERIFY(responseSpy.wait());
    QList<QVariant> arguments = responseSpy.takeFirst();
    QCOMPARE(arguments.count(), 2);
    QCOMPARE(arguments.at(0).toInt(), 0);
}

void ScreenCastTest::testStart()
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral(DBUS_SERVICE_NAME),
                                                          QStringLiteral(DBUS_PATH),
                                                          QStringLiteral(DBUS_SCREENCAST_INTERFACE_NAME),
                                                          QStringLiteral("Start"));

    message << QVariant::fromValue(QDBusObjectPath(m_sessionPath))
            << QString()
            << QVariantMap { { QStringLiteral("handle_token"), getRequestToken() } };

    QDBusReply<QDBusObjectPath> reply = QDBusConnection::sessionBus().asyncCall(message);
    QCOMPARE(reply.isValid(), true);
    QSignalSpy responseSpy(this, SIGNAL(startResponse(uint,QVariantMap)));
    QDBusConnection::sessionBus().connect(QString(), reply.value().path(), QStringLiteral(DBUS_REQUEST_INTERFACE_NAME),
                                          QStringLiteral("Response"), this, SIGNAL(startResponse(uint,QVariantMap)));
    QVERIFY(responseSpy.wait());
    QList<QVariant> arguments = responseSpy.takeFirst();
    QCOMPARE(arguments.count(), 2);
    QCOMPARE(arguments.at(0).toInt(), 0);

    Streams streams = qdbus_cast<Streams>(arguments.at(1).toMap().value(QStringLiteral("streams")));
    QCOMPARE(streams.count(), 1);
    Stream stream = streams.first();
    m_streamNodeId = stream.node_id;
    m_resolution = qdbus_cast<QSize>(stream.map.value(QStringLiteral("size")));
    QVERIFY(m_resolution.isValid());
    QCOMPARE(m_resolution, QSize(8, 8));
}

void ScreenCastTest::testOpenPipeWireRemote()
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral(DBUS_SERVICE_NAME),
                                                          QStringLiteral(DBUS_PATH),
                                                          QStringLiteral(DBUS_SCREENCAST_INTERFACE_NAME),
                                                          QStringLiteral("OpenPipeWireRemote"));

    message << QVariant::fromValue(QDBusObjectPath(m_sessionPath))
            << QVariantMap();

    QDBusReply<QDBusUnixFileDescriptor> reply = QDBusConnection::sessionBus().asyncCall(message);
    QCOMPARE(reply.isValid(), true);
    QVERIFY(reply.value().isValid());

    ScreenCastStream *stream = new ScreenCastStream(m_resolution, reply.value(), m_streamNodeId, this);
    QSignalSpy spy(stream, SIGNAL(framebufferUpdated()));
    stream->init();
    QImage testFb = QImage(m_resolution, QImage::Format_RGBA8888);
    testFb.fill(QColor("red"));
    QVERIFY(spy.wait());
    QCOMPARE(stream->framebuffer(), testFb);
    QVERIFY(spy.wait());
    testFb.fill(QColor("green"));
    QCOMPARE(stream->framebuffer(), testFb);
    QVERIFY(spy.wait());
    testFb.fill(QColor("blue"));
    QCOMPARE(stream->framebuffer(), testFb);
    QVERIFY(spy.wait());
    testFb.fill(QColor("black"));
    QCOMPARE(stream->framebuffer(), testFb);

    delete stream;
}

QTEST_GUILESS_MAIN(ScreenCastTest)

#include "screencasttest.moc"
