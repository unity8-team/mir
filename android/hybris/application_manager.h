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
#ifndef HYBRIS_APPLICATION_MANAGER_H_
#define HYBRIS_APPLICATION_MANAGER_H_

#include <binder/IInterface.h>
#include <utils/String8.h>

namespace android
{

class IClipboardService : public IInterface
{
public:
    DECLARE_META_INTERFACE(ClipboardService);

    static const char* exported_service_name()
    {
        return "UbuntuClipboardService";
    }

    struct Content
    {
        Content();
        Content(const String8& mime_type, void* data, size_t data_size);
        ~Content();
        Content(const Content& rhs);
        Content& operator=(const Content& rhs);
        
        String8 mime_type;
        void* data;
        size_t data_size;
    };

    virtual void set_content(const Content& content) = 0;
    virtual void get_content(Content& content) = 0;

protected:
    enum
    {
        SET_CLIPBOARD_CONTENT_COMMAND = IBinder::FIRST_CALL_TRANSACTION,
        GET_CLIPBOARD_CONTENT_COMMAND
    };
};

class BnClipboardService : public BnInterface<IClipboardService>
{
  public:
    status_t onTransact(uint32_t code,
                        const Parcel& data,
                        Parcel* reply,
                        uint32_t flags = 0);
};

class BpClipboardService : public BpInterface<IClipboardService>
{
public:
    BpClipboardService(const sp<IBinder>& impl);
    
    void set_content(const IClipboardService::Content& content);
    void get_content(IClipboardService::Content& content);

};

class IApplicationManagerSession : public IInterface
{
public:
    DECLARE_META_INTERFACE(ApplicationManagerSession);

    struct SurfaceProperties
    {
        int32_t layer;
        int32_t left;
        int32_t top;
        int32_t right;
        int32_t bottom;
    };

    virtual void raise_application_surfaces_to_layer(int layer) = 0;
    virtual void raise_surface_to_layer(int32_t token, int layer) = 0;
    virtual SurfaceProperties query_surface_properties_for_token(int32_t token) = 0;

protected:
    enum
    {
        RAISE_APPLICATION_SURFACES_TO_LAYER_COMMAND = IBinder::FIRST_CALL_TRANSACTION,
        RAISE_SURFACE_TO_LAYER_COMMAND,
        QUERY_SURFACE_PROPERTIES_FOR_TOKEN_COMMAND
    };
};

class BnApplicationManagerSession : public BnInterface<IApplicationManagerSession>
{
public:
    BnApplicationManagerSession();
    virtual ~BnApplicationManagerSession();

    virtual status_t onTransact(uint32_t code,
                                const Parcel& data,
                                Parcel* reply,
                                uint32_t flags = 0);
};

class BpApplicationManagerSession : public BpInterface<IApplicationManagerSession>
{
public:
    BpApplicationManagerSession(const sp<IBinder>& impl);
    ~BpApplicationManagerSession();

    void raise_application_surfaces_to_layer(int layer);
    void raise_surface_to_layer(int32_t token, int layer);
    IApplicationManagerSession::SurfaceProperties query_surface_properties_for_token(int32_t token);
};

class IApplicationManagerObserver : public IInterface
{
public:
    DECLARE_META_INTERFACE(ApplicationManagerObserver);

    virtual void on_session_requested(uint32_t app) = 0;

    virtual void on_session_born(int id,
                                 int stage_hint,
                                 const String8& desktop_file) = 0;
    
    virtual void on_session_unfocused(int id,
                                      int stage_hint,
                                      const String8& desktop_file) = 0;

    virtual void on_session_focused(int id,
                                    int stage_hint,
                                    const String8& desktop_file) = 0;

    virtual void on_session_requested_fullscreen(int id,
                                                 int stage_hint,
                                                 const String8& desktop_file) = 0;

    virtual void on_keyboard_geometry_changed(int x,
                                              int y,
                                              int width,
                                              int height) = 0;

    virtual void on_session_died(int id,
                                 int stage_hint,
                                 const String8& desktop_file) = 0;

protected:
    enum
    {
        ON_SESSION_REQUESTED_NOTIFICATION = IBinder::FIRST_CALL_TRANSACTION,
        ON_SESSION_BORN_NOTIFICATION,
        ON_SESSION_UNFOCUSED_NOTIFICATION,
        ON_SESSION_FOCUSED_NOTIFICATION,
        ON_KEYBOARD_GEOMETRY_CHANGED_NOTIFICATION,
        ON_SESSION_REQUESTED_FULLSCREEN_NOTIFICATION,
        ON_SESSION_DIED_NOTIFICATION
    };

    IApplicationManagerObserver(const IApplicationManagerObserver&) = delete;
    IApplicationManagerObserver& operator=(const IApplicationManagerObserver&) = delete;
};

class BnApplicationManagerObserver : public BnInterface<IApplicationManagerObserver>
{
public:
    status_t onTransact(uint32_t code,
                        const Parcel& data,
                        Parcel* reply,
                        uint32_t flags = 0);
};

class BpApplicationManagerObserver : public BpInterface<IApplicationManagerObserver>
{
public:
    BpApplicationManagerObserver(const sp<IBinder>& impl);

    void on_session_requested(uint32_t app);

    void on_session_born(int id,
                         int stage_hint,
                         const String8& desktop_file);

    void on_session_unfocused(int id,
                              int stage_hint,
                              const String8& desktop_file);

