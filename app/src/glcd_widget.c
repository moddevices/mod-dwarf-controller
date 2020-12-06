
/*
************************************************************************************************************************
*           INCLUDE FILES
************************************************************************************************************************
*/

#include "glcd_widget.h"
#include "utils.h"
#include "mod-protocol.h"

#include <math.h>
#include <string.h>

/*
************************************************************************************************************************
*           LOCAL DEFINES
************************************************************************************************************************
*/

#define GRAPH_WIDTH         128
#define GRAPH_HEIGHT        32
#define GRAPH_BAR_WIDTH     3
#define GRAPH_BAR_SPACE     1
#define GRAPH_NUM_BARS      sizeof(GraphLinTable)
#define GRAPH_V_NUM_BARS    sizeof(GraphVTable)


/*
************************************************************************************************************************
*           LOCAL CONSTANTS
************************************************************************************************************************
*/

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

#define ABS(x)      ((x) > 0 ? (x) : -(x))
#define ROUND(x)    ((x) > 0.0 ? (((float)(x)) + 0.5) : (((float)(x)) - 0.5))
#define MAP(x, in_min, in_max, out_min, out_max)    ((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min)

/*
************************************************************************************************************************
*           LOCAL GLOBAL VARIABLES
************************************************************************************************************************
*/


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

static uint8_t get_text_width(const char *text, const uint8_t *font)
{
    uint8_t text_width = 0;
    const char *ptext = text;

    if (FONT_IS_MONO_SPACED(font))
    {
        while (*ptext)
        {
            text_width += font[FONT_FIXED_WIDTH] + FONT_INTERCHAR_SPACE;
            ptext++;
        }
    }
    else
    {
        while (*ptext)
        {
            text_width += font[FONT_WIDTH_TABLE + ((*ptext) - font[FONT_FIRST_CHAR])] + FONT_INTERCHAR_SPACE;
            ptext++;
        }
    }

    text_width -= FONT_INTERCHAR_SPACE;

    return text_width;
}


/*
************************************************************************************************************************
*           GLOBAL FUNCTIONS
************************************************************************************************************************
*/

void widget_textbox(glcd_t *display, textbox_t *textbox)
{
    uint8_t text_width, text_height;

    if (textbox->text == NULL) return;

    if (textbox->mode == TEXT_SINGLE_LINE)
    {
        text_width = get_text_width(textbox->text, textbox->font);
        text_height = textbox->font[FONT_HEIGHT];
    }
    else
    {
        text_width = textbox->width;
        text_height = textbox->height;
    }

    if (textbox->width == 0) textbox->width = text_width;
    else if (text_width > textbox->width) text_width = textbox->width;

    if (textbox->height == 0) textbox->height = text_height;

    switch (textbox->align)
    {
        case ALIGN_LEFT_TOP:
            textbox->x = textbox->left_margin;
            textbox->y = textbox->top_margin;
            break;

        case ALIGN_CENTER_TOP:
            textbox->x = (DISPLAY_WIDTH / 2) - (text_width / 2);
            textbox->y = textbox->top_margin;
            break;

        case ALIGN_RIGHT_TOP:
            textbox->x = DISPLAY_WIDTH - text_width - textbox->right_margin;
            textbox->y = textbox->top_margin;
            break;

        case ALIGN_LEFT_MIDDLE:
            textbox->x = textbox->left_margin;
            textbox->y = (DISPLAY_HEIGHT / 2) - (text_height / 2);
            break;

        case ALIGN_CENTER_MIDDLE:
            textbox->x = ((DISPLAY_WIDTH / 2) - (text_width / 2)) + (textbox->left_margin - textbox->right_margin);
            textbox->y = ((DISPLAY_HEIGHT / 2) - (text_height / 2)) + (textbox->top_margin - textbox->bottom_margin);
            break;

        case ALIGN_RIGHT_MIDDLE:
            textbox->x = DISPLAY_WIDTH - text_width - textbox->right_margin;
            textbox->y = (DISPLAY_HEIGHT / 2) - (text_height / 2);
            break;

        case ALIGN_LEFT_BOTTOM:
            textbox->x = textbox->left_margin;
            textbox->y = DISPLAY_HEIGHT - text_height - textbox->bottom_margin;
            break;

        case ALIGN_CENTER_BOTTOM:
            textbox->x = (DISPLAY_WIDTH / 2) - (text_width / 2);
            textbox->y = DISPLAY_HEIGHT - text_height - textbox->bottom_margin;
            break;

        case ALIGN_RIGHT_BOTTOM:
            textbox->x = DISPLAY_WIDTH - text_width - textbox->right_margin;
            textbox->y = DISPLAY_HEIGHT - text_height - textbox->bottom_margin;
            break;

        default:
        case ALIGN_NONE_NONE:
            break;

        case ALIGN_LEFT_NONE:
            textbox->x = textbox->left_margin;
            textbox->y += textbox->top_margin;
            break;

        case ALIGN_RIGHT_NONE:
            textbox->x = DISPLAY_WIDTH - text_width - textbox->right_margin;
            textbox->y += textbox->top_margin;
            break;

        case ALIGN_CENTER_NONE:
            textbox->x = (DISPLAY_WIDTH / 2) - (text_width / 2);
            textbox->y += textbox->top_margin;
            break;

        // TODO: others NONE options
    }

    uint8_t i = 0, index;
    const char *ptext = textbox->text;
    char buffer[DISPLAY_WIDTH/2];

    text_width = 0;
    text_height = 0;
    index = FONT_FIXED_WIDTH;

    while (*ptext)
    {
        // gets the index of the current character
        if (!FONT_IS_MONO_SPACED(textbox->font)) index = FONT_WIDTH_TABLE + ((*ptext) - textbox->font[FONT_FIRST_CHAR]);

        // calculates the text width
        text_width += textbox->font[index] + FONT_INTERCHAR_SPACE;

        // buffering
        buffer[i++] = *ptext;

        // checks the width limit
        if (text_width >= textbox->width || *ptext == '\n')
        {
            if (*ptext == '\n') ptext++;

            // check whether is single line
            if (textbox->mode == TEXT_SINGLE_LINE) break;
            else buffer[i-1] = 0;

            glcd_text(display, textbox->x, textbox->y + text_height, buffer, textbox->font, textbox->color);
            text_height += textbox->font[FONT_HEIGHT] + 1;
            text_width = 0;
            i = 0;

            // checks the height limit
            if (text_height > textbox->height) break;
        }
        else ptext++;
    }

    // draws the line
    if (text_width > 0)
    {
        buffer[i] = 0;

        // checks the text width again
        text_width = get_text_width(buffer, textbox->font);
        if (text_width > textbox->width) buffer[i-1] = 0;

        glcd_text(display, textbox->x, textbox->y + text_height, buffer, textbox->font, textbox->color);
    }
}


