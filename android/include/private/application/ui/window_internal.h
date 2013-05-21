#ifndef UBUNTU_APPLICATION_UI_WINDOW_INTERNAL_H_
#define UBUNTU_APPLICATION_UI_WINDOW_INTERNAL_H_

#include <ubuntu/application/ui/window_properties.h>

namespace ubuntu
{
namespace application
{
namespace ui
{
class WindowProperties : public ubuntu::platform::ReferenceCountedBase
{
public:
    WindowProperties() {}

    typedef ubuntu::platform::shared_ptr<WindowProperties> Ptr;
   
    void set_titlen(const char* title, size_t size)
    {
        this->title = (char *) malloc(sizeof (char) * (size+1));
        memcpy(this->title, title, size+1);
    }

    char *get_title()
    {
        return this->title;
    }
    
    void set_role(UAUiWindowRole role)
    {
        this->role = role;
    }
   
    UAUiWindowRole get_role()
    {
        return this->role;
    }

    void set_input_event_cb_and_ctx(UAUiWindowInputEventCb cb, void* ctx)
    {
        this->cb = cb;
        this->ctx = ctx;
    }

    UAUiWindowInputEventCb get_input_cb()
    {
        return this->cb;
    }

    void* get_ctx()
    {
        return this->ctx;
    }

private:
    char *title;
    UAUiWindowRole role;
    UAUiWindowInputEventCb cb;
    void* ctx;
   
protected:
    virtual ~WindowProperties() {}

    WindowProperties(const WindowProperties&) = delete;
    WindowProperties& operator=(const WindowProperties&) = delete;
};

class SessionProperties : public ubuntu::platform::ReferenceCountedBase
{
public:
    SessionProperties() {}

    typedef ubuntu::platform::shared_ptr<SessionProperties> Ptr;

    void set_type(SessionType type)
    {
        this->type = type;
    }

    SessionType get_type()
    {
        return this->type;
    }

private:
    SessionType type;

protected:
    virtual ~SessionProperties() {}

    SessionProperties(const SessionProperties&) = delete;
    SessionProperties& operator=(const SessionProperties&) = delete;
};
}
}
}
#endif /* UBUNTU_APPLICATION_UI_WINDOW_INTERNAL_H_ */
