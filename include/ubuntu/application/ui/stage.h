#ifndef UBUNTU_APPLICATION_UI_STAGE_H_
#define UBUNTU_APPLICATION_UI_STAGE_H_

typedef enum
{
	U_MAIN_STAGE = 0,
	U_INTEGRATION_STAGE = 1,
    U_SHARE_STAGE = 2,
    U_CONTENT_PICKING_STAGE = 3,
    U_SIDE_STAGE = 4,
    U_CONFIGURATION_STAGE = 5
} UAUiStage;

#ifndef UBUNTU_APPLICATION_UI_STAGE_HINT_H_
#define UBUNTU_APPLICATION_UI_STAGE_HINT_H_

/*namespace ubuntu
{
namespace application
{
namespace ui
{
enum StageHint
{
    main_stage = MAIN_STAGE_HINT,
    integration_stage = INTEGRATION_STAGE_HINT,
    share_stage = SHARE_STAGE_HINT,
    content_picking_stage = CONTENT_PICKING_STAGE_HINT,
    side_stage = SIDE_STAGE_HINT,
    configuration_stage = CONFIGURATION_STAGE_HINT
};  
}
}   
} */  

#endif /* UBUNTU_APPLICATION_UI_STAGE_HINT_H_ */

//typedef ubuntu::application::ui::StageHint UApplicationUiStage;

#endif /* UBUNTU_APPLICATION_UI_STAGE_H_ */
