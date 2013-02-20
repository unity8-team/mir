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
#include "event_loop.h"
#include "input_consumer_thread.h"

#include <ubuntu/application/ui/clipboard.h>
#include <ubuntu/application/ui/init.h>
#include <ubuntu/application/ui/session.h>
#include <ubuntu/application/ui/session_credentials.h>
#include <ubuntu/application/ui/setup.h>
#include <ubuntu/application/ui/surface.h>
#include <ubuntu/application/ui/surface_factory.h>
#include <ubuntu/application/ui/surface_properties.h>

#include <ubuntu/ui/session_enumerator.h>
#include <ubuntu/ui/session_service.h>
#include <ubuntu/ui/well_known_applications.h>

#include <binder/IMemory.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>
#include <ui/DisplayInfo.h>
#include <androidfw/InputTransport.h>
#include <ui/PixelFormat.h>
#include <ui/Region.h>
#include <ui/Rect.h>
#include <utils/Looper.h>

namespace android
{

struct Clipboard : public ubuntu::application::ui::Clipboard
{
    static sp<BpClipboardService> access_clipboard_service()
    {
        static sp<BpClipboardService> remote_instance;

        if (remote_instance == NULL)
        {
            sp<IServiceManager> service_manager = defaultServiceManager();
            sp<IBinder> service = service_manager->getService(
                String16(IClipboardService::exported_service_name()));
            remote_instance = new BpClipboardService(service);
        }

        return remote_instance;
    }
    
    void set_content(const Clipboard::Content& content)
    {
        IClipboardService::Content c;
        c.mime_type = String8(content.mime_type);
        c.data_size = content.data_size;
        c.data = content.data;
        access_clipboard_service()->set_content(c);
    }

    Clipboard::Content get_content()
    {
        IClipboardService::Content content;
        access_clipboard_service()->get_content(content);

        Clipboard::Content result(content.mime_type.string(),
                                  content.data,
                                  content.data_size);

        memcpy(result.data, content.data, result.data_size);

        return result;
    }
};

struct Setup : public ubuntu::application::ui::Setup
{
    Setup() : stage(ubuntu::application::ui::main_stage),
        form_factor(ubuntu::application::ui::desktop_form_factor),
        desktop_file("/usr/share/applications/shotwell.desktop")
    {
    }

    static android::KeyedVector<android::String8, ubuntu::application::ui::StageHint> init_string_to_stage_hint_lut()
    {
        android::KeyedVector<android::String8, ubuntu::application::ui::StageHint> lut;
        lut.add(android::String8("main_stage"), ubuntu::application::ui::main_stage);
        lut.add(android::String8("side_stage"), ubuntu::application::ui::side_stage);
        lut.add(android::String8("share_stage"), ubuntu::application::ui::share_stage);

        return lut;
    }

    static ubuntu::application::ui::StageHint string_to_stage_hint(const android::String8& s)
    {
        static android::KeyedVector<android::String8, ubuntu::application::ui::StageHint> lut = init_string_to_stage_hint_lut();

        return lut.valueFor(s);
    }

    static android::KeyedVector<android::String8, ubuntu::application::ui::FormFactorHint> init_string_to_form_factor_hint_lut()
    {
        android::KeyedVector<android::String8, ubuntu::application::ui::FormFactorHint> lut;
        lut.add(android::String8("desktop"), ubuntu::application::ui::desktop_form_factor);
        lut.add(android::String8("phone"), ubuntu::application::ui::phone_form_factor);
        lut.add(android::String8("tablet"), ubuntu::application::ui::tablet_form_factor);

        return lut;
    }

    static ubuntu::application::ui::FormFactorHint string_to_form_factor_hint(const android::String8& s)
    {
        static android::KeyedVector<android::String8, ubuntu::application::ui::FormFactorHint> lut = init_string_to_form_factor_hint_lut();

        return lut.valueFor(s);
    }

    ubuntu::application::ui::StageHint stage_hint()
    {
        return stage;
    }

    ubuntu::application::ui::FormFactorHintFlags form_factor_hint()
    {
        return ubuntu::application::ui::desktop_form_factor;
    }

    const char* desktop_file_hint()
    {
        return desktop_file.string();
    }

    ubuntu::application::ui::StageHint stage;
    ubuntu::application::ui::FormFactorHintFlags form_factor;
    android::String8 desktop_file;
};

static Setup::Ptr global_setup(new Setup());

struct PhysicalDisplayInfo : public ubuntu::application::ui::PhysicalDisplayInfo
{
    explicit PhysicalDisplayInfo(size_t display_id) : display_id(display_id)
    {
    }

