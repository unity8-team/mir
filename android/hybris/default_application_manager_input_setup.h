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
#ifndef DEFAULT_APPLICATION_MANAGER_INPUT_SETUP_H_
#define DEFAULT_APPLICATION_MANAGER_INPUT_SETUP_H_

#include <input/InputDispatcher.h>
#include <input/InputListener.h>
#include <input/InputManager.h>
#include <input/InputReader.h>
#include <input/PointerController.h>
#include <input/SpriteController.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>

#include <cstdio>

#define REPORT_FUNCTION_CALL()// printf("%s \n", __PRETTY_FUNCTION__);

namespace android
{
class DefaultPointerControllerPolicy : public android::PointerControllerPolicyInterface
{
public:

    static const size_t bitmap_width = 64;
    static const size_t bitmap_height = 64;

    DefaultPointerControllerPolicy()
    {
        bitmap.setConfig(
            SkBitmap::kARGB_8888_Config,
            bitmap_width,
            bitmap_height);
        bitmap.allocPixels();

        // Icon for spot touches
        bitmap.eraseARGB(125, 0, 255, 0);
        spotTouchIcon = android::SpriteIcon(
                            bitmap,
                            bitmap_width/2,
                            bitmap_height/2);
        // Icon for anchor touches
        bitmap.eraseARGB(125, 0, 0, 255);
        spotAnchorIcon = android::SpriteIcon(
                             bitmap,
                             bitmap_width/2,
                             bitmap_height/2);
        // Icon for hovering touches
        bitmap.eraseARGB(125, 255, 0, 0);
        spotHoverIcon = android::SpriteIcon(
                            bitmap,
                            bitmap_width/2,
                            bitmap_height/2);
    }

    void loadPointerResources(android::PointerResources* outResources)
    {
        outResources->spotHover = spotHoverIcon.copy();
        outResources->spotTouch = spotTouchIcon.copy();
        outResources->spotAnchor = spotAnchorIcon.copy();
    }

    android::SpriteIcon spotHoverIcon;
    android::SpriteIcon spotTouchIcon;
    android::SpriteIcon spotAnchorIcon;
    SkBitmap bitmap;
};

class DefaultInputReaderPolicyInterface : public android::InputReaderPolicyInterface
{
public:
    static const int32_t internal_display_id = android::ISurfaceComposer::eDisplayIdMain;
    static const int32_t external_display_id = android::ISurfaceComposer::eDisplayIdHdmi;

    DefaultInputReaderPolicyInterface(const android::sp<android::Looper>& looper)
        : looper(looper),
          default_layer_for_touch_point_visualization(INT_MAX)
    {
        default_configuration.showTouches = false;

        auto display = android::SurfaceComposerClient::getBuiltInDisplay(
            android::ISurfaceComposer::eDisplayIdMain);
        android::DisplayInfo info;
        android::SurfaceComposerClient::getDisplayInfo(
            display,
            &info);

        android::DisplayViewport viewport;
        viewport.setNonDisplayViewport(info.w, info.h);
        viewport.displayId = android::ISurfaceComposer::eDisplayIdMain;
        default_configuration.setDisplayInfo(
            false, /* external */
            viewport);

    }

    void getReaderConfiguration(android::InputReaderConfiguration* outConfig)
    {
        *outConfig = default_configuration;
    }

    android::sp<android::PointerControllerInterface> obtainPointerController(int32_t deviceId)
    {
        (void) deviceId;

        android::sp<android::SpriteController> sprite_controller(
            new android::SpriteController(
                looper,
                default_layer_for_touch_point_visualization));
        android::sp<android::PointerController> pointer_controller(
            new android::PointerController(
                android::sp<DefaultPointerControllerPolicy>(new DefaultPointerControllerPolicy()),
                looper,
                sprite_controller));
        pointer_controller->setPresentation(
            android::PointerControllerInterface::PRESENTATION_SPOT);
        int32_t w, h, o;
        auto display = android::SurfaceComposerClient::getBuiltInDisplay(
            android::ISurfaceComposer::eDisplayIdMain);
        android::DisplayInfo info;
        android::SurfaceComposerClient::getDisplayInfo(
            display,
            &info);

        pointer_controller->setDisplayViewport(info.w, info.h, info.orientation);
        return pointer_controller;
    }