void widget_listbox(glcd_t *display, listbox_t *listbox)
{
    uint8_t i, font_height, max_lines, y_line;
    uint8_t first_line, focus, center_focus, focus_height;
    char aux[DISPLAY_WIDTH/2];
    const char *line_txt;

    glcd_rect_fill(display, listbox->x, listbox->y, listbox->width, listbox->height, ~listbox->color);

    font_height = listbox->font[FONT_HEIGHT];
    max_lines = listbox->height / (font_height + listbox->line_space);

    center_focus = (max_lines / 2) - (1 - (max_lines % 2));
    first_line = 0;

    if (listbox->hover > center_focus && listbox->count > max_lines)
    {
        first_line = listbox->hover - center_focus;
        if (first_line > ABS(listbox->count - max_lines))
        {
            first_line = ABS(listbox->count - max_lines);
        }
    }

    if (max_lines > listbox->count) max_lines = listbox->count;
    focus = listbox->hover - first_line;
    focus_height = font_height + listbox->line_top_margin + listbox->line_bottom_margin;
    y_line = listbox->y + listbox->line_space;

    for (i = 0; i < max_lines; i++)
    {
        if (i < listbox->count)
        {
            line_txt = listbox->list[first_line + i];

            if ((first_line + i) == listbox->selected)
            {
                uint8_t j = 0;
                aux[j++] = ' ';
                aux[j++] = '>';
                aux[j++] = ' ';
                while (*line_txt && j < sizeof(aux)-1) aux[j++] = *line_txt++;
                aux[j] = 0;
                line_txt = aux;
            }

            glcd_text(display, listbox->x + listbox->text_left_margin, y_line, line_txt, listbox->font, listbox->color);

            if (i == focus)
            {
                glcd_rect_invert(display, listbox->x, y_line - listbox->line_top_margin, listbox->width, focus_height);
            }

            y_line += font_height + listbox->line_space;
        }
    }
}

void widget_menu_listbox(glcd_t *display, listbox_t *listbox)
{
    uint8_t i, font_height, max_lines, y_line;
    uint8_t first_line, focus, center_focus, focus_height;
    const char *line_txt;

    glcd_rect_fill(display, listbox->x, listbox->y, listbox->width, listbox->height-3, ~listbox->color);

    font_height = listbox->font[FONT_HEIGHT];
    max_lines = listbox->height / (font_height + listbox->line_space);

    center_focus = (max_lines / 2) - (1 - (max_lines % 2));
    first_line = 0;

    if (listbox->hover > center_focus && listbox->count > max_lines)
    {
        first_line = listbox->hover - center_focus;
        if (first_line > ABS(listbox->count - max_lines))
        {
            first_line = ABS(listbox->count - max_lines);
        }
    }

    if (max_lines > listbox->count) max_lines = listbox->count;
    focus = listbox->hover - first_line;
    focus_height = font_height + listbox->line_top_margin + listbox->line_bottom_margin;
    y_line = listbox->y + listbox->line_space;

    uint8_t end_of_list = 0, beginning_of_list = 0;

    if (listbox->hover > listbox->count-4)
    {
        end_of_list = 1;
    }
    else if (listbox->hover < 3)
    {
        beginning_of_list = 1;
    }

    for (i = 0; i < max_lines; i++)
    {
        if (i < listbox->count)
        {
            line_txt = listbox->list[first_line + i];

            textbox_t menu_line = {};
            menu_line.color = GLCD_BLACK;
            menu_line.mode = TEXT_SINGLE_LINE;
            menu_line.font = Terminal3x5;
            menu_line.align = ALIGN_CENTER_NONE;
            menu_line.x = listbox->x + listbox->text_left_margin;
            menu_line.y = y_line;
            menu_line.text = line_txt;
            widget_textbox(display, &menu_line);

            if (i == focus)
            {
                glcd_rect_invert(display, listbox->x, y_line - listbox->line_top_margin, listbox->width, focus_height);
            }

            y_line += font_height + listbox->line_space;
        }
    }

    //print arrows
    if (!end_of_list)
    {
        glcd_hline(display, 65, 50, 1, GLCD_BLACK);
        glcd_hline(display, 61, 50, 1, GLCD_BLACK);
        glcd_hline(display, 64, 51, 1, GLCD_BLACK);
        glcd_hline(display, 62, 51, 1, GLCD_BLACK);
        glcd_hline(display, 63, 52, 1, GLCD_BLACK);
    }

    if (!beginning_of_list)
    {
        glcd_hline(display, 65, 10, 1, GLCD_BLACK);
        glcd_hline(display, 61, 10, 1, GLCD_BLACK);
        glcd_hline(display, 64, 9, 1, GLCD_BLACK);
        glcd_hline(display, 62, 9, 1, GLCD_BLACK);
        glcd_hline(display, 63, 8, 1, GLCD_BLACK);
    }
}

