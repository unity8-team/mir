#ifndef UBUNTU_UI_SESSION_ENUMERATOR_H_
#define UBUNTU_UI_SESSION_ENUMERATOR_H_

#include "ubuntu/platform/shared_ptr.h"
#include "ubuntu/ui/well_known_applications.h"

#include <GLES2/gl2.h>

namespace ubuntu
{
namespace ui
{

class SessionProperties : public platform::ReferenceCountedBase
{
public:
    static const char* key_application_instance_id();
    static const char* key_application_name();
    static const char* key_desktop_file_hint();

    typedef platform::shared_ptr<SessionProperties> Ptr;

    virtual const char* value_for_key(const char* key) const = 0;

    virtual int application_instance_id() const
    {
        return atoi(value_for_key(SessionProperties::key_application_instance_id()));
    }

    const char* application_name() const
    {
        return value_for_key(SessionProperties::key_application_name());
    }

    virtual const char* desktop_file_hint() const
    {
        return value_for_key(SessionProperties::key_desktop_file_hint());
    }

protected:
    SessionProperties() {}
    virtual ~SessionProperties() {}

    SessionProperties(const SessionProperties&) = delete;
    SessionProperties& operator=(const SessionProperties&) = delete;
};

class SessionLifeCycleObserver : public platform::ReferenceCountedBase
{
public:
    typedef platform::shared_ptr<SessionLifeCycleObserver> Ptr;
    
    virtual void on_session_requested(WellKnownApplication app) = 0;
    virtual void on_session_born(const SessionProperties::Ptr& props) = 0;
    virtual void on_session_unfocused(const SessionProperties::Ptr& props) = 0;
    virtual void on_session_focused(const SessionProperties::Ptr& props) = 0;
    virtual void on_session_died(const SessionProperties::Ptr& props) = 0;

protected:
    SessionLifeCycleObserver() {}
    virtual ~SessionLifeCycleObserver() {}

    SessionLifeCycleObserver(const SessionLifeCycleObserver&) = delete;
    SessionLifeCycleObserver& operator=(const SessionLifeCycleObserver&) = delete;
};

class SessionPreviewProvider : public platform::ReferenceCountedBase
{
public:
    typedef platform::shared_ptr<SessionPreviewProvider> Ptr;

    virtual bool get_or_update_session_preview(GLuint texture, unsigned int& width, unsigned int& height) = 0;

protected:
    SessionPreviewProvider() {}
    virtual ~SessionPreviewProvider() {}

    SessionPreviewProvider(const SessionPreviewProvider&) = delete;
    SessionPreviewProvider& operator=(const SessionPreviewProvider&) = delete;
};

}
}

#endif // UBUNTU_UI_SESSION_ENUMERATOR_H_