    void on_session_focused(int id,
                            int stage_hint,
                            const String8& desktop_file);

    void on_session_requested_fullscreen(int id,
                                         int stage_hint,
                                         const String8& desktop_file);

    void on_keyboard_geometry_changed(int x,
                                      int y,
                                      int width,
                                      int height); 

    void on_session_died(int id,
                         int stage_hint,
                         const String8& desktop_file);
};

class IApplicationManager : public IInterface
{
public:
    DECLARE_META_INTERFACE(ApplicationManager);

    static const char* exported_service_name()
    {
        return "UbuntuApplicationManager";
    }

    virtual void start_a_new_session(int32_t session_type,
                                     int32_t stage_hint,
                                     const String8& app_name,
                                     const String8& desktop_file,
                                     const sp<IApplicationManagerSession>& session,
                                     int fd) = 0;

    virtual void register_a_surface(const String8& title,
                                    const sp<IApplicationManagerSession>& session,
                                    int32_t surface_role,
                                    int32_t token,
                                    int fd) = 0;

    virtual int get_session_pid(const sp<IApplicationManagerSession>& session) = 0;

    virtual void request_fullscreen(const sp<IApplicationManagerSession>& session) = 0;

    virtual void request_update_for_session(const sp<IApplicationManagerSession>& session) = 0;

    virtual void register_an_observer(const sp<IApplicationManagerObserver>& observer) = 0;

    virtual void unfocus_running_sessions() = 0;

    virtual void focus_running_session_with_id(int id) = 0;

    virtual int32_t query_snapshot_layer_for_session_with_id(int id) = 0;
    
    virtual IApplicationManagerSession::SurfaceProperties query_surface_properties_for_session_id(int id) = 0;
    
    virtual void switch_to_well_known_application(int32_t app) = 0;

    virtual int32_t set_surface_trap(int32_t x, int32_t y, int32_t width, int32_t height) = 0;

    virtual void unset_surface_trap(int32_t handle) = 0;
    
    virtual void report_osk_visible(int32_t x, int32_t y, int32_t width, int32_t height) = 0;
    
    virtual void report_osk_invisible() = 0;
    
    virtual void report_notification_visible() = 0;
    
    virtual void report_notification_invisible() = 0;

protected:
    enum
    {
        START_A_NEW_SESSION_COMMAND = IBinder::FIRST_CALL_TRANSACTION,
        REGISTER_A_SURFACE_COMMAND,
        GET_SESSION_PID_COMMAND,
        REQUEST_FULLSCREEN_COMMAND,
        REGISTER_AN_OBSERVER_COMMAND,
        REQUEST_UPDATE_FOR_SESSION_COMMAND,
        UNFOCUS_RUNNING_SESSIONS_COMMAND,
        FOCUS_RUNNING_SESSION_WITH_ID_COMMAND,
        QUERY_SNAPSHOT_LAYER_FOR_SESSION_WITH_ID_COMMAND,
        QUERY_SURFACE_PROPERTIES_FOR_SESSION_ID_COMMAND,
        SWITCH_TO_WELL_KNOWN_APPLICATION_COMMAND,
        SET_SURFACE_TRAP_COMMAND,
        UNSET_SURFACE_TRAP_COMMAND,
        REPORT_OSK_VISIBLE_COMMAND,
        REPORT_OSK_INVISIBLE_COMMAND,
        REPORT_NOTIFICATION_VISIBLE_COMMAND,
        REPORT_NOTIFICATION_INVISIBLE_COMMAND
    };
};

class BnApplicationManager : public BnInterface<IApplicationManager>
{
public:
    BnApplicationManager();
    virtual ~BnApplicationManager();

    virtual status_t onTransact(uint32_t code,
                                const Parcel& data,
                                Parcel* reply,
                                uint32_t flags = 0);
};

class BpApplicationManager : public BpInterface<IApplicationManager>
{
public:
    BpApplicationManager(const sp<IBinder>& impl);
    ~BpApplicationManager();

    void start_a_new_session(int32_t session_type,
                             int32_t stage_hint,
                             const String8& app_name,
                             const String8& desktop_file,
                             const sp<IApplicationManagerSession>& session,
                             int fd);

    void register_a_surface(const String8& title,
                            const android::sp<android::IApplicationManagerSession>& session,
                            int32_t surface_role,
                            int32_t token,
                            int fd);

    int get_session_pid(const android::sp<android::IApplicationManagerSession>& session);

    void request_fullscreen(const android::sp<android::IApplicationManagerSession>& session);

    void request_update_for_session(const sp<IApplicationManagerSession>& session);

    void register_an_observer(const sp<IApplicationManagerObserver>& observer);

    void unfocus_running_sessions();

    void focus_running_session_with_id(int id);
    
    int32_t query_snapshot_layer_for_session_with_id(int id);
    
    IApplicationManagerSession::SurfaceProperties query_surface_properties_for_session_id(int id);
    
    void switch_to_well_known_application(int32_t app);

    int32_t set_surface_trap(int32_t x, int32_t y, int32_t width, int32_t height);

    void unset_surface_trap(int handle);

    void report_osk_visible(int32_t x, int32_t y, int32_t width, int32_t height);
    
    void report_osk_invisible();
    
    void report_notification_visible();
    
    void report_notification_invisible();
};

}

#endif // HYBRIS_APPLICATION_MANAGER_H_