void widget_listbox2(glcd_t *display, listbox_t *listbox) //FIXME: function hardcoded
{
    glcd_rect_fill(display, listbox->x, listbox->y, listbox->width, listbox->height, ~listbox->color);

    if (listbox->selected > 0)
    {
        glcd_text(display, listbox->x + listbox->text_left_margin, 19, listbox->list[listbox->selected-1], Terminal5x7, listbox->color);
    }

    if (listbox->selected < (listbox->count - 1))
    {
        glcd_text(display, listbox->x + listbox->text_left_margin, 41, listbox->list[listbox->selected+1], listbox->font, listbox->color);
    }

    glcd_text(display, listbox->x + listbox->text_left_margin + 2, 27, listbox->list[listbox->selected], alterebro24, listbox->color);
    glcd_rect_invert(display, listbox->x, 27, listbox->width, 13);
}

void widget_listbox4(glcd_t *display, listbox_t *listbox)
{
    const char *line_txt;

    glcd_rect_fill(display, listbox->x, listbox->y, listbox->width, listbox->height, ~listbox->color);

    int8_t first_line;
    uint8_t y_line = listbox->y + listbox->line_space;
    uint8_t font_height = listbox->font[FONT_HEIGHT];
    uint8_t focus_height = listbox->font_highlight[FONT_HEIGHT] + listbox->line_top_margin + listbox->line_bottom_margin;

    //beginning of list
    if (listbox->selected < 2)
        first_line = 0;
    //end of list
    else if (listbox->selected > (listbox->count -2) )   
        first_line = listbox->count -3;
    //anywhere else
    else
        first_line = listbox->selected - 1;

    for (uint8_t i = 0; i < 3; i++)
    {
        if (i < listbox->count)
        {
            line_txt = listbox->list[first_line + i];

            if ((first_line + i) == listbox->selected)
            {
                y_line++;
                glcd_text(display, listbox->x + listbox->text_left_margin, y_line, line_txt, listbox->font_highlight, listbox->color);
                glcd_rect_invert(display, listbox->x, y_line - listbox->line_top_margin, listbox->width, focus_height);
                y_line += focus_height;
            }
            else 
            {
                glcd_text(display, listbox->x + listbox->text_left_margin, y_line, line_txt, listbox->font, listbox->color);
                y_line += font_height + listbox->line_space;
            }
        }
    }
}

void widget_listbox_overlay(glcd_t *display, listbox_t *listbox)
{
    //draw the box and tittle
    //clear the area
    glcd_rect_fill(display, listbox->x, listbox->y, listbox->width, listbox->height, ~listbox->color);
    //hardcoded a bit, removes teh encoder pages that are lower then the widget
    glcd_rect_fill(display, 31, listbox->y+listbox->height, 69, 5, ~listbox->color);

    //draw the box around it
    glcd_hline(display, listbox->x, listbox->y+5, DISPLAY_WIDTH, GLCD_BLACK);
    glcd_hline(display, listbox->x, listbox->y+listbox->height, DISPLAY_WIDTH, GLCD_BLACK);
    glcd_vline(display, 0, listbox->y+5, listbox->height-5, GLCD_BLACK);
    glcd_vline(display, listbox->x+listbox->width-1, listbox->y+5, listbox->height-5, GLCD_BLACK);

    uint8_t char_cnt_name = strlen(listbox->name);
    if (char_cnt_name > 19)
    {
        //limit string
        char_cnt_name = 19;
    }
    char *title_str_bfr = (char *) MALLOC((char_cnt_name + 1) * sizeof(char));
    strncpy(title_str_bfr, listbox->name, char_cnt_name);
    title_str_bfr[char_cnt_name] = '\0';

    //clear the name area
    glcd_rect_fill(display, ((DISPLAY_WIDTH) /2) - char_cnt_name*3-8, 12, ((6*char_cnt_name) +10), 9, ~listbox->color);

    //draw the title
    glcd_text(display,  ((DISPLAY_WIDTH) /2) - char_cnt_name*3 + 1, listbox->y+2, title_str_bfr, listbox->font, listbox->color);

    //draw the icon before
    icon_overlay(display, ((DISPLAY_WIDTH) /2) - char_cnt_name*3 -6, listbox->y+4);

    // invert the name area
    glcd_rect_invert(display, ((DISPLAY_WIDTH) /2) - char_cnt_name*3-8, 12, ((6*char_cnt_name) +10), 9);

    //draw the list
    if (listbox->selected > 0)
    {
        uint8_t line_length =  strlen(listbox->list[listbox->selected-1]);
        if (line_length > 20)
            line_length = 20;

        glcd_text(display, (DISPLAY_WIDTH / 2) - ((line_length * 6) / 2), listbox->y + 12, listbox->list[listbox->selected-1], listbox->font, listbox->color);
    }

    if (listbox->selected < (listbox->count - 1))
    {
        uint8_t line_length =  strlen(listbox->list[listbox->selected+1]);
        if (line_length > 20)
            line_length = 20;

        glcd_text(display, (DISPLAY_WIDTH / 2) - ((line_length * 6) / 2), listbox->y + 30, listbox->list[listbox->selected+1], listbox->font, listbox->color);
    }

    uint8_t line_length =  strlen(listbox->list[listbox->selected]);
    if (line_length > 20)
        line_length = 20;

    glcd_text(display, (DISPLAY_WIDTH / 2) - ((line_length * 6) / 2), listbox->y + 21, listbox->list[listbox->selected], listbox->font, listbox->color);

    glcd_rect_invert(display, listbox->x+1, listbox->y + 20, listbox->width-2, 9);

    FREE(title_str_bfr);
}

