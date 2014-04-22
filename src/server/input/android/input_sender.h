/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_INPUT_ANDROID_INPUT_SENDER_H_
#define MIR_INPUT_ANDROID_INPUT_SENDER_H_

#include "mir/input/input_sender.h"

#include "androidfw/InputTransport.h"

#include <unordered_map>
#include <mutex>
#include <vector>

namespace droidinput = android;

namespace mir
{
namespace graphics { class EventHandlerRegister; } // todo move?
namespace input
{
namespace android
{

class InputSender : public mir::input::InputSender
{
public:
    InputSender(std::shared_ptr<mir::graphics::EventHandlerRegister> const& registrar);
    //void stop(); //needed?
    void set_send_observer(std::shared_ptr<InputSendObserver> const& observer) override;
    std::shared_ptr<InputSendEntry> send_event(MirEvent const& event, Surface const& receiver, std::shared_ptr<InputChannel> const& channel) override;
private:
    uint32_t seq;
    std::shared_ptr<mir::graphics::EventHandlerRegister> const loop;
    std::shared_ptr<InputSendObserver> observer;
    struct ActiveTransfer
    {
        droidinput::InputPublisher publisher;
        // TODO: std::vector<std::shared_ptr<InputSendEntry>> pending_sends;
        std::vector<std::shared_ptr<InputSendEntry>> pending_responses;
        std::mutex transfer_mutex;

        ActiveTransfer(int server_fd);
        //ActiveTransfer(ActiveTransfer const&);
        void send(std::shared_ptr<InputSendEntry> const& item);
        void send_key_event(uint32_t sequence_id, MirKeyEvent const& event);
        void send_motion_event(uint32_t sequence_id, MirMotionEvent const& event);
        void submit_result(std::shared_ptr<InputSendObserver> const& observer);
        std::shared_ptr<InputSendEntry> unqueue_entry(uint32_t sequence_id);
    };
    std::unordered_map<int,ActiveTransfer> transfers;
    std::mutex sender_mutex;


    ActiveTransfer & get_active_transfer(int server_fd);
    uint32_t next_seq();
    void handle_finish_signal(ActiveTransfer & transfer);
};

}
}
}

#endif

