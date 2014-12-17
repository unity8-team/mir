/*
 * Copyright © 2014 Canonical Ltd.
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
 */

/*
 * DEPRECATED: In favor of u_application_instance_get_mir_connection
 * and the mir client library
 */

#ifndef UBUNTU_APPLICATION_UI_WINDOW_ORIENTATION_H
#define UBUNTU_APPLICATION_UI_WINDOW_ORIENTATION_H

typedef enum {
    U_ORIENTATION_NORMAL = 1,
    U_ORIENTATION_LEFT,
    U_ORIENTATION_RIGHT,
    U_ORIENTATION_INVERTED,
} UApplicationUiWindowOrientation;

#endif // UBUNTU_APPLICATION_UI_WINDOW_ORIENTATION_H
