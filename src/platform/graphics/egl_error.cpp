/*
 * Copyright © 2015 Canonical Ltd.
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

#include "mir/graphics/egl_error.h"
#include <sstream>

namespace
{

std::string to_hex_string(int n)
{
    std::stringstream ss;
    ss << std::showbase << std::hex << n;
    return ss.str();
}

struct egl_category : std::error_category
{
    const char* name() const noexcept override { return "egl"; }

    std::string message(int ev) const override
    {
        #define CASE_FOR_ERROR(error) \
            case error: return #error " (" + to_hex_string(error) + ")";
        switch (ev)
        {
            CASE_FOR_ERROR(EGL_SUCCESS)
            CASE_FOR_ERROR(EGL_NOT_INITIALIZED)
            CASE_FOR_ERROR(EGL_BAD_ACCESS)
            CASE_FOR_ERROR(EGL_BAD_ALLOC)
            CASE_FOR_ERROR(EGL_BAD_ATTRIBUTE)
            CASE_FOR_ERROR(EGL_BAD_CONFIG)
            CASE_FOR_ERROR(EGL_BAD_CONTEXT)
            CASE_FOR_ERROR(EGL_BAD_CURRENT_SURFACE)
            CASE_FOR_ERROR(EGL_BAD_DISPLAY)
            CASE_FOR_ERROR(EGL_BAD_MATCH)
            CASE_FOR_ERROR(EGL_BAD_NATIVE_PIXMAP)
            CASE_FOR_ERROR(EGL_BAD_NATIVE_WINDOW)
            CASE_FOR_ERROR(EGL_BAD_PARAMETER)
            CASE_FOR_ERROR(EGL_BAD_SURFACE)
            CASE_FOR_ERROR(EGL_CONTEXT_LOST)

            default:
                return "Unknown error (" + to_hex_string(ev) + ")";
        }
        #undef CASE_ERROR
    }
};

}

std::error_category const& mir::graphics::egl_category()
{
    static class egl_category const egl_category_instance{};

    return egl_category_instance;
}