    float horizontal_dpi()
    {
        DisplayInfo info;
        SurfaceComposerClient::getDisplayInfo(display_id, &info);
        
        return info.xdpi;
    }

    float vertical_dpi()
    {
        DisplayInfo info;
        SurfaceComposerClient::getDisplayInfo(display_id, &info);
        
        return info.ydpi;
    }

    int horizontal_resolution()
    {
        DisplayInfo info;
        SurfaceComposerClient::getDisplayInfo(display_id, &info);

        return info.w;
    }

    int vertical_resolution()
    {
        DisplayInfo info;
        SurfaceComposerClient::getDisplayInfo(display_id, &info);

        return info.h;
    }

    size_t display_id;
};

struct UbuntuSurface : public ubuntu::application::ui::Surface
{
    struct Observer
    {
        virtual ~Observer() {}

        virtual void update() = 0;
    };

    sp<SurfaceComposerClient> client;
    sp<SurfaceControl> surface_control;
    sp<android::Surface> surface;
    sp<InputChannel> input_channel;
    InputConsumer input_consumer;
    sp<Looper> looper;
    PreallocatedInputEventFactory event_factory;
    IApplicationManagerSession::SurfaceProperties properties;

    bool is_visible_flag;
    Observer* observer;

    static int looper_callback(int receiveFd, int events, void* ctxt)
    {
        uint32_t consumeSeq;
        bool result = true;
        UbuntuSurface* s = static_cast<UbuntuSurface*>(ctxt);
        InputEvent* ev;

        android::status_t status;
        while((status = s->input_consumer.consume(&s->event_factory, true, -1, &consumeSeq, &ev)) != android::WOULD_BLOCK)
        {
            switch(status)
            {
                case OK:
                    result = true;
                    //printf("We have a client side event for process %d. \n", getpid());
                    s->translate_and_dispatch_event(ev);
                    s->input_consumer.sendFinishedSignal(consumeSeq, result);
                    break;
                case INVALID_OPERATION:
                    result = true;
                    break;
                case NO_MEMORY:
                    result = true;
                    break;
            }
        }

        return result ? 1 : 0;
    }

    UbuntuSurface(const sp<SurfaceComposerClient>& client,
                  const sp<InputChannel>& input_channel,
                  const sp<Looper>& looper,
                  const ubuntu::application::ui::SurfaceProperties& props,
                  const ubuntu::application::ui::input::Listener::Ptr& listener,
                  Observer* observer = NULL)
            : ubuntu::application::ui::Surface(listener),
              client(client),
              input_channel(input_channel),
              input_consumer(input_channel),
        looper(looper),
        properties( {0, 0, 0, props.width-1, props.height-1}),
        is_visible_flag(false),
        observer(observer)
    {
        assert(client != NULL);

        surface_control = client->createSurface(
                              String8(props.title),
                              props.width,
                              props.height,
                              PIXEL_FORMAT_RGBA_8888,
                              props.flags & ubuntu::application::ui::is_opaque_flag ? android::ISurfaceComposerClient::eOpaque : 0);

        assert(surface_control != NULL);

        surface = surface_control->getSurface();

        assert(surface != NULL);

        // Setup input channel
        looper->addFd(input_channel->getFd(),
                      0,
                      ALOOPER_EVENT_INPUT,
                      looper_callback,
                      this);
    }

    ~UbuntuSurface()
    {
        looper->removeFd(input_channel->getFd());
    }

