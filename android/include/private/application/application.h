#ifndef UBUNTU_APPLICATION_APPLICATION_H_
#define UBUNTU_APPLICATION_APPLICATION_H_

#include <utils/Log.h>

namespace ubuntu
{
namespace application
{
class Instance : public ubuntu::platform::ReferenceCountedBase
{
public:
    Instance(UApplicationDescription *description, UApplicationOptions *options)
            : description(description),
              options(options)
    {}

    typedef ubuntu::platform::shared_ptr<Instance> Ptr;

    void run()
    {
        return;
    }

    void ref()
    {
        refcount++;
    }

    void unref()
    {
        if (refcount)
            refcount--;
    }

    unsigned int get_refcount()
    {
        return this->refcount;
    }

    UApplicationDescription* get_description()
    {
        return this->description;
    }

private:
    UApplicationDescription *description;
    UApplicationOptions *options;
    unsigned int refcount;

protected:
    virtual ~Instance() {}

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
};


class Description : public ubuntu::platform::ReferenceCountedBase
{
public:
    typedef ubuntu::platform::shared_ptr<Description> Ptr;

    void set_lifecycle_delegate(UApplicationLifecycleDelegate *delegate)
    {
        this->lifecycle_delegate = delegate;
    }

    void set_application_id(UApplicationId *id)
    {
        this->application_id = id;
    }

    UApplicationId* get_application_id()
    {
        return this->application_id;
    }

    UApplicationLifecycleDelegate* get_lifecycle_delegate()
    {
        return this->lifecycle_delegate;
    }

    unsigned int refcount;

private:
    UApplicationId *application_id;
    UApplicationLifecycleDelegate *lifecycle_delegate;
   
protected:
    Description() : refcount(1) {}

    virtual ~Description() {}

    Description(const Description&) = delete;
    Description& operator=(const Description&) = delete;
};
}
}

// C-API implementation
namespace
{
struct IUApplicationDescription : public ubuntu::application::Description
{
};
}
#endif /* UBUNTU_APPLICATION_UI_WINDOW_INTERNAL_H_ */
