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
#ifndef EVENT_LOOP_H_
#define EVENT_LOOP_H_

#include <utils/Looper.h>
#include <utils/threads.h>

namespace ubuntu
{
namespace application
{
struct EventLoop : public android::Thread
{
    EventLoop(const android::sp<android::Looper>& looper) : looper(looper)
    {
    }

    bool threadLoop()
    {
        static const int five_seconds_in_milliseconds = 5*1000;

        bool result = true;
        while(true)
        {
            switch(looper->pollOnce(five_seconds_in_milliseconds))
            {
            case ALOOPER_POLL_CALLBACK:
            case ALOOPER_POLL_TIMEOUT:
                result = true;
                break;
            case ALOOPER_POLL_ERROR:
                result = false;
                break;
            }
        }

        return result;
    }

    android::sp<android::Looper> looper;
};
}
}

#endif // EVENT_LOOP_H_
