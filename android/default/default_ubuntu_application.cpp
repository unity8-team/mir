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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 *              Ricardo Mendoza <ricardo.mendoza@canonical.com>
 */

// Private
#include <private/application/ui/init.h>
#include <private/application/ui/setup.h>
#include <private/application/ui/ubuntu_application_ui.h>

// Public C apis
#include <ubuntu/application/id.h>
#include <ubuntu/application/instance.h>
#include <ubuntu/application/lifecycle_delegate.h>
#include <ubuntu/application/ui/options.h>

// ver2.0 Private

#include <private/ui/session_service.h>
#include <private/application/application.h>

#include <utils/Log.h>

// C APIs
namespace
{
struct IUApplicationLifecycleDelegate : public ubuntu::application::LifecycleDelegate
{
    IUApplicationLifecycleDelegate(void *context) :
                                    application_resumed_cb(NULL),
                                    application_about_to_stop_cb(NULL),
                                    context(context),
                                    refcount(1)
    {
    }

    void on_application_resumed()
    {
        ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    
        if (!application_resumed_cb)
            return;
    
        application_resumed_cb(NULL, this->context);
    }

    void on_application_about_to_stop()
    {
        ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);

        if (!application_about_to_stop_cb)
            return;

        application_about_to_stop_cb(NULL, this->context);
    }

    u_on_application_resumed application_resumed_cb;
    u_on_application_about_to_stop application_about_to_stop_cb;
    void *context;

    unsigned refcount;
};

template<typename T>
struct Holder
{
    Holder(const T&value = T()) : value(value)
    {
    }

    T value;
};

template<typename T>
Holder<T>* make_holder(const T& value)
{
    return new Holder<T>(value);
}
}

/*
 * Application Lifecycle 
 */

UApplicationLifecycleDelegate*
u_application_lifecycle_delegate_new()
{
    ALOGI("%s()", __PRETTY_FUNCTION__);

    ubuntu::application::LifecycleDelegate::Ptr p(new IUApplicationLifecycleDelegate(NULL));

    return make_holder(p);
}

void
u_application_lifecycle_delegate_destroy(UApplicationLifecycleDelegate *delegate)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);

    auto s = static_cast<Holder<IUApplicationLifecycleDelegate*>*>(delegate);

    if (s->value->refcount)
        return;

    delete s;
}

void
u_application_lifecycle_delegate_ref(UApplicationLifecycleDelegate *delegate)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);

    auto s = static_cast<Holder<IUApplicationLifecycleDelegate*>*>(delegate);
    s->value->refcount++;
}

void
u_application_lifecycle_delegate_unref(UApplicationLifecycleDelegate *delegate)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    
    auto s = static_cast<Holder<IUApplicationLifecycleDelegate*>*>(delegate);
    if (s->value->refcount)
        s->value->refcount--;
}

void
u_application_lifecycle_delegate_set_application_resumed_cb(
    UApplicationLifecycleDelegate *delegate,
    u_on_application_resumed cb)
{ 
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    
    auto s = static_cast<Holder<IUApplicationLifecycleDelegate*>*>(delegate);
    s->value->application_resumed_cb = cb;
}

u_on_application_resumed
u_application_lifecycle_delegate_get_application_resumed_cb(
    UApplicationLifecycleDelegate *delegate)
{ 
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    
    auto s = static_cast<Holder<IUApplicationLifecycleDelegate*>*>(delegate);
    return s->value->application_resumed_cb;
}

void
u_application_lifecycle_delegate_set_application_about_to_stop_cb(
    UApplicationLifecycleDelegate *delegate,
    u_on_application_about_to_stop cb)
{ 
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    
    auto s = static_cast<Holder<IUApplicationLifecycleDelegate*>*>(delegate);
    s->value->application_about_to_stop_cb = cb;
}

u_on_application_about_to_stop
u_application_lifecycle_delegate_get_application_about_to_stop_cb(
    UApplicationLifecycleDelegate *delegate)
{ 
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    
    auto s = static_cast<Holder<IUApplicationLifecycleDelegate*>*>(delegate);
    return s->value->application_about_to_stop_cb;
}

void
u_application_lifecycle_delegate_set_context(
    UApplicationLifecycleDelegate *delegate,
    void *context)
{
    ALOGI("%s():%d context=%p", __PRETTY_FUNCTION__, __LINE__, context);
    
    auto s = static_cast<Holder<IUApplicationLifecycleDelegate*>*>(delegate);
    if (s->value->context == NULL)
        s->value->context = context;
}

void*
u_application_lifecycle_delegate_get_context(
    UApplicationLifecycleDelegate *delegate,
    void *context)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    
    auto s = static_cast<Holder<IUApplicationLifecycleDelegate*>*>(delegate);
    return s->value->context;
}

