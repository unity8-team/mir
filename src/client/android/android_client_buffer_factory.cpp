/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by:
 *   Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "android_client_buffer_factory.h"
#include "android_client_buffer.h"

namespace mcl=mir::client;
namespace mcla=mir::client::android;
namespace geom=mir::geometry;

mcla::AndroidClientBufferFactory::AndroidClientBufferFactory(std::shared_ptr<AndroidRegistrar> const &buffer_registrar)
 : registrar(buffer_registrar)
{
}

std::shared_ptr<mcl::ClientBuffer> mcla::AndroidClientBufferFactory::create_buffer(std::shared_ptr<mir_toolkit::MirBufferPackage> const & package, geom::Size size, geom::PixelFormat pf)
{
    return std::make_shared<mcla::AndroidClientBuffer>(registrar, package, size, pf);
}
