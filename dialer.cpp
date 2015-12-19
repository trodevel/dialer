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

// $Revision: 3013 $ $Date:: 2015-12-18 #$ $Author: serge $

#include "dialer.h"                     // self

#include "../voip_io/i_voip_service.h"  // IVoipService
#include "../voip_io/object_factory.h"  // voip_service::create_message_t
#include "../utils/dummy_logger.h"      // dummy_log
#include "str_helper.h"                 // StrHelper
#include "../voip_io/object_factory.h"  // create_error_response
#include "i_dialer_callback.h"          // voip_service::IVoipServiceCallback

#include "../utils/mutex_helper.h"      // MUTEX_SCOPE_LOCK
#include "../utils/assert.h"            // ASSERT

#include "namespace_lib.h"              // NAMESPACE_DIALER_START

#define MODULENAME      "Dialer"

NAMESPACE_DIALER_START

class Dialer;

Dialer::Dialer():
    ServerBase( this ),
    state_( UNKNOWN ), voips_( 0L ), sched_( 0L ), callback_( 0L ),
    current_job_id_( 0 ),
    cs_( skype_service::conn_status_e::NONE ),
    us_( skype_service::user_status_e::NONE ),
    call_id_( 0 )
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

    player_.init( sio_, sched );

    return true;
}

bool Dialer::register_callback( voip_service::IVoipServiceCallback * callback )
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

    if( send_reject_if_in_request_processing( req->job_id ) )
        return;

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

    ASSERT( current_job_id_ == 0 );
    current_job_id_    = req->job_id;

    state_  = WAITING_INITIATE_CALL_RESPONSE;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