    virtual void notifyInputDevicesChanged(const Vector<InputDeviceInfo>& inputDevices) {
        mInputDevices = inputDevices;
    }

    virtual sp<KeyCharacterMap> getKeyboardLayoutOverlay(const String8& inputDeviceDescriptor) {
        return NULL;
    }

    virtual String8 getDeviceAlias(const InputDeviceIdentifier& identifier) {
        return String8::empty();
    }
private:
    android::sp<android::Looper> looper;
    int default_layer_for_touch_point_visualization;
    android::InputReaderConfiguration default_configuration;
    Vector<InputDeviceInfo> mInputDevices;
};

class InputFilter : public android::RefBase
{
public:
    virtual bool filter_event(const android::InputEvent* event) = 0;

protected:
    InputFilter() {}
    virtual ~InputFilter() {}

    InputFilter(const InputFilter&) = delete;
    InputFilter& operator=(const InputFilter&) = delete;
};

class DefaultInputDispatcherPolicy : public InputDispatcherPolicyInterface
{
public:
    DefaultInputDispatcherPolicy(const android::sp<InputFilter>& input_filter)
        : input_filter(input_filter)
    {
    }

    ~DefaultInputDispatcherPolicy()
    {
    }

    void notifyConfigurationChanged(nsecs_t when)
    {
        REPORT_FUNCTION_CALL();
        (void) when;
    }

    nsecs_t notifyANR(const sp<InputApplicationHandle>& inputApplicationHandle,
                      const sp<InputWindowHandle>& inputWindowHandle)
    {
        REPORT_FUNCTION_CALL();
        (void) inputApplicationHandle;
        (void) inputWindowHandle;

        return 0;
    }

    void notifyInputChannelBroken(const sp<InputWindowHandle>& inputWindowHandle)
    {
        REPORT_FUNCTION_CALL();
        (void) inputWindowHandle;
    }

    void getDispatcherConfiguration(InputDispatcherConfiguration* outConfig)
    {
        REPORT_FUNCTION_CALL();
        static InputDispatcherConfiguration config;
        //config.maxEventsPerSecond = INT_MAX;
        *outConfig = config;
    }

    bool isKeyRepeatEnabled()
    {
        REPORT_FUNCTION_CALL();
        return true;
    }

    bool filterInputEvent(const InputEvent* event, uint32_t policyFlags)
    {
        REPORT_FUNCTION_CALL();
        (void) event;
        (void) policyFlags;
        return input_filter->filter_event(event);
    }

    void interceptKeyBeforeQueueing(const KeyEvent* event, uint32_t& policyFlags)
    {
        REPORT_FUNCTION_CALL();
        (void) event;
        policyFlags |= POLICY_FLAG_PASS_TO_USER;
    }

    void interceptMotionBeforeQueueing(nsecs_t when, uint32_t& policyFlags)
    {
        REPORT_FUNCTION_CALL();
        (void) when;
        (void) policyFlags;
        policyFlags |= POLICY_FLAG_PASS_TO_USER;
    }

    nsecs_t interceptKeyBeforeDispatching(
        const sp<InputWindowHandle>& inputWindowHandle,
        const KeyEvent* keyEvent,
        uint32_t policyFlags)
    {
        REPORT_FUNCTION_CALL();
        (void) inputWindowHandle;
        (void) keyEvent;
        (void) policyFlags;

        return 0;
    }

    bool dispatchUnhandledKey(
        const sp<InputWindowHandle>& inputWindowHandle,
        const KeyEvent* keyEvent,
        uint32_t policyFlags,
        KeyEvent* outFallbackKeyEvent)
    {
        REPORT_FUNCTION_CALL();
        (void) inputWindowHandle;
        (void) keyEvent;
        (void) policyFlags;
        (void) outFallbackKeyEvent;
        return false;
    }

    virtual void notifySwitch(
        nsecs_t when,
        uint32_t switchCode,
        uint32_t switchValue,
        uint32_t policyFlags)
    {
        REPORT_FUNCTION_CALL();
        (void) when;
        (void) switchCode;
        (void) switchValue;
        (void) policyFlags;
    }

    void pokeUserActivity(nsecs_t eventTime, int32_t eventType)
    {
        REPORT_FUNCTION_CALL();
        (void) eventTime;
        (void) eventType;
    }

