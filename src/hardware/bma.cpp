/****************************************************************************
 *   Tu May 22 21:23:51 2020
 *   Copyright  2020  Dirk Brosswick
 *   Email: dirk.brosswick@googlemail.com
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
#include <TTGO.h>
#include <soc/rtc.h>

#include "bma.h"
#include "powermgm.h"
#include "json_psram_allocator.h"

#include "gui/statusbar.h"

EventGroupHandle_t bma_event_handle = NULL;
bma_config_t bma_config[ BMA_CONFIG_NUM ];

__NOINIT_ATTR uint32_t stepcounter_valid;
__NOINIT_ATTR uint32_t stepcounter_before_reset;
__NOINIT_ATTR uint32_t stepcounter;

bma_event_cb_t *bma_event_cb_table = NULL;
uint32_t bma_event_cb_entrys = 0;

void IRAM_ATTR bma_irq( void );
void bma_send_event_cb( EventBits_t event, const char *msg );

void bma_setup( void ) {
    TTGOClass *ttgo = TTGOClass::getWatch();

    bma_event_handle = xEventGroupCreate();

    for ( int i = 0 ; i < BMA_CONFIG_NUM ; i++ ) {
        bma_config[ i ].enable = true;
    }

    if ( stepcounter_valid != 0xa5a5a5a5 ) {
      stepcounter = 0;
      stepcounter_before_reset = 0;
      stepcounter_valid = 0xa5a5a5a5;
      log_i("stepcounter not valid. reset");
    }

    stepcounter = stepcounter + stepcounter_before_reset;

    bma_read_config();

    ttgo->bma->begin();
    ttgo->bma->attachInterrupt();
    ttgo->bma->direction();

    pinMode( BMA423_INT1, INPUT );
    attachInterrupt( BMA423_INT1, bma_irq, RISING );

    bma_reload_settings();
}

void bma_standby( void ) {
  TTGOClass *ttgo = TTGOClass::getWatch();

  log_i("go standby");

  if ( bma_get_config( BMA_STEPCOUNTER ) )
      ttgo->bma->enableStepCountInterrupt( false );

}

void bma_wakeup( void ) {
    TTGOClass *ttgo = TTGOClass::getWatch();

    log_i("go wakeup");

    if ( bma_get_config( BMA_STEPCOUNTER ) )
        ttgo->bma->enableStepCountInterrupt( true );

    stepcounter_before_reset = ttgo->bma->getCounter();
    char msg[16]="";
    snprintf( msg, sizeof( msg ),"%d", stepcounter + ttgo->bma->getCounter() );
    bma_send_event_cb( BMACTL_STEPCOUNTER, msg );
}

void bma_reload_settings( void ) {

    TTGOClass *ttgo = TTGOClass::getWatch();

    ttgo->bma->enableStepCountInterrupt( bma_config[ BMA_STEPCOUNTER ].enable );
    ttgo->bma->enableWakeupInterrupt( bma_config[ BMA_DOUBLECLICK ].enable );
    ttgo->bma->enableTiltInterrupt( bma_config[ BMA_TILT ].enable );
}

void IRAM_ATTR bma_irq( void ) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xEventGroupSetBitsFromISR( bma_event_handle, BMACTL_EVENT_INT, &xHigherPriorityTaskWoken );

    if ( xHigherPriorityTaskWoken )
    {
        portYIELD_FROM_ISR ();
    }
}

void bma_loop( void ) {
    TTGOClass *ttgo = TTGOClass::getWatch();
    /*
     * handle IRQ event
     */
    if ( xEventGroupGetBitsFromISR( bma_event_handle ) & BMACTL_EVENT_INT ) {                
      while( !ttgo->bma->readInterrupt() );
        if ( ttgo->bma->isDoubleClick() ) {
            powermgm_set_event( POWERMGM_BMA_DOUBLECLICK );
            xEventGroupClearBitsFromISR( bma_event_handle, BMACTL_EVENT_INT );
            bma_send_event_cb( BMACTL_DOUBLECLICK, "" );
            return;
        }
        if ( ttgo->bma->isTilt() ) {
            powermgm_set_event( POWERMGM_BMA_TILT );
            xEventGroupClearBitsFromISR( bma_event_handle, BMACTL_EVENT_INT );
            bma_send_event_cb( BMACTL_TILT, "" );
            return;
        }
        if ( ttgo->bma->isStepCounter() ) {
            stepcounter_before_reset = ttgo->bma->getCounter();
            char msg[16]="";
            snprintf( msg, sizeof( msg ),"%d", stepcounter + ttgo->bma->getCounter() );
            bma_send_event_cb( BMACTL_STEPCOUNTER, msg );
            xEventGroupClearBitsFromISR( bma_event_handle, BMACTL_EVENT_INT );
        }
    }
}

void bma_register_cb( EventBits_t event, BMA_CALLBACK_FUNC bma_event_cb ) {
    bma_event_cb_entrys++;

    if ( bma_event_cb_table == NULL ) {
        bma_event_cb_table = ( bma_event_cb_t * )ps_malloc( sizeof( bma_event_cb_t ) * bma_event_cb_entrys );
        if ( bma_event_cb_table == NULL ) {
            log_e("rtc_event_cb_table malloc faild");
            while(true);
        }
    }
    else {
        bma_event_cb_t *new_bma_event_cb_table = NULL;

        new_bma_event_cb_table = ( bma_event_cb_t * )ps_realloc( bma_event_cb_table, sizeof( bma_event_cb_t ) * bma_event_cb_entrys );
        if ( new_bma_event_cb_table == NULL ) {
            log_e("rtc_event_cb_table realloc faild");
            while(true);
        }
        bma_event_cb_table = new_bma_event_cb_table;
    }

    bma_event_cb_table[ bma_event_cb_entrys - 1 ].event = event;
    bma_event_cb_table[ bma_event_cb_entrys - 1 ].event_cb = bma_event_cb;
    log_i("register rtc_event_cb success (%p)", bma_event_cb_table[ bma_event_cb_entrys - 1 ].event_cb );
}

