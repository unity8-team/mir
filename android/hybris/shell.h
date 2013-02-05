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
#ifndef SHELL_H_
#define SHELL_H_

#include <utils/StrongPointer.h>

namespace mir
{

class ApplicationManager;

class Shell : public android::RefBase
{
public:

    static android::sp<Shell> instance();

protected:
    Shell(const android::sp<ApplicationManager>& app_manager) : app_manager(app_manager)
    {
    }

    virtual ~Shell()
    {
    }

    android::sp<ApplicationManager> app_manager;
};

}

#endif // SHELL_H_
