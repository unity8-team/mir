#ifndef UBUNTU_APPLICATION_INSTANCE_H_
#define UBUNTU_APPLICATION_INSTANCE_H_

#include "description.h"
#include "options.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef void UApplicationInstance;
    
    UApplicationInstance*
    u_application_instance_new_from_description_with_options(
    	UApplicationDescription *desc,
    	UApplicationOptions *options);
    
    void
    u_application_instance_ref(
    	UApplicationInstance *instance);
    
    void
    u_application_instance_unref(
    	UApplicationInstance *instance);
    
    void
    u_application_instance_destroy(
    	UApplicationInstance *instance);
    
    void
    u_application_instance_run(
    	UApplicationInstance *instance);

#ifdef __cplusplus
}
#endif
    
#endif /* UBUNTU_APPLICATION_INSTANCE_H_ */
