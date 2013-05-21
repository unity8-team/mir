#ifndef UBUNTU_APPLICATION_UI_OPTIONS_H_
#define UBUNTU_APPLICATION_UI_OPTIONS_H_

#include "form_factor.h"
#include "stage.h"
#include <ubuntu/application/options.h>

#ifdef __cplusplus
extern "C" {
#endif
   
    UAUiFormFactor
    u_application_options_get_form_factor(
    	UApplicationOptions* options);
   
    UAUiStage
    u_application_options_get_stage(
    	UApplicationOptions* options);
    
#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_UI_OPTIONS_H_ */
