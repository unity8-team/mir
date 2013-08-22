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

#ifndef UBUNTU_APPLICATION_UI_OPTIONS_H_
#define UBUNTU_APPLICATION_UI_OPTIONS_H_

#include <ubuntu/visibility.h>

#include <ubuntu/application/ui/form_factor.h>
#include <ubuntu/application/ui/stage.h>
#include <ubuntu/application/options.h>

#ifdef __cplusplus
extern "C" {
#endif
   
    UBUNTU_DLL_PUBLIC UAUiFormFactor
    u_application_options_get_form_factor(
    	UApplicationOptions* options);
   
    UBUNTU_DLL_PUBLIC UAUiStage
    u_application_options_get_stage(
    	UApplicationOptions* options);
    
#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_UI_OPTIONS_H_ */
