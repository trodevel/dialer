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

// $Id: dialer_impl.cpp 1279 2014-12-23 18:24:10Z serge $

#include "dialer_impl.h"                // self

#include "../voip_io/i_voip_service.h"  // IVoipService
#include "../voip_io/object_factory.h"  // voip_service::create_message_t
#include "../utils/dummy_logger.h"      // dummy_log
#include "../asyncp/i_async_proxy.h"    // IAsyncProxy
#include "../asyncp/event.h"            // new_event
#include "str_helper.h"                 // StrHelper

#include "../utils/wrap_mutex.h"        // SCOPE_LOCK
#include "../utils/assert.h"            // ASSERT

#include "namespace_lib.h"          // NAMESPACE_DIALER_START

#define MODULENAME      "Dialer"

NAMESPACE_DIALER_START

class Dialer;

DialerImpl::DialerImpl():
    state_( UNKNOWN ), voips_( 0L ), sched_( 0L ), callback_( 0L ), call_id_( 0 )
{
}

DialerImpl::~DialerImpl()
{
}

bool DialerImpl::init(
        voip_service::IVoipService  * voips,
        sched::IScheduler           * sched )
{
	SCOPE_LOCK( mutex_ );

    if( !voips || !sched )
        return false;

    voips_      = voips;
    sched_      = sched;
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

void Dialer::consume( const DialerObject * req )
{
    ServerBase::consume( req );
}

// interface IVoipServiceCallback
void Dialer::consume( const voip_service::VoipioCallbackObject * req )
{
    ServerBase::consume( req );
}

void Dialer::handle( const servt::IObject* req )
{
    SCOPE_LOCK( mutex_ );

    if( typeid( *req ) == typeid( DialerInitiateCallRequest ) )
    {
        handle( dynamic_cast< const DialerInitiateCallRequest *>( req ) );
    }
    else if( typeid( *req ) == typeid( DialerPlayFile ) )
    {
        handle( dynamic_cast< const DialerPlayFile *>( req ) );
    }
    else if( typeid( *req ) == typeid( DialerDrop ) )
    {
        handle( dynamic_cast< const DialerDrop *>( req ) );
    }
    else
    {
        dummy_log_fatal( MODULENAME, "handle: cannot cast request to known type - %p", (void *) req );

        ASSERT( 0 );
    }

    delete req;
}


void DialerImpl::handle( const DialerInitiateCallRequest * req )
{
    dummy_log_debug( MODULENAME, "initiate_call: %s", req->party.c_str() );

    ASSERT( is_inited__() );

    switch( state_ )
    {
    case WAITING_VOIP_RESPONSE:
    case DIALLING:
    case RINGING:
    case CONNECTED:
        dummy_log_warn( MODULENAME, "initiate_call: busy, ignored in state %s", StrHelper::to_string( state_ ).c_str() );

        if( callback_ )
            callback_->on_error_response( 0, "busy, cannot proceed in state " + StrHelper::to_string( state_ ) );

        return;

    case IDLE:
    {
        uint32 call_id = 0;
        uint32 status   = 0;

        voips_->consume( voip_service::create_initiate_call_request( req->party ) );

        dummy_log_debug( MODULENAME, "initiate_call: call id = %u, status = %u, result = %u", call_id, status, b );

        call_id_    = call_id;

        if( callback_ )
            callback_->on_call_initiate_response( call_id, status );


        state_  = WAITING_VOIP_RESPONSE;
    }
        return;


    default:
        dummy_log_fatal( MODULENAME, "initiate_call: invalid state %s", StrHelper::to_string( state_ ).c_str() );

        ASSERT( 0 );
    }
}
void DialerImpl::handle( const DialerDrop * req )
{
    dummy_log_debug( MODULENAME, "drop" );

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case IDLE:
    case WAITING_VOIP_RESPONSE:
    case DIALLING:
    case RINGING:
        dummy_log_fatal( MODULENAME, "drop: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case CONNECTED:
    {
        ASSERT( is_call_id_valid( req->call_id ) );

        voips_->drop_call( req->call_id );

        dummy_log_info( MODULENAME, "drop: switching to IDLE" );

        state_      = IDLE;
        call_id_    = 0;

        if( callback_ )
            callback_->on_ready();
    }
        break;

    default:
        dummy_log_fatal( MODULENAME, "drop: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }
}

void DialerImpl::handle( const DialerPlayFile * req )
{
    dummy_log_debug( MODULENAME, "set_input_file" );

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case IDLE:
    case WAITING_VOIP_RESPONSE:
    case DIALLING:
    case RINGING:
        dummy_log_fatal( MODULENAME, "set_input_file: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case CONNECTED:
    {
        ASSERT( is_call_id_valid( req->call_id ) );

        bool b = player_.play_file( req->call_id, req->filename );

        if( b == false )
        {
            dummy_log_error( MODULENAME, "set_input_file: player failed" );
        }
    }
        break;

    default:
        dummy_log_fatal( MODULENAME, "set_input_file: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }
}

void DialerImpl::handle( const DialerRecordFile * req )
{
    dummy_log_debug( MODULENAME, "set_output_file" );

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case IDLE:
    case WAITING_VOIP_RESPONSE:
    case DIALLING:
    case RINGING:
        dummy_log_fatal( MODULENAME, "set_output_file: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case CONNECTED:
    {
        ASSERT( is_call_id_valid( req->call_id ) );

        bool b = voips_->set_output_file( req->call_id, req->filename );

        if( b == false )
        {
            dummy_log_error( MODULENAME, "set_output_file: voip service failed" );
        }
    }
        break;

    default:
        dummy_log_fatal( MODULENAME, "set_output_file: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }
}

bool DialerImpl::shutdown()
{
    SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return false;

    return ServerBase::shutdown();
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
    case WAITING_VOIP_RESPONSE:
    case DIALLING:
    case RINGING:
        dummy_log_warn( MODULENAME, "on_ready: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    default:
        dummy_log_fatal( MODULENAME, "on_ready: invalid state %s", StrHelper::to_string( state_ ).c_str() );

        ASSERT( 0 );
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
        dummy_log_warn( MODULENAME, "on_error: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case WAITING_VOIP_RESPONSE:
    case DIALLING:
    case RINGING:
    case CONNECTED:
    {
        ASSERT( is_call_id_valid( call_id ) );

        if( player_.is_playing() )
            player_.stop();

        if( callback_ )
            callback_->on_error( call_id, errorcode );

        dummy_log_info( MODULENAME, "on_error: switching to IDLE" );

        state_      = IDLE;
        call_id_    = 0;

        if( callback_ )
            callback_->on_ready();
    }
        break;

    default:
        dummy_log_fatal( MODULENAME, "on_error: invalid state %s", StrHelper::to_string( state_ ).c_str() );

        ASSERT( 0 );
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
        dummy_log_warn( MODULENAME, "on_fatal_error: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case WAITING_VOIP_RESPONSE:
    case DIALLING:
    case RINGING:
    case CONNECTED:
    {
        ASSERT( is_call_id_valid( call_id ) );

        if( player_.is_playing() )
            player_.stop();

        if( callback_ )
            callback_->on_fatal_error( call_id, errorcode );

        dummy_log_info( MODULENAME, "on_fatal_error: switching to IDLE" );

        state_      = IDLE;
        call_id_    = 0;

        if( callback_ )
            callback_->on_ready();
    }
        break;

    default:
        dummy_log_fatal( MODULENAME, "on_fatal_error: invalid state %s", StrHelper::to_string( state_ ).c_str() );

        ASSERT( 0 );
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
    case RINGING:
    case CONNECTED:
        dummy_log_fatal( MODULENAME, "on_dial: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case DIALLING:
        dummy_log_warn( MODULENAME, "on_dial: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        break;


    case WAITING_VOIP_RESPONSE:
    {
        ASSERT( is_call_id_valid( call_id ) );

        state_  = DIALLING;

        if( callback_ )
            callback_->on_dial( call_id );
    }
        break;

    default:
        dummy_log_fatal( MODULENAME, "on_dial: invalid state %s", StrHelper::to_string( state_ ).c_str() );

        ASSERT( 0 );
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
    case WAITING_VOIP_RESPONSE:
    case CONNECTED:
        dummy_log_fatal( MODULENAME, "on_ring: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case RINGING:
        dummy_log_warn( MODULENAME, "on_ring: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case DIALLING:
    {
        ASSERT( is_call_id_valid( call_id ) );

        if( callback_ )
            callback_->on_ring( call_id );
    }
        break;

    default:
        dummy_log_fatal( MODULENAME, "on_ring: invalid state %s", StrHelper::to_string( state_ ).c_str() );

        ASSERT( 0 );
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
    case WAITING_VOIP_RESPONSE:
    case CONNECTED:
        dummy_log_fatal( MODULENAME, "on_connect: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case DIALLING:
    {
        ASSERT( is_call_id_valid( call_id ) );

        dummy_log_debug( MODULENAME, "on_connect: switching to CONNECTED ***** valid for PSTN calls *****" );

        state_  = CONNECTED;

        if( callback_ )
            callback_->on_call_started( call_id );
    }
        break;


    case RINGING:
    {
        ASSERT( is_call_id_valid( call_id ) );

        dummy_log_debug( MODULENAME, "on_connect: switching to CONNECTED" );

        state_  = CONNECTED;

        if( callback_ )
            callback_->on_call_started( call_id );
    }
        break;

    default:
        dummy_log_fatal( MODULENAME, "on_connect: invalid state %s", StrHelper::to_string( state_ ).c_str() );

        ASSERT( 0 );
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
    case WAITING_VOIP_RESPONSE:
    case DIALLING:
    case RINGING:
        dummy_log_fatal( MODULENAME, "on_call_duration: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case CONNECTED:
    {
        ASSERT( is_call_id_valid( call_id ) );

        if( callback_ )
            callback_->on_call_duration( call_id, t );
    }
        break;

    default:
        dummy_log_fatal( MODULENAME, "on_call_duration: invalid state %s", StrHelper::to_string( state_ ).c_str() );

        ASSERT( 0 );
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
    case WAITING_VOIP_RESPONSE:
    case DIALLING:
    case RINGING:
        dummy_log_fatal( MODULENAME, "on_play_start: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case CONNECTED:
    {
        ASSERT( is_call_id_valid( call_id ) );

        dummy_log_debug( MODULENAME, "on_play_start: ok" );

        player_.on_play_start( call_id );
    }
        break;

    default:
        dummy_log_fatal( MODULENAME, "on_play_start: invalid state %s", StrHelper::to_string( state_ ).c_str() );

        ASSERT( 0 );
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
    case WAITING_VOIP_RESPONSE:
    case DIALLING:
    case RINGING:
        dummy_log_fatal( MODULENAME, "on_play_stop: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case CONNECTED:
    {
        ASSERT( is_call_id_valid( call_id ) );

        dummy_log_debug( MODULENAME, "on_play_stop: ok" );

        player_.on_play_stop( call_id );
    }
        break;

    default:
        dummy_log_fatal( MODULENAME, "on_play_stop: invalid state %s", StrHelper::to_string( state_ ).c_str() );

        ASSERT( 0 );
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
    case WAITING_VOIP_RESPONSE:
    case DIALLING:
    case RINGING:
        dummy_log_warn( MODULENAME, "on_call_end: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case CONNECTED:
    {
        ASSERT( is_call_id_valid( call_id ) );

        if( player_.is_playing() )
            player_.stop();

        if( callback_ )
            callback_->on_call_end( call_id, errorcode );

        dummy_log_info( MODULENAME, "on_call_end: switching to IDLE" );

        state_      = IDLE;
        call_id_    = 0;

        if( callback_ )
            callback_->on_ready();
    }
        break;

    default:
        dummy_log_fatal( MODULENAME, "on_call_end: invalid state %s", StrHelper::to_string( state_ ).c_str() );

        ASSERT( 0 );
    }
}

bool DialerImpl::is_call_id_valid( uint32 call_id ) const
{
    return call_id == call_id_;
}


NAMESPACE_DIALER_END
