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

#ifndef UBUNTU_APPLICATION_UI_STAGE_H_
#define UBUNTU_APPLICATION_UI_STAGE_H_

typedef enum
{
    U_MAIN_STAGE = 0,
    U_INTEGRATION_STAGE = 1,
    U_SHARE_STAGE = 2,
    U_CONTENT_PICKING_STAGE = 3,
    U_SIDE_STAGE = 4,
    U_CONFIGURATION_STAGE = 5
} UAUiStage;

#endif /* UBUNTU_APPLICATION_UI_STAGE_H_ */
