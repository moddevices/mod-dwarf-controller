
/*
************************************************************************************************************************
*           INCLUDE FILES
************************************************************************************************************************
*/

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "system.h"
#include "config.h"
#include "data.h"
#include "naveg.h"
#include "hardware.h"
#include "actuator.h"
#include "cli.h"
#include "screen.h"
#include "glcd_widget.h"
#include "glcd.h"
#include "utils.h"
#include "mod-protocol.h"
#include "ui_comm.h"
#include "sys_comm.h"
#include "mode_tools.h"
#include "naveg.h"

/*
************************************************************************************************************************
*           LOCAL DEFINES
************************************************************************************************************************
*/

#define SHIFT_MENU_ITEMS_COUNT      14

/*
************************************************************************************************************************
*           LOCAL CONSTANTS
************************************************************************************************************************
*/

// systemctl services names
const char *systemctl_services[] = {
    "jack2",
    "sshd",
    "mod-ui",
    "dnsmasq",
    NULL
};

const char *versions_names[] = {
    "version",
    "restore",
    "system",
    "controller",
    NULL
};

static const uint8_t SHIFT_ITEM_IDS[SHIFT_MENU_ITEMS_COUNT] = {INP_STEREO_LINK, INP_1_GAIN_ID, INP_2_GAIN_ID, OUTP_STEREO_LINK, OUTP_1_GAIN_ID, OUTP_2_GAIN_ID,\
                                                                HEADPHONE_VOLUME_ID, CLOCK_SOURCE_ID, SEND_CLOCK_ID, MIDI_PB_PC_CHANNEL_ID, MIDI_SS_PC_CHANNEL_ID,\
                                                                DISPLAY_BRIGHTNESS_ID, BPM_ID, BPB_ID};

/*
************************************************************************************************************************
*           LOCAL DATA TYPES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           LOCAL MACROS
************************************************************************************************************************
*/

#define UNUSED_PARAM(var)   do { (void)(var); } while (0)
#define ROUND(x)    ((x) > 0.0 ? (((float)(x)) + 0.5) : (((float)(x)) - 0.5))
#define MAP(x, Omin, Omax, Nmin, Nmax)      ( x - Omin ) * (Nmax -  Nmin)  / (Omax - Omin) + Nmin;


/*
************************************************************************************************************************
*           LOCAL GLOBAL VARIABLES
************************************************************************************************************************
*/

static uint8_t g_comm_protocol_bussy = 0;
uint8_t g_bypass[4] = {};
uint8_t g_current_profile = 1;
int8_t g_sl_out = -1;
int8_t g_sl_in = -1;
uint8_t g_snapshot_prog_change = 0;
uint8_t g_pedalboard_prog_change = 0;
uint16_t g_beats_per_minute = 0;
uint8_t g_beats_per_bar = 0;
uint8_t g_MIDI_clk_send = 0;
uint8_t g_MIDI_clk_src = 0;
uint8_t g_play_status = 0;
uint8_t g_tuner_mute = 0;
uint8_t g_tuner_input = 0;
int8_t g_display_brightness = -1;
int8_t g_display_contrast = -1;
int8_t g_actuator_hide = -1;
int8_t g_shift_item[3] = {-1, -1, -1};
int8_t g_default_tool = -1;
int8_t g_list_mode = -1;
int8_t g_control_header = -1;
int8_t g_usb_mode = -1;

struct TAP_TEMPO_T {
    uint32_t time, max;
    uint8_t state;
} g_tool_tap_tempo;

/*
************************************************************************************************************************
*           LOCAL FUNCTION PROTOTYPES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           LOCAL CONFIGURATION ERRORS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           LOCAL FUNCTIONS
************************************************************************************************************************
*/


static void update_gain_item_value(uint8_t menu_id, float value)
{
    menu_item_t *item = TM_get_menu_item_by_ID(menu_id);
    item->data.value = value;

    static char str_bfr[8] = {};
    float value_bfr = 0;
    value_bfr = MAP(item->data.value, item->data.min, item->data.max, 0, 100); 
    int_to_str(value_bfr, str_bfr, 8, 0);
    strcat(str_bfr, "%");
    item->data.unit_text = str_bfr;
}

static void update_status(char *item_to_update, const char *response)
{
    if (!item_to_update) return;

    char *pstr = strstr(item_to_update, ":");
    if (pstr && response)
    {
        pstr++;
        *pstr++ = ' ';
        strcpy(pstr, response);
    }
}

void set_item_value(char *command, uint16_t value)
{
    if (g_comm_protocol_bussy) return;

    uint8_t i;
    char buffer[50];

    i = copy_command((char *)buffer, command);

    // copy the value
    char str_buf[8];
    int_to_str(value, str_buf, 4, 0);
    const char *p = str_buf;
    while (*p)
    {
        buffer[i++] = *p;
        p++;
    }
    buffer[i] = 0;

    // sets the response callback
    ui_comm_webgui_set_response_cb(NULL, NULL);

    // sends the data to GUI
    ui_comm_webgui_send(buffer, i);
}

static void set_menu_item_value(uint16_t menu_id, uint16_t value)
{
    if (g_comm_protocol_bussy) return;

    uint8_t i = 0;
    char buffer[50];
    memset(buffer, 0, sizeof buffer);

    i = copy_command((char *)buffer, CMD_MENU_ITEM_CHANGE);

    // copy the id
    i += int_to_str(menu_id, &buffer[i], sizeof(buffer) - i, 0);

    // inserts one space
    buffer[i++] = ' ';

    // copy the value
    i += int_to_str(value, &buffer[i], sizeof(buffer) - i, 0);

    buffer[i++] = 0;

    // sets the response callback
    ui_comm_webgui_set_response_cb(NULL, NULL);

    // sends the data to GUI
    ui_comm_webgui_send(buffer, i);
}

static void recieve_sys_value(void *data, menu_item_t *item)
{
    char **values = data;

    //protocol ok
    if (atof(values[1]) == 0)
        item->data.value = atof(values[2]);
}

/*
************************************************************************************************************************
*           GLOBAL FUNCTIONS
************************************************************************************************************************
*/

uint8_t system_get_shift_item(uint8_t index)
{
    if (index < SHIFT_MENU_ITEMS_COUNT)
        return SHIFT_ITEM_IDS[index];
    else
        return SHIFT_ITEM_IDS[0];
}

void system_recall_stereo_link_settings(void)
{
    //read EEPROM
    uint8_t read_buffer = 0;

    EEPROM_Read(0, SL_INPUT_ADRESS, &read_buffer, MODE_8_BIT, 1);
    g_sl_in = read_buffer;   

    EEPROM_Read(0, SL_OUTPUT_ADRESS, &read_buffer, MODE_8_BIT, 1);
    g_sl_out = read_buffer; 
}

void system_lock_comm_serial(bool busy)
{
    g_comm_protocol_bussy = busy;
}

