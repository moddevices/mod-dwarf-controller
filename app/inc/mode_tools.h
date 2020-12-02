
/*
************************************************************************************************************************
*
************************************************************************************************************************
*/

#ifndef MODE_TOOLS_H
#define MODE_TOOLS_H


/*
************************************************************************************************************************
*           INCLUDE FILES
************************************************************************************************************************
*/

/*
************************************************************************************************************************
*           DO NOT CHANGE THESE DEFINES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           CONFIGURATION DEFINES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           DATA TYPES
************************************************************************************************************************
*/

/*
************************************************************************************************************************
*           GLOBAL VARIABLES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           MACRO'S
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           FUNCTION PROTOTYPES
************************************************************************************************************************
*/

void TM_init(void);
void TM_trigger_tool(uint8_t tool, uint8_t status);
uint8_t TM_status(void);
uint8_t TM_has_tool_enabled(void);
void TM_enter(uint8_t encoder);
void TM_up(uint8_t encoder);
void TM_down(uint8_t encoder);
void TM_reset_menu(void);
void TM_settings_refresh(void);
void TM_menu_refresh(void);
void TM_update_gain(uint8_t display_id, uint8_t update_id, float value, float min, float max, uint8_t dir);
void TM_menu_item_changed_cb(uint8_t item_ID, uint16_t value);
void TM_launch_tool(uint8_t tool);

/*
************************************************************************************************************************
*           CONFIGURATION ERRORS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           END HEADER
************************************************************************************************************************
*/

#endif
