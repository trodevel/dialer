/*

Call.

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

// $Id: call.cpp 1191 2014-10-23 17:44:23Z serge $

#include "call.h"                       // self

#include <boost/bind.hpp>               // boost::bind

#include "../voip_io/i_voip_service.h"  // IVoipService
#include "../utils/dummy_logger.h"      // dummy_log
#include "../utils/assert.h"            // ASSERT
#include "str_helper.h"                 // StrHelper
#include "i_call_callback.h"            // ICallCallback

#include "../asyncp/event.h"            // utils::new_event
#include "../utils/wrap_mutex.h"        // SCOPE_LOCK

#include "namespace_lib.h"              // NAMESPACE_DIALER_START

#define MODULENAME      "Call"

NAMESPACE_DIALER_START

Call::Call(
        uint32                        call_id,
        voip_service::IVoipService    * voips,
        sched::IScheduler             * sched,
        asyncp::IAsyncProxy           * proxy ):
    proxy_( proxy )
{
    impl_   = new CallImpl( call_id, voips, sched );

    ASSERT( proxy );
}

Call::~Call()
{
    if( impl_ )
    {
        delete impl_;
        impl_   = nullptr;
    }
}

bool Call::drop()
{
    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case DIALLING:
    case RINGING:
    case ENDED:
        dummy_log_warn( MODULENAME, "drop: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return false;

    case CONNECTED:
        break;

    default:
        dummy_log_error( MODULENAME, "drop: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return false;
    }

    return voips_->drop_call( call_id_ );
}

bool Call::set_input_file( const std::string & filename )
{
    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case DIALLING:
    case RINGING:
    case ENDED:
        dummy_log_warn( MODULENAME, "set_input_file: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return false;

    case CONNECTED:
        break;

    default:
        dummy_log_error( MODULENAME, "set_input_file: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return false;
    }

    bool b = player_.play_file( call_id_, filename );

    if( !b )
    {
        dummy_log_error( MODULENAME, "set_input_file: player failed" );
        return false;
    }

    return true;
}
bool Call::set_output_file( const std::string & filename )
{
    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case DIALLING:
    case RINGING:
    case ENDED:
        dummy_log_warn( MODULENAME, "set_output_file: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return false;

    case CONNECTED:
        break;

    default:
        dummy_log_error( MODULENAME, "set_output_file: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return false;
    }

    bool b = voips_->set_output_file( call_id_, filename );

    if( !b )
    {
        dummy_log_error( MODULENAME, "set_output_file: voip service failed" );
        return false;
    }

    return true;
}

bool Call::is_ended() const
{
    SCOPE_LOCK( mutex_ );

    return state_ == ENDED;
}

bool Call::is_active() const
{
    SCOPE_LOCK( mutex_ );

    return state_ == DIALLING || state_ == RINGING || state_ == CONNECTED;
}

bool Call::register_callback( ICallCallbackPtr callback )
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

void Call::on_error( uint32 errorcode )
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
        dummy_log_error( MODULENAME, "on_error: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

    if( player_.is_playing() )
        player_.stop();

    dummy_log_debug( MODULENAME, "on_error: clearing call id %u", call_id_ );

    call_id_    = 0;
    state_      = ENDED;

    if( callback_ )
    {
        proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &ICallCallback::on_error, callback_, errorcode ) ) ) );
    }
}
void Call::on_fatal_error( uint32 errorcode )
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
        dummy_log_error( MODULENAME, "on_fatal_error: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

    if( player_.is_playing() )
        player_.stop();

    dummy_log_debug( MODULENAME, "on_fatal_error: clearing call id %u", call_id_ );

    call_id_    = 0;
    state_      = ENDED;

    if( callback_ )
    {
        proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &ICallCallback::on_fatal_error, callback_, errorcode ) ) ) );
    }
}
void Call::on_dial()
{
    dummy_log_trace( MODULENAME, "on_dial:" );

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case DIALLING:
    case RINGING:
    case CONNECTED:
    case ENDED:
        dummy_log_warn( MODULENAME, "on_dial: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case IDLE:
        dummy_log_debug( MODULENAME, "on_dial: switching to DIALLING" );
        state_  = DIALLING;

        if( callback_ )
        {
            proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &ICallCallback::on_dial, callback_ ) ) ) );
        }

        break;

    default:
        dummy_log_error( MODULENAME, "on_dial: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}
void Call::on_ring()
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
        dummy_log_warn( MODULENAME, "on_ring: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case DIALLING:
        dummy_log_debug( MODULENAME, "on_ring: switching to RINGING" );
        state_  = RINGING;

        if( callback_ )
        {
            proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &ICallCallback::on_ring, callback_ ) ) ) );
        }

        break;

    default:
        dummy_log_error( MODULENAME, "on_ring: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}

void Call::on_connect()
{
    dummy_log_trace( MODULENAME, "on_connect:" );

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case CONNECTED:
    case ENDED:
        dummy_log_warn( MODULENAME, "on_connect: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case DIALLING:
        dummy_log_debug( MODULENAME, "on_connect: switching to CONNECTED ***** valid for PSTN calls *****" );
        state_  = CONNECTED;

        if( callback_ )
        {
            proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &ICallCallback::on_connect, callback_ ) ) ) );
        }

        break;


    case RINGING:
        dummy_log_debug( MODULENAME, "on_connect: switching to CONNECTED" );
        state_  = CONNECTED;

        if( callback_ )
        {
            proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &ICallCallback::on_connect, callback_ ) ) ) );
        }

        break;

    default:
        dummy_log_error( MODULENAME, "on_connect: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}

void Call::on_call_duration( uint32 t )
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
        dummy_log_warn( MODULENAME, "on_call_duration: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case CONNECTED:

        if( callback_ )
        {
            proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &ICallCallback::on_call_duration, callback_, t ) ) ) );
        }

        break;

    default:
        dummy_log_error( MODULENAME, "on_call_duration: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}

void Call::on_play_start()
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
        dummy_log_warn( MODULENAME, "on_play_start: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case CONNECTED:
        dummy_log_debug( MODULENAME, "on_play_start: ok" );
        break;

    default:
        dummy_log_error( MODULENAME, "on_play_start: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

    player_.on_play_start( call_id_ );
}

void Call::on_play_stop()
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
        dummy_log_warn( MODULENAME, "on_play_stop: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case CONNECTED:
        dummy_log_debug( MODULENAME, "on_play_stop: ok" );
        break;

    default:
        dummy_log_error( MODULENAME, "on_play_stop: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

    player_.on_play_stop( call_id_ );
}


void Call::on_call_end( uint32 errorcode )
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
        dummy_log_warn( MODULENAME, "on_call_end: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case CONNECTED:
        if( player_.is_playing() )
            player_.stop();

        dummy_log_debug( MODULENAME, "on_call_end: switching to IDLE" );
        state_      = ENDED;
        call_id_    = 0;

        if( callback_ )
        {
            proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &ICallCallback::on_call_end, callback_, errorcode ) ) ) );
        }

        break;

    default:
        dummy_log_error( MODULENAME, "on_call_end: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}


NAMESPACE_DIALER_END
