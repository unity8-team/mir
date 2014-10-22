/*
 * Copyright (C) 2013,2014 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Daniel d'Andrada <daniel.dandrada@canonical.com>
 *   Gerry Boland <gerry.boland@canonical.com>
 */

// local
#include "screen.h"
#include "logging.h"

// Mir
#include "mir/geometry/size.h"

// Qt
#include <QCoreApplication>
#include <qpa/qwindowsysteminterface.h>
#include <QtSensors/QOrientationSensor>
#include <QtSensors/QOrientationReading>
#include <QThread>

// Qt sensors
#include <QtSensors/QOrientationReading>
#include <QtSensors/QOrientationSensor>

namespace mg = mir::geometry;

Q_LOGGING_CATEGORY(QTMIR_SENSOR_MESSAGES, "qtmir.sensor")

namespace {
bool isLittleEndian() {
    unsigned int i = 1;
    char *c = (char*)&i;
    return *c == 1;
}

enum QImage::Format qImageFormatFromMirPixelFormat(MirPixelFormat mirPixelFormat) {
    switch (mirPixelFormat) {
    case mir_pixel_format_abgr_8888:
        if (isLittleEndian()) {
            // 0xRR,0xGG,0xBB,0xAA
            return QImage::Format_RGBA8888;
        } else {
            // 0xAA,0xBB,0xGG,0xRR
            qFatal("[mirserver QPA] "
                   "Qt doesn't support mir_pixel_format_abgr_8888 in a big endian architecture");
        }
        break;
    case mir_pixel_format_xbgr_8888:
        if (isLittleEndian()) {
            // 0xRR,0xGG,0xBB,0xXX
            return QImage::Format_RGBX8888;
        } else {
            // 0xXX,0xBB,0xGG,0xRR
            qFatal("[mirserver QPA] "
                   "Qt doesn't support mir_pixel_format_xbgr_8888 in a big endian architecture");
        }
        break;
        break;
    case mir_pixel_format_argb_8888:
        // 0xAARRGGBB
        return QImage::Format_ARGB32;
        break;
    case mir_pixel_format_xrgb_8888:
        // 0xffRRGGBB
        return QImage::Format_RGB32;
        break;
    case mir_pixel_format_bgr_888:
        qFatal("[mirserver QPA] Qt doesn't support mir_pixel_format_bgr_888");
        break;
    default:
        qFatal("[mirserver QPA] Unknown mir pixel format");
        break;
    }
}

} // namespace {


class OrientationReadingEvent : public QEvent {
public:
    OrientationReadingEvent(QEvent::Type type, QOrientationReading::Orientation orientation)
        : QEvent(type)
        , m_orientation(orientation) {
    }

    static const QEvent::Type m_type;
    QOrientationReading::Orientation m_orientation;
};

const QEvent::Type OrientationReadingEvent::m_type =
        static_cast<QEvent::Type>(QEvent::registerEventType());

bool Screen::skipDBusRegistration = false;

Screen::Screen(mir::graphics::DisplayConfigurationOutput const &screen)
    : QObject(nullptr)
    , m_orientationSensor(new QOrientationSensor(this))
    , unityScreen(nullptr)
{
    readMirDisplayConfiguration(screen);

    // Set the default orientation based on the initial screen dimmensions.
    m_nativeOrientation = (m_geometry.width() >= m_geometry.height())
        ? Qt::LandscapeOrientation : Qt::PortraitOrientation;
    qCDebug(QTMIR_SENSOR_MESSAGES) << "Screen - nativeOrientation is:" << m_nativeOrientation;

    // If it's a landscape device (i.e. some tablets), start in landscape, otherwise portrait
    m_currentOrientation = (m_nativeOrientation == Qt::LandscapeOrientation)
            ? Qt::LandscapeOrientation : Qt::PortraitOrientation;
    qCDebug(QTMIR_SENSOR_MESSAGES) << "Screen - initial currentOrientation is:" << m_currentOrientation;

    QObject::connect(m_orientationSensor, &QOrientationSensor::readingChanged,
                     this, &Screen::onOrientationReadingChanged);
    m_orientationSensor->start();

    if(!skipDBusRegistration) {
        unityScreen = new QDBusInterface("com.canonical.Unity.Screen",
                                         "/com/canonical/Unity/Screen",
                                         "com.canonical.Unity.Screen",
                                         QDBusConnection::systemBus(), this);

        unityScreen->connection().connect("com.canonical.Unity.Screen",
                                          "/com/canonical/Unity/Screen",
                                          "com.canonical.Unity.Screen",
                                          "DisplayPowerStateChange",
                                          this,
                                          SLOT(onDisplayPowerStateChanged(int, int)));
    }
}

