/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_GRAPHICS_ANDROID_SYNC_OBJECT_H_
#define MIR_GRAPHICS_ANDROID_SYNC_OBJECT_H_

namespace mir
{
namespace graphics
{
namespace android
{

class SyncObject
{
public:
    virtual ~SyncObject() = default;
    virtual void wait() = 0;

protected:
    SyncObject() = default;
    SyncObject(SyncObject const&) = delete;
    SyncObject& operator=(SyncObject const&) = delete;
};


}
}
}

#endif /* MIR_GRAPHICS_ANDROID_SYNC_OBJECT_H_ */
