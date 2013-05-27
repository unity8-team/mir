/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */
#ifndef UBUNTU_UI_SESSION_SNAPSHOT_H_
#define UBUNTU_UI_SESSION_SNAPSHOT_H_

namespace ubuntu
{
namespace ui
{
class SessionSnapshot : public ubuntu::platform::ReferenceCountedBase
{
public:
    typedef ubuntu::platform::shared_ptr<SessionSnapshot> Ptr;

    virtual const void * pixel_data() = 0;

    virtual unsigned int x() = 0;
    virtual unsigned int y() = 0;
    virtual unsigned int source_width() = 0;
    virtual unsigned int source_height() = 0;
    virtual unsigned int width() = 0;
    virtual unsigned int height() = 0;
    virtual unsigned int stride() = 0;

protected:
    SessionSnapshot() {}
    virtual ~SessionSnapshot() {}

    SessionSnapshot(const SessionSnapshot&) = delete;
    SessionSnapshot& operator=(const SessionSnapshot&) = delete;
};
}
}

#endif // UBUNTU_UI_SESSION_SNAPSHOT_H_
