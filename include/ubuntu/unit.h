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

#ifndef UBUNTU_UNIT_H_
#define UBUNTU_UNIT_H_

/**
 * \brief Describes units known to the platform integration layer.
 */
typedef enum
{
    U_DEGREE,
    U_METER,
    U_SECOND,
    U_METER_PER_SECOND,
    U_MICRO_TESLA
} UbuntuUnit;

typedef UbuntuUnit UUnit;

#endif /* UBUNTU_UNIT_H_ */