uint8_t system_get_clock_source(void)
{
    return g_MIDI_clk_src;
}

uint8_t system_get_current_profile(void)
{
    return g_current_profile;
}

void system_save_gains_cb(void *arg, int event)
{
    UNUSED_PARAM(arg);

    if (event == MENU_EV_ENTER)
    {
        if (g_comm_protocol_bussy) return;

        uint8_t i = 0;
        char buffer[50];
        memset(buffer, 0, sizeof buffer);

        i = copy_command((char *)buffer, CMD_SYS_AMIXER_SAVE);

        buffer[i++] = 0;

        // sends the data to GUI
        sys_comm_send(buffer, NULL);

        sys_comm_wait_response();
    }
}

void system_update_menu_value(uint8_t item_ID, uint16_t value)
{
    switch(item_ID)
    {
        //play status
        case MENU_ID_PLAY_STATUS:
            g_play_status = value;
        break;
        //global tempo
        case MENU_ID_TEMPO:
            g_beats_per_minute = value;
            //check if we need to update leds/display
            if (naveg_get_current_mode() == MODE_TOOL_FOOT) && (TM_check_tool_status() == TOOL_SYNC)
            {
                TM_print_tool();
                TM_set_leds();
            }
        break;
        //global tempo status
        case MENU_ID_BEATS_PER_BAR:
            g_beats_per_bar = value;
        break;
        //tuner mute
        case MENU_ID_TUNER_MUTE: 
            g_tuner_mute = value;
        break;
        //bypass channel 1
        case MENU_ID_BYPASS1: 
            g_bypass[0] = value;
        break;
        //bypass channel 2
        case MENU_ID_BYPASS2: 
            g_bypass[1] = value;
        break;
        //sl input
        case MENU_ID_SL_IN: 
            g_sl_in = value;
        break;
        //stereo link output
        case MENU_ID_SL_OUT: 
            g_sl_out = value;
        break;
        //MIDI clock source
        case MENU_ID_MIDI_CLK_SOURCE: 
            g_MIDI_clk_src = value;
        break;
        //send midi clock
        case MENU_ID_MIDI_CLK_SEND: 
            g_MIDI_clk_send = value;
        break;
        //snapshot prog change 
        case MENU_ID_SNAPSHOT_PRGCHGE: 
            g_snapshot_prog_change = value;
        break;
        //pedalboard prog change 
        case MENU_ID_PB_PRGCHNGE: 
            g_pedalboard_prog_change = value;
        break;
        //user profile change 
        case MENU_ID_CURRENT_PROFILE: 
            g_current_profile = value;
        break;
        //display brightness
        case MENU_ID_BRIGHTNESS: 
            g_display_brightness = value;
            hardware_glcd_brightness(g_display_brightness); 
        break;
        default:
            return;
        break;
        
    }
}

float system_get_gain_value(uint8_t item_ID)
{
    return TM_get_menu_item_by_ID(item_ID)->data.value;
}

void system_bluetooth_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    if (event == MENU_EV_ENTER)
    {
        const char *response;
        char resp[LINE_BUFFER_SIZE];
        if (item->desc->id == BLUETOOTH_ID)
        {
            response = cli_command("mod-bluetooth hmi", CLI_RETRIEVE_RESPONSE);
            
            strncpy(resp, response, sizeof(resp)-1);
            char **items = strarr_split(resp, '|');;

            if (items)
            {
                update_status(item->data.list[2], items[0]);
                update_status(item->data.list[3], items[1]);
                update_status(item->data.list[4], items[2]);

                FREE(items);
            } 
        }
        else if (item->desc->id == BLUETOOTH_ID+1)
        {
            cli_command("mod-bluetooth discovery", CLI_DISCARD_RESPONSE);
        }
    }
}

void system_services_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    if (event == MENU_EV_ENTER)
    {
        uint8_t i = 0;
        while (systemctl_services[i])
        {
            const char *response;
            response = cli_systemctl("is-active ", systemctl_services[i]);
            update_status(item->data.list[i+1], response);
            i++;
        }
    }
}

void system_versions_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    if (event == MENU_EV_ENTER)
    {
        const char *response;
        char version[8];

        uint8_t i = 0;
        while (versions_names[i])
        {
            cli_command("mod-version ", CLI_CACHE_ONLY);
            response = cli_command(versions_names[i], CLI_RETRIEVE_RESPONSE);
            strncpy(version, response, (sizeof version) - 1);
            version[(sizeof version) - 1] = 0;
            update_status(item->data.list[i+1], version);
            screen_system_menu(item);
            i++;
        }
    }
}

void system_release_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    if (event == MENU_EV_ENTER)
    {
        const char *response;
        response = cli_command("mod-version release", CLI_RETRIEVE_RESPONSE);
        item->data.popup_content = response;
    }
}


void system_device_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    if (event == MENU_EV_ENTER)
    {
        const char *response;
        response = cli_command("cat /var/cache/mod/tag", CLI_RETRIEVE_RESPONSE);
        update_status(item->data.list[1], response);
    }
}

void system_tag_cb(void *arg, int event)
{

    menu_item_t *item = arg;

    if (event == MENU_EV_ENTER)
    {
        const char *response;
        char *txt = "The serial number of your     device is:                    ";
        response =  cli_command("cat /var/cache/mod/tag", CLI_RETRIEVE_RESPONSE);
        char * bfr = (char *) MALLOC(1 + strlen(txt)+ strlen(response));
        strcpy(bfr, txt);
        strcat(bfr, response);
        item->data.popup_content = bfr;
        item->data.popup_header = "serial number";
    }
}

void system_upgrade_cb(void *arg, int event)
{
    menu_item_t *item = arg;
    if (event == MENU_EV_ENTER)
    {
        button_t *foot = (button_t *) hardware_actuators(FOOTSWITCH2);

        // check if OK option was chosen
        if (item->data.hover == 0)
        {
            uint8_t status = actuator_get_status(foot);

            // check if footswitch is pressed down
            if (BUTTON_PRESSED(status))
            {
                //clear all screens
                screen_clear();

                // start restore
                cli_restore(RESTORE_INIT);

                item->data.value = 1;

                return;
            }
        }
    }

    item->data.value = 0;
}