void widget_foot_overlay(glcd_t *display, overlay_t *overlay)
{
    //clear the area
    glcd_rect_fill(display, overlay->x, overlay->y, overlay->width, overlay->height, ~overlay->color);
    //hardcoded a bit, removes the encoder pages that are lower then the widget
    glcd_rect_fill(display, 31, overlay->y+overlay->height, 69, 5, ~overlay->color);

    //draw the box around it
    glcd_hline(display, overlay->x, overlay->y+5, DISPLAY_WIDTH, GLCD_BLACK);
    glcd_hline(display, overlay->x, overlay->y+overlay->height, DISPLAY_WIDTH, GLCD_BLACK);
    glcd_vline(display, 0, overlay->y+5, overlay->height-5, GLCD_BLACK);
    glcd_vline(display, overlay->x+overlay->width-1, overlay->y+5, overlay->height-5, GLCD_BLACK);

    uint8_t char_cnt_name = strlen(overlay->name);
    if (char_cnt_name > 19)
    {
        //limit string
        char_cnt_name = 19;
    }
    char *title_str_bfr = (char *) MALLOC((char_cnt_name + 1) * sizeof(char));
    strncpy(title_str_bfr, overlay->name, char_cnt_name);
    title_str_bfr[char_cnt_name] = '\0';

    //clear the name area
    glcd_rect_fill(display, ((DISPLAY_WIDTH) /2) - char_cnt_name*3-8, 12, ((6*char_cnt_name) +10), 9, ~overlay->color);

    textbox_t title;
    title.color = GLCD_BLACK;
    title.mode = TEXT_SINGLE_LINE;
    title.font = Terminal5x7;
    title.top_margin = 0;
    title.bottom_margin = 0;
    title.left_margin = 0;
    title.right_margin = 0;
    title.height = 0;
    title.width = 0;
    title.align = ALIGN_NONE_NONE;
    title.y = overlay->y+2;

    title.text = title_str_bfr;

    title.x = ((DISPLAY_WIDTH) /2) - char_cnt_name*3 + 1;
    widget_textbox(display, &title);

    //draw the icon before
    icon_overlay(display, title.x - 7, overlay->y+4);

    // invert the name area
    glcd_rect_invert(display, ((DISPLAY_WIDTH) /2) - char_cnt_name*3-8, 12, ((6*char_cnt_name) +10), 9);

    //draw the value
    textbox_t value;
    uint8_t char_cnt_value = strlen(overlay->value);
    value.color = GLCD_BLACK;
    value.mode = TEXT_SINGLE_LINE;
    value.font = Terminal7x8;
    value.top_margin = 0;
    value.bottom_margin = 0;
    value.left_margin = 0;
    value.right_margin = 0;
    value.height = 0;
    value.width = 0;
    value.align = ALIGN_NONE_NONE;
    value.y = overlay->y+19;
    value.text = overlay->value;
    value.x = ((DISPLAY_WIDTH) /2) - char_cnt_value*4 + 1;
    widget_textbox(display, &value);

    //invert the value area if needed
    if (overlay->properties == FLAG_CONTROL_BYPASS)
    {
        //weird lv2 stuff, bypass on means effect off
        overlay->value_num = !overlay->value_num;
    }
    else if (overlay->properties == FLAG_CONTROL_TAP_TEMPO)
    {
        //always invert a value
        overlay->value_num = 1;
    }

    switch (overlay->properties)
    {
        case FLAG_CONTROL_TRIGGER:
            //draw 2 beatifull lines next to the word 'trigger'
            glcd_hline(display, overlay->x+6, overlay->y+23, 29, GLCD_BLACK);
            glcd_hline(display, overlay->x+94, overlay->y+23, 29, GLCD_BLACK);
        // fall through          
        case FLAG_CONTROL_MOMENTARY:
        case FLAG_CONTROL_TOGGLED:
        case FLAG_CONTROL_BYPASS:
        case FLAG_CONTROL_TAP_TEMPO:
            //invert the area if value is 1
            if (overlay->value_num)
            {
                uint8_t begin_of_tittle_block = (((DISPLAY_WIDTH) /2) - char_cnt_name*3-8);
                uint8_t end_of_tittle_block = (((DISPLAY_WIDTH) /2) - char_cnt_name*3-8) + ((6*char_cnt_name) +10);
                glcd_rect_invert(display, overlay->x + 2, overlay->y + 7, begin_of_tittle_block - 3, 4);
                glcd_rect_invert(display, end_of_tittle_block + 1, overlay->y + 7, DISPLAY_WIDTH - end_of_tittle_block - 3, 4);
                //bigger invert
                glcd_rect_invert(display, overlay->x+2, overlay->y + 11, DISPLAY_WIDTH - 4, overlay->height - 12);
            }
        break;
    }

    FREE(title_str_bfr);
}

