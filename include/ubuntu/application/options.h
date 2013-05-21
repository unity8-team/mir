#ifndef UBUNTU_APPLICATION_OPTIONS_H_
#define UBUNTU_APPLICATION_OPTIONS_H_

#include "operation_mode.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef void UApplicationOptions;
    
    UApplicationOptions*
    u_application_options_new_from_cmd_line(
    	int argc, 
    	char** argv);
    
    void
    u_application_options_destroy(
    	UApplicationOptions *options);
    
    UApplicationOperationMode
    u_application_options_get_operation_mode(
    	UApplicationOptions *options);
       
#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_OPTIONS_H_ */
