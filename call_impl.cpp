/*

CallImpl.

Copyright (C) 2014 Sergey Kolevatov

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

*/

// $Id: call_impl.cpp 1231 2014-10-30 18:24:13Z serge $

#include "call_impl.h"                  // self

#include "../voip_io/i_voip_service.h"  // IVoipService
#include "../utils/dummy_logger.h"      // dummy_log
#include "../utils/assert.h"            // ASSERT
#include "str_helper.h"                 // StrHelper

#include "../utils/wrap_mutex.h"        // SCOPE_LOCK

#include "namespace_lib.h"              // NAMESPACE_DIALER_START

#define MODULENAME      "Call"

NAMESPACE_DIALER_START

CallImpl::CallImpl(
        uint32                        call_id,
        voip_service::IVoipService    * voips,
        sched::IScheduler             * sched ):
    state_( IDLE ), voips_( voips ), sched_( sched ), call_id_( call_id ), callback_( 0L )
{
    player_.init( voips, sched );
}

CallImpl::~CallImpl()
{
}

//CallImpl::state_e CallImpl::get_state() const
//{
//    SCOPE_LOCK( mutex_ );
//
//    return state_;
//}

uint32 CallImpl::get_id() const
{
    SCOPE_LOCK( mutex_ );

    return call_id_;
}

