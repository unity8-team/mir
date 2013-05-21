#ifndef UBUNTU_APPLICATION_DESCRIPTION_H_
#define UBUNTU_APPLICATION_DESCRIPTION_H_

#include "id.h"
#include "lifecycle_delegate.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef void UApplicationDescription;
    
    UApplicationDescription*
    u_application_description_new();
    
    void
    u_application_description_destroy(
    	UApplicationDescription *desc);
    
    void
    u_application_description_set_application_id(
    	UApplicationDescription *desc,
    	UApplicationId *id);
    
    UApplicationId* 
    u_application_description_get_application_id(
    	UApplicationDescription *desc);
    
    void
    u_application_description_set_application_lifecycle_delegate(
    	UApplicationDescription *desc,
    	UApplicationLifecycleDelegate *lifecycle_delegate);
    
    UApplicationLifecycleDelegate* 
    u_application_description_get_application_lifecycle_delegate(
    	UApplicationDescription *desc);
   
#ifdef __cplusplus
}
#endif

/*
void
u_application_description_set_application_error_delegate(
	UApplicationDescription *desc,
	u_application_error_delegate *error_delegate);

u_application_error_delegate*
u_application_description_get_application_error_delegate(
	UApplicationDescription *desc);

void
u_application_description_set_system_delegate(
	UApplicationDescription *desc,
	u_system_delegate *system_delegate);

u_system_delegate*
u_application_description_get_system_delegate(
	UApplicationDescription *desc);
*/

#endif /* UBUNTU_APPLICATION_DESCRIPTION_H_ */