void widget_list_value(glcd_t *display, listbox_t *listbox)
{
    //clear
    glcd_rect_fill(display, listbox->x, listbox->y, listbox->width, listbox->height, ~listbox->color);

    //format list item name
    char value_str_bfr[9] = {0};

    //draw name
    strncpy(value_str_bfr, listbox->list[listbox->selected], 8);
    value_str_bfr[8] = '\0';

    //draw the current list item
    textbox_t value;
    value.color = GLCD_BLACK;
    value.mode = TEXT_SINGLE_LINE;
    value.font = Terminal3x5;
    value.height = 5;
    value.width = listbox->width;
    value.top_margin = 0;
    value.bottom_margin = 0;
    value.left_margin = 0;
    value.right_margin = 0;
    value.text = value_str_bfr;
    value.align = ALIGN_NONE_NONE;
    //allign to middle, (full width / 2) - (text width / 2)
    value.x = (listbox->x + (listbox->width / 2) - 2*strlen(value_str_bfr)) + 1;
    value.y = listbox->y + 6;
    widget_textbox(display, &value);

    //invert text area
    glcd_rect_invert(display, listbox->x, listbox->y+5, listbox->width, 7);

    //check if we need to draw arrow down
    if (listbox->selected != (listbox->count-1))
    {
        glcd_hline(display, listbox->x + 14, listbox->y + 13, 1, GLCD_BLACK);
        glcd_hline(display, listbox->x + 20, listbox->y + 13, 1, GLCD_BLACK);
        glcd_hline(display, listbox->x + 15, listbox->y + 14, 1, GLCD_BLACK);
        glcd_hline(display, listbox->x + 19, listbox->y + 14, 1, GLCD_BLACK);
        glcd_hline(display, listbox->x + 16, listbox->y + 15, 1, GLCD_BLACK);
        glcd_hline(display, listbox->x + 18, listbox->y + 15, 1, GLCD_BLACK);
        glcd_hline(display, listbox->x + 17, listbox->y + 16, 1, GLCD_BLACK);

    }   

    //check if we need to draw arrow up
    if (listbox->selected != 0)
    {
        glcd_hline(display, listbox->x + 14, listbox->y + 3, 1, GLCD_BLACK);
        glcd_hline(display, listbox->x + 20, listbox->y + 3, 1, GLCD_BLACK);
        glcd_hline(display, listbox->x + 15, listbox->y + 2, 1, GLCD_BLACK);
        glcd_hline(display, listbox->x + 19, listbox->y + 2, 1, GLCD_BLACK);
        glcd_hline(display, listbox->x + 16, listbox->y + 1, 1, GLCD_BLACK);
        glcd_hline(display, listbox->x + 18, listbox->y + 1, 1, GLCD_BLACK);
        glcd_hline(display, listbox->x + 17, listbox->y, 1, GLCD_BLACK);
    }
}

void widget_bar_encoder(glcd_t *display, bar_t *bar)
{
    //draw the value
    textbox_t value;
    value.color = GLCD_BLACK;
    value.mode = TEXT_SINGLE_LINE;
    value.font = Terminal3x5;
    value.height = 0;
    value.width = 0;
    value.top_margin = 0;
    value.bottom_margin = 0;
    value.left_margin = 0;
    value.right_margin = 0;
    value.text = bar->value;
    value.align = ALIGN_NONE_NONE;
    //allign to middle, (full width / 2) - (text width / 2)
    value.x = (bar->x + (bar->width / 2) - 2*strlen(bar->value)) + 1;
    value.y = bar->y + 6 + bar->height + 1;
    widget_textbox(display, &value);

    //draw the unit if needed
    if (bar->has_unit)
    {
        textbox_t value;
        value.color = GLCD_BLACK;
        value.mode = TEXT_SINGLE_LINE;
        value.font = Terminal3x5;
        value.height = 0;
        value.width = 0;
        value.top_margin = 0;
        value.bottom_margin = 0;
        value.left_margin = 0;
        value.right_margin = 0;
        value.text = bar->unit;
        value.align = ALIGN_NONE_NONE;
        //allign to middle, (full width / 2) - (text width / 2)
        value.x = (bar->x + (bar->width / 2) - 2*strlen(bar->unit)+1);
        value.y = bar->y + 12 + bar->height + 1;
        widget_textbox(display, &value);
    }

    float OldRange, NewRange, NewValue;
    int32_t bar_possistion, NewMax, NewMin;

    NewMin = 1;
    NewMax = bar->width - 2;

    OldRange = (bar->steps);
    NewRange = (NewMax - NewMin);

    NewValue = (((bar->step) * NewRange) / OldRange) + NewMin;
    bar_possistion = ROUND(NewValue);

    //draw the square
    glcd_rect(display, bar->x, bar->y+6, bar->width, bar->height, GLCD_BLACK);

    //prevent it from trippin 
    if (bar_possistion < 1) bar_possistion = 1;
    if (bar_possistion > bar->width - 2) bar_possistion = bar->width - 2;

    //color in the position area
    glcd_rect_fill(display, (bar->x+1), (bar->y+7), bar_possistion, bar->height - 2, GLCD_BLACK);
}

