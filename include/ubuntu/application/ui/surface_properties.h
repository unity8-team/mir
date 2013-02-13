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
#ifndef UBUNTU_APPLICATION_UI_SURFACE_PROPERTIES_H_
#define UBUNTU_APPLICATION_UI_SURFACE_PROPERTIES_H_

#include "ubuntu/application/ui/surface_role.h"
#include "ubuntu/application/ui/ubuntu_application_ui.h"

namespace ubuntu
{
namespace application
{
namespace ui
{

/**
 * \enum SurfaceFlags 
 * Flags that can be specified for a surface 
 * \attention Requires privileged access to the ui service provider 
 */
enum SurfaceFlags
{
    is_opaque_flag = IS_OPAQUE_FLAG ///< Specifies that a surface is opaque
};

/** 
 * \struct SurfaceProperties surface_properties.h
 * Bundles the properties for surface creation.
 */
struct SurfaceProperties
{
    enum
    {
        max_surface_title_length = 512 ///< Maximum length of the surface title
    };

    const char title[max_surface_title_length]; ///< Surface title
    int width; ///< Requested width
    int height; ///< Requested height
    SurfaceRole role; ///< Requested role \sa ubuntu::application::ui::SurfaceRole
    uint32_t flags; ///< Requested flags \sa ubuntu::application::ui::SurfaceFlags
    bool is_opaque; ///< Signals that the surface should be opaque
};
}
}
}

#endif // UBUNTU_APPLICATION_UI_SURFACE_PROPERTIES_H_
