
/*
************************************************************************************************************************
*
************************************************************************************************************************
*/

#ifndef NAVEG_H
#define NAVEG_H


/*
************************************************************************************************************************
*           INCLUDE FILES
************************************************************************************************************************
*/

#include <stdbool.h>
#include <stdint.h>
#include "data.h"
#include "glcd_widget.h"


/*
************************************************************************************************************************
*           DO NOT CHANGE THESE DEFINES
************************************************************************************************************************
*/

#define ALL_EFFECTS     -1
#define ALL_CONTROLS    ":all"
enum {UI_DISCONNECTED, UI_CONNECTED};


/*
************************************************************************************************************************
*           CONFIGURATION DEFINES
************************************************************************************************************************
*/

#define FOOT_TOOL_AMOUNT		2

/*
************************************************************************************************************************
*           DATA TYPES
************************************************************************************************************************
*/

//device modes
enum{MODE_CONTROL, MODE_NAVIGATION, MODE_TOOL_FOOT, MODE_TOOL_MENU, MODE_BUILDER, MODE_SHIFT, MODE_SELFTEST};

//different tool modes
enum{TOOL_MENU, TOOL_FOOT, TOOL_TUNER, TOOL_SYNC, TOOL_BYPASS};

/*
************************************************************************************************************************
*           GLOBAL VARIABLES
************************************************************************************************************************
*/
bool g_should_wait_for_webgui;
bool g_protocol_busy;
bool g_ui_communication_started;
bool g_device_booted; 
bool g_popup_active;

uint8_t g_encoders_pressed[ENCODERS_COUNT];

uint8_t g_initialized;

bool g_self_test_mode;
bool g_self_test_cancel_button;
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

uint8_t naveg_get_current_mode(void);
void naveg_init(void);
void naveg_turn_off_leds(void);
void naveg_ui_connection(uint8_t status);
uint8_t naveg_ui_status(void);
void naveg_enc_enter(uint8_t encoder);
void naveg_enc_released(uint8_t encoder);
void naveg_enc_hold(uint8_t encoder);
void naveg_enc_down(uint8_t encoder);
void naveg_enc_up(uint8_t encoder);
void naveg_foot_change(uint8_t foot, uint8_t pressed);
void naveg_foot_double_press(uint8_t foot);
void naveg_button_pressed(uint8_t button);
void naveg_button_released(uint8_t button);
void naveg_shift_pressed();
void naveg_shift_releaed();
uint8_t naveg_dialog_status(void);
uint8_t naveg_dialog(char *msg);
void naveg_release_dialog_semaphore(void);
void naveg_trigger_mode_change(uint8_t mode);
void naveg_print_shift_screen(void);

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