    void translate_and_dispatch_event(const android::InputEvent* ev)
    {
        Event e;
        switch(ev->getType())
        {
        case AINPUT_EVENT_TYPE_KEY:
        {
            const android::KeyEvent* kev = static_cast<const android::KeyEvent*>(ev);
            e.type = KEY_EVENT_TYPE;
            e.device_id = ev->getDeviceId();
            e.source_id = ev->getSource();
            e.action = kev->getAction();
            e.flags = kev->getFlags();
            e.meta_state = kev->getMetaState();
            e.details.key.key_code = kev->getKeyCode();
            e.details.key.scan_code = kev->getScanCode();
            e.details.key.repeat_count = kev->getRepeatCount();
            e.details.key.down_time = kev->getDownTime();
            e.details.key.event_time = kev->getEventTime();
            e.details.key.is_system_key = kev->isSystemKey();
            break;
        }
        case AINPUT_EVENT_TYPE_MOTION:
            const android::MotionEvent* mev = static_cast<const android::MotionEvent*>(ev);
            e.type = MOTION_EVENT_TYPE;
            e.device_id = ev->getDeviceId();
            e.source_id = ev->getSource();
            e.action = mev->getAction();
            e.flags = mev->getFlags();
            e.meta_state = mev->getMetaState();
            e.details.motion.edge_flags = mev->getEdgeFlags();
            e.details.motion.button_state = mev->getButtonState();
            e.details.motion.x_offset = mev->getXOffset();
            e.details.motion.y_offset = mev->getYOffset();
            e.details.motion.x_precision = mev->getXPrecision();
            e.details.motion.y_precision = mev->getYPrecision();
            e.details.motion.down_time = mev->getDownTime();
            e.details.motion.event_time = mev->getEventTime();
            e.details.motion.pointer_count = mev->getPointerCount();
            for(unsigned int i = 0; i < mev->getPointerCount(); i++)
            {
                e.details.motion.pointer_coordinates[i].id = mev->getPointerId(i);
                e.details.motion.pointer_coordinates[i].x = mev->getX(i);
                e.details.motion.pointer_coordinates[i].raw_x = mev->getRawX(i);
                e.details.motion.pointer_coordinates[i].y = mev->getY(i);
                e.details.motion.pointer_coordinates[i].raw_y = mev->getRawY(i);
                e.details.motion.pointer_coordinates[i].touch_major = mev->getTouchMajor(i);
                e.details.motion.pointer_coordinates[i].touch_minor = mev->getTouchMinor(i);
                e.details.motion.pointer_coordinates[i].size = mev->getSize(i);
                e.details.motion.pointer_coordinates[i].pressure = mev->getPressure(i);
                e.details.motion.pointer_coordinates[i].orientation = mev->getOrientation(i);
            }
            break;
        }

        registered_input_listener()->on_new_event(e);
    }

    void set_layer(int layer)
    {
        client->openGlobalTransaction();
        surface_control->setLayer(layer);
        client->closeGlobalTransaction();
        properties.layer = layer;
    }

    void set_visible(int id, bool visible)
    {
        sp<IServiceManager> service_manager = defaultServiceManager();
        sp<IBinder> service = service_manager->getService(
                        String16(IApplicationManager::exported_service_name()));

        BpApplicationManager app_manager(service);

        ALOGI("%s: %s", __PRETTY_FUNCTION__, visible ? "true" : "false");
        if (visible)
        {
            client->openGlobalTransaction();
            ALOGI("surface_control->show(INT_MAX): %d id=%d", surface_control->show(), id);
            client->closeGlobalTransaction();
            if (id)
                app_manager.focus_running_session_with_id(id);
        }
        else
        {
            client->openGlobalTransaction();
            ALOGI("surface_control->hide(): %d", surface_control->hide());
            client->closeGlobalTransaction();
        }

    }

    void set_alpha(float alpha)
    {
        client->openGlobalTransaction();
        surface_control->setAlpha(alpha);
        client->closeGlobalTransaction();

        if (observer)
            observer->update();
    }

    void move_to(int x, int y)
    {
        client->openGlobalTransaction();
        surface_control->setPosition(x, y);
        client->closeGlobalTransaction();
        properties.left = x;
        properties.top = y;
        properties.right += x;
        properties.bottom += y;

        if (observer)
            observer->update();
    }

    void resize(int w, int h)
    {
        client->openGlobalTransaction();
        surface_control->setSize(w, h);
        client->closeGlobalTransaction();
        properties.right = properties.left + w;
        properties.bottom = properties.top + h;

        if (observer)
            observer->update();
    }

    EGLNativeWindowType to_native_window_type()
    {
        return surface.get();
    }
};

struct Session : public ubuntu::application::ui::Session, public UbuntuSurface::Observer
{
    struct ApplicationManagerSession : public BnApplicationManagerSession
    {
        ApplicationManagerSession(Session* parent) : parent(parent)
        {
        }

        // From IApplicationManagerSession
        void raise_application_surfaces_to_layer(int layer)
        {
            ALOGI("%s: %d \n", __PRETTY_FUNCTION__, layer);
            parent->raise_application_surfaces_to_layer(layer);
        }

