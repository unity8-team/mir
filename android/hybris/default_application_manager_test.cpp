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
#include "application_manager.h"

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace
{
struct ApplicationManagerSession : public android::BnApplicationManagerSession
{
    ApplicationManagerSession()
    {
    }

    void raise_application_surfaces_to_layer(int layer)
    {
        printf("%s \n", __PRETTY_FUNCTION__);
        printf("%d \n", layer);
    }

    void raise_surface_to_layer(int32_t token, int layer)
    {
        printf("%s: %d, %d \n", __PRETTY_FUNCTION__, token, layer);
    }

    SurfaceProperties query_surface_properties_for_token(int32_t token)
    {
        static const int layer = 100;
        SurfaceProperties props = { layer, 0, 0, 960, 1280 };
        return props;
    }
};

struct ApplicationManagerObserver : public android::BnApplicationManagerObserver
{
    void on_session_requested(uint32_t app)
    {
        printf("%s: %d \n", __PRETTY_FUNCTION__, app);
    }

    void on_session_born(int id,
                         int stage_hint,
                         const android::String8& desktop_file)
    {
        printf("%s: %d, %d, %s \n", __PRETTY_FUNCTION__, id, stage_hint, desktop_file.string());
    }

    void on_session_unfocused(int id,
                              int stage_hint,
                              const android::String8& desktop_file)
    {
        printf("%s: %d, %d, %s \n", __PRETTY_FUNCTION__, id, stage_hint, desktop_file.string());
    }

    void on_session_focused(int id,
                            int stage_hint,
                            const android::String8& desktop_file)
    {
        printf("%s: %d, %d, %s \n", __PRETTY_FUNCTION__, id, stage_hint, desktop_file.string());
    }

    void on_session_requested_fullscreen(int id,
                                         int stage_hint,
                                         const android::String8& desktop_file)
    {
        printf("%s: %d, %s \n", __PRETTY_FUNCTION__, id, desktop_file.string());
    }

    void on_session_died(int id,
                         int stage_hint,
                         const android::String8& desktop_file)
    {
        printf("%s: %d, %d, %s \n", __PRETTY_FUNCTION__, id, stage_hint, desktop_file.string());
    }
};

}

int main(int argc, char** argv)
{
    android::ProcessState::self()->startThreadPool();

    int test_fd = open("test.file", O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);

    android::sp<ApplicationManagerObserver> observer(new ApplicationManagerObserver());
    android::sp<ApplicationManagerSession> session(new ApplicationManagerSession());
    android::sp<android::IServiceManager> service_manager = android::defaultServiceManager();
    android::sp<android::IBinder> service = service_manager->getService(
            android::String16(android::IApplicationManager::exported_service_name()));
    android::BpApplicationManager app_manager(service);

    app_manager.register_an_observer(observer);

    static const int32_t session_type = 0;
    static const int32_t stage_hint = 0;
    
    app_manager.start_a_new_session(
        session_type,
        stage_hint,
        android::String8("default_application_manager_test"),
        android::String8("default_application_manager_test"),
        session,
        test_fd);

    for(;;) {}
}
