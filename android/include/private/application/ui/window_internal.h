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
 */

#ifndef UBUNTU_APPLICATION_UI_WINDOW_INTERNAL_H_
#define UBUNTU_APPLICATION_UI_WINDOW_INTERNAL_H_

#include <ubuntu/application/ui/window_properties.h>

namespace ubuntu
{
namespace application
{
namespace ui
{
class WindowProperties : public ubuntu::platform::ReferenceCountedBase
{
public:
    WindowProperties() {}

    ~WindowProperties()
    {
        free(this->title);
    }

    typedef ubuntu::platform::shared_ptr<WindowProperties> Ptr;
   
    void set_titlen(const char* title, size_t size)
    {
        this->title = (char *) malloc(sizeof (char) * (size+1));
        memcpy(this->title, title, size+1);
    }

    char *get_title()
    {
        return this->title;
    }
    
    void set_role(UAUiWindowRole role)
    {
        this->role = role;
    }
   
    UAUiWindowRole get_role()
    {
        return this->role;
    }

    void set_input_event_cb_and_ctx(UAUiWindowInputEventCb cb, void* ctx)
    {
        this->cb = cb;
        this->ctx = ctx;
    }

    UAUiWindowInputEventCb get_input_cb()
    {
        return this->cb;
    }

    void* get_ctx()
    {
        return this->ctx;
    }

private:
    char *title;
    UAUiWindowRole role;
    UAUiWindowInputEventCb cb;
    void* ctx;
   
protected:
    WindowProperties(const WindowProperties&) = delete;
    WindowProperties& operator=(const WindowProperties&) = delete;
};

class SessionProperties : public ubuntu::platform::ReferenceCountedBase
{
public:
    SessionProperties() : pid(0) {}

    typedef ubuntu::platform::shared_ptr<SessionProperties> Ptr;

    void set_type(SessionType type)
    {
        this->type = type;
    }

    SessionType get_type()
    {
        return this->type;
    }

    void set_remote_pid(uint32_t pid)
    {
        this->pid = pid;
    }

    uint32_t get_remote_pid()
    {
        return this->pid;
    }

private:
    SessionType type;
    uint32_t pid;

protected:
    virtual ~SessionProperties() {}

    SessionProperties(const SessionProperties&) = delete;
    SessionProperties& operator=(const SessionProperties&) = delete;
};
}
}
}
#endif /* UBUNTU_APPLICATION_UI_WINDOW_INTERNAL_H_ */
