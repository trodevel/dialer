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

// $Revision: 2992 $ $Date:: 2015-12-15 #$ $Author: serge $

#include "dialer.h"                     // self

#include "../voip_io/i_voip_service.h"  // IVoipService
#include "../voip_io/object_factory.h"  // voip_service::create_message_t
#include "../utils/dummy_logger.h"      // dummy_log
#include "str_helper.h"                 // StrHelper
#include "../voip_io/object_factory.h"  // create_error_response
#include "i_dialer_callback.h"          // IDialerCallback

#include "../utils/mutex_helper.h"      // MUTEX_SCOPE_LOCK
#include "../utils/assert.h"            // ASSERT

#include "namespace_lib.h"              // NAMESPACE_DIALER_START

#define MODULENAME      "Dialer"

NAMESPACE_DIALER_START

class Dialer;

Dialer::Dialer():
    ServerBase( this ),
    state_( UNKNOWN ), voips_( 0L ), sched_( 0L ), callback_( 0L ), call_id_( 0 )
{
}

Dialer::~Dialer()
{
}

bool Dialer::init(
        skype_service::SkypeService * sw,
        sched::IScheduler           * sched )
{
	MUTEX_SCOPE_LOCK( mutex_ );

    if( !sw || !sched )
        return false;

    sio_        = sw;
    sched_      = sched;
    state_      = IDLE;

    player_.init( voips, sched );

    return true;
}

bool Dialer::register_callback( IDialerCallback * callback )
{
    if( callback == 0L )
        return false;

    MUTEX_SCOPE_LOCK( mutex_ );

    if( callback_ != 0L )
        return false;

    callback_ = callback;

    player_.register_callback( callback );

    return true;
}

bool Dialer::is_inited() const
{
    MUTEX_SCOPE_LOCK( mutex_ );

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
    MUTEX_SCOPE_LOCK( mutex_ );

    return state_;
}

// interface IVoipService
void Dialer::consume( const DialerObject * req )
{
    ServerBase::consume( req );
}

// interface skype_service::ISkypeCallback
void Dialer::consume( const skype_service::Event * e )
{
    voip_service::ObjectWrap * ew = new voip_service::ObjectWrap;

    ew->ptr = e;

    ServerBase::consume( ew );
}

void Dialer::handle( const servt::IObject* req )
{
    if( typeid( *req ) == typeid( voip_service::InitiateCallRequest ) )
    {
        handle( dynamic_cast< const voip_service::InitiateCallRequest *>( req ) );
    }
    else if( typeid( *req ) == typeid( voip_service::PlayFileRequest ) )
    {
        handle( dynamic_cast< const voip_service::PlayFileRequest *>( req ) );
    }
    else if( typeid( *req ) == typeid( voip_service::RecordFileRequest ) )
    {
        handle( dynamic_cast< const voip_service::RecordFileRequest *>( req ) );
    }
    else if( typeid( *req ) == typeid( voip_service::DropRequest ) )
    {
        handle( dynamic_cast< const voip_service::DropRequest *>( req ) );
    }
    else if( typeid( *req ) == typeid( voip_service::ObjectWrap ) )
    {
        handle( dynamic_cast< const voip_service::ObjectWrap *>( req ) );
    }
    else
    {
        dummy_log_fatal( MODULENAME, "handle: cannot cast request to known type - %p", (void *) req );

        ASSERT( 0 );
    }

    delete req;
}