bool Screen::orientationSensorEnabled()
{
    return m_orientationSensor->isActive();
}

void Screen::onDisplayPowerStateChanged(int status, int reason)
{
    Q_UNUSED(reason);
    toggleSensors(status);
}

void Screen::readMirDisplayConfiguration(mir::graphics::DisplayConfigurationOutput const &screen)
{
    // Physical screen size
    m_physicalSize.setWidth(screen.physical_size_mm.width.as_float());
    m_physicalSize.setHeight(screen.physical_size_mm.height.as_float());

    // Pixel Format
    m_format = qImageFormatFromMirPixelFormat(screen.current_format);

    // Pixel depth
    m_depth = 8 * MIR_BYTES_PER_PIXEL(screen.current_format);

    // Mode = Resolution & refresh rate
    mir::graphics::DisplayConfigurationMode mode = screen.modes.at(screen.current_mode_index);
    m_geometry.setWidth(mode.size.width.as_int());
    m_geometry.setHeight(mode.size.height.as_int());

    m_refreshRate = mode.vrefresh_hz;
}

void Screen::toggleSensors(const bool enable) const
{
    qCDebug(QTMIR_SENSOR_MESSAGES) << "Screen::toggleSensors - enable=" << enable;
    if (enable) {
        m_orientationSensor->start();
    } else {
        m_orientationSensor->stop();
    }
}

void Screen::customEvent(QEvent* event)
{
    OrientationReadingEvent* oReadingEvent = static_cast<OrientationReadingEvent*>(event);
    switch (oReadingEvent->m_orientation) {
        case QOrientationReading::LeftUp: {
            m_currentOrientation = (m_nativeOrientation == Qt::LandscapeOrientation) ?
                        Qt::InvertedPortraitOrientation : Qt::LandscapeOrientation;
            break;
        }
        case QOrientationReading::TopUp: {
            m_currentOrientation = (m_nativeOrientation == Qt::LandscapeOrientation) ?
                        Qt::LandscapeOrientation : Qt::PortraitOrientation;
            break;
        }
        case QOrientationReading::RightUp: {
            m_currentOrientation = (m_nativeOrientation == Qt::LandscapeOrientation) ?
                        Qt::PortraitOrientation : Qt::InvertedLandscapeOrientation;
            break;
        }
        case QOrientationReading::TopDown: {
            m_currentOrientation = (m_nativeOrientation == Qt::LandscapeOrientation) ?
                        Qt::InvertedLandscapeOrientation : Qt::InvertedPortraitOrientation;
            break;
        }
        default: {
            qWarning("Unknown orientation.");
            event->accept();
            return;
        }
    }

    // Raise the event signal so that client apps know the orientation changed
    QWindowSystemInterface::handleScreenOrientationChange(screen(), m_currentOrientation);
    event->accept();
    qCDebug(QTMIR_SENSOR_MESSAGES) << "Screen::customEvent - new orientation" << m_currentOrientation << "handled";
}

void Screen::onOrientationReadingChanged()
{
    qCDebug(QTMIR_SENSOR_MESSAGES) << "Screen::onOrientationReadingChanged";

    // Make sure to switch to the main Qt thread context
    QCoreApplication::postEvent(this, new OrientationReadingEvent(
                                              OrientationReadingEvent::m_type,
                                              m_orientationSensor->reading()->orientation()));
}