void system_inp_1_volume_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    char val_buffer[20];
    uint8_t q;

    if ((event == MENU_EV_ENTER) || (event == MENU_EV_NONE))
    {
        sys_comm_set_response_cb(recieve_sys_value, item);

        q = copy_command(val_buffer, "0 2");
        val_buffer[q] = 0;
        
        sys_comm_send(CMD_SYS_GAIN, val_buffer);
        sys_comm_wait_response();

        item->data.min = 0.0f;
        item->data.max = 98.0f;
        item->data.step = 1.0f;
    }
    else if ((event == MENU_EV_UP) ||(event == MENU_EV_DOWN))
    {
        if (event == MENU_EV_UP)
            item->data.value += item->data.step;
        else
            item->data.value -= item->data.step;

        if (item->data.value > item->data.max)
            item->data.value = item->data.max;
        if (item->data.value < item->data.min)
            item->data.value = item->data.min;

        if (g_sl_in)
        {
            q = copy_command(val_buffer, "0 0 ");
            update_gain_item_value(INP_2_GAIN_ID, item->data.value);
        }
        else
        {
            q = copy_command(val_buffer, "0 2 ");
        }

        // insert the value on buffer
        q += float_to_str(item->data.value, &val_buffer[q], sizeof(val_buffer) - q, 1);
        val_buffer[q] = 0;

        sys_comm_send(CMD_SYS_GAIN, val_buffer);
        sys_comm_wait_response();

        item->data.step = 1.0f;
    }

    static char str_bfr[10] = {};
    float scaled_val = MAP(item->data.value, item->data.min, item->data.max, 0, 100);
    int_to_str(scaled_val, str_bfr, sizeof(str_bfr), 0);
    strcat(str_bfr, "%");
    item->data.unit_text = str_bfr;

    if (event != MENU_EV_NONE)
    {
        if (naveg_get_current_mode() == MODE_SHIFT)
            screen_shift_overlay(-1);
    }
}

void system_inp_2_volume_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    char val_buffer[20];
    uint8_t q;

    if ((event == MENU_EV_ENTER) || (event == MENU_EV_NONE))
    {
        sys_comm_set_response_cb(recieve_sys_value, item);

        q = copy_command(val_buffer, "0 1");
        val_buffer[q] = 0;

        sys_comm_send(CMD_SYS_GAIN, val_buffer);
        sys_comm_wait_response();

        item->data.min = 0.0f;
        item->data.max = 98.0f;
        item->data.step = 1.0f;
    }
    else if ((event == MENU_EV_UP) ||(event == MENU_EV_DOWN))
    {
        if (event == MENU_EV_UP)
            item->data.value += item->data.step;
        else
            item->data.value -= item->data.step;

        if (item->data.value > item->data.max)
            item->data.value = item->data.max;
        if (item->data.value < item->data.min)
            item->data.value = item->data.min;

        if (g_sl_in)
        {
            q = copy_command(val_buffer, "0 0 ");
            update_gain_item_value(INP_1_GAIN_ID, item->data.value);
        }
        else
        {
            q = copy_command(val_buffer, "0 1 ");
        }

        // insert the value on buffer
        q += float_to_str(item->data.value, &val_buffer[q], sizeof(val_buffer) - q, 1);
        val_buffer[q] = 0;

        sys_comm_send(CMD_SYS_GAIN, val_buffer);
        sys_comm_wait_response();

        item->data.step = 1.0f;
    }

    static char str_bfr[10] = {};
    float scaled_val = MAP(item->data.value, item->data.min, item->data.max, 0, 100);
    int_to_str(scaled_val, str_bfr, sizeof(str_bfr), 0);
    strcat(str_bfr, "%");
    item->data.unit_text = str_bfr;

    if (event != MENU_EV_NONE)
    {
        if (naveg_get_current_mode() == MODE_SHIFT)
            screen_shift_overlay(-1);
    }
}

void system_outp_1_volume_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    char val_buffer[20];
    uint8_t q;

    if ((event == MENU_EV_ENTER) || (event == MENU_EV_NONE))
    {
        sys_comm_set_response_cb(recieve_sys_value, item);

        q = copy_command(val_buffer, "1 2");
        val_buffer[q] = 0;

        sys_comm_send(CMD_SYS_GAIN, val_buffer);
        sys_comm_wait_response();

        item->data.min = -60.0f;
        item->data.max = 0.0f;
        item->data.step = 2.0f;
    }
    else if ((event == MENU_EV_UP) ||(event == MENU_EV_DOWN))
    {
        if (event == MENU_EV_UP)
            item->data.value += item->data.step;
        else
            item->data.value -= item->data.step;

        if (item->data.value > item->data.max)
            item->data.value = item->data.max;
        if (item->data.value < item->data.min)
            item->data.value = item->data.min;

        if (g_sl_out)
        {
            q = copy_command(val_buffer, "1 0 ");
            update_gain_item_value(OUTP_2_GAIN_ID, item->data.value);
        }
        else
        {
            q = copy_command(val_buffer, "1 2 ");
        }

        // insert the value on buffer
        q += float_to_str(item->data.value, &val_buffer[q], sizeof(val_buffer) - q, 1);
        val_buffer[q] = 0;

        sys_comm_send(CMD_SYS_GAIN, val_buffer);
        sys_comm_wait_response();

        item->data.step = 2.0f;
    }

    static char str_bfr[10] = {};
    float scaled_val = MAP(item->data.value, item->data.min, item->data.max, 0, 100);
    int_to_str(scaled_val, str_bfr, sizeof(str_bfr), 0);
    strcat(str_bfr, "%");
    item->data.unit_text = str_bfr;

    if (event != MENU_EV_NONE)
    {
        if (naveg_get_current_mode() == MODE_SHIFT)
            screen_shift_overlay(-1);
    }
}

void system_outp_2_volume_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    char val_buffer[20];
    uint8_t q;

    if ((event == MENU_EV_ENTER) || (event == MENU_EV_NONE))
    {
        sys_comm_set_response_cb(recieve_sys_value, item);

        q = copy_command(val_buffer, "1 1");
        val_buffer[q] = 0;

        sys_comm_send(CMD_SYS_GAIN, val_buffer);
        sys_comm_wait_response();

        item->data.min = -60.0f;
        item->data.max = 0.0f;
        item->data.step = 2.0f;
    }
    else if ((event == MENU_EV_UP) ||(event == MENU_EV_DOWN))
    {
        if (event == MENU_EV_UP)
            item->data.value += item->data.step;
        else
            item->data.value -= item->data.step;

        if (item->data.value > item->data.max)
            item->data.value = item->data.max;
        if (item->data.value < item->data.min)
            item->data.value = item->data.min;

        if (g_sl_out)
        {
            q = copy_command(val_buffer, "1 0 ");
            update_gain_item_value(OUTP_1_GAIN_ID, item->data.value);
        }
        else
        {
            q = copy_command(val_buffer, "1 1 ");
        }

        // insert the value on buffer
        q += float_to_str(item->data.value, &val_buffer[q], sizeof(val_buffer) - q, 1);
        val_buffer[q] = 0;

        sys_comm_send(CMD_SYS_GAIN, val_buffer);
        sys_comm_wait_response();

        item->data.step = 2.0f;
    }

    static char str_bfr[10] = {};
    float scaled_val = MAP(item->data.value, item->data.min, item->data.max, 0, 100);
    int_to_str(scaled_val, str_bfr, sizeof(str_bfr), 0);
    strcat(str_bfr, "%");
    item->data.unit_text = str_bfr;

    if (event != MENU_EV_NONE)
    {
        if (naveg_get_current_mode() == MODE_SHIFT)
            screen_shift_overlay(-1);
    }
}