void widget_toggle(glcd_t *display, toggle_t *toggle)
{
    //draw the square
    glcd_rect(display, toggle->x, toggle->y, toggle->width, toggle->height, GLCD_BLACK);

    textbox_t label;
    label.color = GLCD_BLACK;
    label.mode = TEXT_SINGLE_LINE;
    label.font = Terminal3x5;
    label.height = 0;
    label.width = 0;
    label.top_margin = 0;
    label.bottom_margin = 0;
    label.left_margin = 0;
    label.right_margin = 0;
    label.align = ALIGN_NONE_NONE;
    label.y = toggle->y + 6;

    //trigger
    if (toggle->value >= 2)
    {
        label.x = toggle->x + (toggle->width / 2) - 7;
        label.text = "TRIG";
        widget_textbox(display, &label);

        //draw trigger lines
        glcd_hline(display, toggle->x + 2, toggle->y + 8, 7, GLCD_BLACK);
        glcd_hline(display, toggle->x + 26, toggle->y + 8, 7, GLCD_BLACK);

        if (toggle->value == 3)
            glcd_rect_invert(display, toggle->x+1, toggle->y+1, toggle->width -2, toggle->height - 2);
    }
    //toggle
    else if (toggle->value == 1)
    {
        label.x = toggle->x + (toggle->width / 2) - 3;
        label.text = "ON";
        widget_textbox(display, &label);

        //color in the position area
        glcd_rect_invert(display, toggle->x+2, toggle->y+2, toggle->width -4, toggle->height - 4);
    }
    else 
    {
        label.x = toggle->x + (toggle->width / 2) - 5;
        label.text = "OFF";
        widget_textbox(display, &label);

        //color in the position area
        glcd_rect(display, toggle->x+2, toggle->y+2, toggle->width -4, toggle->height - 4, GLCD_BLACK);
    }
}

// FIXME: this widget is hardcoded
void widget_peakmeter(glcd_t *display, uint8_t pkm_id, peakmeter_t *pkm)
{
    uint8_t height, y_black, y_chess, y_peak, h_black, h_chess;
    const uint8_t h_black_max = 20, h_chess_max = 22;
    const uint8_t x_bar[] = {4, 30, 57, 83};
    const float h_max = 42.0, max_dB = 0.0, min_dB = -30.0;

    // calculates the bar height
    float value = pkm->value;
    if (value > max_dB) value = max_dB;
    if (value < min_dB) value = min_dB;
    value = ABS(min_dB) - ABS(value);
    height = (uint8_t) ROUND((h_max * value) / (max_dB - min_dB));

    // clean the peakmeter bar
    if (pkm_id == 0) glcd_rect_fill(display, 4, 13, 16, 42, GLCD_WHITE);
    else if (pkm_id == 1) glcd_rect_fill(display, 30, 13, 16, 42, GLCD_WHITE);
    else if (pkm_id == 2) glcd_rect_fill(display, 57, 13, 16, 42, GLCD_WHITE);
    else if (pkm_id == 3) glcd_rect_fill(display, 83, 13, 16, 42, GLCD_WHITE);

    // draws the black area
    if (height > h_chess_max)
    {
        h_black = height - h_chess_max;
        y_black = 13 + (h_black_max - h_black);
        glcd_rect_fill(display, x_bar[pkm_id], y_black, 16, h_black, GLCD_BLACK);
    }

    // draws the chess area
    if (height > 0)
    {
        h_chess = (height > h_chess_max ? h_chess_max : height);
        y_chess = 33 + (h_chess_max - h_chess);
        glcd_rect_fill(display, x_bar[pkm_id], y_chess, 16, h_chess, GLCD_CHESS);
    }

    // draws the peak
    if (pkm->peak > pkm->value)
    {
        y_peak = 13.0 + ABS(ROUND((h_max * pkm->peak) / (max_dB - min_dB)));
        glcd_hline(display, x_bar[pkm_id], y_peak, 16, GLCD_BLACK);
    }
}


