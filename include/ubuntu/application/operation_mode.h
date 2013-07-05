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
 * Authored by: Ricardo Mendoza <ricardo.mendoza@canonical.com>
 *              Thomas Voß <thomas.voss@canonical.com>
 */

#ifndef UBUNTU_APPLICATION_OPERATION_MODE_H_
#define UBUNTU_APPLICATION_OPERATION_MODE_H_

/**
 * \brief Describes the different operational modes that an
 * application can run in.
 */
typedef enum
{
    U_APPLICATION_FOREGROUND_APP,
    U_APPLICATION_BACKGROUND_SERVICE
} UApplicationOperationMode;

#endif /* UBUNTU_APPLICATION_OPERATION_MODE_H_ */