void bma_send_event_cb( EventBits_t event, const char *msg ) {
    if ( bma_event_cb_entrys == 0 ) {
      return;
    }
      
    for ( int entry = 0 ; entry < bma_event_cb_entrys ; entry++ ) {
        yield();
        if ( event & bma_event_cb_table[ entry ].event ) {
            log_i("call bma_event_cb (%p)", bma_event_cb_table[ entry ].event_cb );
            bma_event_cb_table[ entry ].event_cb( event, msg );
        }
    }
}

void bma_save_config( void ) {
    if ( SPIFFS.exists( BMA_COFIG_FILE ) ) {
        SPIFFS.remove( BMA_COFIG_FILE );
        log_i("remove old binary bma config");
    }

    fs::File file = SPIFFS.open( BMA_JSON_COFIG_FILE, FILE_WRITE );

    if (!file) {
        log_e("Can't open file: %s!", BMA_JSON_COFIG_FILE );
    }
    else {
        SpiRamJsonDocument doc( 1000 );

        doc["stepcounter"] = bma_config[ BMA_STEPCOUNTER ].enable;
        doc["doubleclick"] = bma_config[ BMA_DOUBLECLICK ].enable;
        doc["tilt"] = bma_config[ BMA_TILT ].enable;

        if ( serializeJsonPretty( doc, file ) == 0) {
            log_e("Failed to write config file");
        }
        doc.clear();
    }
    file.close();
}

void bma_read_config( void ) {
    if ( SPIFFS.exists( BMA_JSON_COFIG_FILE ) ) {        
        fs::File file = SPIFFS.open( BMA_JSON_COFIG_FILE, FILE_READ );
        if (!file) {
            log_e("Can't open file: %s!", BMA_JSON_COFIG_FILE );
        }
        else {
            int filesize = file.size();
            SpiRamJsonDocument doc( filesize * 2 );

            DeserializationError error = deserializeJson( doc, file );
            if ( error ) {
                log_e("update check deserializeJson() failed: %s", error.c_str() );
            }
            else {
                bma_config[ BMA_STEPCOUNTER ].enable = doc["stepcounter"] | true;
                bma_config[ BMA_DOUBLECLICK ].enable = doc["doubleclick"] | true;
                bma_config[ BMA_TILT ].enable = doc["tilt"] | false;
            }        
            doc.clear();
        }
        file.close();
    }
    else {
        log_i("no json config exists, read from binary");
        fs::File file = SPIFFS.open( BMA_COFIG_FILE, FILE_READ );

        if (!file) {
            log_e("Can't open file: %s!", BMA_COFIG_FILE );
        }
        else {
            int filesize = file.size();
            if ( filesize > sizeof( bma_config ) ) {
                log_e("Failed to read configfile. Wrong filesize!" );
            }
            else {
                file.read( (uint8_t *)bma_config, filesize );
                file.close();
                bma_save_config();
                return; 
            }
        file.close();
        }
    }
}

bool bma_get_config( int config ) {
    if ( config < BMA_CONFIG_NUM ) {
        return( bma_config[ config ].enable );
    }
    return false;
}

void bma_set_config( int config, bool enable ) {
    if ( config < BMA_CONFIG_NUM ) {
        bma_config[ config ].enable = enable;
        bma_save_config();
        bma_reload_settings();
    }
}

void bma_set_rotate_tilt( uint32_t rotation ) {
    struct bma423_axes_remap remap_data;

    TTGOClass *ttgo = TTGOClass::getWatch();

    switch( rotation / 90 ) {
        case 0:     remap_data.x_axis = 0;
                    remap_data.x_axis_sign = 1;
                    remap_data.y_axis = 1;
                    remap_data.y_axis_sign = 1;
                    remap_data.z_axis  = 2;
                    remap_data.z_axis_sign  = 1;
                    ttgo->bma->set_remap_axes(&remap_data);
                    break;
        case 1:     remap_data.x_axis = 1;
                    remap_data.x_axis_sign = 1;
                    remap_data.y_axis = 0;
                    remap_data.y_axis_sign = 0;
                    remap_data.z_axis  = 2;
                    remap_data.z_axis_sign  = 1;
                    ttgo->bma->set_remap_axes(&remap_data);
                    break;
        case 2:     remap_data.x_axis = 0;
                    remap_data.x_axis_sign = 1;
                    remap_data.y_axis = 1;
                    remap_data.y_axis_sign = 0;
                    remap_data.z_axis  = 2;
                    remap_data.z_axis_sign  = 1;
                    ttgo->bma->set_remap_axes(&remap_data);
                    break;
        case 3:     remap_data.x_axis = 1;
                    remap_data.x_axis_sign = 1;
                    remap_data.y_axis = 0;
                    remap_data.y_axis_sign = 1;
                    remap_data.z_axis  = 2;
                    remap_data.z_axis_sign  = 1;
                    ttgo->bma->set_remap_axes(&remap_data);
                    break;
    }
}