        void raise_surface_to_layer(int32_t token, int layer)
        {
            ALOGI("Enter %s (%d): %d, %d", __PRETTY_FUNCTION__, getpid(), token, layer);

            auto surface = parent->surfaces.valueFor(token);
            if (surface != NULL)
            {
                ALOGI("\tFound surface for token: %d", token);
                surface->set_layer(layer);
            } else
            {
                ALOGI("\tFound NO surface for token: %d", token);
            }
            
            ALOGI("Leave %s (%d): %d, %d", __PRETTY_FUNCTION__, getpid(), token, layer);
        }

        SurfaceProperties query_surface_properties_for_token(int32_t token)
        {
            ALOGI("%s: %d \n", __PRETTY_FUNCTION__, token);
            return parent->surfaces.valueFor(token)->properties;
        }

        Session* parent;
    };

    sp<ApplicationManagerSession> app_manager_session;
    sp<SurfaceComposerClient> client;
    sp<Looper> looper;
    sp<ubuntu::application::EventLoop> event_loop;
    Mutex surfaces_guard;
    KeyedVector< int32_t, android::sp<UbuntuSurface> > surfaces;

    Session(const ubuntu::application::ui::SessionCredentials& creds)
        : app_manager_session(new ApplicationManagerSession(this)),
          client(new android::SurfaceComposerClient()),
          looper(new Looper(true)),
          event_loop(new ubuntu::application::EventLoop(looper))
    {
        assert(client);
        //============= This has to die =================
        sp<InputChannel> server_channel, client_channel;
        InputChannel::openInputChannelPair(
            String8("UbuntuApplicationUiSession"),
            server_channel,
            client_channel);

        //printf("Created input channels: \n");
        //printf("\t %d, %d, %d \n",
        //server_channel->getFd());
        //============= This has to die =================
        sp<IServiceManager> service_manager = defaultServiceManager();
        sp<IBinder> service = service_manager->getService(
                                  String16(IApplicationManager::exported_service_name()));
        BpApplicationManager app_manager(service);

        app_manager.start_a_new_session(
            creds.session_type(),
            ubuntu::application::ui::Setup::instance()->stage_hint(),
            String8(creds.application_name()),
            String8(ubuntu::application::ui::Setup::instance()->desktop_file_hint()),
            app_manager_session,
            server_channel->getFd());

        android::ProcessState::self()->startThreadPool();
        event_loop->run(__PRETTY_FUNCTION__, android::PRIORITY_URGENT_DISPLAY);
    }

    void update()
    {
        sp<IServiceManager> service_manager = defaultServiceManager();
        sp<IBinder> service = service_manager->getService(
                                  String16(IApplicationManager::exported_service_name()));
        BpApplicationManager app_manager(service);

        app_manager.request_update_for_session(app_manager_session);
    }

    ubuntu::application::ui::Surface::Ptr create_surface(
        const ubuntu::application::ui::SurfaceProperties& props,
        const ubuntu::application::ui::input::Listener::Ptr& listener)
    {
        sp<IServiceManager> service_manager = defaultServiceManager();
        sp<IBinder> service = service_manager->getService(
                                  String16(IApplicationManager::exported_service_name()));
        BpApplicationManager app_manager(service);

        sp<InputChannel> server_channel, client_channel;

        InputChannel::openInputChannelPair(
            String8(props.title),
            server_channel,
            client_channel);

        UbuntuSurface* surface = new UbuntuSurface(
            client,
            client_channel,
            looper,
            props,
            listener,
            this);

        int32_t token;

        {
            Mutex::Autolock al(surfaces_guard);
            token = next_surface_token();
            surfaces.add(token, sp<UbuntuSurface>(surface));
        }

        app_manager.register_a_surface(
            String8(props.title),
            app_manager_session,
            props.role,
            token,
            server_channel->getFd());

        return ubuntu::application::ui::Surface::Ptr(surface);
    }

    int get_session_pid()
    {
        sp<IServiceManager> service_manager = defaultServiceManager();
        sp<IBinder> service = service_manager->getService(
            String16(IApplicationManager::exported_service_name()));
        BpApplicationManager app_manager(service);
        int pid = app_manager.get_session_pid(
            app_manager_session
        );
        
        return pid;
    }

