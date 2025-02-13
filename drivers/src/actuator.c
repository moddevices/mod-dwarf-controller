
/*
 * Note: Debounce algorithm by Jack J. Ganssle - "A Guide to Debouncing"
 */

/*
*********************************************************************************************************
*   INCLUDE FILES
*********************************************************************************************************
*/

#include "actuator.h"
#include "hardware.h"

/*
*********************************************************************************************************
*   LOCAL DEFINES
*********************************************************************************************************
*/

#define BUTTONS_FLAGS \
    (EV_BUTTON_CLICKED | EV_BUTTON_PRESSED | EV_BUTTON_RELEASED | EV_BUTTON_HELD | EV_BUTTON_PRESSED_DOUBLE)

#define ENCODERS_FLAGS \
    (EV_BUTTON_CLICKED | EV_BUTTON_PRESSED | EV_BUTTON_RELEASED | EV_BUTTON_HELD | \
     EV_ENCODER_TURNED | EV_ENCODER_TURNED_CW | EV_ENCODER_TURNED_ACW)

#define TRIGGER_FLAGS \
    (EV_BUTTON_CLICKED | EV_BUTTON_HELD | \
     EV_ENCODER_TURNED | EV_ENCODER_TURNED_CW | EV_ENCODER_TURNED_ACW)

#define TOGGLE_FLAGS \
    (EV_BUTTON_PRESSED | EV_BUTTON_RELEASED)

#define BUTTON_ON_FLAG      0x01
#define ENCODER_CHA_FLAG    0x02
#define ENCODER_INIT_FLAG   0x04
#define CLICK_CANCEL_FLAG   0x10


/*
*********************************************************************************************************
*   LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*   LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*   LOCAL MACROS
*********************************************************************************************************
*/

#define SET_FLAG(status,flag)       (status |= flag)
#define CLR_FLAG(status,flag)       (status &= ~flag)
#define ACTUATOR_TYPE(act)          (((button_t *)(act))->type)
#define ABS(num)                    (num >= 0 ? num : -num)

/*
*********************************************************************************************************
*   LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

static void *g_actuators_pointers[MAX_ACTUATORS];
static uint8_t g_actuators_count = 0;

/*
*********************************************************************************************************
*   LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*   LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*   LOCAL FUNCTIONS
*********************************************************************************************************
*/

static void event(void *actuator, uint8_t flags)
{
    button_t *button = (button_t *) actuator;
    encoder_t *encoder = (encoder_t *) actuator;
    uint8_t status;

    switch (ACTUATOR_TYPE(actuator))
    {
        case BUTTON:
            if (button->event && (button->events_flags & flags))
            {
                status = button->status;
                button->status &= button->events_flags;
                button->event(button);
                button->status = status & ~(button->events_flags & (flags & TRIGGER_FLAGS));
            }
            break;

        case ROTARY_ENCODER:
            if (encoder->event && (encoder->events_flags & flags))
            {
                status = encoder->status;
                encoder->status &= encoder->events_flags;
                encoder->event(encoder);
                encoder->status = status & ~(encoder->events_flags & (flags & TRIGGER_FLAGS));
            }
            break;
    }
}


/*
*********************************************************************************************************
*   GLOBAL FUNCTIONS
*********************************************************************************************************
*/

void actuator_create(actuator_type_t type, uint8_t id, void *actuator)
{
    button_t *button = (button_t *) actuator;
    encoder_t *encoder = (encoder_t *) actuator;

    switch (type)
    {
        case BUTTON:
            button->id = id;
            button->type = type;
            button->event = 0;
            button->events_flags = 0;
            button->hold_time = 0;
            button->hold_time_counter = 0;
            button->debounce = 0;
            button->control = 0;
            button->status = 0;
            button->hardware_press_time = 0;
            button->last_pressed_time = 0;
            button->last_pressed_time_counter = 0;
            button->double_press_button_id = NO_DOUBLE_PRESS_LINK;
            break;

        case ROTARY_ENCODER:
            encoder->id = id;
            encoder->type = type;
            encoder->event = 0;
            encoder->events_flags = 0;
            encoder->hold_time = 0;
            encoder->hold_time_counter = 0;
            encoder->debounce = 0;
            encoder->control = 0;
            encoder->status = 0;
            encoder->steps = 0;
            encoder->counter = 0;
            break;
    }

    // store the actuator pointer
    g_actuators_pointers[g_actuators_count] = actuator;
    g_actuators_count++;
}


