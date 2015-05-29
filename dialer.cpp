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

// $Revision: 1784 $ $Date:: 2015-05-29 #$ $Author: serge $

#include "dialer.h"                     // self

#include "../voip_io/i_voip_service.h"  // IVoipService
#include "../voip_io/object_factory.h"  // voip_service::create_message_t
#include "../utils/dummy_logger.h"      // dummy_log
#include "str_helper.h"                 // StrHelper
#include "object_factory.h"             // create_error_response
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
        voip_service::IVoipService  * voips,
        sched::IScheduler           * sched )
{
	MUTEX_SCOPE_LOCK( mutex_ );

    if( !voips || !sched )
        return false;

    voips_      = voips;
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
    MUTEX_SCOPE_LOCK( mutex_ );

    ASSERT( is_inited__() );

    if( typeid( *req ) == typeid( DialerInitiateCallRequest ) )
    {
        handle( dynamic_cast< const DialerInitiateCallRequest *>( req ) );
    }
    else if( typeid( *req ) == typeid( DialerPlayFile ) )
    {
        handle( dynamic_cast< const DialerPlayFile *>( req ) );
    }
    else if( typeid( *req ) == typeid( DialerRecordFile ) )
    {
        handle( dynamic_cast< const DialerRecordFile *>( req ) );
    }
    else if( typeid( *req ) == typeid( DialerDrop ) )
    {
        handle( dynamic_cast< const DialerDrop *>( req ) );
    }
    else if( typeid( *req ) == typeid( voip_service::VoipioInitiateCallResponse ) )
    {
        handle( dynamic_cast< const voip_service::VoipioInitiateCallResponse *>( req ) );
    }
    else if( typeid( *req ) == typeid( voip_service::VoipioErrorResponse ) )
    {
        handle( dynamic_cast< const voip_service::VoipioErrorResponse *>( req ) );
    }
    else if( typeid( *req ) == typeid( voip_service::VoipioDropResponse ) )
    {
        handle( dynamic_cast< const voip_service::VoipioDropResponse *>( req ) );
    }
    else if( typeid( *req ) == typeid( voip_service::VoipioCallErrorResponse ) )
    {
        handle( dynamic_cast< const voip_service::VoipioCallErrorResponse *>( req ) );
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


void Dialer::handle( const DialerInitiateCallRequest * req )
{
    dummy_log_debug( MODULENAME, "handle DialerInitiateCallRequest: %s", req->party.c_str() );

    if( state_ != IDLE )
    {
        dummy_log_warn( MODULENAME, "handle DialerInitiateCallRequest: busy, ignored in state %s", StrHelper::to_string( state_ ).c_str() );

        if( callback_ )
            callback_->consume( create_reject_response( 0, "busy, cannot proceed in state " + StrHelper::to_string( state_ ) ) );

        return;
    }

    voips_->consume( voip_service::create_initiate_call_request( req->party ) );

    state_  = WAITING_INITIATE_CALL_RESPONSE;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

void Dialer::handle( const DialerDrop * req )
{
    dummy_log_debug( MODULENAME, "handle DialerDrop" );

    // private: no mutex lock

    if( state_ != WAITING_DIALLING && state_ != DIALLING && state_ != RINGING && state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "handle DialerDrop: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( req->call_id ) );

    voips_->consume( voip_service::create_message_t<voip_service::VoipioDrop>( req->call_id ) );

    state_      = WAITING_DROP_RESPONSE;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );

}

void Dialer::handle( const DialerPlayFile * req )
{
    dummy_log_debug( MODULENAME, "handle DialerPlayFile" );

    // private: no mutex lock

    if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "handle DialerPlayFile: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( req->call_id ) );

    player_.play_file( req->call_id, req->filename );
}

void Dialer::handle( const DialerRecordFile * req )
{
    dummy_log_debug( MODULENAME, "handle DialerRecordFile" );

    // private: no mutex lock

    if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "handle DialerRecordFile: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( req->call_id ) );

    voips_->consume( voip_service::create_record_file( req->call_id, req->filename ) );

    bool b = true;

    if( b == false )
    {
        dummy_log_error( MODULENAME, "handle DialerRecordFile: voip service failed" );
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

void Dialer::handle( const voip_service::VoipioInitiateCallResponse * r )
{
    dummy_log_debug( MODULENAME, "handle VoipioInitiateCallResponse: call id = %u, status = %u", r->call_id, r->status );

    if( state_ != WAITING_INITIATE_CALL_RESPONSE )
    {
        dummy_log_warn( MODULENAME, "handle VoipioInitiateCallResponse: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    call_id_    = r->call_id;

    if( callback_ )
        callback_->consume( create_initiate_call_response( r->call_id, r->status ) );

    state_  = WAITING_DIALLING;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

void Dialer::handle( const voip_service::VoipioErrorResponse * r )
{
    dummy_log_debug( MODULENAME, "handle VoipioErrorResponse: %s", r->descr.c_str() );

    // private: no mutex lock

    if( state_ != WAITING_INITIATE_CALL_RESPONSE )
    {
        dummy_log_fatal( MODULENAME, "handle VoipioErrorResponse: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
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

void Dialer::handle( const voip_service::VoipioDropResponse * r )
{
    dummy_log_debug( MODULENAME, "handle VoipioDropResponse: call id = %u", r->call_id );

    if( state_ != WAITING_DROP_RESPONSE )
    {
        dummy_log_warn( MODULENAME, "handle VoipioDropResponse: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
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

void Dialer::handle( const voip_service::VoipioCallErrorResponse * r )
{
    dummy_log_debug( MODULENAME, "handle VoipioCallErrorResponse: %s", r->descr.c_str() );

    // private: no mutex lock
}
void Dialer::handle( const voip_service::VoipioDial * r )
{
    dummy_log_debug( MODULENAME, "handle VoipioDial: %u", r->call_id );

    // private: no mutex lock

    if( state_ != DIALLING && state_ != WAITING_DIALLING )
    {
        dummy_log_fatal( MODULENAME, "handle VoipioDial: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    if( state_ == DIALLING )
    {
        dummy_log_warn( MODULENAME, "handle VoipioDial: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    state_  = DIALLING;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );

    if( callback_ )
        callback_->consume( create_message_t<DialerDial>( r->call_id ) );
}
void Dialer::handle( const voip_service::VoipioRing * r )
{
    dummy_log_debug( MODULENAME, "handle VoipioRing: %u", r->call_id );

    // private: no mutex lock

    if( state_ != RINGING && state_ != DIALLING )
    {
        dummy_log_fatal( MODULENAME, "handle VoipioRing: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    if( state_ == RINGING )
    {
        dummy_log_warn( MODULENAME, "handle VoipioRing: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    if( callback_ )
        callback_->consume( create_message_t<DialerRing>( r->call_id ) );

    state_  = RINGING;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

void Dialer::handle( const voip_service::VoipioConnect * r )
{
    dummy_log_debug( MODULENAME, "handle VoipioConnect: %u", r->call_id );

    // private: no mutex lock

    if( state_ != RINGING && state_ != DIALLING )
    {
        dummy_log_fatal( MODULENAME, "handle VoipioConnect: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    if( state_ == DIALLING )
    {
        dummy_log_debug( MODULENAME, "handle VoipioConnect: switching to CONNECTED ***** valid for PSTN calls *****" );
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    state_  = CONNECTED;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );

    if( callback_ )
        callback_->consume( create_message_t<DialerConnect>( r->call_id ) );
}

void Dialer::handle( const voip_service::VoipioCallDuration * r )
{
    dummy_log_debug( MODULENAME, "handle VoipioCallDuration: %u", r->call_id );

    // private: no mutex lock

    if( state_ == IDLE )
    {
        dummy_log_warn( MODULENAME, "handle VoipioCallDuration: out-of-order, unexpected in state %s", StrHelper::to_string( state_ ).c_str() );

        return;
    }
    else if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "handle VoipioCallDuration: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    if( callback_ )
        callback_->consume( create_call_duration( r->call_id, r->t ) );
}

void Dialer::handle( const voip_service::VoipioPlayStarted * r )
{
    dummy_log_debug( MODULENAME, "handle VoipioPlayStarted: %u", r->call_id );

    // private: no mutex lock

    if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "handle VoipioPlayStarted: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    dummy_log_debug( MODULENAME, "handle VoipioPlayStarted: ok" );

    player_.on_play_start( r->call_id );
}

void Dialer::handle( const voip_service::VoipioPlayStopped * r )
{
    dummy_log_debug( MODULENAME, "handle VoipioPlayStop: %u", r->call_id );

    // private: no mutex lock

    if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "handle VoipioPlayStop: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( r->call_id ) );

    dummy_log_debug( MODULENAME, "handle VoipioPlayStop: ok" );

    player_.on_play_stop( r->call_id );
}


void Dialer::handle( const voip_service::VoipioCallEnd * r )
{
    dummy_log_debug( MODULENAME, "handle VoipioCallEnd: %u %u '%s'", r->call_id, r->errorcode, r->descr.c_str() );

    // private: no mutex lock

    if( state_ != WAITING_DIALLING && state_ != DIALLING && state_ != RINGING && state_ != CONNECTED && state_ != WAITING_DROP_RESPONSE )
    {
        dummy_log_fatal( MODULENAME, "handle VoipioCallEnd: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
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


NAMESPACE_DIALER_END