void system_hp_volume_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    char val_buffer[20];
    uint8_t q=0;

    if ((event == MENU_EV_ENTER) || (event == MENU_EV_NONE))
    {
        sys_comm_set_response_cb(recieve_sys_value, item);

        sys_comm_send(CMD_SYS_HP_GAIN, NULL);
        sys_comm_wait_response();

        item->data.min = -33.0f;
        item->data.max = 12.0f;
        item->data.step = 3.0f;
    }
    else if ((event == MENU_EV_UP) ||(event == MENU_EV_DOWN))
    {
        if (event == MENU_EV_UP)
            item->data.value += item->data.step;
        else
            item->data.value -= item->data.step;

        if (item->data.value > item->data.max)
            item->data.value = item->data.max;
        if (item->data.value < item->data.min)
            item->data.value = item->data.min;

        // insert the value on buffer
        q += float_to_str(item->data.value, &val_buffer[q], sizeof(val_buffer) - q, 1);
        val_buffer[q] = 0;

        sys_comm_send(CMD_SYS_HP_GAIN, val_buffer);
        sys_comm_wait_response();

        item->data.step = 3.0f;
    }

    static char str_bfr[10] = {};
    float scaled_val = MAP(item->data.value, item->data.min, item->data.max, 0, 100);
    int_to_str(scaled_val, str_bfr, sizeof(str_bfr), 0);
    strcat(str_bfr, "%");
    item->data.unit_text = str_bfr;

    if (event != MENU_EV_NONE)
    {
        if (naveg_get_current_mode() == MODE_SHIFT)
            screen_shift_overlay(-1);
    }
}

void system_display_brightness_cb(void *arg, int event)
{
    menu_item_t *item = arg;
    static char str_bfr[8];

    if (g_display_brightness == -1)
    {
        //read EEPROM
        uint8_t read_buffer = 0;
        EEPROM_Read(0, DISPLAY_BRIGHTNESS_ADRESS, &read_buffer, MODE_8_BIT, 1);

        g_display_brightness = read_buffer;
    }

    if (event == MENU_EV_ENTER)
    {
        if (g_display_brightness < MAX_BRIGHTNESS) g_display_brightness++;
        else g_display_brightness = 0;
    }
    else if (event == MENU_EV_UP)
    {
        if (g_display_brightness < MAX_BRIGHTNESS) g_display_brightness++;
        else return;
    }
    else if (event == MENU_EV_DOWN)
    {
        if (g_display_brightness > 0) g_display_brightness--;
        else return;
    }
    else if (event == MENU_EV_NONE)
    {
        //only display value
        item->data.value = g_display_brightness;
        item->data.min = 0;
        item->data.max = 4;
        item->data.step = 1;
    }

    hardware_glcd_brightness(g_display_brightness); 

    //also write to EEPROM
    uint8_t write_buffer = g_display_brightness;
    EEPROM_Write(0, DISPLAY_BRIGHTNESS_ADRESS, &write_buffer, MODE_8_BIT, 1);

    int_to_str((g_display_brightness * 25), str_bfr, 4, 0);

    item->data.value = g_display_brightness;

    strcat(str_bfr, "%");
    item->data.unit_text = str_bfr;
}

void system_display_contrast_cb(void *arg, int event)
{
    menu_item_t *item = arg;
    static char str_bfr[8];

    if (g_display_contrast == -1)
    {
        //read EEPROM
        uint8_t read_buffer = 0;
        EEPROM_Read(0, DISPLAY_CONTRAST_ADRESS, &read_buffer, MODE_8_BIT, 1);

        g_display_contrast = read_buffer;
    }

    if (event == MENU_EV_UP)
    {
        if (g_display_contrast < item->data.max) g_display_contrast++;
        else return;
    }
    else if (event == MENU_EV_DOWN)
    {
        if (g_display_contrast > item->data.min) g_display_contrast--;
        else return;
    }
    else if (event == MENU_EV_NONE)
    {
        //only display value
        item->data.value = g_display_contrast;
        item->data.min = DISPLAY_CONTRAST_MIN;
        item->data.max = DISPLAY_CONTRAST_MAX;
        item->data.step = 1;
    }

    st7565p_set_contrast(hardware_glcds(0), g_display_contrast); 
        
    //also write to EEPROM
    uint8_t write_buffer = g_display_contrast;
    EEPROM_Write(0, DISPLAY_CONTRAST_ADRESS, &write_buffer, MODE_8_BIT, 1);

    int mapped_value = MAP(g_display_contrast, (int)DISPLAY_CONTRAST_MIN, (int)DISPLAY_CONTRAST_MAX, 0, 100);
    int_to_str(mapped_value, str_bfr, 4, 0);

    item->data.value = g_display_contrast;

    strcat(str_bfr, "%");
    item->data.unit_text = str_bfr;
}

void system_hide_actuator_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    if (g_actuator_hide == -1)
    {
        //read EEPROM
        uint8_t read_buffer = 0;
        EEPROM_Read(0, HIDE_ACTUATOR_ADRESS, &read_buffer, MODE_8_BIT, 1);

        g_actuator_hide = read_buffer;

        //write to screen.c
        screen_set_hide_non_assigned_actuators(g_actuator_hide);

        if (!item) return;
    }

    if (event == MENU_EV_ENTER)
    {
        if (g_actuator_hide == 0) g_actuator_hide = 1;
        else g_actuator_hide = 0;
        
        //also write to EEPROM
        uint8_t write_buffer = g_actuator_hide;
        EEPROM_Write(0, HIDE_ACTUATOR_ADRESS, &write_buffer, MODE_8_BIT, 1);

        //write to screen.c
        screen_set_hide_non_assigned_actuators(g_actuator_hide);
    }
    else if (event == MENU_EV_UP)
    {
        g_actuator_hide = 1;
        
        //also write to EEPROM
        uint8_t write_buffer = g_actuator_hide;
        EEPROM_Write(0, HIDE_ACTUATOR_ADRESS, &write_buffer, MODE_8_BIT, 1);

        //write to screen.c
        screen_set_hide_non_assigned_actuators(g_actuator_hide);
    }
    else if (event == MENU_EV_DOWN)
    {
        g_actuator_hide = 0;
        
        //also write to EEPROM
        uint8_t write_buffer = g_actuator_hide;
        EEPROM_Write(0, HIDE_ACTUATOR_ADRESS, &write_buffer, MODE_8_BIT, 1);

        //write to screen.c
        screen_set_hide_non_assigned_actuators(g_actuator_hide);
    }

    item->data.unit_text = g_actuator_hide ? "HIDE" : "SHOW";
    item->data.value = g_actuator_hide;
}

