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
#ifndef INPUT_CONSUMER_THREAD_H_
#define INPUT_CONSUMER_THREAD_H_

#include <androidfw/InputTransport.h>
#include <utils/Looper.h>

namespace android
{

struct InputConsumerThread : public android::Thread
{
    InputConsumerThread(android::InputConsumer& input_consumer)
        : input_consumer(input_consumer),
          looper(android::Looper::prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS))
    {
        looper->addFd(input_consumer.getChannel()->getFd(),
                      input_consumer.getChannel()->getFd(),
                      ALOOPER_EVENT_INPUT,
                      NULL,
                      NULL);

    }

    bool threadLoop()
    {
        while (true)
        {
            switch(looper->pollOnce(5 * 1000))
            {
            case ALOOPER_POLL_TIMEOUT:
            case ALOOPER_POLL_ERROR:
                continue;
                break;
            }

// printf("%s \n", __PRETTY_FUNCTION__);
            InputEvent* event = NULL;
            uint32_t consumeSeq;
            bool result = true;
            switch(input_consumer.consume(&event_factory, true, -1, &consumeSeq, &event))
            {
            case OK:
//TODO:Dispatch to input listener
                result = true;
//printf("Yeah, we have an event client-side.\n");
                input_consumer.sendFinishedSignal(consumeSeq, result);
                break;
            case INVALID_OPERATION:
                result = true;
                break;
            case NO_MEMORY:
                result = true;
                break;
            }
        }
        return true;
    }

    android::InputConsumer input_consumer;
    android::sp<android::Looper> looper;
    android::PreallocatedInputEventFactory event_factory;
};
}
#endif // INPUT_CONSUMER_THREAD_H_