    bool checkInjectEventsPermissionNonReentrant(
        int32_t injectorPid,
        int32_t injectorUid)
    {
        REPORT_FUNCTION_CALL();
        (void) injectorPid;
        (void) injectorUid;

        return true;
    }

    android::sp<InputFilter> input_filter;
};

class LooperThread : public android::Thread
{
public:
    static const int default_poll_timeout_ms = 500;

    LooperThread(const android::sp<android::Looper>& looper) : looper(looper)
    {
    }

private:
    bool threadLoop()
    {
        if (ALOOPER_POLL_ERROR == looper->pollAll(default_poll_timeout_ms))
            return false;
        return true;
    }

    android::sp<android::Looper> looper;
};

struct InputSetup : public android::RefBase
{
    struct DummyApplication : public android::InputApplicationHandle
    {
        bool updateInfo()
        {
            ALOGI("%s", __PRETTY_FUNCTION__);
            if (mInfo == NULL)
            {
                mInfo = new android::InputApplicationInfo();
                mInfo->name = "Shell";
                mInfo->dispatchingTimeout = INT_MAX;
            }

            return true;
        }
    };

    struct DummyApplicationWindow : public android::InputWindowHandle
    {
        DummyApplicationWindow(
            const android::sp<android::InputApplicationHandle>& app_handle,
            int x = 0,
            int y = 0,
            int w = 720,
            int h = 1280) 
                : android::InputWindowHandle(app_handle),
                  x(x),
                  y(y),
                  w(w),
                  h(h)
        {
        }
        
        bool updateInfo()
        {
            ALOGI("%s", __PRETTY_FUNCTION__);
            if (mInfo == NULL)
            {
                mInfo = new android::InputWindowInfo();
                SkRegion touchable_region;
                touchable_region.setRect(x, y, x+w, y+h);
                
                mInfo->name = "ShellInputWindow";
                mInfo->layoutParamsFlags = android::InputWindowInfo::FLAG_NOT_TOUCH_MODAL | android::InputWindowInfo::FLAG_SPLIT_TOUCH;
                mInfo->layoutParamsType = android::InputWindowInfo::TYPE_APPLICATION;
                mInfo->touchableRegion = touchable_region;
                mInfo->frameLeft = x;
                mInfo->frameTop = y;
                mInfo->frameRight = x + w;
                mInfo->frameBottom = y + h;
                mInfo->scaleFactor = 1.f;
                mInfo->visible = true;
                mInfo->canReceiveKeys = true;
                mInfo->hasFocus = true;
                mInfo->hasWallpaper = false;
                mInfo->paused = false;
                mInfo->layer = 0;
                mInfo->dispatchingTimeout = INT_MAX;
                mInfo->ownerPid = 0;
                mInfo->ownerUid = 0;
                mInfo->inputFeatures = 0;
                mInfo->inputChannel = input_channel;            
            }

            return true;
        }

        android::sp<android::InputChannel> input_channel;
        int x;
        int y;
        int w;
        int h;
    };

    InputSetup(const android::sp<InputFilter>& input_filter)
        : looper(new android::Looper(false)),
          looper_thread(new LooperThread(looper)),
          event_hub(new android::EventHub()),
          input_reader_policy(new DefaultInputReaderPolicyInterface(looper)),
          input_dispatcher_policy(new DefaultInputDispatcherPolicy(input_filter)),
          input_manager(new InputManager(event_hub, input_reader_policy, input_dispatcher_policy))
    {
        input_manager->getDispatcher()->setInputFilterEnabled(true);
    }

    void start()
    {
        input_manager->start();
        looper_thread->run(__PRETTY_FUNCTION__,
                           android::PRIORITY_URGENT_DISPLAY);
    }

    void stop()
    {
        input_manager->stop();
        looper_thread->requestExitAndWait();
    }

    ~InputSetup()
    {
        stop();
    }
    android::sp<android::Looper> looper;
    android::sp<LooperThread> looper_thread;

    android::sp<android::EventHubInterface> event_hub;
    android::sp<android::InputReaderPolicyInterface> input_reader_policy;
    android::sp<android::InputDispatcherPolicyInterface> input_dispatcher_policy;
    android::sp<android::InputManager> input_manager;

    android::Condition wait_condition;
    android::Mutex wait_guard;
};

}

#endif // DEFAULT_APPLICATION_MANAGER_INPUT_SETUP_H_
