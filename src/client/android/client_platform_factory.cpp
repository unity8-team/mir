/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "../client_platform_factory.h"
#include "mir_toolkit/client_types.h"
#include "../client_context.h"
#include "android_client_platform.h"

#include <stdexcept>

namespace mcl = mir::client;
namespace mcla = mcl::android;

extern "C" std::shared_ptr<mcl::ClientPlatform>
mcl::create_client_platform(mcl::ClientContext* context)
{
    MirPlatformPackage platform;
    context->populate(platform);
    if (platform.data_items != 0 || platform.fd_items != 0)
    {
        throw new std::runtime_error{"Attempted to create Android client platform on non-Android server"};
    }
    return std::make_shared<mcla::AndroidClientPlatform>();
}
