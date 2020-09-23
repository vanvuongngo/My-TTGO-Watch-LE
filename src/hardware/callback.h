/****************************************************************************
 *   Sep 14 08:11:10 2020
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
#ifndef _CALLBACK_H
    #define _CALLBACK_H

    #include "config.h"

    typedef bool ( * CALLBACK_FUNC ) ( EventBits_t event, void *arg );

    typedef struct {
        EventBits_t event;
        CALLBACK_FUNC callback_func;
        const char *id;
        uint64_t counter;
    } callback_table_t;

    typedef struct {
        uint32_t entrys;
        callback_table_t *table;
        const char *name;
    } callback_t;

    /**
     * @brief init the callback structure
     * 
     * @param   name        pointer to an string thats contains the name for the callback table
     * 
     * @return  pointer to a callback_t structure if success, NULL if failed
     */
    callback_t *callback_init( const char *name );
    /**
     * @brief   register an callback function
     * 
     * @param   callback        pointer to a callback_t structure
     * @param   event           event filter mask
     * @param   callback_func   pointer to a callbackfunc
     * @param   id              pointer to an string thats contains the id aka name for the callback function
     * 
     * @return  true if success, false if failed
     */
    bool callback_register( callback_t *callback, EventBits_t event, CALLBACK_FUNC callback_func, const char *id );
    /**
     * @brief   call all callback function thats match with the event filter mask
     * 
     * @param   callback        pointer to a callback_t structure
     * @param   event           event filter mask
     * @param   arg             argument for the called callback function
     * 
     * @return  true if success, false if failed
     */
    bool callback_send( callback_t *callback, EventBits_t event, void *arg );
    /**
     * @brief   call all callback function thats match with the event filter mask without logging
     * 
     * @param   callback        pointer to a callback_t structure
     * @param   event           event filter mask
     * @param   arg             argument for the called callback function
     * 
     * @return  true if success, false if failed
     */
    bool callback_send_no_log( callback_t *callback, EventBits_t event, void *arg );
    /**
     * @brief enable/disable SPIFFS event logging
     * 
     * @param enable    true if logging enabled, false if logging disabled
     */
    void display_event_logging_enable( bool enable );

#endif // _CALLBACK_H