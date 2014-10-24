/*

DialerImpl.

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

// $Id: dialer_impl.cpp 1207 2014-10-24 20:25:55Z serge $

#include "dialer_impl.h"                // self

#include "../voip_io/i_voip_service.h"  // IVoipService
#include "../utils/dummy_logger.h"      // dummy_log
#include "../asyncp/i_async_proxy.h"    // IAsyncProxy
#include "../asyncp/event.h"            // new_event
#include "str_helper.h"                 // StrHelper

#include "../utils/wrap_mutex.h"        // SCOPE_LOCK
#include "../utils/assert.h"            // ASSERT

#include "namespace_lib.h"          // NAMESPACE_DIALER_START

#define MODULENAME      "Dialer"

NAMESPACE_DIALER_START

DialerImpl::DialerImpl():
    state_( UNKNOWN ), voips_( 0L ), sched_( 0L ), callback_( 0L ), proxy_( nullptr ), call_id_( 0 )
{
}

DialerImpl::~DialerImpl()
{
}

bool DialerImpl::init(
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

bool DialerImpl::register_callback( IDialerCallback * callback )
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

bool DialerImpl::is_inited() const
{
    SCOPE_LOCK( mutex_ );

    return is_inited__();
}

bool DialerImpl::is_inited__() const
{
    if( !voips_ || !sched_ )
        return false;

    return true;
}

DialerImpl::state_e DialerImpl::get_state() const
{
    SCOPE_LOCK( mutex_ );

    return state_;
}

boost::shared_ptr< CallI > DialerImpl::get_call()
{
    // private: no mutex

    return boost::static_pointer_cast< CallI >( call_ );
}

void DialerImpl::initiate_call( const std::string & party )
{
    dummy_log_debug( MODULENAME, "initiate_call: %s", party.c_str());

    SCOPE_LOCK( mutex_ );

    ASSERT( is_inited__() );

    switch( state_ )
    {
    case BUSY:
        dummy_log_warn( MODULENAME, "initiate_call: ignored in state %s", StrHelper::to_string( state_ ).c_str() );

        if( callback_ )
            callback_->on_call_initiate_response( false, 0, CallIPtr() );

        return;

    case IDLE:
    {
        uint32 call_id = 0;
        uint32 status   = 0;

        bool b = voips_->initiate_call( party, call_id, status );

        dummy_log_debug( MODULENAME, "initiate_call: call id = %u, status = %u, result = %u", call_id, status, b );

        if( !b )
        {
            dummy_log_error( MODULENAME, "initiate_call: voip service failed" );

            if( callback_ )
                callback_->on_call_initiate_response( b, status, CallIPtr() );

            return;
        }

        call_.reset( new Call( call_id, voips_, sched_, proxy_ ) );
        call_id_    = call_id;

        if( callback_ )
            callback_->on_call_initiate_response( b, status, get_call() );


//        if( callback_ )
//            callback_->on_busy();

        state_  = BUSY;
    }
        return;


    default:
        dummy_log_error( MODULENAME, "initiate_call: invalid state %s", StrHelper::to_string( state_ ).c_str() );

        if( callback_ )
            callback_->on_call_initiate_response( false, 0, CallIPtr() );

        return;
    }
}
void DialerImpl::drop_all_calls()
{
}

bool DialerImpl::shutdown()
{
    SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return false;

    return voips_->shutdown();
}


void DialerImpl::on_ready( uint32 errorcode )
{
    dummy_log_debug( MODULENAME, "on_ready: %u", errorcode );

    SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return;

    switch( state_ )
    {
    case UNKNOWN:
        dummy_log_debug( MODULENAME, "on_ready: switching to IDLE" );
        state_      = IDLE;
        call_id_    = 0;
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
void DialerImpl::on_error( uint32 call_id, uint32 errorcode )
{
    dummy_log_debug( MODULENAME, "on_error: %u", errorcode );

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
        ASSERT( is_call_id_valid( call_id ) );

        call_->on_error( errorcode );

        check_call_end( "on_error" );
    }
        break;

    default:
        dummy_log_error( MODULENAME, "on_error: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

}
void DialerImpl::on_fatal_error( uint32 call_id, uint32 errorcode )
{
    dummy_log_debug( MODULENAME, "on_fatal_error: %u", errorcode );

    SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return;

    switch( state_ )
    {
    case IDLE:
        dummy_log_warn( MODULENAME, "on_fatal_error: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case BUSY:
    {
        ASSERT( is_call_id_valid( call_id ) );

        call_->on_error( errorcode );

        check_call_end( "on_error" );
    }
        break;

    default:
        dummy_log_error( MODULENAME, "on_fatal_error: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

}
void DialerImpl::on_dial( uint32 call_id )
{
    dummy_log_debug( MODULENAME, "on_dial: %u", call_id );

    SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return;

    switch( state_ )
    {
    case IDLE:
        dummy_log_warn( MODULENAME, "on_dial: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case BUSY:
    {
        ASSERT( is_call_id_valid( call_id ) );

        call_->on_dial();
    }
        break;

    default:
        dummy_log_error( MODULENAME, "on_dial: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}
void DialerImpl::on_ring( uint32 call_id )
{
    dummy_log_debug( MODULENAME, "on_ring: %u", call_id );

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
        ASSERT( is_call_id_valid( call_id ) );

        call_->on_ring();

        check_call_end( "on_ring" );
    }
        break;

    default:
        dummy_log_error( MODULENAME, "on_ring: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}

void DialerImpl::on_connect( uint32 call_id )
{
    dummy_log_debug( MODULENAME, "on_connect: %u", call_id );

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
        ASSERT( is_call_id_valid( call_id ) );

        call_->on_connect();

        check_call_end( "on_connect" );
    }
        break;

    default:
        dummy_log_error( MODULENAME, "on_connect: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}

void DialerImpl::on_call_duration( uint32 call_id, uint32 t )
{
    dummy_log_debug( MODULENAME, "on_call_duration: %u", call_id );

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
        ASSERT( is_call_id_valid( call_id ) );

        call_->on_call_duration( t );
    }
        break;

    default:
        dummy_log_error( MODULENAME, "on_call_duration: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}

void DialerImpl::on_play_start( uint32 call_id )
{
    dummy_log_debug( MODULENAME, "on_play_start: %u", call_id );

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
        ASSERT( is_call_id_valid( call_id ) );

        call_->on_play_start();

        check_call_end( "on_play_start" );
    }
        break;

    default:
        dummy_log_error( MODULENAME, "on_play_start: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}

void DialerImpl::on_play_stop( uint32 call_id )
{
    dummy_log_debug( MODULENAME, "on_play_stop: %u", call_id );

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
        ASSERT( is_call_id_valid( call_id ) );

        call_->on_play_stop();

        check_call_end( "on_play_stop" );
    }
        break;

    default:
        dummy_log_error( MODULENAME, "on_play_stop: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}


void DialerImpl::on_call_end( uint32 call_id, uint32 errorcode )
{
    dummy_log_debug( MODULENAME, "on_call_end: %u %u", call_id, errorcode );

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
        ASSERT( is_call_id_valid( call_id ) );

        call_->on_call_end( errorcode );

        check_call_end( "on_call_end" );
    }
        break;

    default:
        dummy_log_error( MODULENAME, "on_call_end: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }
}

bool DialerImpl::is_call_id_valid( uint32 call_id ) const
{
    if( call_ == 0L )
        return false;

    return call_id == call_id_;
}

void DialerImpl::check_call_end( const char * event_name )
{
    if( call_->is_ended() )
    {
        dummy_log_debug( MODULENAME, "%s: switching to IDLE", event_name );
        state_      = IDLE;
        call_id_    = 0;

        if( callback_ )
            callback_->on_ready();
    }

}


NAMESPACE_DIALER_END
