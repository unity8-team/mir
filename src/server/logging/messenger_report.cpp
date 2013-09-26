/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/logging/messenger_report.h"
#include "mir/logging/logger.h"

#include "std/MirLog.h"
#include <std/Log.h>


#include <sstream>
#include <cstring>
#include <mutex>

namespace ml = mir::logging;

ml::MessengerReport::MessengerReport(const std::shared_ptr<Logger>& logger) 
    : logger(logger)
{
}

const char* ml::MessengerReport::component()
{
    static const char* s = "messenger";
    return s;
}

void ml::MessengerReport::error(std::string const& error_message)
{
    logger->log<Logger::informational>(error_message, component());
}
