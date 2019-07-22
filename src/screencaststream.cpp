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

#include "screencaststream.h"

#include <math.h>
#include <sys/mman.h>
#include <stdio.h>

#include <QLoggingCategory>
#include <QSize>

Q_LOGGING_CATEGORY(XdgDesktopPortalTestScreenCastStream, "xdp-test-screencast-stream")

class PwFraction {
public:
    int num;
    int denom;
};

// Stolen from mutter
#define MAX_TERMS       30
#define MIN_DIVISOR     1.0e-10
#define MAX_ERROR       1.0e-20

#define PROP_RANGE(min, max) 2, (min), (max)

#define BITS_PER_PIXEL  4

static int greatestCommonDivisor(int a, int b)
{
    while (b != 0) {
        int temp = a;

        a = b;
        b = temp % b;
    }

    return ABS(a);
}

static PwFraction pipewireFractionFromDouble(double src)
{
    double V, F;                 /* double being converted */
    int N, D;                    /* will contain the result */
    int A;                       /* current term in continued fraction */
    int64_t N1, D1;              /* numerator, denominator of last approx */
    int64_t N2, D2;              /* numerator, denominator of previous approx */
    int i;
    int gcd;
    gboolean negative = FALSE;

    /* initialize fraction being converted */
    F = src;
    if (F < 0.0) {
        F = -F;
        negative = TRUE;
    }

    V = F;
    /* initialize fractions with 1/0, 0/1 */
    N1 = 1;
    D1 = 0;
    N2 = 0;
    D2 = 1;
    N = 1;
    D = 1;

    for (i = 0; i < MAX_TERMS; ++i) {
        /* get next term */
        A = (gint) F;               /* no floor() needed, F is always >= 0 */
        /* get new divisor */
        F = F - A;

        /* calculate new fraction in temp */
        N2 = N1 * A + N2;
        D2 = D1 * A + D2;

        /* guard against overflow */
        if (N2 > G_MAXINT || D2 > G_MAXINT)
            break;

        N = N2;
        D = D2;

        /* save last two fractions */
        N2 = N1;
        D2 = D1;
        N1 = N;
        D1 = D;

        /* quit if dividing by zero or close enough to target */
        if (F < MIN_DIVISOR || fabs (V - ((gdouble) N) / D) < MAX_ERROR)
            break;

        /* Take reciprocal */
        F = 1 / F;
    }

    /* fix for overflow */
    if (D == 0) {
        N = G_MAXINT;
        D = 1;
    }

    /* fix for negative */
    if (negative)
        N = -N;

    /* simplify */
    gcd = greatestCommonDivisor(N, D);
    if (gcd) {
        N /= gcd;
        D /= gcd;
    }

    PwFraction fraction;
    fraction.num = N;
    fraction.denom = D;

    return fraction;
}

static void onStateChanged(void *data, pw_remote_state old, pw_remote_state state, const char *error)
{
    Q_UNUSED(old);

    ScreenCastStream *pw = static_cast<ScreenCastStream*>(data);

    switch (state) {
    case PW_REMOTE_STATE_ERROR:
        // TODO notify error
        qCWarning(XdgDesktopPortalTestScreenCastStream) << "Remote error: " << error;
        break;
    case PW_REMOTE_STATE_CONNECTED:
        // TODO notify error
        qCDebug(XdgDesktopPortalTestScreenCastStream) << "Remote state: " << pw_remote_state_as_string(state);
        if (!pw->createStream()) {
            if (pw->streamDirection == ScreenCastStream::DirectionOutput)
                Q_EMIT pw->stopStreaming();
        }
        break;
    default:
        qCDebug(XdgDesktopPortalTestScreenCastStream) << "Remote state: " << pw_remote_state_as_string(state);
        break;
    }
}

