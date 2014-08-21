/*

Dialer.

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

// $Id: dialer.cpp 971 2014-08-20 18:09:32Z serge $

#include "dialer.h"                 // state

#include "../voip_io/i_voip_service.h"  // IVoipService
#include "../utils/dummy_logger.h"      // dummy_log
#include "str_helper.h"                 // StrHelper

#include "../utils/wrap_mutex.h"        // SCOPE_LOCK

#include "namespace_lib.h"          // NAMESPACE_DIALER_START

#define MODULENAME      "Dialer"

NAMESPACE_DIALER_START

Dialer::Dialer():
    state_( UNKNOWN ), voips_( 0L ), sched_( 0L ), proxy_( 0L ), callback_( 0L )
{
}

Dialer::~Dialer()
{
}

bool Dialer::init(
        voip_service::IVoipService  * voips,
        sched::IScheduler           * sched,
        asyncp::IAsyncProxy         * proxy )
{
	SCOPE_LOCK( mutex_ );

    if( !voips || !sched || !proxy )
        return false;

    voips_      = voips;
    sched_      = sched;
    proxy_      = proxy;
    //state_  = IDLE;

    return true;
}

bool Dialer::register_callback( IDialerCallback * callback )
{
    if( callback == 0L )
        return false;

    SCOPE_LOCK( mutex_ );

    if( callback_ != 0L )
        return false;

    callback_ = callback;

    callback->on_registered( true );

    return true;
}

bool Dialer::is_inited() const
{
    SCOPE_LOCK( mutex_ );

    return is_inited__();
}

bool Dialer::is_inited__() const
{
    if( !voips_ || !sched_ )
        return false;

    return true;
}

Dialer::state_e Dialer::get_state() const
{
    SCOPE_LOCK( mutex_ );

    return state_;
}

boost::shared_ptr< CallI > Dialer::get_call()
{
    SCOPE_LOCK( mutex_ );

    return boost::static_pointer_cast< CallI >( call_ );
}

bool Dialer::initiate_call( const std::string & party, uint32 & status )
{
    dummy_log_trace( MODULENAME, "initiate_call: %s", party.c_str());

    SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return false;

    switch( state_ )
    {
    case BUSY:
        dummy_log_warn( MODULENAME, "initiate_call: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return false;

    case IDLE:
    {
        uint32 call_id = 0;

        bool b = voips_->initiate_call( party, call_id, status );

        dummy_log_debug( MODULENAME, "initiate_call: call id = %u, status = %u, result = %u", call_id, status, b );

        if( !b )
        {
            dummy_log_error( MODULENAME, "initiate_call: voip service failed" );
            return false;
        }

        call_.reset( new Call( call_id, voips_, sched_, proxy_ ) );
    }
        return true;


    default:
        dummy_log_error( MODULENAME, "initiate_call: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return false;
    }

    return false;
}
bool Dialer::drop_all_calls()
{
    return true;
}

bool Dialer::shutdown()
{
    SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return false;

    return voips_->shutdown();
}


void Dialer::on_ready( uint32 errorcode )
{
    dummy_log_trace( MODULENAME, "on_ready: %u", errorcode );

    SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return;

    switch( state_ )
    {
    case UNKNOWN:
        dummy_log_debug( MODULENAME, "on_ready: switching to IDLE" );
        state_  = IDLE;
        break;

    case IDLE:
    case BUSY:
        dummy_log_warn( MODULENAME, "on_ready: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    default:
        dummy_log_error( MODULENAME, "on_ready: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}
void Dialer::on_error( uint32 call_id, uint32 errorcode )
{
    dummy_log_trace( MODULENAME, "on_error: %u", errorcode );

    SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return;

    switch( state_ )
    {
    case IDLE:
        dummy_log_warn( MODULENAME, "on_error: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case BUSY:
    {
        if( !is_call_id_valid( call_id ) )
        {
            dummy_log_error( MODULENAME, "on_error: invalid call id %u", call_id );
            return;
        }

        call_->on_error( errorcode );

        check_call_end( "on_error" );
    }
        break;

    default:
        dummy_log_error( MODULENAME, "on_error: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

}
void Dialer::on_dial( uint32 call_id )
{
    dummy_log_trace( MODULENAME, "on_dial: %u", call_id );

    SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return;

    switch( state_ )
    {
    case BUSY:
        dummy_log_warn( MODULENAME, "on_dial: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case IDLE:
    {
        if( !is_call_id_valid( call_id ) )
        {
            dummy_log_error( MODULENAME, "on_dial: invalid call id %u", call_id );
            return;
        }

        call_->on_dial();

        if( call_->is_active() )
        {
            dummy_log_debug( MODULENAME, "on_dial: switching to BUSY" );
            state_ = BUSY;

            if( callback_ )
                callback_->on_busy();
        }
    }
        break;

    default:
        dummy_log_error( MODULENAME, "on_dial: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}
void Dialer::on_ring( uint32 call_id )
{
    dummy_log_trace( MODULENAME, "on_ring: %u", call_id );

    SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return;

    switch( state_ )
    {
    case IDLE:
        dummy_log_warn( MODULENAME, "on_ring: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case BUSY:
    {
        if( !is_call_id_valid( call_id ) )
        {
            dummy_log_error( MODULENAME, "on_ring: invalid call id %u", call_id );
            return;
        }

        call_->on_ring();

        check_call_end( "on_ring" );
    }
        break;

    default:
        dummy_log_error( MODULENAME, "on_ring: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}

void Dialer::on_connect( uint32 call_id )
{
    dummy_log_trace( MODULENAME, "on_connect: %u", call_id );

    SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return;

    switch( state_ )
    {
    case IDLE:
        dummy_log_warn( MODULENAME, "on_connect: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case BUSY:
    {
        if( !is_call_id_valid( call_id ) )
        {
            dummy_log_error( MODULENAME, "on_connect: invalid call id %u", call_id );
            return;
        }

        call_->on_connect();

        check_call_end( "on_connect" );
    }
        break;

    default:
        dummy_log_error( MODULENAME, "on_connect: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}

void Dialer::on_call_duration( uint32 call_id, uint32 t )
{
    dummy_log_trace( MODULENAME, "on_call_duration: %u", call_id );

    SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return;

    switch( state_ )
    {
    case IDLE:
        dummy_log_warn( MODULENAME, "on_call_duration: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case BUSY:
    {
        if( !is_call_id_valid( call_id ) )
        {
            dummy_log_error( MODULENAME, "on_call_duration: invalid call id %u", call_id );
            return;
        }

        call_->on_call_duration( t );
    }
        break;

    default:
        dummy_log_error( MODULENAME, "on_call_duration: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}

void Dialer::on_play_start( uint32 call_id )
{
    dummy_log_trace( MODULENAME, "on_play_start: %u", call_id );

    SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return;

    switch( state_ )
    {
    case IDLE:
        dummy_log_warn( MODULENAME, "on_play_start: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case BUSY:
    {
        if( !is_call_id_valid( call_id ) )
        {
            dummy_log_error( MODULENAME, "on_play_start: invalid call id %u", call_id );
            return;
        }

        call_->on_play_start();

        check_call_end( "on_play_start" );
    }
        break;

    default:
        dummy_log_error( MODULENAME, "on_play_start: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}

void Dialer::on_play_stop( uint32 call_id )
{
    dummy_log_trace( MODULENAME, "on_play_stop: %u", call_id );

    SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return;

    switch( state_ )
    {
    case IDLE:
        dummy_log_warn( MODULENAME, "on_play_stop: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case BUSY:
    {
        if( !is_call_id_valid( call_id ) )
        {
            dummy_log_error( MODULENAME, "on_play_stop: invalid call id %u", call_id );
            return;
        }

        call_->on_play_stop();

        check_call_end( "on_play_stop" );
    }
        break;

    default:
        dummy_log_error( MODULENAME, "on_play_stop: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}


void Dialer::on_call_end( uint32 call_id, uint32 errorcode )
{
    dummy_log_trace( MODULENAME, "on_call_end: %u %u", call_id, errorcode );

    SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return;

    switch( state_ )
    {
    case IDLE:
        dummy_log_warn( MODULENAME, "on_call_end: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case BUSY:
    {
        if( !is_call_id_valid( call_id ) )
        {
            dummy_log_error( MODULENAME, "on_call_end: invalid call id %u", call_id );
            return;
        }

        call_->on_call_end( errorcode );

        check_call_end( "on_call_end" );
    }
        break;

    default:
        dummy_log_error( MODULENAME, "on_call_end: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}

bool Dialer::is_call_id_valid( uint32 call_id ) const
{
    if( call_ == 0L )
        return false;

    return call_id == call_->get_id();
}

void Dialer::check_call_end( const char * event_name )
{
    if( call_->is_ended() )
    {
        dummy_log_debug( MODULENAME, "%s: switching to IDLE", event_name );
        state_ = IDLE;

        if( callback_ )
            callback_->on_ready();
    }

}


NAMESPACE_DIALER_END