void Dialer::handle( const voip_service::DropRequest * req )
{
    dummy_log_debug( MODULENAME, "handle voip_service::DropRequest" );

    // private: no mutex lock

    if( send_reject_if_in_request_processing( req->job_id ) )
        return;

    if( state_ != WAITING_CONNECTION && state_ != CONNECTED )
    {
        dummy_log_warn( MODULENAME, "handle voip_service::DropRequest: cannot process in state %s", StrHelper::to_string( state_ ).c_str() );

        send_reject_response( req->job_id, 0, "cannot proceed in state " + StrHelper::to_string( state_ ) );

        return;
    }

    ASSERT( is_call_id_valid( req->call_id ) );

    bool b = sio_->set_call_status( req->call_id, skype_service::call_status_e::FINISHED, req->job_id );

    if( b == false )
    {
        callback_consume( voip_service::create_error_response( req->job_id, 0, "voip io failed" ) );
        return;
    }

    current_job_id_    = req->job_id;

    state_      = WAITING_DROP_RESPONSE;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

void Dialer::handle( const voip_service::PlayFileRequest * req )
{
    dummy_log_debug( MODULENAME, "handle voip_service::PlayFileRequest" );

    // private: no mutex lock

    if( send_reject_if_in_request_processing( req->job_id ) )
        return;

    if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "handle voip_service::PlayFileRequest: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( req->call_id ) );

    player_.play_file( req->job_id, req->call_id, req->filename );

    current_job_id_    = req->job_id;
}

void Dialer::handle( const voip_service::RecordFileRequest * req )
{
    dummy_log_debug( MODULENAME, "handle voip_service::RecordFileRequest" );

    // private: no mutex lock

    if( send_reject_if_in_request_processing( req->job_id ) )
        return;

    if( state_ != CONNECTED )
    {
        dummy_log_fatal( MODULENAME, "handle voip_service::RecordFileRequest: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        return;
    }

    ASSERT( is_call_id_valid( req->call_id ) );

    bool b = sio_->alter_call_set_output_file( req->call_id, req->filename, req->job_id );

    if( b == false )
    {
        dummy_log_error( MODULENAME, "failed setting output file: %s", req->filename.c_str() );

        callback_consume( voip_service::create_error_response( req->job_id, 0, "failed output input file: " + req->filename ) );

        return;
    }

    current_job_id_ = req->job_id;
}

void Dialer::handle( const voip_service::ObjectWrap * req )
{
    // private: no mutex lock

    const skype_service::Event * ev = static_cast<const skype_service::Event*>( req->ptr );

    ASSERT( ev );

    switch( state_ )
    {
    case IDLE:
        handle_in_state_idle( ev );
        break;
    case WAITING_INITIATE_CALL_RESPONSE:
        handle_in_state_w_ical( ev );
        break;
    case WAITING_CONNECTION:
        handle_in_state_w_conn( ev );
        break;
    case CONNECTED:
        handle_in_state_connected( ev );
        break;
    case WAITING_DROP_RESPONSE:
        handle_in_state_w_drpr( ev );
        break;
    default:
        handle_in_state_unknown( ev );
        break;
    }

    delete ev;
}

void Dialer::handle_in_state_unknown( const skype_service::Event * ev )
{
}

void Dialer::handle_in_state_idle( const skype_service::Event * ev )
{
    // private: no mutex lock

    skype_service::Event::type_e id = ev->get_type();

    switch( id )
    {
    case skype_service::Event::CONNSTATUS:
        handle( static_cast<const skype_service::ConnStatusEvent*>( ev ) );
        break;

    case skype_service::Event::USERSTATUS:
        handle( static_cast<const skype_service::UserStatusEvent*>( ev ) );
        break;

    case skype_service::Event::CURRENTUSERHANDLE:
        handle( static_cast<const skype_service::CurrentUserHandleEvent*>( ev ) );
        break;

    case skype_service::Event::USER_ONLINE_STATUS:
        break;

    case skype_service::Event::CALL:
    case skype_service::Event::CALL_DURATION:
    case skype_service::Event::CALL_STATUS:
    case skype_service::Event::CALL_PSTN_STATUS:
    case skype_service::Event::CALL_FAILUREREASON:
    case skype_service::Event::CALL_VAA_INPUT_STATUS:
    case skype_service::Event::ALTER_CALL_SET_INPUT_FILE:
    case skype_service::Event::ALTER_CALL_SET_OUTPUT_FILE:
    case skype_service::Event::ERROR:
    {
        dummy_log_error( MODULENAME, "handle_in_state_idle: event %s, unexpected in state %s",
                skype_service::StrHelper::to_string( id ).c_str(), StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
    }
        break;

    case skype_service::Event::UNDEF:
        break;

    case skype_service::Event::CHAT:
    case skype_service::Event::CHATMEMBER:
        // simply ignore
        break;

    case skype_service::Event::UNKNOWN:
    default:
        on_unknown( "???" );
        break;
    }
}

void Dialer::handle_in_state_w_ical( const skype_service::Event * ev )
{
    // private: no mutex lock

    skype_service::Event::type_e id = ev->get_type();

    switch( id )
    {
    case skype_service::Event::CONNSTATUS:
    case skype_service::Event::USERSTATUS:
        // TODO process disconnect
        break;

    case skype_service::Event::CALL:
    case skype_service::Event::CALL_DURATION:
    case skype_service::Event::CALL_PSTN_STATUS:
    case skype_service::Event::CALL_FAILUREREASON:
    case skype_service::Event::CALL_VAA_INPUT_STATUS:
    case skype_service::Event::ALTER_CALL_SET_INPUT_FILE:
    case skype_service::Event::ALTER_CALL_SET_OUTPUT_FILE:
    {
        dummy_log_error( MODULENAME, "handle_in_state_w_ical: event %s, unexpected in state %s",
                skype_service::StrHelper::to_string( id ).c_str(), StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
    }
        break;

    case skype_service::Event::CALL_STATUS:
        {
            if( ignore_non_response( ev ) )
            {
                return;
            }

            uint32 call_id = static_cast<const skype_service::BasicCallEvent*>( ev )->get_call_id();

            skype_service::call_status_e status_code = static_cast<const skype_service::CallStatusEvent*>( ev )->get_call_s();
            uint32 status  = static_cast<uint32>( status_code );

            dummy_log_debug( MODULENAME, "job_id %u, call initiated: %u, status %s", current_job_id_, call_id, skype_service::to_string( status_code ).c_str() );

            callback_consume( voip_service::create_initiate_call_response( current_job_id_, call_id, status ) );

            current_job_id_ = 0;
            call_id_        = call_id;
            state_          = WAITING_CONNECTION;

            dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
        }
        break;

    case skype_service::Event::ERROR:
        {
            if( ignore_non_response( ev ) )
            {
                return;
            }

            uint32 errorcode    = static_cast<const skype_service::ErrorEvent*>( ev )->get_par_int();
            std::string descr   = static_cast<const skype_service::ErrorEvent*>( ev )->get_par_str();

            dummy_log_error( MODULENAME, "job_id %u, error %u '%s'", current_job_id_, errorcode, descr.c_str() );

            callback_consume( voip_service::create_error_response( current_job_id_, errorcode, descr ) );

            current_job_id_ = 0;
            state_          = IDLE;

            dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
        }
        break;

    case skype_service::Event::UNDEF:
    case skype_service::Event::CURRENTUSERHANDLE:
    case skype_service::Event::USER_ONLINE_STATUS:
    case skype_service::Event::CHAT:
    case skype_service::Event::CHATMEMBER:
        // simply ignore
        break;

    case skype_service::Event::UNKNOWN:
    default:
        on_unknown( "???" );
        break;
    }
}

void Dialer::handle_in_state_w_conn( const skype_service::Event * ev )
{
    // private: no mutex lock

    ASSERT( current_job_id_ == 0 );

    skype_service::Event::type_e id = ev->get_type();

    switch( id )
    {
    case skype_service::Event::CONNSTATUS:
    case skype_service::Event::USERSTATUS:
        // TODO process disconnect
        break;

    case skype_service::Event::CALL:
    case skype_service::Event::CALL_DURATION:
    case skype_service::Event::CALL_VAA_INPUT_STATUS:
    case skype_service::Event::ALTER_CALL_SET_INPUT_FILE:
    case skype_service::Event::ALTER_CALL_SET_OUTPUT_FILE:
    {
        dummy_log_error( MODULENAME, "handle_in_state_w_conn: event %s, unexpected in state %s",
                skype_service::StrHelper::to_string( id ).c_str(), StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
    }
    break;

    case skype_service::Event::CALL_PSTN_STATUS:
        handle( static_cast<const skype_service::CallPstnStatusEvent*>( ev ) );
        break;

    case skype_service::Event::CALL_FAILUREREASON:
        handle( static_cast<const skype_service::CallFailureReasonEvent*>( ev ) );
        break;

    case skype_service::Event::CALL_STATUS:
        handle_in_w_conn( static_cast<const skype_service::CallStatusEvent*>( ev ) );
        break;

    case skype_service::Event::ERROR:
        {
            const skype_service::ErrorEvent * ev_c = static_cast<const skype_service::ErrorEvent*>( ev );

            uint32 errorcode    = ev_c->get_par_int();
            std::string descr   = ev_c->get_par_str();

            dummy_log_error( MODULENAME, "error %u '%s'", errorcode, descr.c_str() );

            callback_consume( voip_service::create_failed( call_id_, voip_service::Failed::UNDEF, errorcode, descr ) );

            switch_to_idle_and_cleanup();
        }
        break;

    case skype_service::Event::UNDEF:
    case skype_service::Event::CURRENTUSERHANDLE:
    case skype_service::Event::USER_ONLINE_STATUS:
    case skype_service::Event::CHAT:
    case skype_service::Event::CHATMEMBER:
        // simply ignore
        break;

    case skype_service::Event::UNKNOWN:
    default:
        on_unknown( "???" );
        break;
    }
}

void Dialer::handle_in_state_connected( const skype_service::Event * ev )
{
    // private: no mutex lock
    skype_service::Event::type_e id = ev->get_type();

    switch( id )
    {
    case skype_service::Event::CONNSTATUS:
    case skype_service::Event::USERSTATUS:
        // TODO process disconnect
        break;

    case skype_service::Event::CALL:
        break;

    case skype_service::Event::CALL_DURATION:
        handle( static_cast<const skype_service::CallDurationEvent*>( ev ) );
        break;

    case skype_service::Event::CALL_VAA_INPUT_STATUS:
        handle( static_cast<const skype_service::CallVaaInputStatusEvent*>( ev ) );
        break;

    case skype_service::Event::ALTER_CALL_SET_INPUT_FILE:
    {
        if( ignore_non_response( ev ) )
        {
            return;
        }

        //player_.  // TODO forward to player
    }
    break;

    case skype_service::Event::ALTER_CALL_SET_OUTPUT_FILE:
        break;

    case skype_service::Event::CALL_PSTN_STATUS:
        handle( static_cast<const skype_service::CallPstnStatusEvent*>( ev ) );
        break;

    case skype_service::Event::CALL_FAILUREREASON:
        handle( static_cast<const skype_service::CallFailureReasonEvent*>( ev ) );
        break;

    case skype_service::Event::CALL_STATUS:
        handle_in_connected( static_cast<const skype_service::CallStatusEvent*>( ev ) );
        break;

    case skype_service::Event::ERROR:
        {
            const skype_service::ErrorEvent * ev_c = static_cast<const skype_service::ErrorEvent*>( ev );

            uint32 errorcode    = ev_c->get_par_int();
            std::string descr   = ev_c->get_par_str();

            dummy_log_error( MODULENAME, "error %u '%s'", errorcode, descr.c_str() );

            callback_consume( voip_service::create_failed( call_id_, voip_service::Failed::UNDEF, errorcode, descr ) );

            switch_to_idle_and_cleanup();
        }
        break;

    case skype_service::Event::UNDEF:
    case skype_service::Event::CURRENTUSERHANDLE:
    case skype_service::Event::USER_ONLINE_STATUS:
    case skype_service::Event::CHAT:
    case skype_service::Event::CHATMEMBER:
        // simply ignore
        break;

    case skype_service::Event::UNKNOWN:
    default:
        on_unknown( "???" );
        break;
    }
}

void Dialer::handle_in_state_w_drpr( const skype_service::Event * ev )
{
    // private: no mutex lock
    skype_service::Event::type_e id = ev->get_type();

    switch( id )
    {
    case skype_service::Event::CONNSTATUS:
    case skype_service::Event::USERSTATUS:
        // TODO process disconnect
        break;

    case skype_service::Event::CALL:
        break;

    case skype_service::Event::CALL_DURATION:
        handle( static_cast<const skype_service::CallDurationEvent*>( ev ) );
        break;

    case skype_service::Event::CALL_VAA_INPUT_STATUS:
        handle( static_cast<const skype_service::CallVaaInputStatusEvent*>( ev ) );
        break;

    case skype_service::Event::ALTER_CALL_SET_OUTPUT_FILE:
    case skype_service::Event::ALTER_CALL_SET_INPUT_FILE:
    {
        dummy_log_error( MODULENAME, "handle_in_state_w_ical: event %s, unexpected in state %s",
                skype_service::StrHelper::to_string( id ).c_str(), StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
    }
    break;

    case skype_service::Event::CALL_PSTN_STATUS:
        handle( static_cast<const skype_service::CallPstnStatusEvent*>( ev ) );
        break;

    case skype_service::Event::CALL_FAILUREREASON:
        handle( static_cast<const skype_service::CallFailureReasonEvent*>( ev ) );
        break;

    case skype_service::Event::CALL_STATUS:
        handle_in_w_drpr( static_cast<const skype_service::CallStatusEvent*>( ev ) );
        break;

    case skype_service::Event::ERROR:
        {
            const skype_service::ErrorEvent * ev_c = static_cast<const skype_service::ErrorEvent*>( ev );

            uint32 errorcode    = ev_c->get_par_int();
            std::string descr   = ev_c->get_par_str();

            dummy_log_error( MODULENAME, "error %u '%s'", errorcode, descr.c_str() );

            callback_consume( voip_service::create_failed( call_id_, voip_service::Failed::UNDEF, errorcode, descr ) );

            switch_to_idle_and_cleanup();
        }
        break;

    case skype_service::Event::UNDEF:
    case skype_service::Event::CURRENTUSERHANDLE:
    case skype_service::Event::USER_ONLINE_STATUS:
    case skype_service::Event::CHAT:
    case skype_service::Event::CHATMEMBER:
        // simply ignore
        break;

    case skype_service::Event::UNKNOWN:
    default:
        on_unknown( "???" );
        break;
    }


    if( ev->get_type() == skype_service::Event::CALL_STATUS )
    {
        const skype_service::CallStatusEvent * cse = static_cast<const skype_service::CallStatusEvent *>( ev );
        if( cse->get_call_s() == skype_service::call_status_e::FINISHED )
        {
            callback_consume( voip_service::create_drop_response( current_job_id_ ) );

            current_job_id_ = 0;
            state_          = IDLE;
        }
        else
        {
            dummy_log_debug( MODULENAME, "ignoring, current state: %s", skype_service::to_string( cse->get_call_s() ).c_str() );
        }
        return;
    }

    dummy_log_error( MODULENAME, "unexpected response: %s", skype_service::StrHelper::to_string( ev->get_type() ).c_str() );

    callback_consume( voip_service::create_error_response( current_job_id_, 0, "unexpected response: " + skype_service::StrHelper::to_string( ev->get_type() ) ) );

    current_job_id_ = 0;
}

void Dialer::handle_in_state_w_re( const skype_service::Event * ev )
{
    // private: no mutex lock

    req_state_      = NONE;
    req_hash_id_    = 0;

    if( ev->get_type() != skype_service::Event::ALTER_CALL_SET_OUTPUT_FILE )
    {
        dummy_log_error( MODULENAME, "unexpected response: %s", skype_service::StrHelper::to_string( ev->get_type() ).c_str() );

        callback_consume( create_call_error_response(
                static_cast<const skype_service::BasicCallEvent*>( ev )->get_call_id(), 0,
                "unexpected response: " +skype_service::StrHelper::to_string( ev->get_type() ) ) );
        return;
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


void Dialer::handle( const skype_service::ConnStatusEvent * e )
{
    dummy_log_info( MODULENAME, "conn status %u", e->get_conn_s() );

    cs_ = e->get_conn_s();

    switch_to_ready_if_possible();
}

void Dialer::handle( const skype_service::UserStatusEvent * e )
{
    dummy_log_info( MODULENAME, "user status %u", e->get_user_s() );

    us_ = e->get_user_s();

    switch_to_ready_if_possible();
}

void Dialer::switch_to_ready_if_possible()
{
    if( state_ == UNKNOWN )
    {
        if( cs_ == skype_service::conn_status_e::ONLINE  &&
                ( us_ == skype_service::user_status_e::ONLINE
                        || us_ == skype_service::user_status_e::AWAY
                        || us_ == skype_service::user_status_e::DND
                        || us_ == skype_service::user_status_e::INVISIBLE
                        || us_ == skype_service::user_status_e::NA ) )
        {
            state_ = IDLE;

            dummy_log_info( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
        }
    }
    else if( state_ == IDLE )
    {
        if( cs_ == skype_service::conn_status_e::OFFLINE
                || cs_ == skype_service::conn_status_e::CONNECTING
                || us_ == skype_service::user_status_e::OFFLINE )
        {
            state_ = UNKNOWN;

            dummy_log_info( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
        }
    }
}

void Dialer::switch_to_idle_and_cleanup()
{
    state_          = IDLE;
    call_id_        = 0;
    current_job_id_ = 0;

    pstn_status_    = 0;
    pstn_status_msg_.clear();

    failure_reason_ = 0;
    failure_reason_msg_.clear();

    dummy_log_info( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

void Dialer::handle( const skype_service::CurrentUserHandleEvent * e )
{
    dummy_log_info( MODULENAME, "current user handle %s", e->get_par_str().c_str() );
}
void Dialer::on_unknown( const std::string & s )
{
    dummy_log_warn( MODULENAME, "unknown response: %s", s.c_str() );
}
void Dialer::handle( const skype_service::ErrorEvent * e )
{
    dummy_log_error( MODULENAME, "unhandled error %u '%s'", e->get_par_int(), e->get_par_str().c_str() );

    callback_consume( voip_service::create_error_response( 0, e->get_par_int(), e->get_par_str() ) );
}

void Dialer::handle_in_w_conn( const skype_service::CallStatusEvent * e )
{
    uint32_t                        call_id = e->get_call_id();
    skype_service::call_status_e    s       = e->get_call_s();

    dummy_log_debug( MODULENAME, "call %u status %s", call_id, skype_service::to_string( s ).c_str() );

    switch( s )
    {
    case skype_service::call_status_e::CANCELLED:
        callback_consume( voip_service::create_failed( call_id, voip_service::Failed::CANCELLED, 0, "cancelled by user" ) );
        switch_to_idle_and_cleanup();
        break;

    case skype_service::call_status_e::FINISHED:
        if( pstn_status_ != 0 )
            callback_consume( voip_service::create_failed( call_id, voip_service::Failed::FAILED_PSTN, pstn_status_, pstn_status_msg_ ) );
        else
            callback_consume( voip_service::create_failed( call_id, voip_service::Failed::FAILED, 0, "cancelled by user" ) );

        switch_to_idle_and_cleanup();
        break;

    case skype_service::call_status_e::ROUTING:
        callback_consume( voip_service::create_message_t<voip_service::Dial>( call_id ) );
        break;

    case skype_service::call_status_e::RINGING:
        callback_consume( voip_service::create_message_t<voip_service::Ring>( call_id ) );
        break;

    case skype_service::call_status_e::INPROGRESS:
        callback_consume( voip_service::create_message_t<voip_service::Connected>( call_id ) );
        state_          = CONNECTED;

        dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );

        break;

    case skype_service::call_status_e::NONE:
        callback_consume( voip_service::create_failed( call_id, voip_service::Failed::FAILED, 0, "call ended unexpectedly" ) );
        switch_to_idle_and_cleanup();
        break;

    case skype_service::call_status_e::FAILED:
        callback_consume( voip_service::create_failed( call_id, voip_service::Failed::FAILED, 0, "call failed" ) );
        switch_to_idle_and_cleanup();
        break;

    case skype_service::call_status_e::MISSED:
        callback_consume( voip_service::create_failed( call_id, voip_service::Failed::REFUSED, 0, "call was missed" ) );
        switch_to_idle_and_cleanup();
        break;

    case skype_service::call_status_e::BUSY:
        callback_consume( voip_service::create_failed( call_id, voip_service::Failed::BUSY, 0, "number is busy" ) );
        switch_to_idle_and_cleanup();

        break;
    case skype_service::call_status_e::REFUSED:
        callback_consume( voip_service::create_failed( call_id, voip_service::Failed::REFUSED, 0, "call was refused" ) );
        switch_to_idle_and_cleanup();
        break;

    default:
        dummy_log_warn( MODULENAME, "unhandled status %s (%u)", skype_service::to_string( s ).c_str(), s );
        break;
    }
}

void Dialer::handle_in_connected( const skype_service::CallStatusEvent * e )
{
    uint32_t                        call_id = e->get_call_id();
    skype_service::call_status_e    s       = e->get_call_s();

    dummy_log_debug( MODULENAME, "call %u status %s", call_id, skype_service::to_string( s ).c_str() );

    switch( s )
    {
    case skype_service::call_status_e::CANCELLED:
        callback_consume( voip_service::create_connection_lost( call_id, voip_service::ConnectionLost::CANCELLED, 0, "cancelled by user" ) );
        switch_to_idle_and_cleanup();
        break;

    case skype_service::call_status_e::FINISHED:
        if( pstn_status_ != 0 )
            callback_consume( voip_service::create_connection_lost( call_id, voip_service::ConnectionLost::FAILED_PSTN, pstn_status_, pstn_status_msg_ ) );
        else
            callback_consume( voip_service::create_connection_lost( call_id, voip_service::ConnectionLost::FAILED, 0, "cancelled by user" ) );

        switch_to_idle_and_cleanup();
        break;

    case skype_service::call_status_e::ROUTING:
    case skype_service::call_status_e::RINGING:
    case skype_service::call_status_e::INPROGRESS:
    case skype_service::call_status_e::BUSY:
    case skype_service::call_status_e::REFUSED:
    case skype_service::call_status_e::MISSED:
    {
        dummy_log_error( MODULENAME, "handle_in_connected: call %u, status %u, unexpected in state %s",
                call_id_, s, StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
    }
        break;

    case skype_service::call_status_e::NONE:
        callback_consume( voip_service::create_connection_lost( call_id, voip_service::ConnectionLost::FINISHED, 0, "call ended unexpectedly" ) );
        switch_to_idle_and_cleanup();
        break;

    case skype_service::call_status_e::FAILED:
        callback_consume( voip_service::create_connection_lost( call_id, voip_service::ConnectionLost::FAILED, 0, "call failed" ) );
        switch_to_idle_and_cleanup();
        break;

    default:
        dummy_log_warn( MODULENAME, "unhandled status %s (%u)", skype_service::to_string( s ).c_str(), s );
        break;
    }
}

void Dialer::handle_in_w_drpr( const skype_service::CallStatusEvent * e )
{
    uint32_t                        call_id = e->get_call_id();
    skype_service::call_status_e    s       = e->get_call_s();

    dummy_log_debug( MODULENAME, "call %u status %s", call_id, skype_service::to_string( s ).c_str() );

    switch( s )
    {

    case skype_service::call_status_e::FINISHED:
    {
        if( ignore_non_response( e ) )
        {
            return;
        }

        callback_consume( voip_service::create_drop_response( current_job_id_ ) );

        switch_to_idle_and_cleanup();

    }
        break;

    case skype_service::call_status_e::NONE:
    case skype_service::call_status_e::FAILED:
    case skype_service::call_status_e::CANCELLED:
    case skype_service::call_status_e::ROUTING:
    case skype_service::call_status_e::RINGING:
    case skype_service::call_status_e::INPROGRESS:
    case skype_service::call_status_e::BUSY:
    case skype_service::call_status_e::REFUSED:
    case skype_service::call_status_e::MISSED:
    {
        dummy_log_error( MODULENAME, "handle_in_connected: call %u, status %u, unexpected in state %s",
                call_id_, s, StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
    }
        break;

    default:
        dummy_log_warn( MODULENAME, "unhandled status %s (%u)", skype_service::to_string( s ).c_str(), s );
        break;
    }
}


void Dialer::handle( const skype_service::CallPstnStatusEvent * ev )
{
    uint32 n    = ev->get_call_id();
    uint32 e    = ev->get_par_int();
    const std::string & descr = ev->get_par_str();

    dummy_log_debug( MODULENAME, "call %u PSTN status %u '%s'", n, e, descr.c_str() );

    ASSERT( pstn_status_ == 0 );
    ASSERT( pstn_status_msg_.empty() );

    pstn_status_        = ev->get_par_int();
    pstn_status_msg_    = ev->get_par_str();
}

void Dialer::handle( const skype_service::CallDurationEvent * e )
{
    dummy_log_debug( MODULENAME, "call %u dur %u", e->get_call_id(), e->get_par_int() );

    callback_consume( voip_service::create_call_duration( e->get_call_id(), e->get_par_int() ) );
}

void Dialer::handle( const skype_service::CallFailureReasonEvent * e )
{
    dummy_log_info( MODULENAME, "call %u failure %u", e->get_call_id(), e->get_par_int() );

    ASSERT( failure_reason_ == 0 );
    ASSERT( failure_reason_msg_.empty() );

    failure_reason_     = e->get_par_int();
    failure_reason_msg_ = decode_failure_reason( failure_reason_ );
}

const char* Dialer::decode_failure_reason( const uint32 c )
{
    static const char* table[] =
    {
        "",
        "Miscellaneous error",
        "User or phone number does not exist. Check that a prefix is entered for the phone number, either in the form 003725555555 or +3725555555; the form 3725555555 is incorrect.",
        "User is offline",
        "No proxy found",
        "Session terminated.",
        "No common codec found.",
        "Sound I/O error.",
        "Problem with remote sound device.",
        "Call blocked by recipient.",
        "Recipient not a friend.",
        "Current user not authorized by recipient.",
        "Sound recording error.",
        "Failure to call a commercial contact.",
        "Conference call has been dropped by the host. Note that this does not normally indicate abnormal call termination. Call being dropped for all the participants when the conference host leavs the call is expected behaviour."
    };

    if( c > 14 )
        return "";

    return table[ c ];
}

void Dialer::handle( const skype_service::CallVaaInputStatusEvent * e )
{
    uint32 n    = e->get_call_id();
    uint32 s    = e->get_par_int();

    dummy_log_debug( MODULENAME, "call %u vaa_input_status %u", n, s );

    if( s )
        callback_consume( create_message_t<PlayFileResponse>( n ) );
}


bool Dialer::is_call_id_valid( uint32 call_id ) const
{
    return call_id == call_id_;
}

void Dialer::send_reject_response( uint32_t job_id, uint32_t errorcode, const std::string & descr )
{
    callback_consume( voip_service::create_reject_response( job_id, errorcode, descr ) );
}

void Dialer::send_error_response( uint32_t job_id, uint32_t errorcode, const std::string & descr )
{
    callback_consume( voip_service::create_error_response( job_id, errorcode, descr ) );
}

void Dialer::callback_consume( const voip_service::CallbackObject * req )
{
    if( callback_ )
        callback_->consume( req );
}

bool Dialer::send_reject_if_in_request_processing( uint32_t job_id )
{
    // called from locked area

    if( current_job_id_ == 0 )
    {
        return false;
    }

    dummy_log_info( MODULENAME, "cannot process request id %u, currently processing request %u", job_id, current_job_id_ );

    send_reject_response( job_id, 0,
            "cannot process request id " + std::string( job_id ) +
            ", currently processing request " + std::string( current_job_id_ ) );

    return true;
}

bool Dialer::ignore_non_response( const skype_service::Event * ev )
{
    if( ev->has_hash_id() == false )
    {
        dummy_log_info( MODULENAME, "state %s, ignoring a non-response notification: %s", StrHelper::to_string( state_ ).c_str(), skype_service::StrHelper::to_string( ev->get_type() ).c_str() );

        return true;
    }

    if( ev->get_hash_id() != current_job_id_ )
    {
        dummy_log_error( MODULENAME, "state %s, unexpected job_id: %u, expected %u, msg %s, ignoring",
                StrHelper::to_string( state_ ).c_str(),
                ev->get_hash_id(), current_job_id_,
                skype_service::StrHelper::to_string( ev->get_type() ).c_str() );

        return true;
    }

    return false;
}

NAMESPACE_DIALER_END