static void onStreamStateChanged(void *data, pw_stream_state old, pw_stream_state state, const char *error_message)
{
    Q_UNUSED(old)

    ScreenCastStream *pw = static_cast<ScreenCastStream*>(data);

    switch (state) {
    case PW_STREAM_STATE_ERROR:
        qCWarning(XdgDesktopPortalTestScreenCastStream) << "Stream error: " << error_message;
        break;
    case PW_STREAM_STATE_CONFIGURE:
        qCDebug(XdgDesktopPortalTestScreenCastStream) << "Stream state: " << pw_stream_state_as_string(state);
        if (pw->streamDirection == ScreenCastStream::DirectionOutput)
            Q_EMIT pw->streamReady((uint)pw_stream_get_node_id(pw->pwStream));
        else
            pw_stream_set_active(pw->pwStream, true);
        break;
    case PW_STREAM_STATE_UNCONNECTED:
    case PW_STREAM_STATE_CONNECTING:
    case PW_STREAM_STATE_READY:
    case PW_STREAM_STATE_PAUSED:
        if (pw->streamDirection == ScreenCastStream::DirectionOutput) {
            qCDebug(XdgDesktopPortalTestScreenCastStream) << "Stream state: " << pw_stream_state_as_string(state);
            Q_EMIT pw->stopStreaming();
        }
        break;
    case PW_STREAM_STATE_STREAMING:
        if (pw->streamDirection == ScreenCastStream::DirectionOutput) {
            qCDebug(XdgDesktopPortalTestScreenCastStream) << "Stream state: " << pw_stream_state_as_string(state);
            Q_EMIT pw->startStreaming();
        }
        break;
    }
}

static void onStreamFormatChanged(void *data, const struct spa_pod *format)
{
    qCDebug(XdgDesktopPortalTestScreenCastStream) << "Stream format changed";

    ScreenCastStream *pw = static_cast<ScreenCastStream*>(data);

    uint8_t paramsBuffer[1024];
    int32_t width, height, stride, size;
    struct spa_pod_builder pod_builder;
    const struct spa_pod *params[1];
    const int bpp = 4;

    if (!format) {
        pw_stream_finish_format(pw->pwStream, 0, nullptr, 0);
        return;
    }

#if PW_CHECK_VERSION(0, 2, 9)
    spa_format_video_raw_parse (format, &pw->videoFormat);
#else
    spa_format_video_raw_parse (format, &pw->videoFormat, &pw->pwType->format_video);
#endif

    width = pw->videoFormat.size.width;
    height =pw->videoFormat.size.height;
    stride = SPA_ROUND_UP_N (width * bpp, 4);
    size = height * stride;

    pod_builder = SPA_POD_BUILDER_INIT (paramsBuffer, sizeof (paramsBuffer));

#if PW_CHECK_VERSION(0, 2, 9)
    params[0] = (spa_pod*) spa_pod_builder_add_object(&pod_builder,
                                                   SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
                                                   ":", SPA_PARAM_BUFFERS_buffers, "?ri", SPA_CHOICE_RANGE(16, 2, 16),
                                                   ":", SPA_PARAM_BUFFERS_size, "i", size,
                                                   ":", SPA_PARAM_BUFFERS_stride, "i", stride,
                                                   ":", SPA_PARAM_BUFFERS_align, "i", 16);
    params[0] = reinterpret_cast<spa_pod *>(spa_pod_builder_add_object(&pod_builder,
                SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
                ":", SPA_PARAM_BUFFERS_size, "i", size,
                ":", SPA_PARAM_BUFFERS_stride, "i", stride,
                ":", SPA_PARAM_BUFFERS_buffers, "?ri", SPA_CHOICE_RANGE(16, 2, 16),
                ":", SPA_PARAM_BUFFERS_align, "i", 16));
    if (pw->streamDirection == ScreenCastStream::DirectionInput) {
        params[1] = reinterpret_cast<spa_pod *>(spa_pod_builder_add_object(&pod_builder,
                    SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                    ":", SPA_PARAM_META_type, "I", SPA_META_Header,
                    ":", SPA_PARAM_META_size, "i", sizeof(struct spa_meta_header)));
    }
#else
    params[0] = reinterpret_cast<spa_pod *>(spa_pod_builder_object(&pod_builder,
                pw->pwCoreType->param.idBuffers, pw->pwCoreType->param_buffers.Buffers,
                ":", pw->pwCoreType->param_buffers.size, "i", size,
                ":", pw->pwCoreType->param_buffers.stride, "i", stride,
                ":", pw->pwCoreType->param_buffers.buffers, "iru", 16, SPA_POD_PROP_MIN_MAX(2, 16),
                ":", pw->pwCoreType->param_buffers.align, "i", 16));
    if (pw->streamDirection == ScreenCastStream::DirectionInput) {
        params[1] = reinterpret_cast<spa_pod *>(spa_pod_builder_object(&pod_builder,
                    pw->pwCoreType->param.idMeta, pw->pwCoreType->param_meta.Meta,
                    ":", pw->pwCoreType->param_meta.type, "I", pw->pwCoreType->meta.Header,
                    ":", pw->pwCoreType->param_meta.size, "i", sizeof(struct spa_meta_header)));
    }
#endif
    pw_stream_finish_format (pw->pwStream, 0,
                             params, G_N_ELEMENTS (params));
}

