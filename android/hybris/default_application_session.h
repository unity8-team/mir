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
#ifndef DEFAULT_APPLICATION_SESSION_H_
#define DEFAULT_APPLICATION_SESSION_H_

#include "application_manager.h"

#include "ubuntu/application/ui/session_credentials.h"
#include "ubuntu/application/ui/surface_role.h"
#include "ubuntu/application/ui/stage_hint.h"

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>

namespace mir
{

struct ApplicationSession : public android::RefBase
{
    struct Surface : public android::RefBase
    {
        Surface(ApplicationSession* parent,
                const android::sp<android::InputChannel>& input_channel,
                int32_t surface_role,
                int32_t token) 
                : parent(parent),
                  input_channel(input_channel),
                  role(static_cast<ubuntu::application::ui::SurfaceRole>(surface_role)),
                  token(token)
        {
        }

        android::IApplicationManagerSession::SurfaceProperties query_properties()
        {
            android::IApplicationManagerSession::SurfaceProperties props =
                parent->remote_session->query_surface_properties_for_token(token);

            return props;
        }

        android::sp<android::InputWindowHandle> make_input_window_handle()
        {
            return android::sp<android::InputWindowHandle>(new InputWindowHandle(parent, android::sp<Surface>(this)));
        }

        ApplicationSession* parent;
        android::sp<android::InputChannel> input_channel;
        ubuntu::application::ui::SurfaceRole role;
        int32_t token;
    };

    ApplicationSession(
        pid_t remote_pid,
        android::sp<android::IApplicationManagerSession> remote_session,
        int32_t session_type,
        int32_t stage_hint,
        const android::String8& app_name,
        const android::String8& desktop_file)
            : running_state(ubuntu::application::ui::process_running),
            remote_pid(remote_pid),
            app_layer(0),
            remote_session(remote_session),
            session_type(static_cast<ubuntu::application::ui::SessionType>(session_type)),
            stage_hint(stage_hint),
            app_name(app_name),
            desktop_file(desktop_file)
    {
    }

    struct InputApplicationHandle : public android::InputApplicationHandle
    {
        InputApplicationHandle(ApplicationSession* parent) : parent(parent)
        {
            updateInfo();
        }

        bool updateInfo()
        {
            if (mInfo == NULL)
            {
                mInfo = new android::InputApplicationInfo();
                mInfo->name = parent->app_name;
                mInfo->dispatchingTimeout = INT_MAX;
            }

            return true;
        }

        ApplicationSession* parent;
    };

    struct InputWindowHandle : public android::InputWindowHandle
    {
        InputWindowHandle(ApplicationSession* parent, const android::sp<Surface>& surface)
            : android::InputWindowHandle(
                android::sp<InputApplicationHandle>(
                    new InputApplicationHandle(parent))),
            parent(parent),
            surface(surface)
        {
            updateInfo();
        }

        bool updateInfo()
        {
            if (mInfo == NULL)
            {
                mInfo = new android::InputWindowInfo();
            }
            
            android::IApplicationManagerSession::SurfaceProperties props;
            if (parent->running_state == ubuntu::application::ui::process_stopped)
            {
                kill(parent->remote_pid, SIGCONT);            
                props = surface->query_properties();
                kill(parent->remote_pid, SIGSTOP);
            } else
                props = surface->query_properties();

            ALOGI("%s: touchable_region = (%d, %d, %d, %d)", 
                 __PRETTY_FUNCTION__,
                 props.left, 
                 props.top, 
                 props.right, 
                 props.bottom);
            
            SkRegion touchable_region;
            touchable_region.setRect(props.left, props.top, props.right+1, props.bottom+1);
            
            mInfo->name = parent->app_name;
            mInfo->layoutParamsFlags = android::InputWindowInfo::FLAG_NOT_TOUCH_MODAL;
            mInfo->layoutParamsType = android::InputWindowInfo::TYPE_APPLICATION;
            mInfo->touchableRegion = touchable_region;
            mInfo->frameLeft = props.left;
            mInfo->frameTop = props.top;
            mInfo->frameRight = props.right+1;
            mInfo->frameBottom = props.bottom+1;
            mInfo->scaleFactor = 1.f;
            mInfo->visible = true;
            mInfo->canReceiveKeys = true;
            mInfo->hasFocus = true;
            mInfo->hasWallpaper = false;
            mInfo->paused = false;
            mInfo->layer = props.layer;
            mInfo->dispatchingTimeout = INT_MAX;
            mInfo->ownerPid = 0;
            mInfo->ownerUid = 0;
            mInfo->inputFeatures = 0;
            mInfo->inputChannel = surface->input_channel;
            
            return true;
        }

        ApplicationSession* parent;
        android::sp<Surface> surface;
    };

    android::Vector< android::sp<android::InputWindowHandle> > input_window_handles()
    {
        android::Vector< android::sp<android::InputWindowHandle> > v;
        //for(size_t i = 0; i < registered_surfaces.size(); i++)
        for(int i = registered_surfaces.size()-1; i >= 0; i--)
        {
            v.push_back(registered_surfaces.valueAt(i)->make_input_window_handle());
        }

        return v;
    }

    android::sp<android::InputApplicationHandle> input_application_handle()
    {
        return android::sp<android::InputApplicationHandle>(new InputApplicationHandle(this));
    }

    int32_t layer() const
    {
        return app_layer;
    }

    android::IApplicationManagerSession::SurfaceProperties query_properties() const
    {
        if (!registered_surfaces.size())
            return android::IApplicationManagerSession::SurfaceProperties();

        android::IApplicationManagerSession::SurfaceProperties props =
            registered_surfaces.valueAt(registered_surfaces.size()-1)->query_properties();

        return props;
    }
  
    void raise_application_surfaces_to_layer(int layer)
    {
        app_layer = layer;
        remote_session->raise_application_surfaces_to_layer(layer);
    }

    void raise_surface_to_layer(int32_t token, int layer)
    {        
        remote_session->raise_surface_to_layer(token, layer);
    }

    void register_surface(const android::sp<Surface>& surface)
    {
        registered_surfaces.add(surface->token, surface);
    }

    pid_t remote_pid;
    int32_t running_state;
    int32_t app_layer;

    android::sp<android::IApplicationManagerSession> remote_session;
    ubuntu::application::ui::SessionType session_type;
    int32_t stage_hint;
    android::String8 app_name;
    android::String8 desktop_file;
    android::KeyedVector<int32_t, android::sp<Surface>> registered_surfaces;
};

}
#endif // DEFAULT_APPLICATION_SESSION_H_
