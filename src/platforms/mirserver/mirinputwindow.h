/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _MIRQPA_INPUT_WINDOW_H
#define _MIRQPA_INPUT_WINDOW_H

#include <androidfw/Input.h>
#include <androidfw/InputTransport.h>
#include <std/RefBase.h>
#include <std/Timers.h>
#include <std/String8.h>

namespace android {

/*
 * Handle for a window that can receive input.
 *
 * Used by the native input dispatcher to indirectly refer to the window manager objects
 * that describe a window.
 */
class MirInputWindowHandle : public RefBase {
public:
    inline sp<InputChannel> getInputChannel() const {
        return inputChannel;
    }

    inline String8 getName() const {
        return name;
    }

protected:
    MirInputWindowHandle(const sp<InputApplicationHandle>& inputApplicationHandle);
    virtual ~MirInputWindowHandle();
    sp<InputChannel> inputChannel;
    String8 name;
    float scaleFactor;
};

} // namespace android

#endif // _MIRQPA_INPUT_WINDOW_H