void CallImpl::drop()
{
    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case DIALLING:
    case RINGING:
    case ENDED:
        dummy_log_fatal( MODULENAME, "drop: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case CONNECTED:
        break;

    default:
        dummy_log_fatal( MODULENAME, "drop: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }

    voips_->drop_call( call_id_ );
}

void CallImpl::set_input_file( const std::string & filename )
{
    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case DIALLING:
    case RINGING:
    case ENDED:
        dummy_log_fatal( MODULENAME, "set_input_file: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case CONNECTED:
        break;

    default:
        dummy_log_fatal( MODULENAME, "set_input_file: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }

    bool b = player_.play_file( call_id_, filename );

    if( !b )
    {
        dummy_log_error( MODULENAME, "set_input_file: player failed" );
    }
}
void CallImpl::set_output_file( const std::string & filename )
{
    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case DIALLING:
    case RINGING:
    case ENDED:
        dummy_log_fatal( MODULENAME, "set_output_file: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case CONNECTED:
        break;

    default:
        dummy_log_fatal( MODULENAME, "set_output_file: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }

    bool b = voips_->set_output_file( call_id_, filename );

    if( !b )
    {
        dummy_log_error( MODULENAME, "set_output_file: voip service failed" );
    }
}

bool CallImpl::register_callback( ICallCallbackPtr callback )
{
    if( callback == 0L )
        return false;

    SCOPE_LOCK( mutex_ );

    if( callback_ != 0L )
        return false;

    callback_ = callback;

    //callback->on_registered( true );

    return true;
}

void CallImpl::on_error( uint32 errorcode )
{
    dummy_log_trace( MODULENAME, "on_error: %u", errorcode );

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case ENDED:
        return;

    case DIALLING:
    case RINGING:
    case CONNECTED:
        break;

    default:
        dummy_log_fatal( MODULENAME, "on_error: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }

    if( player_.is_playing() )
        player_.stop();

    dummy_log_debug( MODULENAME, "on_error: clearing call id %u", call_id_ );

    call_id_    = 0;
    state_      = ENDED;

    if( callback_ )
    {
        callback_->on_error( errorcode );
    }
}
void CallImpl::on_fatal_error( uint32 errorcode )
{
    dummy_log_trace( MODULENAME, "on_fatal_error: %u", errorcode );

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case ENDED:
        return;

    case DIALLING:
    case RINGING:
    case CONNECTED:
        break;

    default:
        dummy_log_fatal( MODULENAME, "on_fatal_error: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }

    if( player_.is_playing() )
        player_.stop();

    dummy_log_debug( MODULENAME, "on_fatal_error: clearing call id %u", call_id_ );

    call_id_    = 0;
    state_      = ENDED;

    if( callback_ )
    {
        callback_->on_fatal_error( errorcode );
    }
}
void CallImpl::on_dial()
{
    dummy_log_trace( MODULENAME, "on_dial:" );

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case RINGING:
    case CONNECTED:
    case ENDED:
        dummy_log_fatal( MODULENAME, "on_dial: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case DIALLING:
        dummy_log_warn( MODULENAME, "on_dial: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        break;

    case IDLE:
        dummy_log_debug( MODULENAME, "on_dial: switching to DIALLING" );
        state_  = DIALLING;

        if( callback_ )
        {
            callback_->on_dial();
        }

        break;

    default:
        dummy_log_fatal( MODULENAME, "on_dial: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }
}
void CallImpl::on_ring()
{
    dummy_log_trace( MODULENAME, "on_ring:");

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case RINGING:
    case CONNECTED:
    case ENDED:
        dummy_log_fatal( MODULENAME, "on_ring: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case DIALLING:
        dummy_log_debug( MODULENAME, "on_ring: switching to RINGING" );
        state_  = RINGING;

        if( callback_ )
        {
            callback_->on_ring();
        }

        break;

    default:
        dummy_log_fatal( MODULENAME, "on_ring: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }
}

void CallImpl::on_connect()
{
    dummy_log_trace( MODULENAME, "on_connect:" );

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case CONNECTED:
    case ENDED:
        dummy_log_fatal( MODULENAME, "on_connect: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case DIALLING:
        dummy_log_debug( MODULENAME, "on_connect: switching to CONNECTED ***** valid for PSTN calls *****" );
        state_  = CONNECTED;

        if( callback_ )
        {
            callback_->on_connect();
        }

        break;


    case RINGING:
        dummy_log_debug( MODULENAME, "on_connect: switching to CONNECTED" );
        state_  = CONNECTED;

        if( callback_ )
        {
            callback_->on_connect();
        }

        break;

    default:
        dummy_log_fatal( MODULENAME, "on_connect: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }
}

void CallImpl::on_call_duration( uint32 t )
{
    dummy_log_trace( MODULENAME, "on_call_duration:" );

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case DIALLING:
    case RINGING:
    case ENDED:
        dummy_log_fatal( MODULENAME, "on_call_duration: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case CONNECTED:

        if( callback_ )
        {
            callback_->on_call_duration( t );
        }

        break;

    default:
        dummy_log_fatal( MODULENAME, "on_call_duration: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }
}

void CallImpl::on_play_start()
{
    dummy_log_trace( MODULENAME, "on_play_start:");

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case DIALLING:
    case RINGING:
    case ENDED:
        dummy_log_fatal( MODULENAME, "on_play_start: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case CONNECTED:
        dummy_log_debug( MODULENAME, "on_play_start: ok" );
        break;

    default:
        dummy_log_fatal( MODULENAME, "on_play_start: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }

    player_.on_play_start( call_id_ );
}

void CallImpl::on_play_stop()
{
    dummy_log_trace( MODULENAME, "on_play_stop:");

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case DIALLING:
    case RINGING:
    case ENDED:
        dummy_log_fatal( MODULENAME, "on_play_stop: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case CONNECTED:
        dummy_log_debug( MODULENAME, "on_play_stop: ok" );
        break;

    default:
        dummy_log_fatal( MODULENAME, "on_play_stop: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }

    player_.on_play_stop( call_id_ );
}


void CallImpl::on_call_end( uint32 errorcode )
{
    dummy_log_trace( MODULENAME, "on_call_end: %u", errorcode );

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case DIALLING:
    case RINGING:
    case ENDED:
        dummy_log_fatal( MODULENAME, "on_call_end: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case CONNECTED:
        if( player_.is_playing() )
            player_.stop();

        dummy_log_debug( MODULENAME, "on_call_end: switching to IDLE" );
        state_      = ENDED;
        call_id_    = 0;

        if( callback_ )
        {
            callback_->on_call_end( errorcode );
        }

        break;

    default:
        dummy_log_fatal( MODULENAME, "on_call_end: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }
}


NAMESPACE_DIALER_END
