/*
 * Copyright (C) 2014 Canonical Ltd
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
 * Authored by: Ricardo Mendoza <ricardo.mendoza@canonical.com>
 */

#include <ubuntu/application/init.h>

#include "module_version.h"

void u_application_module_version(uint32_t *major, uint32_t *minor, uint32_t *patch)
{
    *major = (uint32_t) MODULE_VERSION_MAJOR;
    *minor = (uint32_t) MODULE_VERSION_MINOR;
    *patch = (uint32_t) MODULE_VERSION_PATCH;
}