static void onStreamProcess(void *data)
{
    ScreenCastStream *pw = static_cast<ScreenCastStream*>(data);

    if (pw->streamDirection == ScreenCastStream::DirectionInput) {
        pw_buffer *buf;
        if (!(buf = pw_stream_dequeue_buffer(pw->pwStream)))
            return;

        pw->readFrame(buf);

        pw_stream_queue_buffer(pw->pwStream, buf);
    }
}

static const struct pw_remote_events pwRemoteEvents = {
    .version = PW_VERSION_REMOTE_EVENTS,
    .destroy = nullptr,
    .info_changed = nullptr,
    .sync_reply = nullptr,
    .state_changed = onStateChanged,
};

static const struct pw_stream_events pwStreamEvents = {
    .version = PW_VERSION_STREAM_EVENTS,
    .destroy = nullptr,
    .state_changed = onStreamStateChanged,
    .format_changed = onStreamFormatChanged,
    .add_buffer = nullptr,
    .remove_buffer = nullptr,
    .process = onStreamProcess,
};

ScreenCastStream::ScreenCastStream(const QSize &resolution, QObject *parent)
    : QObject(parent)
    , streamDirection(ScreenCastStream::DirectionOutput)
    , resolution(resolution)
{
}

ScreenCastStream::ScreenCastStream(const QSize &resolution, const QDBusUnixFileDescriptor &fd, uint streamNodeId, QObject *parent)
    : QObject(parent)
    , streamDirection(ScreenCastStream::DirectionInput)
    , resolution(resolution)
    , pipewireFd(fd)
    , pwStreamNodeId(streamNodeId)

{
    fb = QImage(resolution, QImage::Format_RGBA8888);
}

ScreenCastStream::~ScreenCastStream()
{
    if (pwMainLoop)
        pw_thread_loop_stop(pwMainLoop);

#if !PW_CHECK_VERSION(0, 2, 9)
    if (pwType)
        delete pwType;
#endif

    if (pwStream)
        pw_stream_destroy(pwStream);

    if (pwRemote)
        pw_remote_destroy(pwRemote);

    if (pwCore)
        pw_core_destroy(pwCore);

    if (pwMainLoop)
        pw_thread_loop_destroy(pwMainLoop);

    if (pwLoop)
        pw_loop_destroy(pwLoop);
}