void actuator_set_pins(void *actuator, const uint8_t *pins)
{
    button_t *button = (button_t *) actuator;
    encoder_t *encoder = (encoder_t *) actuator;

    switch (ACTUATOR_TYPE(actuator))
    {
        case BUTTON:
            button->port = pins[0];
            button->pin = pins[1];
            CONFIG_PIN_INPUT(button->port, button->pin);
            break;

        case ROTARY_ENCODER:
            button->port = pins[0];
            button->pin = pins[1];
            CONFIG_PIN_INPUT(button->port, button->pin);
            encoder->port_chA = pins[2];
            encoder->pin_chA = pins[3];
            encoder->port_chB = pins[4];
            encoder->pin_chB = pins[5];
            CONFIG_PIN_INPUT(encoder->port_chA, encoder->pin_chA);
            CONFIG_PIN_INPUT(encoder->port_chB, encoder->pin_chB);
            break;
    }
}


void actuator_set_prop(void *actuator, actuator_prop_t prop, uint16_t value)
{
    button_t *button = (button_t *) actuator;
    encoder_t *encoder = (encoder_t *) actuator;

    switch (ACTUATOR_TYPE(actuator))
    {
        case BUTTON:
            if (prop == BUTTON_HOLD_TIME)
            {
                button->hold_time = value;
                button->hold_time_counter = value / CLOCK_PERIOD;
            }
            else if (prop == BUTTON_DOUBLE_TIME)
            {
                button->last_pressed_time = value;
                button->last_pressed_time_counter = value / CLOCK_PERIOD;
            }
            break;

        case ROTARY_ENCODER:
            if (prop == BUTTON_HOLD_TIME)
            {
                encoder->hold_time = value;
                encoder->hold_time_counter = value / CLOCK_PERIOD;
            }
            else if (prop == ENCODER_STEPS)
            {
                encoder->steps = value;
            }
            break;
    }
}

void actuator_set_link(void *actuator, uint8_t linked_actuator_id)
{
    if (ACTUATOR_TYPE(actuator) != BUTTON)
        return;

    button_t *button = (button_t *) actuator;
    button->double_press_button_id = linked_actuator_id;

    button_t *other_button;
    other_button = (button_t *) g_actuators_pointers[button->double_press_button_id];

    //configure other button constants
    other_button->double_press_button_id = DOUBLE_PRESSED_LINKED;

    //we need to copy over the double press time to the other actuator if not set manually
    if (other_button->last_pressed_time == 0)
    {
       other_button->last_pressed_time = button->last_pressed_time;
       other_button->last_pressed_time_counter = button->last_pressed_time_counter;
    }
}

void actuator_enable_event(void *actuator, uint8_t events_flags)
{
    switch (ACTUATOR_TYPE(actuator))
    {
        case BUTTON:
            ((button_t *)actuator)->events_flags = events_flags;
            break;

        case ROTARY_ENCODER:
            ((encoder_t *)actuator)->events_flags = events_flags;
            break;
    }
}


void actuator_set_event(void *actuator, void (*callback)(void *actuator))
{
    switch (ACTUATOR_TYPE(actuator))
    {
        case BUTTON:
            ((button_t *)actuator)->event = callback;
            break;

        case ROTARY_ENCODER:
            ((encoder_t *)actuator)->event = callback;
            break;
    }
}

