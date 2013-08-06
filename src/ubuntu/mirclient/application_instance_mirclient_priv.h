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

#ifndef UBUNTU_APPLICATION_INSTANCE_MIRCLIENT_PRIV_H_
#define UBUNTU_APPLICATION_INSTANCE_MIRCLIENT_PRIV_H_

#include <ubuntu/application/instance.h>

#include <memory>
#include <string>

#include <mir_toolkit/mir_client_library.h>

namespace ubuntu
{
namespace application
{
namespace mir
{
namespace client
{

class Instance
{
public:
    Instance();
    ~Instance() = default;
    
    UApplicationInstance* as_u_application_instance();
    static Instance* from_u_application_instance(UApplicationInstance* u_instance);
    
    void ref();
    void unref();
    
    MirConnection* connection() const;
    bool connect(std::string const& application_name);
    
protected:
    Instance(Instance const&) = delete;
    Instance& operator=(Instance const&) = delete;

private:
    typedef std::unique_ptr<MirConnection, std::function<void(MirConnection*)>> ConnectionPtr;
    ConnectionPtr con;
    int ref_count;
};

}
}
}
} // namespace ubuntu

#endif // UBUNTU_APPLICATION_INSTANCE_MIRCLIENT_PRIV_H_
