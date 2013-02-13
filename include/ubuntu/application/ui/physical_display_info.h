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
#ifndef UBUNTU_APPLICATION_UI_PHYSICAL_DISPLAY_INFO_H_
#define UBUNTU_APPLICATION_UI_PHYSICAL_DISPLAY_INFO_H_

#include "ubuntu/platform/shared_ptr.h"

namespace ubuntu
{
namespace application
{
namespace ui
{

/**
 * Marks physical displays connected to the system
 */
enum PhysicalDisplayIdentifier
{
    first_physical_display = 0,
    second_physical_display = 1,
    third_physical_display = 2,
    fourth_physical_display = 3,
    fifth_physical_display = 4,
    sixth_physical_display = 5,
    seventh_physical_display = 6,
    eigth_physical_display = 7,
    ninth_physical_display = 8,
    tenth_physical_display = 9,

    primary_physical_display = first_physical_display
};

/**
 * Models information about a physical display.
 */
class PhysicalDisplayInfo : public ubuntu::platform::ReferenceCountedBase
{
public:
    typedef ubuntu::platform::shared_ptr<PhysicalDisplayInfo> Ptr;

    /** Access to the horizontal dpi of the physical display. */
    virtual float horizontal_dpi() = 0;
    /** Access to the vertical dpi of the physical display. */
    virtual float vertical_dpi() = 0;
    /** Access to the horizontal resolution of the physical display. */
    virtual int horizontal_resolution() = 0;
    /** Access to the vertical resolution of the physical display. */
    virtual int vertical_resolution() = 0;

protected:
    PhysicalDisplayInfo() {}
    virtual ~PhysicalDisplayInfo() {}

    PhysicalDisplayInfo(const PhysicalDisplayInfo&) = delete;
    PhysicalDisplayInfo& operator=(const PhysicalDisplayInfo&) = delete;
};
}
}
}

#endif // UBUNTU_APPLICATION_UI_PHYSICAL_DISPLAY_INFO_H_