    void toggle_fullscreen_for_surface(const ubuntu::application::ui::Surface::Ptr& /*surface*/)
    {
        sp<IServiceManager> service_manager = defaultServiceManager();
        sp<IBinder> service = service_manager->getService(
            String16(IApplicationManager::exported_service_name()));
        BpApplicationManager app_manager(service);
        app_manager.request_fullscreen(
            app_manager_session
        );
    }

    void destroy_surface(
        const ubuntu::application::ui::Surface::Ptr& surf)
    {
        (void) surf;
    }

    void raise_application_surfaces_to_layer(int layer)
    {
        Mutex::Autolock al(surfaces_guard);
        ALOGI("%s: %d\n", __PRETTY_FUNCTION__, layer);
        for(size_t i = 0; i < surfaces.size(); i++)
        {
            surfaces.valueAt(i)->set_layer(layer+i);
            ALOGI("\tLayer: %d\n", layer+i);
        }
    }

    int32_t next_surface_token()
    {
        static int32_t t = 0;
        t++;
        return t;
    }
};

struct SessionProperties : public ubuntu::ui::SessionProperties
{
    SessionProperties(int id, int stage_hint, const android::String8& desktop_file)
        : id(id),
          stage_hint(stage_hint),
          desktop_file(desktop_file)
    {
    }

    int application_instance_id() const
    {
        return id;
    }

    int application_stage_hint() const
    {
        return stage_hint;
    }

    const char* value_for_key(const char* key) const
    {
        return "lalelu";
    }

    virtual const char* desktop_file_hint() const
    {
        return desktop_file.string();
    }

    int id;
    int stage_hint;
    android::String8 desktop_file;
};

struct ApplicationManagerObserver : public android::BnApplicationManagerObserver
{
    void on_session_requested(uint32_t app)
    {
        if (observer == NULL)
            return;

        observer->on_session_requested(static_cast<ubuntu::ui::WellKnownApplication>(app));
    }

    void on_session_born(int id,
                         int stage_hint,
                         const String8& desktop_file)
    {
        if (observer == NULL)
            return;

        observer->on_session_born(ubuntu::ui::SessionProperties::Ptr(new SessionProperties(id, stage_hint, desktop_file)));
    }

    virtual void on_session_unfocused(int id,
                                      int stage_hint,
                                      const String8& desktop_file)
    {
        if (observer == NULL)
            return;

        observer->on_session_unfocused(ubuntu::ui::SessionProperties::Ptr(new SessionProperties(id, stage_hint, desktop_file)));
    }

    virtual void on_session_focused(int id,
                                    int stage_hint,
                                    const String8& desktop_file)
    {
        if (observer == NULL)
            return;

        observer->on_session_focused(ubuntu::ui::SessionProperties::Ptr(new SessionProperties(id, stage_hint, desktop_file)));
    }

    virtual void on_session_requested_fullscreen(int id,
                                                 int stage_hint,	
                                                 const String8& desktop_file)
    {
        if (observer == NULL)
            return;

        observer->on_session_requested_fullscreen(ubuntu::ui::SessionProperties::Ptr(new SessionProperties(id, stage_hint, desktop_file)));
    }

    virtual void on_keyboard_geometry_changed(int x,
                                              int y,
                                              int width,
                                              int height)
    {
        if (observer == NULL)
            return;
        
        observer->on_keyboard_geometry_changed(x, y, width, height);
    }

    virtual void on_session_died(int id,
                                 int stage_hint,
                                 const String8& desktop_file)
    {
        if (observer == NULL)
            return;

        observer->on_session_died(ubuntu::ui::SessionProperties::Ptr(new SessionProperties(id, stage_hint, desktop_file)));
    }

    void install_session_lifecycle_observer(const ubuntu::ui::SessionLifeCycleObserver::Ptr& observer)
    {
        this->observer = observer;

        sp<IServiceManager> service_manager = defaultServiceManager();
        sp<IBinder> service = service_manager->getService(
                                  String16(IApplicationManager::exported_service_name()));
        BpApplicationManager app_manager(service);

        app_manager.register_an_observer(android::sp<IApplicationManagerObserver>(this));
    }

    ubuntu::ui::SessionLifeCycleObserver::Ptr observer;
};

struct SessionService : public ubuntu::ui::SessionService
{    
    struct SessionSnapshot : public ubuntu::ui::SessionSnapshot
    {
        const void* snapshot_pixels;
        unsigned int snapshot_x;
        unsigned int snapshot_y;
        unsigned int surface_width;
        unsigned int surface_height;
        unsigned int snapshot_width;
        unsigned int snapshot_height;
        unsigned int snapshot_stride;
        
