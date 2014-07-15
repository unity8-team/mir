/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_EXAMPLES_BASIC_SERVER_CONFIGURATION_H_
#define MIR_EXAMPLES_BASIC_SERVER_CONFIGURATION_H_

#include "server_configuration.h"

namespace mir
{
namespace examples
{

/**
 * \brief BasicServerConfiguration extends ServerConfiguration with the ability to launch a client application
 */
class BasicServerConfiguration : public ServerConfiguration
{
public:
    BasicServerConfiguration(int argc, char const** argv);

    void launch_client();
};

}
}

#endif /* MIR_EXAMPLES_BASIC_SERVER_CONFIGURATION_H_ */