void system_sl_in_cb (void *arg, int event)
{
    menu_item_t *item = arg;

    if (event == MENU_EV_ENTER)
    {
        if (g_sl_in == 0) g_sl_in = 1;
        else g_sl_in = 0;

        set_menu_item_value(MENU_ID_SL_IN, g_sl_in);
    }
    else if (event == MENU_EV_UP)
    {
        g_sl_in = 1;

        set_menu_item_value(MENU_ID_SL_IN, g_sl_in);
    }
    else if (event == MENU_EV_DOWN)
    {
        g_sl_in = 0;

        set_menu_item_value(MENU_ID_SL_IN, g_sl_in);
    }

    //if we toggled to 1, we need to change gain 2 to  gain 1
    char value_bfr[8] = {};
    if ((g_sl_in == 1) && (event != MENU_EV_NONE))
    {
        float_to_str(system_get_gain_value(INP_1_GAIN_ID), value_bfr, 8, 1);
        cli_command("mod-amixer in 0 xvol ", CLI_CACHE_ONLY);
        cli_command(value_bfr, CLI_DISCARD_RESPONSE);

        system_save_gains_cb(NULL, MENU_EV_ENTER);
    }

    if (g_sl_in != -1)
    {
        uint8_t write_buffer = g_sl_in;
        EEPROM_Write(0, SL_INPUT_ADRESS, &write_buffer, MODE_8_BIT, 1);
    }

    item->data.value = g_sl_in;

    if (event != MENU_EV_NONE)
    {
        update_gain_item_value(INP_2_GAIN_ID, system_get_gain_value(INP_1_GAIN_ID));
    }
}

void system_sl_out_cb (void *arg, int event)
{
    menu_item_t *item = arg;

    if (event == MENU_EV_ENTER)
    {
        if (g_sl_out == 0) g_sl_out = 1; 
        else g_sl_out = 0;

        set_menu_item_value(MENU_ID_SL_OUT, g_sl_out);
    }
    if (event == MENU_EV_UP)
    {
        g_sl_out = 1; 
        
        set_menu_item_value(MENU_ID_SL_OUT, g_sl_out);
    }
    else if (event == MENU_EV_DOWN)
    {
        g_sl_out = 0;
        
        set_menu_item_value(MENU_ID_SL_OUT, g_sl_out);
    }

    //also set the gains to the same value
    char value_bfr[8] = {};
    if ((g_sl_out == 1) && (event != MENU_EV_NONE))
    {
        float_to_str(system_get_gain_value(OUTP_1_GAIN_ID), value_bfr, 8, 1);
        cli_command("mod-amixer out 0 xvol ", CLI_CACHE_ONLY);
        cli_command(value_bfr, CLI_DISCARD_RESPONSE);

        system_save_gains_cb(NULL, MENU_EV_ENTER);
    }

    if (g_sl_out != -1)
    {
        uint8_t write_buffer = g_sl_out;
        EEPROM_Write(0, SL_OUTPUT_ADRESS, &write_buffer, MODE_8_BIT, 1);
    }

    item->data.value = g_sl_out;

    if (event != MENU_EV_NONE)
    {
        update_gain_item_value(OUTP_2_GAIN_ID, system_get_gain_value(OUTP_1_GAIN_ID));
    }
}

void system_midi_src_cb (void *arg, int event)
{
    menu_item_t *item = arg;

    if (event == MENU_EV_ENTER)
    {
        if (g_MIDI_clk_src < 2) g_MIDI_clk_src++;
        else g_MIDI_clk_src = 0;
        set_menu_item_value(MENU_ID_MIDI_CLK_SOURCE, g_MIDI_clk_src);
    }
    else if (event == MENU_EV_UP)
    {
        if (g_MIDI_clk_src < 2) g_MIDI_clk_src++;
        set_menu_item_value(MENU_ID_MIDI_CLK_SOURCE, g_MIDI_clk_src);
    }
    else if (event == MENU_EV_DOWN)
    {
        if (g_MIDI_clk_src > 0) g_MIDI_clk_src--;
        set_menu_item_value(MENU_ID_MIDI_CLK_SOURCE, g_MIDI_clk_src);
    }

    //translate the int to string value for the menu
    if (g_MIDI_clk_src == 0) item->data.unit_text ="INTERNAL";
    else if (g_MIDI_clk_src == 1) item->data.unit_text = "MIDI";
    else if (g_MIDI_clk_src == 2) item->data.unit_text ="ABLETON LINK";
}

void system_midi_send_cb (void *arg, int event)
{
    menu_item_t *item = arg;

    if (event == MENU_EV_ENTER)
    {
        if (g_MIDI_clk_send == 0) g_MIDI_clk_send = 1;
        else g_MIDI_clk_send = 0;
        set_menu_item_value(MENU_ID_MIDI_CLK_SEND, g_MIDI_clk_send);
    }
    else if (event == MENU_EV_UP)
    {
        if (g_MIDI_clk_send < 1) g_MIDI_clk_send++;
        set_menu_item_value(MENU_ID_MIDI_CLK_SEND, g_MIDI_clk_send);
    }
    else if (event == MENU_EV_DOWN)
    {
        if (g_MIDI_clk_send > 0) g_MIDI_clk_send--;
        set_menu_item_value(MENU_ID_MIDI_CLK_SEND, g_MIDI_clk_send);
    }

    item->data.value = g_MIDI_clk_send;
}

void system_ss_prog_change_cb (void *arg, int event)
{
    menu_item_t *item = arg;

    if (event == MENU_EV_ENTER)
    {
        set_menu_item_value(MENU_ID_SNAPSHOT_PRGCHGE, item->data.value);
    }
    else if (event == MENU_EV_NONE)
    {
        //set the item value to the snapshot_prog_change since mod-ui is master
        item->data.value = g_snapshot_prog_change;
        item->data.min = 0;
        item->data.max = 16;
        item->data.step = 1;
    }
    else if (event == MENU_EV_UP)
    {
        if (g_snapshot_prog_change < item->data.max) g_snapshot_prog_change++;
        //let mod-ui know
        set_menu_item_value(MENU_ID_SNAPSHOT_PRGCHGE, g_snapshot_prog_change);
    }
    else if (event == MENU_EV_DOWN)
    {
        if (g_snapshot_prog_change > item->data.min) g_snapshot_prog_change--;
        //let mod-ui know
        set_menu_item_value(MENU_ID_SNAPSHOT_PRGCHGE, g_snapshot_prog_change);
    }

    static char str_bfr[8] = {};
    int_to_str(g_snapshot_prog_change, str_bfr, 3, 0);
    //a value of 0 means we turn off
    if (g_snapshot_prog_change == 0) strcpy(str_bfr, "OFF");
    item->data.unit_text = str_bfr;
}