void ScreenCastStream::init()
{
    pw_init(nullptr, nullptr);

    pwLoop = pw_loop_new(nullptr);
    pwMainLoop = pw_thread_loop_new(pwLoop, "pipewire-main-loop");

    pwCore = pw_core_new(pwLoop, nullptr);
#if !PW_CHECK_VERSION(0, 2, 9)
    pwCoreType = pw_core_get_type(pwCore);
#endif
    pwRemote = pw_remote_new(pwCore, nullptr, 0);

#if !PW_CHECK_VERSION(0, 2, 9)
    initializePwTypes();
#endif

    pw_remote_add_listener(pwRemote, &remoteListener, &pwRemoteEvents, this);

    if (streamDirection == ScreenCastStream::DirectionInput)
        pw_remote_connect_fd(pwRemote, pipewireFd.fileDescriptor());
    else
        pw_remote_connect(pwRemote);

    if (pw_thread_loop_start(pwMainLoop) < 0)
        qCWarning(XdgDesktopPortalTestScreenCastStream) << "Failed to start main PipeWire loop";
}

uint ScreenCastStream::framerate() const
{
    if (pwStream)
        return videoFormat.max_framerate.num / videoFormat.max_framerate.denom;

    return 0;
}

uint ScreenCastStream::nodeId() const
{
    if (pwStream)
        return (uint)pw_stream_get_node_id(pwStream);

    return 0;
}

QImage ScreenCastStream::framebuffer() const
{
    return fb;
}

