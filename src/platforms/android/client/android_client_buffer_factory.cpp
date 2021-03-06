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
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by:
 *   Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "android_client_buffer_factory.h"
#include "android_format_conversion-inl.h"
#include "buffer_registrar.h"
#include "buffer.h"

namespace mcl=mir::client;
namespace mcla=mir::client::android;
namespace geom=mir::geometry;
namespace mga=mir::graphics::android;

mcla::AndroidClientBufferFactory::AndroidClientBufferFactory(
    std::shared_ptr<BufferRegistrar> const& buffer_registrar) :
    registrar(buffer_registrar)
{
}

std::shared_ptr<mcl::ClientBuffer> mcla::AndroidClientBufferFactory::create_buffer(std::shared_ptr<MirBufferPackage> const& package, geom::Size size, MirPixelFormat pf)
{
    (void)size; // TODO: remove this unused parameter
    return std::make_shared<mcla::Buffer>(registrar, *package, pf);
}

std::shared_ptr<mcl::ClientBuffer> mcla::AndroidClientBufferFactory::create_buffer(
    std::shared_ptr<MirBufferPackage> const& package,
    unsigned int native_pf, unsigned int)
{
    return std::make_shared<mcla::Buffer>(registrar, *package, mga::to_mir_format(native_pf));
}
