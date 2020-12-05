
/*
************************************************************************************************************************
*
************************************************************************************************************************
*/

#ifndef MODE_NAVIGATION_H
#define MODE_NAVIGATION_H


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

void NM_init(void);
void NM_initial_state(uint16_t max_menu, uint16_t page_min, uint16_t page_max, char *bank_uid, char *pedalboard_uid, char **pedalboards_list);
void NM_enter(void);
void NM_up(void);
void NM_down(void);
void NM_set_banks(bp_list_t *bp_list);
bp_list_t *NM_get_banks(void);
void NM_set_pedalboards(bp_list_t *bp_list);
bp_list_t *NM_get_pedalboards(void);
char* NM_get_current_pb_name(void);
void NM_update(void);
int NM_need_update(void);
void NM_print_screen();

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
