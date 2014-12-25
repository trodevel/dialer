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

// $Id: dialer_impl.cpp 1284 2014-12-24 16:00:13Z serge $

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

    ASSERT( is_inited__() );

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
    else if( typeid( *req ) == typeid( voip_service::VoipioInitiateCallResponse ) )
    {
        handle( dynamic_cast< const voip_service::VoipioInitiateCallResponse *>( req ) );
    }
    else if( typeid( *req ) == typeid( voip_service::VoipioError ) )
    {
        handle( dynamic_cast< const voip_service::VoipioError *>( req ) );
    }
    else if( typeid( *req ) == typeid( voip_service::VoipioFatalError ) )
    {
        handle( dynamic_cast< const voip_service::VoipioFatalError *>( req ) );
    }
    else if( typeid( *req ) == typeid( voip_service::VoipioCallEnd ) )
    {
        handle( dynamic_cast< const voip_service::VoipioCallEnd *>( req ) );
    }
    else if( typeid( *req ) == typeid( voip_service::VoipioDial ) )
    {
        handle( dynamic_cast< const voip_service::VoipioDial *>( req ) );
    }
    else if( typeid( *req ) == typeid( voip_service::VoipioRing ) )
    {
        handle( dynamic_cast< const voip_service::VoipioRing *>( req ) );
    }
    else if( typeid( *req ) == typeid( voip_service::VoipioConnect ) )
    {
        handle( dynamic_cast< const voip_service::VoipioConnect *>( req ) );
    }
    else if( typeid( *req ) == typeid( voip_service::VoipioCallDuration ) )
    {
        handle( dynamic_cast< const voip_service::VoipioCallDuration *>( req ) );
    }
    else if( typeid( *req ) == typeid( voip_service::VoipioPlayStarted ) )
    {
        handle( dynamic_cast< const voip_service::VoipioPlayStarted *>( req ) );
    }
    else if( typeid( *req ) == typeid( voip_service::VoipioPlayStopped ) )
    {
        handle( dynamic_cast< const voip_service::VoipioPlayStopped *>( req ) );
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

    if( state_ != IDLE )
    {
        dummy_log_warn( MODULENAME, "initiate_call: busy, ignored in state %s", StrHelper::to_string( state_ ).c_str() );

        if( callback_ )
            callback_->consume( create_error_response( 0, "busy, cannot proceed in state " + StrHelper::to_string( state_ ) ) );

        return;
    }

    voips_->consume( voip_service::create_initiate_call_request( req->party ) );

    state_  = WAITING_VOIP_RESPONSE;
}