void Dialer::handle( const voip_service::InitiateCallRequest * req )
{
    dummy_log_debug( MODULENAME, "handle voip_service::InitiateCallRequest: %s", req->party.c_str() );

    // private: no mutex lock

    if( state_ != IDLE )
    {
        dummy_log_warn( MODULENAME, "handle voip_service::InitiateCallRequest: busy, ignored in state %s", StrHelper::to_string( state_ ).c_str() );

        send_reject_response( req->job_id, 0, "busy, cannot proceed in state " + StrHelper::to_string( state_ ) );
        return;
    }

    bool b = sio_->call( req->party, req->job_id );

    if( b == false )
    {
        dummy_log_error( MODULENAME, "failed calling: %s", req->party.c_str() );

        callback_consume( voip_service::create_error_response( req->job_id, 0, "voip io failed" ) );

        return;
    }

    req_hash_id_    = req->job_id;

    state_  = WAITING_INITIATE_CALL_RESPONSE;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

void Dialer::handle( const voip_service::DropRequest * req )
{
    dummy_log_debug( MODULENAME, "handle voip_service::DropRequest" );

    // private: no mutex lock

    if( state_ != WAITING_DIALLING && state_ != DIALLING && state_ != RINGING && state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "handle voip_service::DropRequest: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( req->call_id ) );

    voips_->consume( voip_service::create_message_t<voip_service::Drop>( req->call_id ) );

    state_      = WAITING_DROP_RESPONSE;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );

}

void Dialer::handle( const voip_service::PlayFileRequest * req )
{
    dummy_log_debug( MODULENAME, "handle voip_service::PlayFileRequest" );

    // private: no mutex lock

    if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "handle voip_service::PlayFileRequest: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( req->call_id ) );

    player_.play_file( req->call_id, req->filename );
}

void Dialer::handle( const voip_service::RecordFileRequest * req )
{
    dummy_log_debug( MODULENAME, "handle voip_service::RecordFileRequest" );

    // private: no mutex lock

    if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "handle voip_service::RecordFileRequest: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( req->call_id ) );

    voips_->consume( voip_service::create_record_file( req->call_id, req->filename ) );

    bool b = true;

    if( b == false )
    {
        dummy_log_error( MODULENAME, "handle voip_service::RecordFileRequest: voip service failed" );
    }
}

bool Dialer::shutdown()
{
    dummy_log_debug( MODULENAME, "shutdown()" );

    MUTEX_SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return false;

    bool b = ServerBase::shutdown();

    return b;
}

void Dialer::handle( const voip_service::InitiateCallResponse * r )
{
    dummy_log_debug( MODULENAME, "handle InitiateCallResponse: call id = %u, status = %u", r->call_id, r->status );

    if( state_ != WAITING_INITIATE_CALL_RESPONSE )
    {
        dummy_log_warn( MODULENAME, "handle InitiateCallResponse: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    call_id_    = r->call_id;

    if( callback_ )
        callback_->consume( create_initiate_call_response( r->call_id, r->status ) );

    state_  = WAITING_DIALLING;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

void Dialer::handle( const voip_service::ErrorResponse * r )
{
    dummy_log_debug( MODULENAME, "handle ErrorResponse: %s", r->descr.c_str() );

    // private: no mutex lock

    if( state_ != WAITING_INITIATE_CALL_RESPONSE )
    {
        dummy_log_fatal( MODULENAME, "handle ErrorResponse: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( call_id_ == 0 );

    if( callback_ )
        callback_->consume( create_error_response( r->errorcode, r->descr ) );

    state_      = IDLE;
    call_id_    = 0;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

void Dialer::handle( const voip_service::RejectResponse * r )
{
    dummy_log_debug( MODULENAME, "handle RejectResponse: %s", r->descr.c_str() );

    // private: no mutex lock

    if( state_ != WAITING_INITIATE_CALL_RESPONSE )
    {
        dummy_log_fatal( MODULENAME, "handle RejectResponse: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( call_id_ == 0 );

    if( callback_ )
        callback_->consume( create_reject_response( r->errorcode, r->descr ) );

    state_      = IDLE;
    call_id_    = 0;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

void Dialer::handle( const voip_service::DropResponse * r )
{
    dummy_log_debug( MODULENAME, "handle DropResponse: call id = %u", r->call_id );

    if( state_ != WAITING_DROP_RESPONSE )
    {
        dummy_log_warn( MODULENAME, "handle DropResponse: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    if( callback_ )
        callback_->consume( create_message_t<DialerDropResponse>( r->call_id ) );

    state_      = IDLE;
    call_id_    = 0;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

void Dialer::handle( const voip_service::CallErrorResponse * r )
{
    dummy_log_debug( MODULENAME, "handle CallErrorResponse: %s", r->descr.c_str() );

    // private: no mutex lock
}
void Dialer::handle( const voip_service::Dial * r )
{
    dummy_log_debug( MODULENAME, "handle Dial: %u", r->call_id );

    // private: no mutex lock

    if( state_ != DIALLING && state_ != WAITING_DIALLING )
    {
        dummy_log_fatal( MODULENAME, "handle Dial: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    if( state_ == DIALLING )
    {
        dummy_log_warn( MODULENAME, "handle Dial: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    state_  = DIALLING;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );

    if( callback_ )
        callback_->consume( create_message_t<DialerDial>( r->call_id ) );
}
void Dialer::handle( const voip_service::Ring * r )
{
    dummy_log_debug( MODULENAME, "handle Ring: %u", r->call_id );

    // private: no mutex lock

    if( state_ != RINGING && state_ != DIALLING )
    {
        dummy_log_fatal( MODULENAME, "handle Ring: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    if( state_ == RINGING )
    {
        dummy_log_warn( MODULENAME, "handle Ring: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    if( callback_ )
        callback_->consume( create_message_t<DialerRing>( r->call_id ) );

    state_  = RINGING;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

void Dialer::handle( const voip_service::Connect * r )
{
    dummy_log_debug( MODULENAME, "handle Connect: %u", r->call_id );

    // private: no mutex lock

    if( state_ != RINGING && state_ != DIALLING )
    {
        dummy_log_fatal( MODULENAME, "handle Connect: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    if( state_ == DIALLING )
    {
        dummy_log_debug( MODULENAME, "handle Connect: switching to CONNECTED ***** valid for PSTN calls *****" );
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    state_  = CONNECTED;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );

    if( callback_ )
        callback_->consume( create_message_t<DialerConnect>( r->call_id ) );
}

void Dialer::handle( const voip_service::CallDuration * r )
{
    dummy_log_debug( MODULENAME, "handle CallDuration: %u", r->call_id );

    // private: no mutex lock

    if( state_ == IDLE )
    {
        dummy_log_warn( MODULENAME, "handle CallDuration: out-of-order, unexpected in state %s", StrHelper::to_string( state_ ).c_str() );

        return;
    }
    else if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "handle CallDuration: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    if( callback_ )
        callback_->consume( create_call_duration( r->call_id, r->t ) );
}

void Dialer::handle( const voip_service::PlayStarted * r )
{
    dummy_log_debug( MODULENAME, "handle PlayStarted: %u", r->call_id );

    // private: no mutex lock

    if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "handle PlayStarted: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    dummy_log_debug( MODULENAME, "handle PlayStarted: ok" );

    player_.on_play_start( r->call_id );
}

void Dialer::handle( const voip_service::PlayStopped * r )
{
    dummy_log_debug( MODULENAME, "handle PlayStop: %u", r->call_id );

    // private: no mutex lock

    if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "handle PlayStop: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    dummy_log_debug( MODULENAME, "handle PlayStop: ok" );

    player_.on_play_stop( r->call_id );
}


void Dialer::handle( const voip_service::CallEnd * r )
{
    dummy_log_debug( MODULENAME, "handle CallEnd: %u %u '%s'", r->call_id, r->errorcode, r->descr.c_str() );

    // private: no mutex lock

    if( state_ != WAITING_DIALLING && state_ != DIALLING && state_ != RINGING && state_ != CONNECTED && state_ != WAITING_DROP_RESPONSE )
    {
        dummy_log_fatal( MODULENAME, "handle CallEnd: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    if( player_.is_playing() )
        player_.stop();

    if( state_ == WAITING_DROP_RESPONSE )
    {
        if( callback_ )
            callback_->consume( create_message_t<DialerDropResponse>( r->call_id ) );
    }
    else
    {
        if( callback_ )
            callback_->consume( create_call_end( r->call_id, r->errorcode, r->descr ) );
    }

    state_      = IDLE;
    call_id_    = 0;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

bool Dialer::is_call_id_valid( uint32 call_id ) const
{
    return call_id == call_id_;
}

void Dialer::send_reject_response( uint32_t job_id, uint32_t errorcode, const std::string & descr )
{
    callback_consume( voip_service::create_reject_response( job_id, errorcode, descr ) );
}

void Dialer::callback_consume( const voip_service::ResponseObject * req )
{
    if( callback_ )
        callback_->consume( req );
}

NAMESPACE_DIALER_END