/*
 * Application Options
 */

UApplicationOptions*
u_application_options_new_from_cmd_line(
    int argc,
    char** argv)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);

    ubuntu::application::ui::init(argc, argv);

    return ubuntu::application::ui::Setup::instance().get();
}

UAUiFormFactor
u_application_options_get_form_factor(
    UApplicationOptions *options)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    
    auto setup = static_cast<ubuntu::application::ui::Setup*>(options);
    return static_cast<UAUiFormFactor>(setup->form_factor_hint());
}

UAUiStage
u_application_options_get_stage(
    UApplicationOptions *options)
{
    ALOGI("%s():%d", __PRETTY_FUNCTION__, __LINE__);
    
    auto setup = static_cast<ubuntu::application::ui::Setup*>(options);
    return static_cast<UAUiStage>(setup->stage_hint());
}

/*
 * Application Id
 */

UApplicationId*
u_application_id_new_from_stringn(
    const char *string,
    size_t size)
{
    ubuntu::application::Id::Ptr id(
        new ubuntu::application::Id(string, size)
        );

    return make_holder(id);
}

void
u_application_id_destroy(UApplicationId *id)
{
    
    auto p = static_cast<Holder<ubuntu::application::Id::Ptr>*>(id);

    if (p)
        delete p;
}

int
u_application_id_compare(
    UApplicationId *lhs,
    UApplicationId *rhs)
{    
    auto ilhs = static_cast<Holder<ubuntu::application::Id::Ptr>*>(lhs);
    auto irhs = static_cast<Holder<ubuntu::application::Id::Ptr>*>(rhs);
    
    if (ilhs->value->size != irhs->value->size)
        return 1;

    for (size_t i = 0; i < ilhs->value->size; i++)
    {
        if ((char) ilhs->value->string[i] == (char) irhs->value->string[i])
            continue;

        return 1;           
    }

    return 0;
}

/*
 * Description
 */

UApplicationDescription*
u_application_description_new()
{
    ubuntu::application::Description::Ptr desc(
        new ubuntu::application::Description()
        );

    return make_holder(desc);
}

void
u_application_description_destroy(
    UApplicationDescription *desc)
{
    auto p = static_cast<Holder<ubuntu::application::Description::Ptr>*>(desc);

    if (p)
        delete p;
}

void
u_application_description_set_application_id(
    UApplicationDescription *desc,
    UApplicationId *id)
{
    if (id == NULL)
        return;

    auto p = static_cast<Holder<ubuntu::application::Description::Ptr>*>(desc);
    p->value->set_application_id(id);
}

UApplicationId*
u_application_description_get_application_id(
    UApplicationDescription *desc)
{
    auto p = static_cast<Holder<ubuntu::application::Description::Ptr>*>(desc);
    return p->value->get_application_id();
}

void
u_application_description_set_application_lifecycle_delegate(
    UApplicationDescription *desc,
    UApplicationLifecycleDelegate *delegate)
{
    if (delegate == NULL)
        return;
    
    ALOGI("%s():%d -- delegate=%p", __PRETTY_FUNCTION__, __LINE__, delegate);

    auto p = static_cast<Holder<ubuntu::application::Description::Ptr>*>(desc);
    p->value->set_lifecycle_delegate(delegate);
}

UApplicationLifecycleDelegate*
u_application_description_get_application_lifecycle_delegate(
    UApplicationDescription *desc)
{
    auto p = static_cast<Holder<ubuntu::application::Description::Ptr>*>(desc);
    return p->value->get_lifecycle_delegate();
}

/*
 * Instance
 */

UApplicationInstance*
u_application_instance_new_from_description_with_options(
    UApplicationDescription *desc,
    UApplicationOptions *options)
{
    if (desc == NULL || options == NULL)
        return NULL;

    ubuntu::application::Instance::Ptr instance(
        new ubuntu::application::Instance(desc, options)
        );

    return make_holder(instance);
}

void
u_application_instance_ref(
    UApplicationInstance *instance)
{
    auto p = static_cast<Holder<ubuntu::application::Instance::Ptr>*>(instance);
    p->value->ref();
}

void
u_application_instance_unref(
    UApplicationInstance *instance)
{
    auto p = static_cast<Holder<ubuntu::application::Instance::Ptr>*>(instance);
    p->value->unref();
}

void
u_application_instance_destroy(
    UApplicationInstance *instance)
{
    auto p = static_cast<Holder<ubuntu::application::Instance::Ptr>*>(instance);

    if (p->value->get_refcount() == 0)
        delete p;
}

void
u_application_instance_run(
    UApplicationInstance *instance)
{
    auto p = static_cast<Holder<ubuntu::application::Instance::Ptr>*>(instance);

    if (p)
        p->value->run();
}
