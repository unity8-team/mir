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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 *              Ricardo Mendoza <ricardo.mendoza@canonical.com
 */

#ifndef UBUNTU_STATUS_H_
#define UBUNTU_STATUS_H_

/**
 * \brief Indicates the status of an operation.
 */
typedef enum
{
    U_STATUS_SUCCESS, ///< Operation finished successfully.
    U_STATUS_ERROR ///< Operation finished with an error.
} UbuntuStatus;

typedef UbuntuStatus UStatus;

#endif /* UBUNTU_STATUS_H_ */