void system_pb_prog_change_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    if (event == MENU_EV_ENTER)
    {
        set_menu_item_value(MENU_ID_PB_PRGCHNGE, item->data.value);
    }
    else if (event == MENU_EV_NONE)
    {
        //set the item value to the snapshot_prog_change since mod-ui is master
        item->data.value = g_pedalboard_prog_change;
        item->data.min = 0;
        item->data.max = 16;
        item->data.step = 1;
    }
    else if (event == MENU_EV_UP)
    {
        if (g_pedalboard_prog_change < item->data.max) g_pedalboard_prog_change++;
        //let mod-ui know
        set_menu_item_value(MENU_ID_PB_PRGCHNGE, g_pedalboard_prog_change);
    }
    else if (event == MENU_EV_DOWN)
    {
        if (g_pedalboard_prog_change > item->data.min) g_pedalboard_prog_change--;
        //let mod-ui know
        set_menu_item_value(MENU_ID_PB_PRGCHNGE, g_pedalboard_prog_change);
    }

    static char str_bfr[8] = {};
    int_to_str(g_pedalboard_prog_change, str_bfr, 3, 0);
    //a value of 0 means we turn off
    if (g_pedalboard_prog_change == 0) strcpy(str_bfr, "OFF");
    item->data.unit_text = str_bfr;
}

//USER PROFILE X (loading)
void system_load_pro_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    //if clicked and YES was selected from the pop-up
    if ((event == MENU_EV_ENTER) && (item->data.hover == 0))
    {
        //current profile is the ID (A=1, B=2, C=3, D=4)
        g_current_profile = item->data.value;
        set_item_value(CMD_PROFILE_LOAD, g_current_profile);
    }

    if (item->data.popup_active)
        return;

    if ((event == MENU_EV_UP) && (item->data.value < item->data.max))
        item->data.value++;
    else if ((event == MENU_EV_DOWN) && (item->data.value > item->data.min))
        item->data.value--;
    else if (event == MENU_EV_NONE)
    {
        //display current profile number
        item->data.value = g_current_profile;
        item->data.min = 1;
        item->data.max = 4;
        item->data.step = 1;
        item->data.list_count = 2;
        item->data.hover = 1;
    }

    //display the current profile
    switch ((int)item->data.value)
    {
        case 1: item->data.unit_text = "[A]"; break;
        case 2: item->data.unit_text = "[B]"; break;
        case 3: item->data.unit_text = "[C]"; break;
        case 4: item->data.unit_text = "[D]"; break;
    }
}

//OVERWRITE CURRENT PROFILE
void system_save_pro_cb(void *arg, int event)
{
   menu_item_t *item = arg;

    //if clicked and YES was selected from the pop-up
    if ((event == MENU_EV_ENTER) && (item->data.hover == 0))
        set_item_value(CMD_PROFILE_STORE, item->data.value);

    if (item->data.popup_active)
        return;

    if ((event == MENU_EV_UP) && (item->data.value < item->data.max))
        item->data.value++;
    else if ((event == MENU_EV_DOWN) && (item->data.value > item->data.min))
        item->data.value--;
    else if (event == MENU_EV_NONE)
    {
        //display current profile number
        item->data.value = 1;
        item->data.min = 1;
        item->data.max = 4;
        item->data.step = 1;
        item->data.list_count = 2;
        item->data.hover = 1;
    }

    //display the current profile
    switch ((int)item->data.value)
    {
        case 1: item->data.unit_text = "[A]"; break;
        case 2: item->data.unit_text = "[B]"; break;
        case 3: item->data.unit_text = "[C]"; break;
        case 4: item->data.unit_text = "[D]"; break;
    }
}

void system_shift_item_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    if (g_shift_item[item->desc->id - SHIFT_ITEMS_ID - 1] == -1)
    {
        //read EEPROM
        uint8_t read_buffer = 0;
        EEPROM_Read(0, SHIFT_ITEM_ADRESS + item->desc->id - SHIFT_ITEMS_ID - 1, &read_buffer, MODE_8_BIT, 1);

        if (read_buffer < SHIFT_MENU_ITEMS_COUNT)
            g_shift_item[item->desc->id - SHIFT_ITEMS_ID - 1] = read_buffer;
        else
            g_shift_item[item->desc->id - SHIFT_ITEMS_ID - 1] = 0;
    }

    if (event == MENU_EV_ENTER)
    {
        if (g_shift_item[item->desc->id - SHIFT_ITEMS_ID - 1] < item->data.max) g_shift_item[item->desc->id - SHIFT_ITEMS_ID - 1]++;
        else g_shift_item[item->desc->id - SHIFT_ITEMS_ID - 1] = 0;
    }
    else if (event == MENU_EV_UP)
    {
        if (g_shift_item[item->desc->id - SHIFT_ITEMS_ID - 1] < item->data.max) g_shift_item[item->desc->id - SHIFT_ITEMS_ID - 1]++;
        else return;
    }
    else if (event == MENU_EV_DOWN)
    {
        if (g_shift_item[item->desc->id - SHIFT_ITEMS_ID - 1] > 0) g_shift_item[item->desc->id - SHIFT_ITEMS_ID - 1]--;
        else return;
    }
    else if (event == MENU_EV_NONE)
    {
        //only display value
        item->data.value = g_shift_item[item->desc->id - SHIFT_ITEMS_ID - 1];
        item->data.min = 0;
        item->data.max = SHIFT_MENU_ITEMS_COUNT-1;
        item->data.step = 1;
    }

    //also write to EEPROM
    uint8_t write_buffer = g_shift_item[item->desc->id - SHIFT_ITEMS_ID - 1];
    EEPROM_Write(0, SHIFT_ITEM_ADRESS + item->desc->id - SHIFT_ITEMS_ID - 1, &write_buffer, MODE_8_BIT, 1);

    item->data.value = g_shift_item[item->desc->id - SHIFT_ITEMS_ID - 1];

    //add item to text
    item->data.unit_text = TM_get_menu_item_by_ID(SHIFT_ITEM_IDS[(int)item->data.value])->name;
}