        SessionSnapshot(
            const void* pixels, 
            unsigned int x,
            unsigned int y,
            unsigned int sf_width,
            unsigned int sf_height,
            unsigned int width, 
            unsigned height, 
            unsigned int stride) : snapshot_pixels(pixels),
                                   snapshot_x(x),
                                   snapshot_y(y),
                                   surface_width(sf_width),
                                   surface_height(sf_height),
                                   snapshot_width(width),
                                   snapshot_height(height),
                                   snapshot_stride(stride)
        {
        }

        const void* pixel_data() {
            return snapshot_pixels;
        }

        unsigned int x() { return snapshot_x; }
        unsigned int y() { return snapshot_y; }
        unsigned int source_width() { return surface_width; }
        unsigned int source_height() { return surface_height; }
        unsigned int width() { return snapshot_width; }
        unsigned int height() { return snapshot_height; }
        unsigned int stride() { return snapshot_stride; }       
    };

    static sp<BpApplicationManager> access_application_manager()
    {
        static sp<BpApplicationManager> remote_instance;

        if (remote_instance == NULL)
        {
            sp<IServiceManager> service_manager = defaultServiceManager();
            sp<IBinder> service = service_manager->getService(
                String16(IApplicationManager::exported_service_name()));
            remote_instance = new BpApplicationManager(service);
        }

        return remote_instance;
    }

    SessionService() : observer(new ApplicationManagerObserver())
    {
        android::ProcessState::self()->startThreadPool();
    }

    const ubuntu::application::ui::Session::Ptr& start_a_new_session(const ubuntu::application::ui::SessionCredentials& cred)
    {
        (void) cred;
        static ubuntu::application::ui::Session::Ptr session(new Session(cred));
        return session;
    }

    void install_session_lifecycle_observer(const ubuntu::ui::SessionLifeCycleObserver::Ptr& lifecycle_observer)
    {
        this->observer->install_session_lifecycle_observer(lifecycle_observer);
    }

    void unfocus_running_sessions()
    {
        ALOGI("%s", __PRETTY_FUNCTION__);
        access_application_manager()->unfocus_running_sessions();
    }

    void focus_running_session_with_id(int id)
    {
        access_application_manager()->focus_running_session_with_id(id);
    }

    ubuntu::ui::SessionSnapshot::Ptr snapshot_running_session_with_id(int id)
    {
        static android::DisplayInfo info;
        const void* pixels;

        int32_t layer_min = id > 0 
                ? access_application_manager()->query_snapshot_layer_for_session_with_id(id) 
                : 0;
        int32_t layer_max = id > 0 
                ? access_application_manager()->query_snapshot_layer_for_session_with_id(id) 
                : id;  

        android::IApplicationManagerSession::SurfaceProperties props =
             access_application_manager()->query_surface_properties_for_session_id(id);

        static android::ScreenshotClient screenshot_client;
        android::sp<android::IBinder> display(
                android::SurfaceComposerClient::getBuiltInDisplay(
                android::ISurfaceComposer::eDisplayIdMain));

        android::SurfaceComposerClient::getDisplayInfo(
                display,
                &info);

        screenshot_client.update(display, info.w / 2, info.h / 2, layer_min, layer_max);

        ALOGI("screenshot: (%d, %d, %d, %d)\n", props.left, props.top, props.right, props.bottom);
        if (props.left == 0 && props.top == 0 && props.right == 0 && props.bottom == 0)
            pixels = NULL;
        else
            pixels = screenshot_client.getPixels();

        SessionSnapshot::Ptr ss(
            new SessionSnapshot(
                pixels,
                (props.left+1) / 2,
                (props.top+1) / 2,
                (props.right-props.left+1) / 2,
                (props.bottom-props.top+1) / 2,
                screenshot_client.getWidth(),
                screenshot_client.getHeight(),
                screenshot_client.getStride()));
       
        return ss;
    }

    void trigger_switch_to_well_known_application(ubuntu::ui::WellKnownApplication app)
    {
        access_application_manager()->switch_to_well_known_application(app);
    }

    int32_t set_surface_trap(int32_t x, int32_t y, int32_t width, int32_t height)
    {
        ALOGI("%s", __PRETTY_FUNCTION__);
        return access_application_manager()->set_surface_trap(x, y, width, height);
    }
    
