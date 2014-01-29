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
 */

// mir
#include <mir/graphics/display.h>
#include <mir/graphics/display_configuration.h>
#include <mir_toolkit/common.h>

// qt
#include <QDBusConnection>
#include <QDebug>

// local
#include "mirserverconfiguration.h"
#include "dbusscreen.h"

namespace mc = mir::compositor;
namespace mg = mir::graphics;

// Note: this class should be created only after when the Mir DisplayServer has started
DBusScreen::DBusScreen(MirServerConfiguration *config, QObject *parent)
    : QObject(parent)
    , m_serverConfig(config)
{
    QDBusConnection::systemBus().registerService("com.canonical.Unity.Screen");
    // TODO ExportScriptableSlots shouldn't be needed but without it I don't get the methods :-/
    QDBusConnection::systemBus().registerObject("/com/canonical/Unity/Screen", this,
                                                QDBusConnection::ExportScriptableSlots |
                                                QDBusConnection::ExportScriptableInvokables );
}

bool DBusScreen::setScreenPowerMode(const QString &mode)
{
    MirPowerMode newPowerMode;
    // Note: the "standby" and "suspend" modes are mostly unused

    if (mode == "on") {
        newPowerMode = MirPowerMode::mir_power_mode_on;
    } else if (mode == "standby") {
        newPowerMode = MirPowerMode::mir_power_mode_standby; // higher power "off" mode (fastest resume)
    } else if (mode == "suspend") {
        newPowerMode = MirPowerMode::mir_power_mode_suspend; // medium power "off" mode
    } else if (mode == "off") {
        newPowerMode = MirPowerMode::mir_power_mode_off;     // lowest power "off" mode (slowest resume)
    } else {
        qWarning() << "DBusScreen: unknown mode type" << mode;
        return false;
    }

    std::shared_ptr<mg::Display> display = m_serverConfig->the_display();
    std::shared_ptr<mg::DisplayConfiguration> displayConfig = display->configuration();

    displayConfig->for_each_output([&](const mg::DisplayConfigurationOutput displayConfigOutput) {
        if (displayConfigOutput.power_mode != newPowerMode) {
            displayConfig->configure_output(
                        displayConfigOutput.id,         //unchanged
                        displayConfigOutput.used,       //unchanged
                        displayConfigOutput.top_left,   //unchanged
                        displayConfigOutput.current_mode_index, //unchanged
                        displayConfigOutput.current_format, //unchanged
                        newPowerMode,
                        displayConfigOutput.orientation
                        );
        }
    });

    display->configure(*displayConfig.get());
    return true;
}