void system_default_tool_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    if (g_default_tool == -1)
    {
        //read EEPROM
        uint8_t read_buffer = 0;
        EEPROM_Read(0, DEFAULT_TOOL_ADRESS, &read_buffer, MODE_8_BIT, 1);

        g_default_tool = read_buffer;

        TM_set_first_foot_tool(g_default_tool+1);

        if (!item) return;
    }

    if (event == MENU_EV_ENTER)
    {
        if (g_default_tool < FOOT_TOOL_AMOUNT-1) g_default_tool++;
        else g_default_tool = 0;
    }
    else if (event == MENU_EV_UP)
    {
        if (g_default_tool < FOOT_TOOL_AMOUNT-1)
            g_default_tool += item->data.step;
    }
    else if (event == MENU_EV_DOWN)
    {
        if (g_default_tool > 0)
            g_default_tool -= item->data.step;
    }
    else if (event == MENU_EV_NONE)
    {
        //only display value
        item->data.value = g_default_tool;
        item->data.min = 0;
        item->data.max = FOOT_TOOL_AMOUNT;
        item->data.step = 1;
    }

    TM_set_first_foot_tool(g_default_tool+1);

    //also write to EEPROM
    uint8_t write_buffer = g_default_tool;
    EEPROM_Write(0, DEFAULT_TOOL_ADRESS, &write_buffer, MODE_8_BIT, 1);

    item->data.value = g_default_tool;
    
    switch ((int)item->data.value)
    {
        case 0: item->data.unit_text = "TUNER"; break;
        case 1: item->data.unit_text = "TEMPO"; break;
        //case 3: item->data.unit_text = "[C]"; break;
        //case 4: item->data.unit_text = "[D]"; break;
    }
}

void system_list_mode_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    if (g_list_mode == -1)
    {
        //read EEPROM
        uint8_t read_buffer = 0;
        EEPROM_Read(0, LIST_MODE_ADRESS, &read_buffer, MODE_8_BIT, 1);

        g_list_mode = read_buffer;
    }

    if (event == MENU_EV_ENTER)
        g_list_mode = !g_list_mode;
    else if (event == MENU_EV_UP)
        g_list_mode = 1;
    else if (event == MENU_EV_DOWN)
        g_list_mode = 0;
    else if (event == MENU_EV_NONE)
    {
        //only display value
        item->data.value = g_list_mode;
        item->data.min = 0;
        item->data.max = 1;
        item->data.step = 1;
    }

    //hardware_glcd_brightness(g_display_brightness); 

    //also write to EEPROM
    uint8_t write_buffer = g_list_mode;
    EEPROM_Write(0, LIST_MODE_ADRESS, &write_buffer, MODE_8_BIT, 1);

    item->data.value = g_list_mode;

    if (g_list_mode)
        item->data.unit_text = "Click to load";
    else
        item->data.unit_text = "load on select";
}

void system_control_header_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    if (g_control_header == -1)
    {
        //read EEPROM
        uint8_t read_buffer = 0;
        EEPROM_Read(0, CONTROL_HEADER_ADRESS, &read_buffer, MODE_8_BIT, 1);

        g_control_header = read_buffer;

        screen_set_control_mode_header(g_control_header); 

        if (!item) return;
    }

    if (event == MENU_EV_ENTER)
        g_control_header = !g_control_header;
    else if (event == MENU_EV_UP)
        g_control_header = 1;
    else if (event == MENU_EV_DOWN)
        g_control_header = 0;
    else if (event == MENU_EV_NONE)
    {
        //only display value
        item->data.value = g_control_header;
        item->data.min = 0;
        item->data.max = 1;
        item->data.step = 1;
    }

    screen_set_control_mode_header(g_control_header); 

    //also write to EEPROM
    uint8_t write_buffer = g_control_header;
    EEPROM_Write(0, CONTROL_HEADER_ADRESS, &write_buffer, MODE_8_BIT, 1);

    item->data.value = g_control_header;
    
    if (g_control_header)
        item->data.unit_text = "Snapshot name";
    else
        item->data.unit_text = "Pedalboard name";
}

//Callbacks below are not part of the device menu
void system_tuner_mute_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    if (event == MENU_EV_NONE)
    {
        item->data.value = g_tuner_mute;
    }
    else if (event == MENU_EV_ENTER)
    {
        if (item->data.value == 0) item->data.value = 1;
        else item->data.value = 0;

        g_tuner_mute = item->data.value;

        set_menu_item_value(MENU_ID_TUNER_MUTE, g_tuner_mute);
    }
}

void system_tuner_input_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    if (event == MENU_EV_NONE)
    {
        item->data.value = g_tuner_mute;
    }
    else if (event == MENU_EV_ENTER)
    {
        if (item->data.value == 0) item->data.value = 1;
        else item->data.value = 0;

        g_tuner_input = item->data.value;

        ui_comm_webgui_set_response_cb(NULL, NULL);

        char buffer[40];
        memset(buffer, 0, 20);
        uint8_t i;

        i = copy_command(buffer, CMD_TUNER_INPUT); 

        //insert the input
        i += int_to_str(g_tuner_input + 1, &buffer[i], sizeof(buffer) - i, 0);

        g_protocol_busy = true;
        system_lock_comm_serial(g_protocol_busy);

        // sends the data to GUI
        ui_comm_webgui_send(buffer, i);

        // waits the pedalboards list be received
        ui_comm_webgui_wait_response();

        g_protocol_busy = false;
        system_lock_comm_serial(g_protocol_busy);
    }
}

void system_play_cb (void *arg, int event)
{
    menu_item_t *item = arg;

    if (event == MENU_EV_NONE)
    {
        item->data.value = g_play_status;
    }
    else if (event == MENU_EV_ENTER)
    {
        if (item->data.value == 0) item->data.value = 1;
        else item->data.value = 0;

        g_play_status = item->data.value;

        set_menu_item_value(MENU_ID_PLAY_STATUS, g_play_status);
    }
}

void system_tempo_cb (void *arg, int event)
{
    menu_item_t *item = arg;

    if ((event == MENU_EV_NONE) || (event == MENU_EV_ENTER))
    {
        item->data.value = g_beats_per_minute;
        item->data.min = 20;
        item->data.max = 280;
        item->data.step = 1;
    }
    //scrolling up/down
    else if ((event == MENU_EV_UP) && (g_MIDI_clk_src != 1))
    {
        item->data.value += item->data.step;

        if (item->data.value > item->data.max)
            item->data.value = item->data.max;

        g_beats_per_minute = item->data.value;

        //let mod-ui know
        set_menu_item_value(MENU_ID_TEMPO, g_beats_per_minute);
    }
    else if ((event == MENU_EV_DOWN) && (g_MIDI_clk_src != 1))
    {
        item->data.value -= item->data.step;

        if (item->data.value < item->data.min)
            item->data.value = item->data.min;

        g_beats_per_minute = item->data.value;

        //let mod-ui know
        set_menu_item_value(MENU_ID_TEMPO, g_beats_per_minute);
    }

    static char str_bfr[12] = {};
    int_to_str(g_beats_per_minute, str_bfr, 4, 0);
    strcat(str_bfr, " BPM");
    item->data.unit_text = str_bfr;
}

