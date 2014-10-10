/*
 * Copyright (C) 2013 Canonical, Ltd.
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
 * Author: Gerry Boland <gerry.boland@canonical.com>
 */

#include "display.h"

#include "screen.h"

#include <mir/graphics/display_configuration.h>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusError>

namespace mg = mir::graphics;

// TODO: Listen for display changes and update the list accordingly

Display::Display(const QSharedPointer<mir::DefaultServerConfiguration> &config, QObject *parent)
  : QObject(parent)
  , m_mirConfig(config)
  , unityScreen(nullptr)
{
    std::shared_ptr<mir::graphics::DisplayConfiguration> displayConfig = m_mirConfig->the_display()->configuration();

    displayConfig->for_each_output([this](mg::DisplayConfigurationOutput const& output) {
        if (output.used) {
            auto screen = new Screen(output);
            m_screens.push_back(screen);
        }
    });
    register_with_dbus();
}

void Display::register_with_dbus()
{
    QDBusInterface *unityScreen = new QDBusInterface
    (
        "com.canonical.Unity.Screen",
        "/com/canonical/Unity/Screen",
        "com.canonical.Unity.Screen",
        QDBusConnection::systemBus(), this
    );

    unityScreen->connection().connect
    (
        "com.canonical.Unity.Screen",
        "/com/canonical/Unity/Screen",
        "com.canonical.Unity.Screen",
        "DisplayPowerStateChange", this,
        SLOT(onDisplayPowerStateChanged(int, int))
    );
}

void Display::onDisplayPowerStateChanged(int status, int reason)
{
    qDebug() << "JOSH onDisplayPowerStateChanged" << status << reason;
}

Display::~Display()
{
    for (auto screen : m_screens)
        delete screen;
    m_screens.clear();
}