void widget_tuner(glcd_t *display, tuner_t *tuner)
{
    // draws the title
    glcd_rect_fill(display, 0, 0, DISPLAY_WIDTH, 9, GLCD_WHITE);
    glcd_hline(display, 0, 9, DISPLAY_WIDTH, GLCD_BLACK_WHITE);
    textbox_t title;
    title.color = GLCD_BLACK;
    title.mode = TEXT_SINGLE_LINE;
    title.font = Terminal3x5;
    title.top_margin = 0;
    title.bottom_margin = 0;
    title.left_margin = 0;
    title.right_margin = 0;
    title.height = 0;
    title.width = 0;
    title.text = "TUNER";
    title.align = ALIGN_CENTER_TOP;
    title.y = 0;
    title.x = 0;
    widget_textbox(display, &title);

    // clears subtitles
    glcd_rect_fill(display, 0, 51, DISPLAY_WIDTH, 12, GLCD_WHITE);

    // draws the frequency subtitle
    char freq_str[16], input_str[16];
    uint8_t i = float_to_str(tuner->frequency, freq_str, sizeof(freq_str), 2);
    freq_str[i++] = 'H';
    freq_str[i++] = 'z';
    freq_str[i++] = 0;
    textbox_t freq, input;
    freq.color = GLCD_BLACK;
    freq.mode = TEXT_SINGLE_LINE;
    freq.align = ALIGN_LEFT_NONE;
    freq.y = 51;
    freq.height = 0;
    freq.width = 0;
    freq.top_margin = 0;
    freq.bottom_margin = 0;
    freq.left_margin = 1;
    freq.right_margin = 0;
    freq.font = alterebro24;
    freq.text = freq_str;
    widget_textbox(display, &freq);

    // draws the input subtitle
    i = 0;
    input_str[i++] = 'I';
    input_str[i++] = 'N';
    input_str[i++] = 'P';
    input_str[i++] = 'U';
    input_str[i++] = 'T';
    input_str[i++] = ' ';
    input_str[i++] = '0' + tuner->input;
    input_str[i++] = 0;
    input.color = GLCD_BLACK;
    input.mode = TEXT_SINGLE_LINE;
    input.align = ALIGN_RIGHT_NONE;
    input.y = 51;
    input.height = 0;
    input.width = 0;
    input.top_margin = 0;
    input.bottom_margin = 0;
    input.left_margin = 0;
    input.right_margin = 1;
    input.font = alterebro24;
    input.text = input_str;
    widget_textbox(display, &input);

    // constants configurations
    const uint8_t num_bars = 5, y_bars = 21, w_bar = 4, h_bar = 21, bars_space = 3;
    const uint8_t cents_range = 50;

    uint8_t x, n;

    // calculates the number of bars that need be filled
    n = (ABS(tuner->cents) + num_bars) / (cents_range / num_bars);

    // draws the left side bars
    for (i = 0, x = 1; i < num_bars; i++)
    {
        // checks if need fill the bar
        if (tuner->cents < 0 && i >= (num_bars - n))
            glcd_rect_fill(display, x, y_bars, w_bar, h_bar, GLCD_BLACK);
        else
            glcd_rect_fill(display, x, y_bars, w_bar, h_bar, GLCD_WHITE);

        glcd_rect(display, x, y_bars, w_bar, h_bar, GLCD_BLACK);

        x += w_bar + bars_space;
    }

    // draws the right side bars
    for (i = 0, x = 95; i < num_bars; i++)
    {
        // checks if need fill the bar
        if (tuner->cents > 0 && i < n)
            glcd_rect_fill(display, x, y_bars, w_bar, h_bar, GLCD_BLACK);
        else
            glcd_rect_fill(display, x, y_bars, w_bar, h_bar, GLCD_WHITE);

        glcd_rect(display, x, y_bars, w_bar, h_bar, GLCD_BLACK);

        x += w_bar + bars_space;
    }

    // draws the note box
    glcd_rect_fill(display, 36, 17, 56, 29, GLCD_WHITE);
    textbox_t note;
    note.color = GLCD_BLACK;
    note.mode = TEXT_SINGLE_LINE;
    note.align = ALIGN_CENTER_NONE;
    note.top_margin = 0;
    note.bottom_margin = 2;
    note.left_margin = 0;
    note.right_margin = 0;
    note.y = 18;
    note.height = 0;
    note.width = 0;
    note.font = alterebro49;
    note.text = tuner->note;
    widget_textbox(display, &note);

    // checks if is tuned
    if (n == 0) glcd_rect_invert(display, 36, 17, 56, 29);
    else glcd_rect(display, 36, 17, 56, 29, GLCD_BLACK);
}