bool ScreenCastStream::createStream()
{
    if (pw_remote_get_state(pwRemote, nullptr) != PW_REMOTE_STATE_CONNECTED) {
        qCWarning(XdgDesktopPortalTestScreenCastStream) << "Cannot create pipewire stream";
        return false;
    }

    uint8_t buffer[1024];
    spa_pod_builder podBuilder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    const float frameRate = 25;

    spa_fraction maxFramerate;
    spa_fraction minFramerate;
    const spa_pod *params[1];

    if (streamDirection == ScreenCastStream::DirectionOutput) {
        pwStream = pw_stream_new(pwRemote, "xdp-test-screen-cast", nullptr);
    } else {
        auto reuseProps = pw_properties_new("pipewire.client.reuse", "1", nullptr); // null marks end of varargs
        pwStream = pw_stream_new(pwRemote, "xdp-test-consume-stream", reuseProps);
    }

    PwFraction fraction = pipewireFractionFromDouble(frameRate);

    minFramerate = SPA_FRACTION(1, 1);
    maxFramerate = SPA_FRACTION((uint32_t)fraction.num, (uint32_t)fraction.denom);

    spa_rectangle minResolution = SPA_RECTANGLE(1, 1);
    spa_rectangle maxResolution = SPA_RECTANGLE((uint32_t)resolution.width(), (uint32_t)resolution.height());

    spa_fraction paramFraction = SPA_FRACTION(0, 1);

#if PW_CHECK_VERSION(0, 2, 9)
    params[0] = (spa_pod*)spa_pod_builder_add_object(&podBuilder,
                                        SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
                                        ":", SPA_FORMAT_mediaType, "I", SPA_MEDIA_TYPE_video,
                                        ":", SPA_FORMAT_mediaSubtype, "I", SPA_MEDIA_SUBTYPE_raw,
                                        ":", SPA_FORMAT_VIDEO_format, "I", SPA_VIDEO_FORMAT_RGBx,
                                        ":", SPA_FORMAT_VIDEO_size, "?rR", SPA_CHOICE_RANGE(&maxResolution, &minResolution, &maxResolution),
                                        ":", SPA_FORMAT_VIDEO_framerate, "F", &paramFraction,
                                        ":", SPA_FORMAT_VIDEO_maxFramerate, "?rF", SPA_CHOICE_RANGE(&maxFramerate, &minFramerate, &maxFramerate));
#else
    params[0] = (spa_pod*)spa_pod_builder_object(&podBuilder,
                                        pwCoreType->param.idEnumFormat, pwCoreType->spa_format,
                                        "I", pwType->media_type.video,
                                        "I", pwType->media_subtype.raw,
                                        ":", pwType->format_video.format, "I", pwType->video_format.RGBx,
                                        ":", pwType->format_video.size, "Rru", &maxResolution, SPA_POD_PROP_MIN_MAX(&minResolution, &maxResolution),
                                        ":", pwType->format_video.framerate, "F", &paramFraction,
                                        ":", pwType->format_video.max_framerate, "Fru", &maxFramerate, PROP_RANGE (&minFramerate, &maxFramerate));
#endif

    pw_stream_add_listener(pwStream, &streamListener, &pwStreamEvents, this);

    const bool isOutput = streamDirection == ScreenCastStream::DirectionOutput;

    auto flags = static_cast<pw_stream_flags>(isOutput ? PW_STREAM_FLAG_DRIVER | PW_STREAM_FLAG_MAP_BUFFERS :
                                                         PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_INACTIVE | PW_STREAM_FLAG_MAP_BUFFERS);

#if PW_CHECK_VERSION(0, 2, 9)
    if (pw_stream_connect(pwStream, isOutput ? PW_DIRECTION_OUTPUT : PW_DIRECTION_INPUT, isOutput ? 0 : pwStreamNodeId , flags, params, G_N_ELEMENTS(&params)) != 0) {
#else
    if (pw_stream_connect(pwStream, isOutput ? PW_DIRECTION_OUTPUT : PW_DIRECTION_INPUT, nullptr, flags, params, G_N_ELEMENTS(&params)) != 0) {
#endif
        qCWarning(XdgDesktopPortalTestScreenCastStream) << "Could not connect to stream";
        return false;
   }

    return true;
}

bool ScreenCastStream::writeFrame(uint8_t *screenData)
{
    struct pw_buffer *buffer;
    struct spa_buffer *spa_buffer;
    uint8_t *data = nullptr;

    if (!(buffer = pw_stream_dequeue_buffer(pwStream)))
        return false;

    spa_buffer = buffer->buffer;

    if (!(data = (uint8_t *) spa_buffer->datas[0].data))
        return false;

    memcpy(data, screenData, BITS_PER_PIXEL * videoFormat.size.height * videoFormat.size.width * sizeof(uint8_t));

    spa_buffer->datas[0].chunk->size = spa_buffer->datas[0].maxsize;

    pw_stream_queue_buffer(pwStream, buffer);
    return true;
}

bool ScreenCastStream::readFrame(pw_buffer *pwBuffer)
{
    auto *spaBuffer = pwBuffer->buffer;
    void *src = nullptr;

    src = spaBuffer->datas[0].data;
    if (!src)
        return false;

    quint32 maxSize = spaBuffer->datas[0].maxsize;
    qint32 srcStride = spaBuffer->datas[0].chunk->stride;
    if (srcStride != resolution.width() * 4) {
        qCWarning(XdgDesktopPortalTestScreenCastStream) << "Got buffer with stride different from screen stride" << srcStride << "!=" << resolution.width() * 4;
        return false;
    }

    memcpy(fb.bits(), src, maxSize);
    Q_EMIT framebufferUpdated();
    return true;
}

void ScreenCastStream::removeStream()
{
    // FIXME destroying streams seems to be crashing, Mutter also doesn't remove them, maybe Pipewire does this automatically
    // pw_stream_destroy(pwStream);
    // pwStream = nullptr;
    pw_stream_disconnect(pwStream);
}

#if !PW_CHECK_VERSION(0, 2, 9)
void ScreenCastStream::initializePwTypes()
{
    // raw C-like ScreenCastStream type map
    auto map = pwCoreType->map;

    pwType = new PwType();

    spa_type_media_type_map(map, &pwType->media_type);
    spa_type_media_subtype_map(map, &pwType->media_subtype);
    spa_type_format_video_map (map, &pwType->format_video);
    spa_type_video_format_map (map, &pwType->video_format);
}
#endif
