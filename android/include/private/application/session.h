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
#ifndef UBUNTU_APPLICATION_SESSION_H_
#define UBUNTU_APPLICATION_SESSION_H_

#include <private/platform/shared_ptr.h>
#include <ubuntu/application/lifecycle_delegate.h>

#include <utils/Log.h>

namespace ubuntu
{
namespace application
{
/**
 * Represents a session with the service providers abstracted by Ubuntu platform API.
 */
class Session : public ubuntu::platform::ReferenceCountedBase
{
public:
    typedef ubuntu::platform::shared_ptr<Session> Ptr;

protected:
    Session() {}
    virtual ~Session() {}

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
};

/**
 * Represents a session lifecycle delegate.
 */
class LifecycleDelegate : public ubuntu::platform::ReferenceCountedBase
{
public:
    typedef ubuntu::platform::shared_ptr<LifecycleDelegate> Ptr;

    virtual void on_application_started() = 0;
    virtual void on_application_about_to_stop() = 0;

    //virtual void on_application_started(const Options *options, void *context) = 0;
    //virtual void on_application_about_to_stop(Archive *archive, void *context) = 0;

protected:
    LifecycleDelegate() {}

    virtual ~LifecycleDelegate() {}

    LifecycleDelegate(const LifecycleDelegate&) = delete;
    LifecycleDelegate& operator=(const LifecycleDelegate&) = delete;
};

class Id : public ubuntu::platform::ReferenceCountedBase
{
public:
    typedef ubuntu::platform::shared_ptr<Id> Ptr;
    
    char *string;
    size_t size;

protected:
    Id() {}

    virtual ~Id() {}

    Id(const Id&) = delete;
    Id& operator=(const Id&) = delete;
};
}
}

// C APIs
namespace
{
struct IUApplicationLifecycleDelegate : public ubuntu::application::LifecycleDelegate
{
    IUApplicationLifecycleDelegate(void *context) :
                                    application_started_cb(NULL),
                                    application_about_to_stop_cb(NULL),
                                    context(context),
                                    refcount(1)
    {
    }

    void on_application_started()
    {
        if (!application_started_cb)
            return;

        ALOGI("ON_APPLICATION_STARTED()");
        application_started_cb(NULL, this->context);
    }

    void on_application_about_to_stop()
    {
        if (!application_about_to_stop_cb)
            return;

        ALOGI("ON_APPLICATION_ABOUT_TO_STOP()");
        application_about_to_stop_cb(NULL, this->context);
    }

    u_on_application_started application_started_cb;
    u_on_application_about_to_stop application_about_to_stop_cb;
    void *context;

    unsigned refcount;
};

struct IUApplicationId : public ubuntu::application::Id
{
    IUApplicationId(const char *string, size_t size)
    {
        this->size = size;
        this->string = (char*) malloc(sizeof (char) * (size+1));
        memcpy(this->string, string, (size+1));
    }
};
}

#endif // UBUNTU_APPLICATION_SESSION_H_