void widget_popup(glcd_t *display, popup_t *popup)
{
    // clears the popup area
    glcd_rect_fill(display, popup->x, popup->y, popup->width, popup->height, GLCD_WHITE);

    // draws the contour
    glcd_rect(display, popup->x, popup->y, popup->width, popup->height, GLCD_BLACK);

    // draws the title text
    textbox_t title;
    title.color = GLCD_BLACK;
    title.mode = TEXT_SINGLE_LINE;
    title.align = ALIGN_CENTER_TOP;
    title.font = popup->font;
    title.top_margin = 1;
    title.bottom_margin = 0;
    title.left_margin = 0;
    title.right_margin = 0;
    title.height = 5;
    title.width = DISPLAY_WIDTH;
    title.text = popup->title;
    widget_textbox(display, &title);

    // draws the title background
    glcd_rect_invert(display, popup->x+1, popup->y+1, popup->width-2, 6);

    // draws the content
    textbox_t content;
    content.color = GLCD_BLACK;
    content.mode = TEXT_MULTI_LINES;
    content.align = ALIGN_NONE_NONE;
    content.top_margin = 0;
    content.bottom_margin = 0;
    content.left_margin = 0;
    content.right_margin = 0;
    content.font = popup->font;
    content.x = popup->x + 4;
    content.y = popup->y + popup->font[FONT_HEIGHT] + 3;
    content.width = popup->width - 4;
    content.height = ((popup->font[FONT_HEIGHT]+1) * 8); // FIXME: need be relative to popup height
    content.text = popup->content;
    widget_textbox(display, &content);

    uint8_t button_x, button_y, button_w, button_h;
    const char *button_text;

    // draws the buttons
    switch (popup->type)
    {
        case OK_ONLY:
        case CANCEL_ONLY:
            button_text = (popup->type == OK_ONLY ? "OK" : "CANCEL");
            button_w = get_text_width(button_text, popup->font) + 8;
            button_h = popup->font[FONT_HEIGHT]+2;
            button_x = popup->x + (popup->width / 2) - (button_w / 2);
            button_y = popup->y + popup->height - button_h;
            glcd_text(display, button_x + 4, button_y, button_text, popup->font, GLCD_BLACK);

            if (popup->button_selected == 0)
                glcd_rect_invert(display, button_x+1, button_y-1, button_w-2, button_h);
            break;

        case OK_CANCEL:
        case YES_NO:
            button_text = (popup->type == OK_CANCEL ? "OK" : "YES");
            button_w = get_text_width(button_text, popup->font) + 8;
            button_h = popup->font[FONT_HEIGHT]+2;
            button_x = popup->x + (popup->width / 4) - (button_w / 2);
            button_y = popup->y + popup->height - button_h;
            glcd_text(display, button_x + 4, button_y, button_text, popup->font, GLCD_BLACK);

            if (popup->button_selected == 0)
                glcd_rect_invert(display, button_x+1, button_y-1, button_w-2, button_h);

            button_text = (popup->type == OK_CANCEL ? "CANCEL" : "NO");
            button_w = get_text_width(button_text, popup->font) + 8;
            button_h = popup->font[FONT_HEIGHT]+2;
            button_x = popup->x + popup->width - (popup->width / 4) - (button_w / 2);
            button_y = popup->y + popup->height - button_h;
            glcd_text(display, button_x + 4, button_y, button_text, popup->font, GLCD_BLACK);

            if (popup->button_selected == 1)
                glcd_rect_invert(display, button_x+1, button_y-1, button_w-2, button_h);
            break;
 
        case EMPTY_POPUP:
        //we dont have a button
        break;
    }
    glcd_hline(display, 0, DISPLAY_HEIGHT-1, DISPLAY_WIDTH, GLCD_BLACK);
}

//draw function works in buffers of 8 so ofsets are not possible, thats why we draw icons ourselves

void icon_snapshot(glcd_t *display, uint8_t x, uint8_t y)
{
    // clears the icon area
    glcd_rect_fill(display, x, y, 9, 7, GLCD_WHITE);

    // draws the icon

    //outer bourders
    glcd_rect(display, x, y+1, 1, 6, GLCD_BLACK);
    glcd_rect(display, x+8, y+1, 1, 6, GLCD_BLACK);

    //top 3 
    glcd_rect(display, x+6, y+1, 3, 1, GLCD_BLACK);    
    glcd_rect(display, x, y+1, 3, 1, GLCD_BLACK);
    glcd_rect(display, x+2, y, 5, 1, GLCD_BLACK);

    //bottom
    glcd_rect(display, x, y+6, 8, 1, GLCD_BLACK);

    //dots
    glcd_rect(display, x+4, y+2, 1, 1, GLCD_BLACK);
    glcd_rect(display, x+3, y+3, 1, 1, GLCD_BLACK);
    glcd_rect(display, x+5, y+3, 1, 1, GLCD_BLACK);
    glcd_rect(display, x+4, y+4, 1, 1, GLCD_BLACK);
}

void icon_pedalboard(glcd_t *display, uint8_t x, uint8_t y)
{
    // clears the icon area
    glcd_rect_fill(display, x, y, 9, 7, GLCD_WHITE);

    // draws the icon
    
    //outer bourders    
    glcd_rect(display, x, y, 1, 7, GLCD_BLACK);
    glcd_rect(display, x+8, y, 1, 7, GLCD_BLACK);

    //vertical lines
    glcd_rect(display, x, y, 9, 1, GLCD_BLACK);
    glcd_rect(display, x, y+2, 9, 1, GLCD_BLACK);
    glcd_rect(display, x, y+4, 9, 1, GLCD_BLACK);
    glcd_rect(display, x, y+6, 9, 1, GLCD_BLACK);
}

void icon_overlay(glcd_t *display, uint8_t x, uint8_t y)
{
    // clears the icon area
    glcd_rect_fill(display, x, y, 6, 3, GLCD_WHITE);

    // draws the icon
    
    //outer bourders    
    glcd_rect(display, x, y, 6, 3, GLCD_BLACK);

    //vertical lines
    glcd_rect(display, x+1, y+1, 1, 1, GLCD_BLACK);
}