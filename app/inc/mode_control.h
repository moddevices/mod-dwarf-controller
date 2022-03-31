
/*
************************************************************************************************************************
*
************************************************************************************************************************
*/

#ifndef TOOL_CONTROL_H
#define TOOL_CONTROL_H


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

void CM_init(void);
void CM_add_control(control_t *control, uint8_t protocol);
void CM_remove_control(uint8_t hw_id);
control_t *CM_get_control(uint8_t hw_id);
void CM_inc_control(uint8_t encoder);
void CM_dec_control(uint8_t encoder);
void CM_toggle_control(uint8_t encoder);
void CM_foot_control_change(uint8_t foot, uint8_t value);
void CM_set_control(uint8_t hw_id, float value);
float CM_get_control_value(uint8_t hw_id);
uint8_t CM_tap_tempo_status(uint8_t id);
void CM_draw_encoders(void);
void CM_draw_foots(void);
void CM_draw_foot(uint8_t foot_id);
void CM_load_next_page();
void CM_load_next_encoder_page(uint8_t button);
void CM_print_screen(void);
void CM_set_state(void);
void CM_set_foot_led(control_t *control, uint8_t update_led);
void CM_close_overlay(void);
void CM_set_leds(void);
void CM_print_control_overlay(control_t *control, uint16_t overlay_time);
void CM_set_pages_available(uint8_t page_toggles[8]);
void CM_reset_encoder_page(void);
void CM_reset_page(void);
void CM_set_list_behaviour(uint8_t click_list);
void CM_reset_list_actuators(void);
void CM_reset_momentary_control(uint8_t foot);

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
