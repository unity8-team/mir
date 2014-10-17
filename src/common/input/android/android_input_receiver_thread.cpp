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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "android_input_receiver_thread.h"
#include "android_input_receiver.h"

#include <thread>

namespace mircva = mir::input::receiver::android;

mircva::InputReceiverThread::InputReceiverThread(std::shared_ptr<mircva::InputReceiver> const& receiver,
                                                std::function<void(MirEvent*)> const& event_handling_callback)
  : receiver(receiver),
    handler(event_handling_callback),
    running(false)
{
    last_frame_time = std::chrono::high_resolution_clock::now();
}

mircva::InputReceiverThread::~InputReceiverThread()
{
    if (running)
        stop();
    if (thread.joinable())
        join();
}

void mircva::InputReceiverThread::start()
{
    running = true;
    thread = std::thread(std::mem_fn(&mircva::InputReceiverThread::thread_loop), this);
}

void mircva::InputReceiverThread::stop()
{
    running = false;
    receiver->wake();
}

void mircva::InputReceiverThread::join()
{
    thread.join();
}

void mircva::InputReceiverThread::thread_loop()
{
    while (running)
    {
        std::unique_lock<std::recursive_mutex> lg(frame_time_mutex);
        MirEvent ev;
        nsecs_t frame_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(last_frame_time.time_since_epoch()).count();
        // TODO: Pass time to receiver
//        (void)frame_time_ns;
        if (std::chrono::high_resolution_clock::now() - last_frame_time > std::chrono::milliseconds(32))
            frame_time_ns = -1;

        while(running && receiver->next_event(ev, frame_time_ns))
            handler(&ev);

        // TODO: Hack
//        last_frame_time = std::chrono::high_resolution_clock::now();

        frame_cv.wait_for(lg, std::chrono::milliseconds(32));
    }
}

void mircva::InputReceiverThread::notify_of_frame_start(std::chrono::high_resolution_clock::time_point frame_time)
{
    (void) frame_time;
    {
        std::lock_guard<std::recursive_mutex> lg(frame_time_mutex);
        last_frame_time = frame_time;
    }
  frame_cv.notify_all();
}
