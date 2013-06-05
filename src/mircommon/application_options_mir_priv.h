/*
 * Copyright (C) 2013 Canonical Ltd
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef UBUNTU_APPLICATION_OPTIONS_MIR_PRIV_H_
#define UBUNTU_APPLICATION_OPTIONS_MIR_PRIV_H_

#include <ubuntu/application/options.h>
#include <ubuntu/application/ui/options.h>

#include <string>

namespace ubuntu
{
namespace application
{
namespace mir
{

class Options
{
public:
    Options() = default;
    ~Options() = default;
    
    UApplicationOptions* as_u_application_options();
    static Options* from_u_application_options(UApplicationOptions* u_options);

    UApplicationOperationMode operation_mode = U_APPLICATION_FOREGROUND_APP;
    UAUiFormFactor form_factor = U_DESKTOP;
    UAUiStage stage = U_MAIN_STAGE;
    
    std::string desktop_file;

protected:
    Options(Options const&) = delete;
    Options& operator=(Options const&) = delete;
};

}
}
} // namespace ubuntu

#endif // UBUNTU_APPLICATION_OPTIONS_MIR_PRIV_H_
