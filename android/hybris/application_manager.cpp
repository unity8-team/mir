/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */
#include "application_manager.h"

#include <binder/Parcel.h>
#include <utils/String8.h>

namespace android
{
IMPLEMENT_META_INTERFACE(ClipboardService, "UbuntuClipboardService");
IMPLEMENT_META_INTERFACE(ApplicationManagerObserver, "UbuntuApplicationManagerObserver");
IMPLEMENT_META_INTERFACE(ApplicationManagerSession, "UbuntuApplicationManagerSession");
IMPLEMENT_META_INTERFACE(ApplicationManager, "UbuntuApplicationManager");

IClipboardService::Content::Content() : data(NULL),
                                        data_size(0)
{
}

IClipboardService::Content::Content(
    const String8& mime_type, 
    void* _data, 
    size_t size) : mime_type(mime_type),
                        data(malloc(size)),
                        data_size(size)
{
    memcpy(this->data, _data, size);
}

IClipboardService::Content::~Content()
{
    if (data != NULL && data_size != 0)
        free(data);
}

IClipboardService::Content::Content(const IClipboardService::Content& content) 
        : mime_type(content.mime_type),
          data(malloc(content.data_size)),          
          data_size(content.data_size)
{
    memcpy(data, content.data, data_size);
}

IClipboardService::Content& IClipboardService::Content::operator=(const IClipboardService::Content& content) 
{
    mime_type = content.mime_type;
    data_size = content.data_size;
    data = realloc(data, data_size);
    memcpy(data, content.data, data_size);

    return *this;
}

status_t BnClipboardService::onTransact(uint32_t code,
                                        const Parcel& data,
                                        Parcel* reply,
                                        uint32_t flags)
{
    switch(code)
    {
        case SET_CLIPBOARD_CONTENT_COMMAND:
            {
                IClipboardService::Content content;
                String8 mime_type = data.readString8();
                size_t data_size = data.readInt32();
                void* p = malloc(data_size);
                data.read(p, data_size);
                set_content(Content(mime_type, p, data_size));
                free(p);
                break;
            }
        case GET_CLIPBOARD_CONTENT_COMMAND:
            {
                IClipboardService::Content content;
                get_content(content);

                reply->writeString8(String8(content.mime_type));
                reply->writeInt32(content.data_size);
                reply->write(content.data, content.data_size);
            }
            break;
    }

    return NO_ERROR;
}

BpClipboardService::BpClipboardService(const sp<IBinder>& impl) : BpInterface<IClipboardService>(impl)
{
}

void BpClipboardService::set_content(const IClipboardService::Content& content)
{
    Parcel in, out;

    in.writeString8(String8(content.mime_type));
    in.writeInt32(content.data_size);
    in.write(content.data, content.data_size);
    
    remote()->transact(
        SET_CLIPBOARD_CONTENT_COMMAND,
        in,
        &out);
}

void BpClipboardService::get_content(IClipboardService::Content& content)
{
    Parcel in, out;

    remote()->transact(
        GET_CLIPBOARD_CONTENT_COMMAND,
        in,
        &out);

    content.mime_type = out.readString8();
    content.data_size = out.readInt32();
    content.data = malloc(content.data_size);
    out.read(content.data, content.data_size);
}

BnApplicationManagerSession::BnApplicationManagerSession()
{
}

BnApplicationManagerSession::~BnApplicationManagerSession() {}

status_t BnApplicationManagerSession::onTransact(uint32_t code,
        const Parcel& data,
        Parcel* reply,
        uint32_t flags)
{
    switch(code)
    {
    case RAISE_APPLICATION_SURFACES_TO_LAYER_COMMAND:
    {
        int32_t layer;
        data.readInt32(&layer);

        raise_application_surfaces_to_layer(layer);
    }
    break;
    case RAISE_SURFACE_TO_LAYER_COMMAND:
    {
        int32_t token, layer;
        token = data.readInt32();
        layer = data.readInt32();
        
        raise_surface_to_layer(token, layer);
    }
    break;
    case QUERY_SURFACE_PROPERTIES_FOR_TOKEN_COMMAND:
    {
        int32_t token = data.readInt32();
        IApplicationManagerSession::SurfaceProperties props =
            query_surface_properties_for_token(token);
        reply->writeInt32(props.layer);
        reply->writeInt32(props.left);
        reply->writeInt32(props.top);
        reply->writeInt32(props.right);
        reply->writeInt32(props.bottom);
    }
    }
    return NO_ERROR;
}

BpApplicationManagerSession::BpApplicationManagerSession(const sp<IBinder>& impl)
    : BpInterface<IApplicationManagerSession>(impl)
{
}

BpApplicationManagerSession::~BpApplicationManagerSession()
{
}

void BpApplicationManagerSession::raise_surface_to_layer(int32_t token, int layer)
{
    Parcel in, out;

    in.writeInt32(token);
    in.writeInt32(layer);
    
    remote()->transact(
        RAISE_SURFACE_TO_LAYER_COMMAND,
        in,
        &out);
}

void BpApplicationManagerSession::raise_application_surfaces_to_layer(int layer)
{
    Parcel in, out;
    in.writeInt32(layer);

    remote()->transact(
        RAISE_APPLICATION_SURFACES_TO_LAYER_COMMAND,
        in,
        &out);
}

IApplicationManagerSession::SurfaceProperties BpApplicationManagerSession::query_surface_properties_for_token(int32_t token)
{
    Parcel in, out;
    in.writeInt32(token);

    remote()->transact(
        QUERY_SURFACE_PROPERTIES_FOR_TOKEN_COMMAND,
        in,
        &out);

    IApplicationManagerSession::SurfaceProperties props;
    props.layer = out.readInt32();
    props.left = out.readInt32();
    props.top = out.readInt32();
    props.right = out.readInt32();
    props.bottom = out.readInt32();

    return props;
}

status_t BnApplicationManagerObserver::onTransact(uint32_t code,
        const Parcel& data,
        Parcel* reply,
        uint32_t flags)
{
    

    switch(code)
    {
    case ON_SESSION_REQUESTED_NOTIFICATION:
        {
            uint32_t app = data.readInt32();
            on_session_requested(app);
            break;
        }
    case ON_SESSION_BORN_NOTIFICATION:
        {
            int id = data.readInt32();
            int stage_hint = data.readInt32();
            String8 desktop_file = data.readString8();
            on_session_born(id, stage_hint, desktop_file);
            break;
        }
    case ON_SESSION_UNFOCUSED_NOTIFICATION:
        {
            int id = data.readInt32();
            int stage_hint = data.readInt32();
            String8 desktop_file = data.readString8();
            on_session_unfocused(id, stage_hint, desktop_file);
            break;
        }
    case ON_SESSION_FOCUSED_NOTIFICATION:
        {
            int id = data.readInt32();
            int stage_hint = data.readInt32();
            String8 desktop_file = data.readString8();
            on_session_focused(id, stage_hint, desktop_file);
            break;
        }
    case ON_KEYBOARD_GEOMETRY_CHANGED_NOTIFICATION:
        {
            int x = data.readInt32();
            int y = data.readInt32();
            int width = data.readInt32();
            int height = data.readInt32();
            on_keyboard_geometry_changed(x, y, width, height);
            break;
        }
    case ON_SESSION_REQUESTED_FULLSCREEN_NOTIFICATION:
        {
            int id = data.readInt32();
            int stage_hint = data.readInt32();
            String8 desktop_file = data.readString8();
            on_session_requested_fullscreen(id, stage_hint, desktop_file);
            break;
        }
    case ON_SESSION_DIED_NOTIFICATION:
        {
            int id = data.readInt32();
            int stage_hint = data.readInt32();
            String8 desktop_file = data.readString8();
            on_session_died(id, stage_hint, desktop_file);
            break;
        }
    }

    return NO_ERROR;
}

BpApplicationManagerObserver::BpApplicationManagerObserver(const sp<IBinder>& impl)
    : BpInterface<IApplicationManagerObserver>(impl)
{
}

void BpApplicationManagerObserver::on_session_requested(
    uint32_t app)
{
    Parcel in, out;
    in.writeInt32(app);

    remote()->transact(
        ON_SESSION_REQUESTED_NOTIFICATION,
        in,
        &out,
        android::IBinder::FLAG_ONEWAY);
}

void BpApplicationManagerObserver::on_session_born(int id,
                                                   int stage_hint,
                                                   const String8& desktop_file_hint)
{
    Parcel in, out;
    in.writeInt32(id);
    in.writeInt32(stage_hint);
    in.writeString8(desktop_file_hint);

    remote()->transact(
        ON_SESSION_BORN_NOTIFICATION,
        in,
        &out,
        android::IBinder::FLAG_ONEWAY);
}

void BpApplicationManagerObserver::on_session_unfocused(int id,
                                                        int stage_hint,
                                                        const String8& desktop_file_hint)
{
    Parcel in, out;
    in.writeInt32(id);
    in.writeInt32(stage_hint);
    in.writeString8(desktop_file_hint);

    remote()->transact(
        ON_SESSION_UNFOCUSED_NOTIFICATION,
        in,
        &out,
        android::IBinder::FLAG_ONEWAY);
}

void BpApplicationManagerObserver::on_session_focused(int id,
                                                      int stage_hint,
                                                      const String8& desktop_file_hint)
{
    Parcel in, out;
    in.writeInt32(id);
    in.writeInt32(stage_hint);
    in.writeString8(desktop_file_hint);

    remote()->transact(
        ON_SESSION_FOCUSED_NOTIFICATION,
        in,
        &out,
        android::IBinder::FLAG_ONEWAY);
}

void BpApplicationManagerObserver::on_keyboard_geometry_changed(int x,
                                                                int y,
                                                                int width,
                                                                int height)
{
    Parcel in, out;
    in.writeInt32(x);
    in.writeInt32(y);
    in.writeInt32(width);
    in.writeInt32(height);

    remote()->transact(
        ON_KEYBOARD_GEOMETRY_CHANGED_NOTIFICATION,
        in,
        &out,
        android::IBinder::FLAG_ONEWAY);
}

void BpApplicationManagerObserver::on_session_requested_fullscreen(int id,
                                                                   int stage_hint,
                                                                   const String8& desktop_file_hint)
{
    Parcel in, out;
    in.writeInt32(id);
    in.writeInt32(stage_hint);
    in.writeString8(desktop_file_hint);

    remote()->transact(
        ON_SESSION_REQUESTED_FULLSCREEN_NOTIFICATION,
        in,
        &out,
        android::IBinder::FLAG_ONEWAY);
}

void BpApplicationManagerObserver::on_session_died(int id,
                                                   int stage_hint,
                                                   const String8& desktop_file_hint)
{
    Parcel in, out;
    in.writeInt32(id);
    in.writeInt32(stage_hint);
    in.writeString8(desktop_file_hint);

    remote()->transact(
        ON_SESSION_DIED_NOTIFICATION,
        in,
        &out,
        android::IBinder::FLAG_ONEWAY);
}

BnApplicationManager::BnApplicationManager()
{
}

BnApplicationManager::~BnApplicationManager()
{
}

status_t BnApplicationManager::onTransact(uint32_t code,
        const Parcel& data,
        Parcel* reply,
        uint32_t flags)
{
    switch(code)
    {
    case START_A_NEW_SESSION_COMMAND:
    {
        int32_t session_type = data.readInt32();
        int32_t stage_hint = data.readInt32();
        String8 app_name = data.readString8();
        String8 desktop_file = data.readString8();
        sp<IBinder> binder = data.readStrongBinder();
        sp<BpApplicationManagerSession> session(new BpApplicationManagerSession(binder));
        int fd = data.readFileDescriptor();

        start_a_new_session(session_type, stage_hint, app_name, desktop_file, session, fd);
    }
    break;
    case REGISTER_A_SURFACE_COMMAND:
    {
        String8 title = data.readString8();
        sp<IBinder> binder = data.readStrongBinder();
        sp<BpApplicationManagerSession> session(new BpApplicationManagerSession(binder));
        int32_t surface_role = data.readInt32();
        int32_t surface_token = data.readInt32();
        int fd = data.readFileDescriptor();

        register_a_surface(title, session, surface_role, surface_token, fd);
    }
    break;
    case GET_SESSION_PID_COMMAND:
    {
        sp<IBinder> binder = data.readStrongBinder();
        sp<BpApplicationManagerSession> session(new BpApplicationManagerSession(binder));
        int pid = get_session_pid(session);
        reply->writeInt32(pid);
    }
    break;
    case REQUEST_FULLSCREEN_COMMAND:
    {
        sp<IBinder> binder = data.readStrongBinder();
        sp<BpApplicationManagerSession> session(new BpApplicationManagerSession(binder));
        request_fullscreen(session);
    }
    break;
    case REGISTER_AN_OBSERVER_COMMAND:
    {
        sp<IBinder> binder = data.readStrongBinder();
        sp<BpApplicationManagerObserver> observer(new BpApplicationManagerObserver(binder));
        register_an_observer(observer);
        break;
    }
    case REQUEST_UPDATE_FOR_SESSION_COMMAND:
    {
        sp<IBinder> binder = data.readStrongBinder();
        sp<BpApplicationManagerSession> session(new BpApplicationManagerSession(binder));
        request_update_for_session(session);
        break;
    }
    case UNFOCUS_RUNNING_SESSIONS_COMMAND:
    {
        unfocus_running_sessions();
        break;
    }
    case FOCUS_RUNNING_SESSION_WITH_ID_COMMAND:
    {
        int32_t id = data.readInt32();
        focus_running_session_with_id(id);
        break;
    }
    case QUERY_SNAPSHOT_LAYER_FOR_SESSION_WITH_ID_COMMAND:
    {
        int32_t id = data.readInt32();
        int32_t layer = query_snapshot_layer_for_session_with_id(id);
        reply->writeInt32(layer);
        break;
    }     
    case QUERY_SURFACE_PROPERTIES_FOR_SESSION_ID_COMMAND:
    {
        int32_t id = data.readInt32();
        IApplicationManagerSession::SurfaceProperties props =
                    query_surface_properties_for_session_id(id);
        reply->writeInt32(props.layer);
        reply->writeInt32(props.left);
        reply->writeInt32(props.top);
        reply->writeInt32(props.right);
        reply->writeInt32(props.bottom);
        break;
    }
    case SWITCH_TO_WELL_KNOWN_APPLICATION_COMMAND:
    {
        int32_t app = data.readInt32();
        switch_to_well_known_application(app);
        break;
    }
    case SET_SURFACE_TRAP_COMMAND:
    {
        int32_t x = data.readInt32();
        int32_t y = data.readInt32();
        int32_t width = data.readInt32();
        int32_t height = data.readInt32();
        int32_t handle = set_surface_trap(x, y, width, height);
        reply->writeInt32(handle);
        break;
    }
    case UNSET_SURFACE_TRAP_COMMAND:
    {
        int32_t handle = data.readInt32();
        unset_surface_trap(handle);
        break;
    }
    case REPORT_OSK_VISIBLE_COMMAND:
    {
        int32_t x = data.readInt32();
        int32_t y = data.readInt32();
        int32_t width = data.readInt32();
        int32_t height = data.readInt32();
        report_osk_visible(x, y, width, height);
        break;
    }
    case REPORT_OSK_INVISIBLE_COMMAND:
    {
        report_osk_invisible();
        break;
    }
    case REPORT_NOTIFICATION_VISIBLE_COMMAND:
    {
        report_notification_visible();
        break;
    }
    case REPORT_NOTIFICATION_INVISIBLE_COMMAND:
    {
        report_notification_invisible();
        break;
    }
    }
    return NO_ERROR;
}

BpApplicationManager::BpApplicationManager(const sp<IBinder>& impl)
    : BpInterface<IApplicationManager>(impl)
{
}

BpApplicationManager::~BpApplicationManager()
{
}

void BpApplicationManager::start_a_new_session(
    int32_t session_type,
    int32_t stage_hint,
    const String8& app_name,
    const String8& desktop_file,
    const sp<IApplicationManagerSession>& session,
    int fd)
{
    //printf("%s \n", __PRETTY_FUNCTION__);
    Parcel in, out;
    in.pushAllowFds(true);
    in.writeInt32(session_type);
    in.writeInt32(stage_hint);
    in.writeString8(app_name);
    in.writeString8(desktop_file);
    in.writeStrongBinder(session->asBinder());
    in.writeFileDescriptor(fd);

    remote()->transact(START_A_NEW_SESSION_COMMAND,
                       in,
                       &out);
}

void BpApplicationManager::register_a_surface(
    const String8& title,
    const sp<IApplicationManagerSession>& session,
    int32_t surface_role,
    int32_t token,
    int fd)
{
    //printf("%s \n", __PRETTY_FUNCTION__);
    Parcel in, out;
    in.pushAllowFds(true);
    in.writeString8(title);
    in.writeStrongBinder(session->asBinder());
    in.writeInt32(surface_role);
    in.writeInt32(token);
    in.writeFileDescriptor(fd);

    remote()->transact(REGISTER_A_SURFACE_COMMAND,
                       in,
                       &out);
}

int BpApplicationManager::get_session_pid(
    const sp<IApplicationManagerSession>& session)
{
    Parcel in, out;
    in.writeStrongBinder(session->asBinder());

    remote()->transact(GET_SESSION_PID_COMMAND,
                       in,
                       &out);                   
    
    int32_t pid = out.readInt32();

    return pid; 
}


void BpApplicationManager::request_fullscreen(
    const sp<IApplicationManagerSession>& session)
{
    //printf("%s \n", __PRETTY_FUNCTION__);
    Parcel in, out;
    in.writeStrongBinder(session->asBinder());

    remote()->transact(REQUEST_FULLSCREEN_COMMAND,
                       in,
                       &out);
}

void BpApplicationManager::register_an_observer(const sp<IApplicationManagerObserver>& observer)
{
    Parcel in, out;
    in.writeStrongBinder(observer->asBinder());

    remote()->transact(REGISTER_AN_OBSERVER_COMMAND,
                       in,
                       &out);
}

void BpApplicationManager::request_update_for_session(const sp<IApplicationManagerSession>& session)
{
    Parcel in, out;
    in.writeStrongBinder(session->asBinder());
    remote()->transact(REQUEST_UPDATE_FOR_SESSION_COMMAND,
                       in,
                       &out);
}

void BpApplicationManager::unfocus_running_sessions()
{
    Parcel in, out;

    remote()->transact(UNFOCUS_RUNNING_SESSIONS_COMMAND,
                       in,
                       &out);
}

void BpApplicationManager::focus_running_session_with_id(int id)
{
    Parcel in, out;
    in.writeInt32(id);

    remote()->transact(FOCUS_RUNNING_SESSION_WITH_ID_COMMAND,
                       in,
                       &out);
}

int32_t BpApplicationManager::query_snapshot_layer_for_session_with_id(int id)
{
    Parcel in, out;
    in.writeInt32(id);
    remote()->transact(QUERY_SNAPSHOT_LAYER_FOR_SESSION_WITH_ID_COMMAND,
                       in,
                       &out);

    int32_t layer = out.readInt32();
    return layer;
}

IApplicationManagerSession::SurfaceProperties BpApplicationManager::query_surface_properties_for_session_id(int id)
{
    Parcel in, out;
    in.writeInt32(id);

    remote()->transact(QUERY_SURFACE_PROPERTIES_FOR_SESSION_ID_COMMAND,
                       in,
                       &out);

    IApplicationManagerSession::SurfaceProperties props; 
    props.layer = out.readInt32();
    props.left = out.readInt32();
    props.top = out.readInt32();
    props.right = out.readInt32();
    props.bottom = out.readInt32();

    return props;
}
void BpApplicationManager::switch_to_well_known_application(int32_t app)
{
    Parcel in, out;
    in.writeInt32(app);

    remote()->transact(SWITCH_TO_WELL_KNOWN_APPLICATION_COMMAND,
                       in,
                       &out);
}

int32_t BpApplicationManager::set_surface_trap(int32_t x, int32_t y, int32_t width, int32_t height)
{
    Parcel in, out;
    in.writeInt32(x);
    in.writeInt32(y);
    in.writeInt32(width);
    in.writeInt32(height);

    remote()->transact(SET_SURFACE_TRAP_COMMAND,
                       in,
                       &out);

    int32_t handle = out.readInt32();

    return handle;
}

void BpApplicationManager::unset_surface_trap(int32_t handle)
{
    Parcel in, out;
    in.writeInt32(handle);

    remote()->transact(UNSET_SURFACE_TRAP_COMMAND,
                       in,
                       &out);
}

void BpApplicationManager::report_osk_visible(int32_t x, int32_t y, int32_t width, int32_t height)
{
    Parcel in, out;
    in.writeInt32(x);
    in.writeInt32(y);
    in.writeInt32(width);
    in.writeInt32(height);

    remote()->transact(REPORT_OSK_VISIBLE_COMMAND,
                       in,
                       &out);
}
    
void BpApplicationManager::report_osk_invisible()
{
    Parcel in, out;

    remote()->transact(REPORT_OSK_INVISIBLE_COMMAND,
                       in,
                       &out);
}
    
void BpApplicationManager::report_notification_visible()
{
    Parcel in, out;

    remote()->transact(REPORT_NOTIFICATION_VISIBLE_COMMAND,
                       in,
                       &out);
}
    
void BpApplicationManager::report_notification_invisible()
{
    Parcel in, out;

    remote()->transact(REPORT_NOTIFICATION_INVISIBLE_COMMAND,
                       in,
                       &out);
}

}