void system_bpb_cb (void *arg, int event)
{
    menu_item_t *item = arg;

    if ((event == MENU_EV_NONE) || (event == MENU_EV_ENTER))
    {
        item->data.value = g_beats_per_bar;
        item->data.min = 1;
        item->data.max = 16;
        item->data.step = 1;
    }
    //scrolling up/down
    else if (event == MENU_EV_UP)
    {
        if (item->data.value < item->data.max) item->data.value++;
        g_beats_per_bar = item->data.value;

        //let mod-ui know
        set_menu_item_value(MENU_ID_BEATS_PER_BAR, g_beats_per_bar);
    }
    else if (event == MENU_EV_DOWN)
    {
        if (item->data.value > item->data.min) item->data.value--;
        g_beats_per_bar = item->data.value;

        //let mod-ui know
        set_menu_item_value(MENU_ID_BEATS_PER_BAR, g_beats_per_bar);
    }

    //add the items to the 
    static char str_bfr[8] = {};
    int_to_str(g_beats_per_bar, str_bfr, 4, 0);
    strcat(str_bfr, "/4");
    item->data.unit_text = str_bfr;
}

void system_taptempo_cb (void *arg, int event)
{
    menu_item_t *item = arg;
    uint32_t now, delta;

    if (event == MENU_EV_NONE)
    {
        item->data.value = g_beats_per_minute;
        item->data.min = 20;
        item->data.max = 280;
        item->data.step = 1;

        // calculates the maximum tap tempo value
        uint32_t max = (uint32_t)(convert_to_ms("bpm", item->data.min) + 0.5);

        //makes sure we enforce a proper timeout
        if (max > TAP_TEMPO_DEFAULT_TIMEOUT)
            max = TAP_TEMPO_DEFAULT_TIMEOUT;

        g_tool_tap_tempo.max = max;
    }
    else if ((event == MENU_EV_ENTER) && (g_MIDI_clk_src != 1))
    {
        now = actuator_get_click_time(hardware_actuators(FOOTSWITCH1));
        delta = now - g_tool_tap_tempo.time;
        g_tool_tap_tempo.time = now;

        // checks if delta almost suits maximum allowed value
        if ((delta > g_tool_tap_tempo.max) &&
            ((delta - TAP_TEMPO_MAXVAL_OVERFLOW) < g_tool_tap_tempo.max))
        {
            // sets delta to maxvalue if just slightly over, instead of doing nothing
            delta = g_tool_tap_tempo.max;
        }

        // checks the tap tempo timeout
        if (delta <= g_tool_tap_tempo.max)
        {
            //get current value of tap tempo in ms
            float currentTapVal = convert_to_ms("bpm", g_beats_per_minute);
            //check if it should be added to running average
            if (fabs(currentTapVal - delta) < TAP_TEMPO_TAP_HYSTERESIS)
            {
                // converts and update the tap tempo value
                item->data.value = (2*(item->data.value) + convert_from_ms("bpm", delta)) / 3;
            }
            else
            {
                // converts and update the tap tempo value
                item->data.value = convert_from_ms("bpm", delta);
            }
            // checks the values bounds
            if (item->data.value > item->data.max) item->data.value = item->data.max;
            if (item->data.value < item->data.min) item->data.value = item->data.min;

            // updates the foot
            g_beats_per_minute = item->data.value;
        }

        set_menu_item_value(MENU_ID_TEMPO, g_beats_per_minute);
    }
    
    static char str_bfr[12] = {};
    int_to_str(g_beats_per_minute, str_bfr, 4, 0);
    item->data.unit_text = str_bfr;
}

/*
void system_bypass_cb (void *arg, int event)
{
    menu_item_t *item = arg; 

    //0=in1, 1=in2, 2=in1&2
    switch (item->desc->id)
    {
        //in 1
        case BP1_ID:
            //we need to toggle the bypass
            if (event == MENU_EV_ENTER)
            {
                //we toggle the bypass 
                g_bypass[0] = !g_bypass[0];
                set_menu_item_value(MENU_ID_BYPASS1, g_bypass[0]);
            }
            add_chars_to_menu_name(item, (g_bypass[0]? option_enabled : option_disabled));
        break;

        //in2
        case BP2_ID:
            //we need to toggle the bypass
            if (event == MENU_EV_ENTER)
            {
                //we toggle the bypass 
                g_bypass[1] = !g_bypass[1];
                set_menu_item_value(MENU_ID_BYPASS2, g_bypass[1]);
            }
            add_chars_to_menu_name(item, (g_bypass[1]? option_enabled : option_disabled));
        break;

        case BP12_ID:
            if (event == MENU_EV_ENTER)
            {
                //toggle the bypasses
                g_bypass[2] = !g_bypass[2];
                set_menu_item_value(MENU_ID_BYPASS1, g_bypass[2]);
                set_menu_item_value(MENU_ID_BYPASS2, g_bypass[2]);
                g_bypass[0] = g_bypass[2];
                g_bypass[1] = g_bypass[2];
            }
            add_chars_to_menu_name(item, (g_bypass[2]? option_enabled : option_disabled));
        break;
    }

    //if both are on after a change we need to change bypass 1&2 as well
    if (g_bypass[0] && g_bypass[1])
    {
        g_bypass[2] = 1;
    }
    else g_bypass[2] = 0;

    //other items can change because of this, update the whole menu on the right, and left because of the quick bypass
    if (event == MENU_EV_ENTER)
    {
        TM_menu_refresh(DISPLAY_LEFT);
        TM_menu_refresh(DISPLAY_RIGHT);
    }
}*/

void system_usb_mode_cb(void *arg, int event)
{
    menu_item_t *item = arg;

    //first time, fetch value
    if (g_usb_mode == -1)
    {
        sys_comm_set_response_cb(recieve_sys_value, item);

        sys_comm_send(CMD_SYS_USB_MODE, NULL);
        sys_comm_wait_response();

        g_usb_mode = item->data.value;
        item->data.min = 0;
        item->data.max = 2;
        item->data.step = 1;
        item->data.list_count = 2;
        item->data.hover = 1;
    }

    //if clicked and YES was selected from the pop-up
    if ((event == MENU_EV_ENTER) && (item->data.hover == 0))
    {
        g_usb_mode = item->data.value;

        //set the value
        char str_buf[8];
        int_to_str(item->data.value, str_buf, 4, 0);
        str_buf[1] = 0;

        //send value
        sys_comm_send(CMD_SYS_USB_MODE, str_buf);
        sys_comm_wait_response();

        //tell the system to reboot
        sys_comm_send(CMD_SYS_REBOOT, NULL);
    }

    if (item->data.popup_active)
        return;

    if ((event == MENU_EV_UP) && (item->data.value < item->data.max))
        item->data.value++;
    else if ((event == MENU_EV_DOWN) && (item->data.value > item->data.min))
        item->data.value--;

    //display the current profile
    switch ((int)item->data.value)
    {
        case 0: item->data.unit_text = "NETWORK (DEFAULT)"; break;
        case 1: item->data.unit_text = "NET+MIDI"; break;
        case 2: item->data.unit_text = "NET+MIDI (WINDOWS)"; break;
    }
}