    void unset_surface_trap(int32_t handle)
    {
        ALOGI("%s(%d)", __PRETTY_FUNCTION__, handle);
        access_application_manager()->unset_surface_trap(handle);
    }


    void report_osk_visible(int32_t x, int32_t y, int32_t width, int32_t height)
    {
        ALOGI("%s", __PRETTY_FUNCTION__);
        access_application_manager()->report_osk_visible(x, y, width, height);
    }
    
    void report_osk_invisible()
    {
        ALOGI("%s", __PRETTY_FUNCTION__);
        access_application_manager()->report_osk_invisible();
    }

    void report_notification_visible()
    {
        ALOGI("%s", __PRETTY_FUNCTION__);
        access_application_manager()->report_notification_visible();
    }
    
    void report_notification_invisible()
    {
        ALOGI("%s", __PRETTY_FUNCTION__);
        access_application_manager()->report_notification_invisible();
    }

    android::sp<ApplicationManagerObserver> observer;
};

}

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

// We need to inject some platform specific symbols here.
namespace ubuntu
{
namespace application
{
namespace ui
{

void print_help_and_exit()
{
    printf("Usage: executable "
           "[--form_factor_hint={desktop, phone, tablet}] "
           "[--stage_hint={main_stage, side_stage, share_stage}] "
           "[--desktop_file_hint=absolute_path_to_desktop_file]\n");
    exit(EXIT_SUCCESS);
}

void init(int argc, char** argv)
{
    static const int uninteresting_flag_value = 0;
    static struct option long_options[] =
    {        
        {"form_factor_hint", required_argument, NULL, uninteresting_flag_value},
        {"stage_hint", required_argument, NULL, uninteresting_flag_value},
        {"desktop_file_hint", required_argument, NULL, uninteresting_flag_value},
        {"help", no_argument, NULL, uninteresting_flag_value},
        {0, 0, 0, 0}
    };

    static const int form_factor_hint_index = 0;
    static const int stage_hint_index = 1;
    static const int desktop_file_hint_index = 2;
    static const int help_index = 3;

    android::Setup* setup = new android::Setup();

    while(true)
    {
        int option_index = 0;

        int c = getopt_long(argc,
                            argv,
                            "",
                            long_options,
                            &option_index);

        if (c == -1)
            break;

        switch (c)
        {
        case 0:
            // If this option set a flag, do nothing else now.
            if (long_options[option_index].flag != 0)
                break;
            if (option_index == help_index)
                print_help_and_exit();
            if (optarg)
            {
                switch(option_index)
                {
                case form_factor_hint_index:
                    setup->form_factor = android::Setup::string_to_form_factor_hint(android::String8(optarg));
                    break;
                case stage_hint_index:
                    setup->stage = android::Setup::string_to_stage_hint(android::String8(optarg));
                    break;
                case desktop_file_hint_index:
                    setup->desktop_file = android::String8(optarg);
                    break;                
                }
            }
            break;
        case '?':
            break;
        }
    }

    android::global_setup = setup;
}

const ubuntu::application::ui::Setup::Ptr& ubuntu::application::ui::Setup::instance()
{
    return android::global_setup;
}

ubuntu::application::ui::PhysicalDisplayInfo::Ptr ubuntu::application::ui::Session::physical_display_info(
        ubuntu::application::ui::PhysicalDisplayIdentifier id)
{
    ubuntu::application::ui::PhysicalDisplayInfo::Ptr display(
        new android::PhysicalDisplayInfo(static_cast<size_t>(id)));
    
    return display;
}

ubuntu::application::ui::Clipboard::Ptr ubuntu::application::ui::Session::clipboard()
{
    static ubuntu::application::ui::Clipboard::Ptr instance(new android::Clipboard());
    return instance;
}

}
}
namespace ui
{

const ubuntu::ui::SessionService::Ptr& ubuntu::ui::SessionService::instance()
{
    static ubuntu::ui::SessionService::Ptr instance(new android::SessionService());
    return instance;
}

const char* SessionProperties::key_application_stage_hint()
{
    static const char* key = "application_stage_hint";
    return key;
}

const char* SessionProperties::key_application_instance_id()
{
    static const char* key = "application_instance_id";
    return key;
}

const char* SessionProperties::key_application_name()
{
    static const char* key = "application_name";
    return key;
}

const char* SessionProperties::key_desktop_file_hint()
{
    static const char* key = "desktop_file_hint";
    return key;
}
}
}