void DialerImpl::handle( const DialerDrop * req )
{
    dummy_log_debug( MODULENAME, "drop" );

    // private: no mutex lock

    switch( state_ )
    {
    case IDLE:
    case WAITING_VOIP_RESPONSE:
        dummy_log_fatal( MODULENAME, "drop: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;

    case WAITING_DIALLING:
    case DIALLING:
    case RINGING:
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

    // private: no mutex lock

    if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "set_input_file: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( req->call_id ) );

    bool b = player_.play_file( req->call_id, req->filename );

    if( b == false )
    {
        dummy_log_error( MODULENAME, "set_input_file: player failed" );
    }
}

void DialerImpl::handle( const DialerRecordFile * req )
{
    dummy_log_debug( MODULENAME, "set_output_file" );

    // private: no mutex lock

    if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "set_output_file: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( req->call_id ) );

    bool b = voips_->set_output_file( req->call_id, req->filename );

    if( b == false )
    {
        dummy_log_error( MODULENAME, "set_output_file: voip service failed" );
    }
}

bool DialerImpl::shutdown()
{
    SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return false;

    return ServerBase::shutdown();
}

void DialerImpl::handle( voip_service::VoipioInitiateCallResponse * r )
{
    dummy_log_debug( MODULENAME, "on_initiate_call_response: call id = %u, status = %u", r->call_id, r->status );

    if( state_ != WAITING_VOIP_RESPONSE )
    {
        dummy_log_warn( MODULENAME, "on_initiate_call_response: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    call_id_    = r->call_id;

    if( callback_ )
        callback_->consume( create_initiate_call_response( r->call_id, r->status ) );

    state_  = WAITING_DIALLING;
}

void DialerImpl::handle( voip_service::VoipioError * r )
{
    dummy_log_debug( MODULENAME, "on_error: %s", r->error.c_str() );

    // private: no mutex lock

    if( state_ == IDLE || state_ == WAITING_VOIP_RESPONSE )
    {
        dummy_log_warn( MODULENAME, "on_error: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    if( player_.is_playing() )
        player_.stop();

    if( callback_ )
        callback_->on_error( r->call_id, r->error );

    dummy_log_info( MODULENAME, "on_error: switching to IDLE" );

    state_      = IDLE;
    call_id_    = 0;

    if( callback_ )
        callback_->on_ready();
}
void DialerImpl::handle( voip_service::VoipioFatalError * r )
{
    dummy_log_debug( MODULENAME, "on_fatal_error: %u", r->error.c_str() );

    // private: no mutex lock

    if( state_ == IDLE || state_ == WAITING_VOIP_RESPONSE )
    {
        dummy_log_warn( MODULENAME, "on_fatal_error: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    if( player_.is_playing() )
        player_.stop();

    if( callback_ )
        callback_->on_fatal_error( r->call_id, r->error );

    dummy_log_info( MODULENAME, "on_fatal_error: switching to IDLE" );

    state_      = IDLE;
    call_id_    = 0;

    if( callback_ )
        callback_->on_ready();
}
void DialerImpl::handle( voip_service::VoipioDial * r )
{
    dummy_log_debug( MODULENAME, "on_dial: %u", r->call_id );

    // private: no mutex lock

    if( state_ != DIALLING && state_ != WAITING_DIALLING )
    {
        dummy_log_fatal( MODULENAME, "on_dial: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    if( state_ == DIALLING )
    {
        dummy_log_warn( MODULENAME, "on_dial: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    state_  = DIALLING;

    if( callback_ )
        callback_->on_dial( r->call_id );
}
void DialerImpl::handle( voip_service::VoipioRing * r )
{
    dummy_log_debug( MODULENAME, "on_ring: %u", r->call_id );

    // private: no mutex lock

    if( state_ != RINGING && state_ != DIALLING )
    {
        dummy_log_fatal( MODULENAME, "on_ring: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    if( state_ == RINGING )
    {
        dummy_log_warn( MODULENAME, "on_ring: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    if( callback_ )
        callback_->on_ring( r->call_id );

    state_  = RINGING;
}

void DialerImpl::handle( voip_service::VoipioConnect * r )
{
    dummy_log_debug( MODULENAME, "on_connect: %u", r->call_id );

    // private: no mutex lock

    if( state_ != RINGING && state_ != DIALLING )
    {
        dummy_log_fatal( MODULENAME, "on_connect: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    if( state_ == DIALLING )
    {
        ASSERT( is_call_id_valid( r->call_id ) );

        dummy_log_debug( MODULENAME, "on_connect: switching to CONNECTED ***** valid for PSTN calls *****" );

        state_  = CONNECTED;

        if( callback_ )
            callback_->on_call_started( r->call_id );

        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    dummy_log_debug( MODULENAME, "on_connect: switching to CONNECTED" );

    state_  = CONNECTED;

    if( callback_ )
        callback_->on_call_started( r->call_id );
}

void DialerImpl::handle( voip_service::VoipioCallDuration * r )
{
    dummy_log_debug( MODULENAME, "on_call_duration: %u", r->call_id );

    // private: no mutex lock

    if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "on_call_duration: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    if( callback_ )
        callback_->on_call_duration( r->call_id, r->t );
}

void DialerImpl::handle( voip_service::VoipioPlayStarted * r )
{
    dummy_log_debug( MODULENAME, "on_play_start: %u", r->call_id );

    // private: no mutex lock

    if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "on_play_start: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    dummy_log_debug( MODULENAME, "on_play_start: ok" );

    player_.on_play_start( r->call_id );
}

void DialerImpl::handle( voip_service::VoipioPlayStopped * r )
{
    dummy_log_debug( MODULENAME, "on_play_stop: %u", r->call_id );

    // private: no mutex lock

    if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "on_play_stop: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    dummy_log_debug( MODULENAME, "on_play_stop: ok" );

    player_.on_play_stop( r->call_id );
}


void DialerImpl::handle( voip_service::VoipioCallEnd * r )
{
    dummy_log_debug( MODULENAME, "on_call_end: %u %u", r->call_id, r->errorcode );

    // private: no mutex lock

    if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "on_call_end: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    if( player_.is_playing() )
        player_.stop();

    if( callback_ )
        callback_->on_call_end( r->call_id, r->errorcode );

    dummy_log_info( MODULENAME, "on_call_end: switching to IDLE" );

    state_      = IDLE;
    call_id_    = 0;

    if( callback_ )
        callback_->on_ready();
}

bool DialerImpl::is_call_id_valid( uint32 call_id ) const
{
    return call_id == call_id_;
}


NAMESPACE_DIALER_END