uint8_t actuator_get_status(void *actuator)
{
    uint8_t status = 0;
    button_t *button = (button_t *) actuator;
    encoder_t *encoder = (encoder_t *) actuator;

    switch (ACTUATOR_TYPE(actuator))
    {
        case BUTTON:
            status = button->status;
            CLR_FLAG(button->status, TRIGGER_FLAGS);
            break;

        case ROTARY_ENCODER:
            status = encoder->status;
            CLR_FLAG(encoder->status, TRIGGER_FLAGS);
            break;
    }

    return status;
}

uint32_t actuator_get_click_time(void *actuator)
{
    uint32_t time = 0;
    button_t *button = (button_t *) actuator;
    //encoder_t *encoder = (encoder_t *) actuator;

    switch (ACTUATOR_TYPE(actuator))
    {
        case BUTTON:
            time = button->hardware_press_time;
            break;

        //we dont track click time here
        case ROTARY_ENCODER:
            break;
    }

    return time;
}

void actuators_clock(void)
{
    button_t *button;
    encoder_t *encoder;
    uint8_t i, button_on;

    for (i = 0; i < g_actuators_count; i++)
    {
        button = (button_t *) g_actuators_pointers[i];
        encoder = (encoder_t *) g_actuators_pointers[i];

        switch (ACTUATOR_TYPE(g_actuators_pointers[i]))
        {
            case BUTTON:
            {
                if (READ_PIN(button->port, button->pin) == BUTTON_ACTIVATED) button_on = BUTTON_ON_FLAG;
                else button_on = 0;

                button_t *other_button = NULL;
                
                if (button->double_press_button_id > 0)
                    other_button = (button_t *) g_actuators_pointers[button->double_press_button_id];

                // button on same state
                if (button_on == (button->control & BUTTON_ON_FLAG))
                {
                    // button pressed
                    if (button_on)
                    {
                        // reset debounce counter
                        button->debounce = BUTTON_RELEASE_DEBOUNCE / CLOCK_PERIOD;

                        // button hold
                        if (button->hold_time_counter > 0)
                        {
                            button->hold_time_counter--;

                            if (button->hold_time_counter == 0)
                            {
                                CLR_FLAG(button->status, EV_BUTTON_PRESSED);
                                CLR_FLAG(button->status, EV_BUTTON_RELEASED);
                                CLR_FLAG(button->status, EV_BUTTON_PRESSED_DOUBLE);
                                SET_FLAG(button->status, EV_BUTTON_HELD);
                                SET_FLAG(button->control, CLICK_CANCEL_FLAG);

                                event(button, EV_BUTTON_HELD);
                            }
                        }

                        //we passed the debounce, now we need to wait for the timer of the double press to be ready before we can set an event
                        if (button->last_pressed_time_counter > 0)
                        {
                            button->last_pressed_time_counter--;

                            //check if we need to toggle double press
                            if (button->double_press_button_id > 0)
                            {
                                if (button_on == (other_button->control & BUTTON_ON_FLAG))
                                {
                                    //double press
                                    CLR_FLAG(button->status, EV_BUTTON_RELEASED);
                                    CLR_FLAG(button->status, EV_BUTTON_PRESSED);
                                    SET_FLAG(button->status, EV_BUTTON_PRESSED_DOUBLE);

                                    event(button, EV_BUTTON_PRESSED_DOUBLE); 

                                    //make sure we only trigger once, and no pressed action
                                    button->last_pressed_time_counter = -1;

                                    return;
                                }
                            }

                            //timeout
                            if (button->last_pressed_time_counter <= 0)
                            {
                                //no action when the button is linked and the other one is pressed
                                if (button->double_press_button_id == DOUBLE_PRESSED_LOCKED)
                                    return;

                                // update status flags
                                CLR_FLAG(button->status, EV_BUTTON_RELEASED);
                                SET_FLAG(button->status, EV_BUTTON_PRESSED);

                                event(button, EV_BUTTON_PRESSED);
                            }
                        }
                    }
                    // button released
                    else
                    {
                        // reset debounce counter
                        button->debounce = BUTTON_PRESS_DEBOUNCE / CLOCK_PERIOD;

                        // reload hold time counter
                        if (button->hold_time > 0)
                            button->hold_time_counter = button->hold_time / CLOCK_PERIOD;

                        // reload double press time counter
                        if (button->last_pressed_time > 0)
                            button->last_pressed_time_counter = button->last_pressed_time / CLOCK_PERIOD;
                    }
                }
                // button state change
                else
                {
                    button->debounce--;
                    if (button->debounce == 0) // debounce OK
                    {
                        // update control flags
                        CLR_FLAG(button->control, BUTTON_ON_FLAG);
                        button->control |= button_on;

                        // button pressed
                        if (button_on)
                        {
                            //keep time for tap tempo
                            button->hardware_press_time = hardware_timestamp();

                            if (button->double_press_button_id == NO_DOUBLE_PRESS_LINK)
                            {
                                CLR_FLAG(button->status, EV_BUTTON_RELEASED);
                                SET_FLAG(button->status, EV_BUTTON_PRESSED);

                                event(button, EV_BUTTON_PRESSED); 
                            }
                            else if (button->double_press_button_id >= 0)
                            {
                                button_t *group_button;
                                group_button = (button_t *) g_actuators_pointers[button->double_press_button_id];

                                group_button->double_press_button_id = DOUBLE_PRESSED_LOCKED;
                            }

                            // reload debounce counter
                            button->debounce = BUTTON_RELEASE_DEBOUNCE / CLOCK_PERIOD;
                        }
                        // button released
                        else
                        {
                            // update status flags
                            CLR_FLAG(button->status, EV_BUTTON_PRESSED);
                            CLR_FLAG(button->status, EV_BUTTON_PRESSED_DOUBLE);
                            SET_FLAG(button->status, EV_BUTTON_RELEASED);

                            // check if must set click flag
                            if (!(button->control & CLICK_CANCEL_FLAG))
                            {
                                SET_FLAG(button->status, EV_BUTTON_CLICKED);
                            }

                            CLR_FLAG(button->control, CLICK_CANCEL_FLAG);

                            event(button, EV_BUTTON_RELEASED | EV_BUTTON_CLICKED);

                            // reload debounce counter
                            button->debounce = BUTTON_PRESS_DEBOUNCE / CLOCK_PERIOD;

                            //make linked button available again if applicable
                            if (button->double_press_button_id >= 0)
                            {
                                button_t *group_button;
                                group_button = (button_t *) g_actuators_pointers[button->double_press_button_id];
                                group_button->double_press_button_id = DOUBLE_PRESSED_LINKED;
                            }
                        }
                    }
                }
                break;
            }

            case ROTARY_ENCODER:
                // --- button processing ---

                // read button pin
                if (READ_PIN(encoder->port, encoder->pin) == ENCODER_ACTIVATED) button_on = BUTTON_ON_FLAG;
                else button_on = 0;

                // button on same state
                if (button_on == (encoder->control & BUTTON_ON_FLAG))
                {
                    // button pressed
                    if (button_on)
                    {
                        // reset debounce counter
                        encoder->debounce =  ENCODER_RELEASE_DEBOUNCE / CLOCK_PERIOD;

                        // button hold
                        if (encoder->hold_time_counter > 0)
                        {
                            encoder->hold_time_counter--;
                            if (encoder->hold_time_counter == 0)
                            {
                                SET_FLAG(encoder->status, EV_BUTTON_HELD);
                                SET_FLAG(encoder->status, EV_BUTTON_PRESSED);

                                event(button, EV_BUTTON_HELD | EV_BUTTON_PRESSED);
                            }
                        }
                    }
                    // button released
                    else
                    {
                        // reset debounce counter
                        encoder->debounce = ENCODER_PRESS_DEBOUNCE / CLOCK_PERIOD;

                        // reload hold time counter
                        encoder->hold_time_counter = encoder->hold_time / CLOCK_PERIOD;
                    }
                }
                // button state change
                else
                {
                    encoder->debounce--;
                    if (encoder->debounce == 0) // debounce OK
                    {
                        // update control flags
                        CLR_FLAG(encoder->control, BUTTON_ON_FLAG);
                        encoder->control |= button_on;

                        // button pressed
                        if (button_on)
                        {
                            // update status flags
                            CLR_FLAG(encoder->status, EV_BUTTON_RELEASED);
                            SET_FLAG(encoder->status, EV_BUTTON_PRESSED);

                            event(button, EV_BUTTON_PRESSED);

                            // reload debounce counter
                            encoder->debounce = ENCODER_RELEASE_DEBOUNCE / CLOCK_PERIOD;
                        }
                        // button released
                        else
                        {
                            // update status flags
                            CLR_FLAG(encoder->status, EV_BUTTON_PRESSED);
                            SET_FLAG(encoder->status, EV_BUTTON_RELEASED);

                            // check if must set click flag
                            if (!(encoder->control & CLICK_CANCEL_FLAG))
                            {
                                SET_FLAG(encoder->status, EV_BUTTON_CLICKED);
                            }

                            CLR_FLAG(encoder->control, CLICK_CANCEL_FLAG);

                            event(button, EV_BUTTON_RELEASED | EV_BUTTON_CLICKED);

                            // reload debounce counter
                            encoder->debounce = ENCODER_PRESS_DEBOUNCE / CLOCK_PERIOD;
                        }
                    }
                }
                // --- rotary processing ---
                // encoder algorithm from PaulStoffregen
                // https://github.com/PaulStoffregen/Encoder
                uint8_t seq = encoder->state & 3;

                seq |= READ_PIN(encoder->port_chA, encoder->pin_chA) ? 4 : 0;
                seq |= READ_PIN(encoder->port_chB, encoder->pin_chB) ? 8 : 0;

                switch (seq)
                {
                    // these ones are sent quite often during normal operation
                    case 0: case 5: case 10: case 15:
                        break;

                    // 1 step up
                    case 1: case 7: case 8: case 14:
                        if (encoder->counter > 0) // this fixes inverting direction
                            encoder->counter = 0;
                        encoder->counter--;
                        break;

                    // 1 step down
                    case 2: case 4: case 11: case 13:
                        if (encoder->counter < 0) // this fixes inverting direction
                            encoder->counter = 0;
                        encoder->counter++;
                        break;

                    // 2 steps up
                    case 3: case 12:
                        encoder->counter -= 2;
                        break;

                    // 2 steps down
                    case 6: case 9:
                        encoder->counter += 2;
                        break;

                    // default should never trigger (because math)
                    default:
                        continue;
                }

                encoder->state = (seq >> 2);

                // checks the steps
                if (ABS(encoder->counter) >= encoder->steps)
                {
                    // update flags
                    CLR_FLAG(encoder->status, EV_ENCODER_TURNED_CW);
                    CLR_FLAG(encoder->status, EV_ENCODER_TURNED_ACW);
                    SET_FLAG(encoder->status, EV_ENCODER_TURNED);

                    // set the direction flag
                    if (encoder->counter > 0)
                        SET_FLAG(encoder->status, EV_ENCODER_TURNED_CW);
                    else
                        SET_FLAG(encoder->status, EV_ENCODER_TURNED_ACW);

                    event(encoder, EV_ENCODER_TURNED | EV_ENCODER_TURNED_CW | EV_ENCODER_TURNED_ACW | EV_BUTTON_HELD);

                    encoder->counter = 0;
                }
            break;
        }
    }
}
