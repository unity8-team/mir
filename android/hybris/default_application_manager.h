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
#ifndef DEFAULT_APPLICATION_MANAGER_H_
#define DEFAULT_APPLICATION_MANAGER_H_

#include "application_manager.h"
#include "default_application_manager_input_setup.h"
#include "default_application_session.h"
#include "event_loop.h"

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>

#include <input/InputListener.h>
#include <input/InputReader.h>
#include <androidfw/InputTransport.h>
#include <utils/threads.h>

namespace mir
{
struct ApplicationManager : 
            public android::BnApplicationManager,
            public android::IBinder::DeathRecipient
{
    static const int default_shell_component_layer = 1000000;
    
    static const int default_dash_layer = default_shell_component_layer + 1;
    static const int default_indicator_layer = default_shell_component_layer + 2;
    static const int default_notifications_layer = default_shell_component_layer + 3;
    static const int default_greeter_layer = default_shell_component_layer + 4;
    static const int default_launcher_layer = default_shell_component_layer + 5;
    static const int default_osk_layer = default_shell_component_layer + 6;
    static const int default_shutdown_dialog_layer = default_shell_component_layer + 7;
    
    static const int focused_application_base_layer = 100;
    static const int wallpaper_layer = 0;
    static const int non_focused_application_layer = -1;

    struct ShellInputSetup : public android::RefBase
    {
        struct DisplayInfo
        {
            DisplayInfo();

            android::DisplayInfo info;
        };

        template<int x, int y, int w, int h>
        struct Window : public android::RefBase
        {
            static int looper_callback(int receiveFd, int events, void* ctxt);

            Window(ShellInputSetup* parent, 
                   int _x = x,
                   int _y = y,
                   int _w = w,
                   int _h = h);
            
            android::sp<android::InputSetup::DummyApplicationWindow> input_window;
            
            ShellInputSetup* parent;
            android::sp<android::InputChannel> server_channel;
            android::sp<android::InputChannel> client_channel;
            android::InputConsumer input_consumer;
            android::PreallocatedInputEventFactory event_factory;
        };
        
        ShellInputSetup(const android::sp<android::InputManager>& input_manager);

        bool shell_has_focus;
    
        DisplayInfo display_info;
        
        android::sp<android::InputManager> input_manager;
        android::sp<android::InputApplicationHandle> shell_application;
        
        android::sp<android::Looper> looper;
        ubuntu::application::EventLoop event_loop;

        android::KeyedVector<int32_t,  android::sp<android::InputWindowHandle> > trap_windows;

        // TODO(tvoss): Get rid of hard coded values.
        Window<0, 0, 720, 1280> event_trap_window;
        // TODO(tvoss): This is really hacky, but we need to
        // synchronize/reflect state changes across multiple processes
        // here, i.e.: 
        //   * maliit-server, which takes care of hiding and showing the osk 
        //   * notify-osd, which takes care of hiding and showing notifications
        Window<0, 812, 720, 468> osk_window;
        Window<36, 18, 684, 216> notifications_window;
    };

    class InputFilter : public android::InputFilter
    {
      public:
        InputFilter(ApplicationManager* manager);

        bool filter_event(const android::InputEvent* event);

      private:
        ApplicationManager* manager;

        bool handle_key_event(const android::KeyEvent* event);
    };

    class LockingIterator : public android::RefBase
    {
      public:
        void advance() ;

        bool is_valid() const;

        void make_current();

        const android::sp<mir::ApplicationSession>& operator*();

      protected:
        friend class ApplicationManager;

        LockingIterator(
            ApplicationManager* manager,
            size_t index);

        virtual ~LockingIterator();

      private:
        ApplicationManager* manager;
        size_t it;
    };

    ApplicationManager();

    void update_app_lists();

    // From DeathRecipient
    void binderDied(const android::wp<android::IBinder>& who);

    void lock();

    void unlock();

    android::sp<LockingIterator> iterator();

    void start_a_new_session(
        int32_t session_type,
        int32_t stage_hint,
        const android::String8& app_name,
        const android::String8& desktop_file,
        const android::sp<android::IApplicationManagerSession>& session,
        int fd);

    void register_a_surface(
        const android::String8& title,
        const android::sp<android::IApplicationManagerSession>& session,
        int32_t surface_role,
        int32_t token,
        int fd);

    void request_fullscreen(const android::sp<android::IApplicationManagerSession>& session);

    void register_an_observer(const android::sp<android::IApplicationManagerObserver>& observer);

    int get_session_pid(const android::sp<android::IApplicationManagerSession>& session);

    void request_update_for_session(const android::sp<android::IApplicationManagerSession>& session);

    void focus_running_session_with_id(int id);

    void unfocus_running_sessions();

    int32_t query_snapshot_layer_for_session_with_id(int id);

    android::IApplicationManagerSession::SurfaceProperties query_surface_properties_for_session_id(int id);

    void switch_to_well_known_application(int32_t app);

    int32_t set_surface_trap(int32_t x, int32_t y, int32_t width, int32_t height);

    void unset_surface_trap(int32_t handle);

    void report_osk_visible(int32_t x, int32_t y, int32_t width, int32_t height);
    
    void report_osk_invisible();

    void report_notification_visible();

    void report_notification_invisible();

    void switch_focused_application_locked(size_t index_of_next_focused_app);
    void switch_focus_to_next_application_locked();

    void kill_focused_application_locked();
    
  private:
    void update_input_setup_locked();

    size_t session_id_to_index(int id);
    void notify_observers_about_session_requested(uint32_t app);
    void notify_observers_about_session_born(int id, int stage_hint, const android::String8& desktop_file);
    void notify_observers_about_session_unfocused(int id, int stage_hint, const android::String8& desktop_file);
    void notify_observers_about_session_focused(int id, int stage_hint, const android::String8& desktop_file);
    void notify_observers_about_keyboard_geometry_changed(int x, int y, int width, int height);
    void notify_observers_about_session_requested_fullscreen(int id, int stage_hint, const android::String8& desktop_file);
    void notify_observers_about_session_died(int id, int stage_hint, const android::String8& desktop_file);

    android::sp<android::InputListenerInterface> input_listener;
    android::sp<InputFilter> input_filter;
    android::sp<android::InputSetup> input_setup;
    android::sp<ShellInputSetup> shell_input_setup;
    bool is_osk_visible;
    bool are_notifications_visible;
    android::Mutex guard;
    android::KeyedVector< android::sp<android::IBinder>, android::sp<mir::ApplicationSession> > apps;
    android::Vector< android::sp<android::IBinder> > apps_as_added;
    android::Mutex observer_guard;
    android::Vector< android::sp<android::IApplicationManagerObserver> > app_manager_observers;
    size_t focused_application;    
    size_t side_stage_application;
    size_t main_stage_application;
};

}

#endif // DEFAULT_APPLICATION_MANAGER_H_
