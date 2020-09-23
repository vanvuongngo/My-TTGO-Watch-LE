/****************************************************************************
 *   Copyright  2020  Jakub Vesely
 *   Email: jakub_vesely@seznam.cz
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "config.h"
#include "alarm_in_progress.h"
#include "alarm_clock_main.h"
#include "alarm_data.h"
#include "hardware/powermgm.h"
#include "hardware/motor.h"
#include "gui/mainbar/mainbar.h"
#include "widget_factory.h"
#include "widget_styles.h"
#include "hardware/rtcctl.h"
#include "gui/statusbar.h"
#include "hardware/display.h"
#include "hardware/timesync.h"

//#include "gui/mainbar/setup_tile/setup.h"

LV_FONT_DECLARE(Ubuntu_72px);


static const int highlight_time = 1000; //ms
static const int vibe_time = 500; //ms

static char time_str[9]; // (23:50 or 11:50 pm) + /0
static lv_obj_t *tile=NULL;
static uint32_t tile_num = 0;
static bool in_progress = false;
static bool highlighted = false;
static lv_obj_t *container = NULL;
static lv_obj_t *label = NULL;
static lv_style_t popup_style;
static lv_style_t label_style;
static int brightness = 0;

LV_IMG_DECLARE(cancel_32px);
LV_IMG_DECLARE(alarm_clock_64px);

static void exit_event_callback( lv_obj_t * obj, lv_event_t event ){
    switch( event ) {
        case( LV_EVENT_CLICKED ):
            in_progress = false;
            break;
    }
}

char * alarm_in_progress_get_clock_label()
{
    //FIXME: there should be one source for the string in main_tile and alarm_clock - the format should be exactly the same
    if (timesync_get_24hr()){
        sprintf(time_str, "%d:%.2d", alarm_get_hour(), alarm_get_minute());
    }
    else{
        sprintf(
            time_str,
            "%d:%.2d",
            alarm_clock_main_get_am_pm_hour(alarm_get_hour()),
            alarm_get_minute()
        );
    }
    return time_str;
}

static void alarm_task_function(lv_task_t * task){
    if (in_progress && !alarm_is_time()){
        in_progress = false;
    }

    if (!in_progress){ //last turn
        lv_task_del(task);
        highlighted = false; //set default value
    }

    if (highlighted && alarm_is_vibe_allowed()){
        motor_vibe(vibe_time / 10, true);
    }

    if (alarm_is_fade_allowed()){
        //used brightmess because is smooth for SW dimming would be necessary to use double buffer display
        display_set_brightness(highlighted ? DISPLAY_MAX_BRIGHTNESS : DISPLAY_MIN_BRIGHTNESS);
    }
    //lv_style_set_text_color( &label_style, LV_OBJ_PART_MAIN, highlighted ? LV_COLOR_BLACK : LV_COLOR_WHITE );
    //lv_style_set_text_opa(&label_style, LV_OBJ_PART_MAIN, highlighted ? LV_OPA_100 : LV_OPA_0);

    lv_obj_invalidate(tile);

    if (in_progress){
        highlighted = !highlighted;
        lv_disp_trig_activity( NULL ); //to stay display on
    }
    else{
        display_set_brightness(brightness);
        mainbar_jump_to_maintile( LV_ANIM_OFF );
    }
}

bool alarm_occurred_event_event_callback ( EventBits_t event, void* msg ) {
    switch ( event ){
        case ( RTCCTL_ALARM_OCCURRED ):
            statusbar_hide( true );
            mainbar_jump_to_tilenumber( tile_num, LV_ANIM_OFF );

            lv_label_set_text(label, alarm_in_progress_get_clock_label());
            lv_obj_align(label, container, LV_ALIGN_IN_TOP_MID, 0, 0);

            highlighted = true;
            in_progress = true;
            brightness = display_get_brightness();
            lv_task_create( alarm_task_function, highlight_time, LV_TASK_PRIO_MID, NULL );
            break;
    }
    return( true );
}

bool powermgmt_callback( EventBits_t event, void *arg ){
    switch( event ) {
        case( POWERMGM_STANDBY ):
            in_progress = false;
            break;
    }
    return( true );
}

void alarm_in_progress_tile_setup( void ) {
    // get an app tile and copy mainstyle
    tile_num = mainbar_add_app_tile( 1, 1, "alarm in progress" );
    tile = mainbar_get_tile_obj( tile_num );

    lv_style_init( &popup_style );
    lv_style_copy( &popup_style, ws_get_popup_style());
    lv_obj_add_style( tile, LV_OBJ_PART_MAIN, &popup_style );
    lv_style_set_bg_opa(&popup_style, LV_OBJ_PART_MAIN, LV_OPA_10);
    lv_style_set_bg_color( &popup_style, LV_OBJ_PART_MAIN, LV_COLOR_BLACK);

    lv_obj_t * cancel_btm = wf_add_image_button(tile, cancel_32px, tile, LV_ALIGN_IN_TOP_LEFT, 10, 10, exit_event_callback);
    static lv_style_t cancel_btn_style;
    lv_style_init(&cancel_btn_style);
    lv_style_set_image_recolor_opa(&cancel_btn_style, LV_OBJ_PART_MAIN, LV_OPA_COVER);
    lv_style_set_image_recolor(&cancel_btn_style, LV_OBJ_PART_MAIN, LV_COLOR_WHITE);
    lv_style_set_pad_all(&cancel_btn_style, LV_OBJ_PART_MAIN, 100);
    lv_obj_add_style(cancel_btm, LV_OBJ_PART_MAIN, &cancel_btn_style);

    container = wf_add_container(tile, tile, LV_ALIGN_CENTER, 0, 0, lv_disp_get_hor_res( NULL ), 72 + 20 + 64 );

    label = wf_add_label(tile, "00:00", container, LV_ALIGN_IN_TOP_MID, 0, 0 );
    lv_style_init( &label_style );
    lv_style_copy( &label_style, ws_get_label_style() );
    lv_style_set_text_color( &label_style, LV_OBJ_PART_MAIN, LV_COLOR_WHITE );
    lv_style_set_text_font( &label_style, LV_STATE_DEFAULT, &Ubuntu_72px);
    lv_obj_add_style( label, LV_OBJ_PART_MAIN, &label_style );

    wf_add_image( tile, alarm_clock_64px, container, LV_ALIGN_IN_BOTTOM_MID, 0, 0 );

    rtcctl_register_cb( RTCCTL_ALARM_OCCURRED , alarm_occurred_event_event_callback, "alarm in progress" );
    powermgm_register_cb( POWERMGM_STANDBY, powermgmt_callback, "alarm in progress" );
}
