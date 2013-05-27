/*
 * Copyright © 2012 Canonical Ltd.
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
 */
#ifndef UBUNTU_APPLICATION_UI_INIT_H_
#define UBUNTU_APPLICATION_UI_INIT_H_

namespace ubuntu
{
namespace application
{
namespace ui
{
/** Initializes internals with the given command line arguments. 
 *  \attention Has to be called before any other function is called.
 */
void init(int argc, char** argv);
}
}
}

#endif // UBUNTU_APPLICATION_UI_INIT_H_
