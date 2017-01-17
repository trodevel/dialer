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

// $Revision: 5559 $ $Date:: 2017-01-16 #$ $Author: serge $

#include "dialer.h"                     // self

#include "../simple_voip/object_factory.h"      // simple_voip::create_message_t
#include "../voip_io/str_helper.h"      // simple_voip::to_string
#include "../skype_service/skype_service.h"     // skype_service::SkypeService
#include "../skype_service/str_helper.h"        // skype_service::to_string
#include "../utils/dummy_logger.h"      // dummy_log
#include "../utils/mutex_helper.h"      // MUTEX_SCOPE_LOCK
#include "../utils/assert.h"            // ASSERT

#include "str_helper.h"                 // StrHelper
#include "regex_match.h"                // regex_match

#include "namespace_lib.h"              // NAMESPACE_DIALER_START

#define MODULENAME      "Dialer"

NAMESPACE_DIALER_START

struct DetectedTone: public workt::IObject
{
    dtmf::tone_e tone;
};

// object wrapper for skype_service events
struct ObjectWrap: public workt::IObject
{
    const skype_service::Event *ev;
};

// object wrapper for simple_voip::ForwardObject messages
struct SimpleVoipWrap: public workt::IObject
{
    const simple_voip::ForwardObject *obj;
};

class Dialer;

Dialer::Dialer():
    WorkerBase( this ),
    state_( UNKNOWN ), sio_( 0L ), sched_( 0L ), callback_( 0L ),
    data_port_( 0 ),
    current_job_id_( 0 ),
    call_id_( 0 ),
    cs_( skype_service::conn_status_e::NONE ),
    us_( skype_service::user_status_e::NONE ),
    failure_reason_( 0 ),
    pstn_status_( 0 )
{
}

Dialer::~Dialer()
{
}

bool Dialer::init(
        skype_service::SkypeService * sw,
        sched::IScheduler           * sched,
        uint16_t                    data_port )
{
	MUTEX_SCOPE_LOCK( mutex_ );

    if( !sw || !sched )
        return false;

    sio_        = sw;
    sched_      = sched;
    state_      = UNKNOWN;
    data_port_  = data_port;

    player_.init( sio_, sched );

    dummy_log_info( MODULENAME, "init: port %u", data_port );

    return true;
}

bool Dialer::register_callback( simple_voip::ISimpleVoipCallback * callback )
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
    if( !sio_ || !sched_ )
        return false;

    return true;
}

Dialer::state_e Dialer::get_state() const
{
    MUTEX_SCOPE_LOCK( mutex_ );

    return state_;
}

// interface ISimpleVoip
void Dialer::consume( const simple_voip::ForwardObject * req )
{
    auto w = new SimpleVoipWrap;

    w->obj  = req;

    WorkerBase::consume( w );
}

// interface skype_service::ISkypeCallback
void Dialer::consume( const skype_service::Event * e )
{
    ObjectWrap * ew = new ObjectWrap;

    ew->ev = e;

    WorkerBase::consume( ew );
}

// interface dtmf::IDtmfDetectorCallback
void Dialer::on_detect( dtmf::tone_e button )
{
    auto ev = new DetectedTone;

    ev->tone = button;

    WorkerBase::consume( ev );
}

void Dialer::handle( const workt::IObject* req )
{
    if( typeid( *req ) == typeid( SimpleVoipWrap ) )
    {
        handle( dynamic_cast< const SimpleVoipWrap *>( req ) );
    }
    else if( typeid( *req ) == typeid( ObjectWrap ) )
    {
        handle( dynamic_cast< const ObjectWrap *>( req ) );
    }
    else if( typeid( *req ) == typeid( DetectedTone ) )
    {
        handle( dynamic_cast< const DetectedTone *>( req ) );
    }
    else
    {
        dummy_log_fatal( MODULENAME, "handle: cannot cast request to known type - %s", typeid( *req ).name() );

        ASSERT( 0 );
    }

    delete req;
}


void Dialer::handle( const simple_voip::InitiateCallRequest * req )
{
    dummy_log_debug( MODULENAME, "handle %s: ", typeid( *req ).name(), req->job_id, req->party.c_str() );

    // private: no mutex lock

    if( send_reject_if_in_request_processing( req->job_id ) )
        return;

    if( state_ != IDLE )
    {
        send_reject_due_to_wrong_state( req->job_id );
        return;
    }

    std::string party;

    if( transform_party( req->party, party ) == false )
    {
        dummy_log_error( MODULENAME, "invalid number format: %s", req->party.c_str() );

        callback_consume( simple_voip::create_error_response( req->job_id, 0, "invalid number format: " + req->party ) );

        return;
    }

    dummy_log_debug( MODULENAME, "transformed party: %s into %s", req->party.c_str(), party.c_str() );

    bool b = sio_->call( party, req->job_id );

    if( b == false )
    {
        dummy_log_error( MODULENAME, "failed calling: %s", req->party.c_str() );

        callback_consume( simple_voip::create_error_response( req->job_id, 0, "voip io failed" ) );

        return;
    }

    ASSERT( current_job_id_ == 0 );
    current_job_id_    = req->job_id;

    state_  = WAITING_INITIATE_CALL_RESPONSE;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

void Dialer::handle( const simple_voip::DropRequest * req )
{
    dummy_log_debug( MODULENAME, "handle %s: req id %u, call id %u", typeid( *req ).name(), req->job_id, req->call_id );

    // private: no mutex lock

    if( send_reject_if_in_request_processing( req->job_id ) )
        return;

    if( state_ != WAITING_CONNECTION && state_ != CONNECTED )
    {
        send_reject_due_to_wrong_state( req->job_id );
        return;
    }

    ASSERT( is_call_id_valid( req->call_id ) );

    bool b = sio_->set_call_status( req->call_id, skype_service::call_status_e::FINISHED, req->job_id );

    if( b == false )
    {
        callback_consume( simple_voip::create_error_response( req->job_id, 0, "voip io failed" ) );
        return;
    }

    current_job_id_    = req->job_id;

    if( state_ == WAITING_CONNECTION )
        state_      = CANCELED_IN_WC;
    else /* if( state_ == CONNECTED ) */
        state_      = CANCELED_IN_C;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

void Dialer::handle( const simple_voip::PlayFileRequest * req )
{
    dummy_log_debug( MODULENAME, "handle %s: req id %u, call id %u, filename %s", typeid( *req ).name(), req->job_id, req->call_id, req->filename.c_str() );

    // private: no mutex lock

    if( send_reject_if_in_request_processing( req->job_id ) )
        return;

    if( state_ != CONNECTED )
    {
        send_reject_due_to_wrong_state( req->job_id );
        return;
    }

    ASSERT( is_call_id_valid( req->call_id ) );

    player_.play_file( req->job_id, req->call_id, req->filename );
}

void Dialer::handle( const simple_voip::PlayFileStopRequest * req )
{
    dummy_log_debug( MODULENAME, "handle %s: req id %u, call id %u", typeid( *req ).name(), req->job_id, req->call_id );

    // private: no mutex lock

    if( send_reject_if_in_request_processing( req->job_id ) )
        return;

    if( state_ != CONNECTED )
    {
        send_reject_due_to_wrong_state( req->job_id );
        return;
    }

    ASSERT( is_call_id_valid( req->call_id ) );

    player_.stop( req->job_id, req->call_id );
}

void Dialer::handle( const simple_voip::RecordFileRequest * req )
{
    dummy_log_debug( MODULENAME, "handle %s: req id %u, call id %u, filename %s", typeid( *req ).name(), req->job_id, req->call_id, req->filename.c_str() );

    // private: no mutex lock

    if( send_reject_if_in_request_processing( req->job_id ) )
        return;

    if( state_ != CONNECTED )
    {
        send_reject_due_to_wrong_state( req->job_id );
        return;
    }

    ASSERT( is_call_id_valid( req->call_id ) );

    bool b = sio_->alter_call_set_output_file( req->call_id, req->filename, req->job_id );

    if( b == false )
    {
        dummy_log_error( MODULENAME, "failed setting output file: %s", req->filename.c_str() );

        callback_consume( simple_voip::create_error_response( req->job_id, 0, "failed output input file: " + req->filename ) );

        return;
    }

    callback_consume( simple_voip::create_record_file_response( req->job_id ) );
}

void Dialer::handle( const SimpleVoipWrap * w )
{
    auto * req = w->obj;

    if( typeid( *req ) == typeid( simple_voip::InitiateCallRequest ) )
    {
        handle( dynamic_cast< const simple_voip::InitiateCallRequest *>( req ) );
    }
    else if( typeid( *req ) == typeid( simple_voip::PlayFileRequest ) )
    {
        handle( dynamic_cast< const simple_voip::PlayFileRequest *>( req ) );
    }
    else if( typeid( *req ) == typeid( simple_voip::PlayFileStopRequest ) )
    {
        handle( dynamic_cast< const simple_voip::PlayFileStopRequest *>( req ) );
    }
    else if( typeid( *req ) == typeid( simple_voip::RecordFileRequest ) )
    {
        handle( dynamic_cast< const simple_voip::RecordFileRequest *>( req ) );
    }
    else if( typeid( *req ) == typeid( simple_voip::DropRequest ) )
    {
        handle( dynamic_cast< const simple_voip::DropRequest *>( req ) );
    }
    else
    {
        dummy_log_fatal( MODULENAME, "handle: cannot cast request to known type - %s", typeid( *req ).name() );

        ASSERT( 0 );
    }

    delete req;
}

void Dialer::handle( const ObjectWrap * req )
{
    // private: no mutex lock

    const skype_service::Event * ev = req->ev;

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
    case CANCELED_IN_C:
    case CANCELED_IN_WC:
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
    if( typeid( *ev ) == typeid( skype_service::ConnStatusEvent ) )
    {
        handle( dynamic_cast<const skype_service::ConnStatusEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::UserStatusEvent ) )
    {
        handle( dynamic_cast<const skype_service::UserStatusEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::CurrentUserHandleEvent ) )
    {
        handle( dynamic_cast<const skype_service::CurrentUserHandleEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::UserOnlineStatusEvent ) )
    {
    }
    else if(
            ( typeid( *ev ) == typeid( skype_service::CallEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallDurationEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallPstnStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallFailureReasonEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallVaaInputStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::AlterCallSetInputFileEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::AlterCallSetOutputFileEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::ChatEvent) ) ||
            ( typeid( *ev ) == typeid( skype_service::ChatMemberEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::ErrorEvent ) ) )

    {
        dummy_log_error( MODULENAME, "handle_in_state_unknown: event %s, unexpected in state %s",
                typeid( *ev ).name(), StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
    }
    else if( typeid( *ev ) == typeid( skype_service::UnknownEvent ) )
    {
        on_unknown( ( dynamic_cast<const skype_service::UnknownEvent*>( ev ) )->descr );
    }
    else
    {
        dummy_log_fatal( MODULENAME, "handle_in_state_unknown: cannot cast request to known type - %p (%s)", (void *) ev, typeid( *ev ).name() );

        ASSERT( 0 );
    }
}

void Dialer::handle_in_state_idle( const skype_service::Event * ev )
{
    // private: no mutex lock

    if( typeid( *ev ) == typeid( skype_service::ConnStatusEvent ) )
    {
        handle( dynamic_cast<const skype_service::ConnStatusEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::UserStatusEvent ) )
    {
        handle( dynamic_cast<const skype_service::UserStatusEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::CurrentUserHandleEvent ) )
    {
        handle( dynamic_cast<const skype_service::CurrentUserHandleEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::UserOnlineStatusEvent ) )
    {
    }
    else if(
            ( typeid( *ev ) == typeid( skype_service::CallEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallPstnStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallFailureReasonEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::AlterCallSetInputFileEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::AlterCallSetOutputFileEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::ErrorEvent ) ) )
    {
        dummy_log_error( MODULENAME, "handle_in_state_idle: event %s, unexpected in state %s",
                typeid( *ev ).name(), StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
    }
    else if(
            ( typeid( *ev ) == typeid( skype_service::CallVaaInputStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallDurationEvent ) ) )
    {
        dummy_log_warn( MODULENAME, "handle_in_state_idle: event %s, unexpected in state %s, probably out-of-order",
                typeid( *ev ).name(), StrHelper::to_string( state_ ).c_str() );
    }
//    else if( typeid( *ev ) == typeid( skype_service::UNDEF:
//        break;
    else if(
            ( typeid( *ev ) == typeid( skype_service::UserEvent) ) ||
            ( typeid( *ev ) == typeid( skype_service::ChatEvent) ) ||
            ( typeid( *ev ) == typeid( skype_service::ChatMemberEvent ) ) )
    {
        // simply ignore
    }
    else if( typeid( *ev ) == typeid( skype_service::UnknownEvent ) )
    {
        on_unknown( ( dynamic_cast<const skype_service::UnknownEvent*>( ev ) )->descr );
    }
    else
    {
        dummy_log_fatal( MODULENAME, "handle_in_state_idle: cannot cast request to known type - %p (%s)", (void *) ev, typeid( *ev ).name() );
        ASSERT( 0 );
    }
}

void Dialer::handle_in_state_w_ical( const skype_service::Event * ev )
{
    // private: no mutex lock

    if(
            ( typeid( *ev ) == typeid( skype_service::ConnStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::UserStatusEvent ) ) )
    {
        // TODO process disconnect
    }
    else if(
            ( typeid( *ev ) == typeid( skype_service::CallEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallDurationEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallPstnStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallFailureReasonEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallVaaInputStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::AlterCallSetInputFileEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::AlterCallSetOutputFileEvent ) ) )
    {
        dummy_log_error( MODULENAME, "handle_in_state_w_ical: event %s, unexpected in state %s",
                typeid( *ev ).name(), StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
    }
    else if( typeid( *ev ) == typeid( skype_service::CallStatusEvent ) )
    {
        handle_in_w_ical( dynamic_cast<const skype_service::CallStatusEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::ErrorEvent ) )
    {
        if( ignore_non_expected_response( ev ) )
        {
            return;
        }

        uint32_t errorcode  = dynamic_cast<const skype_service::ErrorEvent*>( ev )->error_code;
        std::string descr   = dynamic_cast<const skype_service::ErrorEvent*>( ev )->descr;

        dummy_log_error( MODULENAME, "job_id %u, error %u '%s'", current_job_id_, errorcode, descr.c_str() );

        callback_consume( simple_voip::create_error_response( current_job_id_, errorcode, descr ) );

        current_job_id_ = 0;
        state_          = IDLE;

        dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
    }
//    else if( typeid( *ev ) == typeid( skype_service::UNDEF:
    else if(
            ( typeid( *ev ) == typeid( skype_service::CurrentUserHandleEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::UserOnlineStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::ChatEvent) ) ||
            ( typeid( *ev ) == typeid( skype_service::ChatMemberEvent ) ) )
    {
        // simply ignore
    }
    else if( typeid( *ev ) == typeid( skype_service::UnknownEvent ) )
    {
        on_unknown( ( dynamic_cast<const skype_service::UnknownEvent*>( ev ) )->descr );
    }
    else
    {
        dummy_log_fatal( MODULENAME, "handle_in_state_w_ical: cannot cast request to known type - %p (%s)", (void *) ev, typeid( *ev ).name() );
        ASSERT( 0 );
    }
}

void Dialer::handle_in_state_w_conn( const skype_service::Event * ev )
{
    // private: no mutex lock

    ASSERT( current_job_id_ == 0 );

    if(
            ( typeid( *ev ) == typeid( skype_service::ConnStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::UserStatusEvent ) ) )
    {
        // TODO process disconnect
    }
    else if(
            ( typeid( *ev ) == typeid( skype_service::CallEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallDurationEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallVaaInputStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::AlterCallSetInputFileEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::AlterCallSetOutputFileEvent ) ) )
    {
        dummy_log_error( MODULENAME, "handle_in_state_w_conn: event %s, unexpected in state %s",
                typeid( *ev ).name(), StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
    }
    else if( typeid( *ev ) == typeid( skype_service::CallPstnStatusEvent ) )
    {
        handle( dynamic_cast<const skype_service::CallPstnStatusEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::CallFailureReasonEvent ) )
    {
        handle( dynamic_cast<const skype_service::CallFailureReasonEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::CallStatusEvent ) )
    {
        handle_in_w_conn( dynamic_cast<const skype_service::CallStatusEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::ErrorEvent ) )
    {
        const skype_service::ErrorEvent * ev_c = dynamic_cast<const skype_service::ErrorEvent*>( ev );

        uint32_t errorcode  = ev_c->error_code;
        std::string descr   = ev_c->descr;

        dummy_log_error( MODULENAME, "error %u '%s'", errorcode, descr.c_str() );

        callback_consume( simple_voip::create_failed( call_id_, simple_voip::Failed::FAILED, "ERROR: " + descr ) );

        switch_to_idle_and_cleanup();
    }
    //    else if( typeid( *ev ) == typeid( skype_service::UNDEF:
    else if(
            ( typeid( *ev ) == typeid( skype_service::CurrentUserHandleEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::UserOnlineStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::ChatEvent) ) ||
            ( typeid( *ev ) == typeid( skype_service::ChatMemberEvent ) ) )
    {
        // simply ignore
    }
    else if( typeid( *ev ) == typeid( skype_service::VoicemailDurationEvent ) )
    {
        // simply ignored
    }
    else if( typeid( *ev ) == typeid( skype_service::UnknownEvent ) )
    {
        on_unknown( ( dynamic_cast<const skype_service::UnknownEvent*>( ev ) )->descr );
    }
    else
    {
        dummy_log_fatal( MODULENAME, "handle_in_state_w_conn: cannot cast request to known type - %p (%s)", (void *) ev, typeid( *ev ).name() );
        ASSERT( 0 );
    }
}

void Dialer::handle_in_state_connected( const skype_service::Event * ev )
{
    // private: no mutex lock

    if(
            ( typeid( *ev ) == typeid( skype_service::ConnStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::UserStatusEvent ) ) )
    {
        // TODO process disconnect
    }
    else if( typeid( *ev ) == typeid( skype_service::CallEvent ) )
    {
    }
    else if( typeid( *ev ) == typeid( skype_service::CallDurationEvent ) )
    {
        handle( dynamic_cast<const skype_service::CallDurationEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::VoicemailDurationEvent ) )
    {
        handle( dynamic_cast<const skype_service::VoicemailDurationEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::CallPstnStatusEvent ) )
    {
        handle( dynamic_cast<const skype_service::CallPstnStatusEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::CallFailureReasonEvent ) )
    {
        handle( dynamic_cast<const skype_service::CallFailureReasonEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::CallStatusEvent ) )
    {
        handle_in_connected( dynamic_cast<const skype_service::CallStatusEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::ErrorEvent ) )
    {
        if( ev->req_id == 0 ) // do not handle unexpected responses
        {
            const skype_service::ErrorEvent * ev_c = dynamic_cast<const skype_service::ErrorEvent*>( ev );

            uint32_t errorcode  = ev_c->error_code;
            std::string descr   = ev_c->descr;

            dummy_log_error( MODULENAME, "error %u '%s'", errorcode, descr.c_str() );

            callback_consume( simple_voip::create_connection_lost( call_id_, descr ) );

            switch_to_idle_and_cleanup();
        }
    }
    else if(
            ( typeid( *ev ) == typeid( skype_service::CurrentUserHandleEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::UserOnlineStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::ChatEvent) ) ||
            ( typeid( *ev ) == typeid( skype_service::ChatMemberEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallVaaInputStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::AlterCallSetInputFileEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::AlterCallSetOutputFileEvent ) ) )
    {
        // simply ignore
    }
    else if( typeid( *ev ) == typeid( skype_service::UnknownEvent ) )
    {
        on_unknown( ( dynamic_cast<const skype_service::UnknownEvent*>( ev ) )->descr );
    }
    else
    {
        dummy_log_fatal( MODULENAME, "handle_in_state_connected: cannot cast request to known type - %p (%s)", (void *) ev, typeid( *ev ).name() );
        ASSERT( 0 );
    }

    forward_to_player( ev );
}

void Dialer::handle_in_state_w_drpr( const skype_service::Event * ev )
{
    // private: no mutex lock

    if(
            ( typeid( *ev ) == typeid( skype_service::ConnStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::UserStatusEvent ) ) )
    {
        // TODO process disconnect
    }
    else if( typeid( *ev ) == typeid( skype_service::CallEvent ) )
    {
    }
    else if( typeid( *ev ) == typeid( skype_service::CallDurationEvent ) )
    {
        handle( dynamic_cast<const skype_service::CallDurationEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::CallVaaInputStatusEvent ) )
    {
        auto e = dynamic_cast<const skype_service::CallVaaInputStatusEvent *>( ev );

        uint32_t n  = e->call_id;
        uint32_t s  = e->status;

        dummy_log_debug( MODULENAME, "call %u vaa_input_status %u", n, s );

        if( s )
        {
            // play start is really not expected while waiting for drop response
            dummy_log_error( MODULENAME, "handle_in_state_w_drpr: event %s, unexpected in state %s",
                    typeid( *ev ).name(), StrHelper::to_string( state_ ).c_str() );
            ASSERT( 0 );
        }
        else
        {
            // play end may come, because the player doesn't wait for it
            dummy_log_debug( MODULENAME, "handle_in_state_w_drpr: event %s, unexpected in state %s, ignored",
                    typeid( *ev ).name(), StrHelper::to_string( state_ ).c_str() );
        }
    }
    else if(
            ( typeid( *ev ) == typeid( skype_service::AlterCallSetOutputFileEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::AlterCallSetInputFileEvent ) ) )
    {
        dummy_log_error( MODULENAME, "handle_in_state_w_drpr: event %s, unexpected in state %s",
                typeid( *ev ).name(), StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
    }
    else if( typeid( *ev ) == typeid( skype_service::CallPstnStatusEvent ) )
    {
        handle( dynamic_cast<const skype_service::CallPstnStatusEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::CallFailureReasonEvent ) )
    {
        handle( dynamic_cast<const skype_service::CallFailureReasonEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::CallStatusEvent ) )
    {
        if( state_ == CANCELED_IN_C )
            handle_in_w_drpr( dynamic_cast<const skype_service::CallStatusEvent*>( ev ) );
        else
            handle_in_w_drpr_2( dynamic_cast<const skype_service::CallStatusEvent*>( ev ) );
    }
    else if( typeid( *ev ) == typeid( skype_service::ErrorEvent ) )
    {
        const skype_service::ErrorEvent * ev_c = dynamic_cast<const skype_service::ErrorEvent*>( ev );

        uint32_t errorcode  = ev_c->error_code;
        std::string descr   = ev_c->descr;

        dummy_log_error( MODULENAME, "error %u '%s'", errorcode, descr.c_str() );

        callback_consume( simple_voip::create_connection_lost( call_id_, "ERROR: " + std::to_string( errorcode ) + ", " + descr ) );

        switch_to_idle_and_cleanup();
    }
    else if(
            ( typeid( *ev ) == typeid( skype_service::CurrentUserHandleEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::UserOnlineStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::ChatEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::ChatMemberEvent ) ) )
    {
    // simply ignore
    }
    else if( typeid( *ev ) == typeid( skype_service::UnknownEvent ) )
    {
        on_unknown( ( dynamic_cast<const skype_service::UnknownEvent*>( ev ) )->descr );
    }
    else
    {
        dummy_log_fatal( MODULENAME, "handle_in_state_connected: cannot cast request to known type - %p (%s)", (void *) ev, typeid( *ev ).name() );
        ASSERT( 0 );
    }
}

void Dialer::forward_to_player( const skype_service::Event * ev )
{
    // called from locked area: no mutex lock

    if(
            ( typeid( *ev ) == typeid( skype_service::ConnStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::UserStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallDurationEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::AlterCallSetOutputFileEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallPstnStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallFailureReasonEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CallStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::CurrentUserHandleEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::UserOnlineStatusEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::ChatEvent ) ) ||
            ( typeid( *ev ) == typeid( skype_service::ChatMemberEvent ) ) )
    {
        // simply ignore
    }
    else if( typeid( *ev ) == typeid( skype_service::CallVaaInputStatusEvent ) )
    {
        auto e = dynamic_cast<const skype_service::CallVaaInputStatusEvent *>( ev );

        uint32_t n  = e->call_id;
        uint32_t s  = e->status;

        dummy_log_debug( MODULENAME, "call %u vaa_input_status %u", n, s );

        if( s )
            player_.on_play_start( n );
        else
            player_.on_play_stop( n );
    }
    else if( typeid( *ev ) == typeid( skype_service::AlterCallSetInputFileEvent ) )
    {
        if( ignore_non_response( ev ) )
        {
            return;
        }

        player_.on_play_file_response( ev->req_id );
    }
    else if( typeid( *ev ) == typeid( skype_service::ErrorEvent ) )
    {
        if( ignore_non_response( ev ) )
        {
            return;
        }

        player_.on_error_response( ev->req_id );
    }
}

void Dialer::start()
{
    dummy_log_debug( MODULENAME, "start()" );

    WorkerBase::start();
}

bool Dialer::shutdown()
{
    dummy_log_debug( MODULENAME, "shutdown()" );

    MUTEX_SCOPE_LOCK( mutex_ );

    if( !is_inited__() )
        return false;

    WorkerBase::shutdown();

    return true;
}

void Dialer::handle( const skype_service::ConnStatusEvent * e )
{
    dummy_log_info( MODULENAME, "conn status %u", e->status );

    cs_ = e->status;

    switch_to_ready_if_possible();
}

void Dialer::handle( const skype_service::UserStatusEvent * e )
{
    dummy_log_info( MODULENAME, "user status %u", e->status );

    us_ = e->status;

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

    player_.on_loss();

    dummy_log_info( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

void Dialer::handle( const skype_service::CurrentUserHandleEvent * e )
{
    dummy_log_info( MODULENAME, "current user handle %s", e->user_handle.c_str() );
}
void Dialer::on_unknown( const std::string & s )
{
    dummy_log_warn( MODULENAME, "unknown response: %s", s.c_str() );
}
void Dialer::handle( const skype_service::ErrorEvent * e )
{
    dummy_log_error( MODULENAME, "unhandled error %u '%s'", e->error_code, e->descr.c_str() );

    callback_consume( simple_voip::create_error_response( 0, e->error_code, e->descr ) );
}

void Dialer::handle_in_w_ical( const skype_service::CallStatusEvent * e )
{
    uint32_t                        call_id = e->call_id;
    skype_service::call_status_e    s       = e->status;

    dummy_log_debug( MODULENAME, "call %u status %s", call_id, skype_service::to_string( s ).c_str() );

    if( ignore_non_expected_response( e ) )
    {
        return;
    }

    dummy_log_debug( MODULENAME, "job_id %u, call initiated: %u, status %s", current_job_id_, call_id, skype_service::to_string( s ).c_str() );

    callback_consume( simple_voip::create_initiate_call_response( current_job_id_, call_id ) );

    current_job_id_ = 0;
    call_id_        = call_id;
    state_          = WAITING_CONNECTION;

    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

void Dialer::handle_in_w_conn( const skype_service::CallStatusEvent * e )
{
    uint32_t                        call_id = e->call_id;
    skype_service::call_status_e    s       = e->status;

    dummy_log_debug( MODULENAME, "call %u status %s", call_id, skype_service::to_string( s ).c_str() );

    switch( s )
    {
    case skype_service::call_status_e::CANCELLED:
        callback_consume( simple_voip::create_failed( call_id, simple_voip::Failed::FAILED, "cancelled by user" ) );
        switch_to_idle_and_cleanup();
        break;

    case skype_service::call_status_e::FINISHED:
        if( pstn_status_ != 0 )
            callback_consume( simple_voip::create_failed( call_id, simple_voip::Failed::FAILED, "PSTN: " + std::to_string( pstn_status_ ) + ", " + pstn_status_msg_ ) );
        else
            callback_consume( simple_voip::create_failed( call_id, simple_voip::Failed::FAILED, "cancelled by user" ) );

        switch_to_idle_and_cleanup();
        break;

    case skype_service::call_status_e::ROUTING:
        callback_consume( simple_voip::create_message_t<simple_voip::Dialing>( call_id ) );
        break;

    case skype_service::call_status_e::RINGING:
        callback_consume( simple_voip::create_message_t<simple_voip::Ringing>( call_id ) );
        break;

    case skype_service::call_status_e::VM_RECORDING:
        callback_consume( simple_voip::create_message_t<simple_voip::Connected>( call_id ) );
        state_          = CONNECTED;
        dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
        break;

    case skype_service::call_status_e::INPROGRESS:
        callback_consume( simple_voip::create_message_t<simple_voip::Connected>( call_id ) );
        state_          = CONNECTED;

        dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );

        if( data_port_ != 0 )
        {
            dummy_log_debug( MODULENAME, "redirected input data to port %u", data_port_ );

            bool b = sio_->alter_call_set_output_port( call_id, data_port_ );

            if( b == false )
            {
                dummy_log_error( MODULENAME, "failed to redirect input data to port %u", data_port_ );
            }
        }

        break;

    case skype_service::call_status_e::NONE:
        callback_consume( simple_voip::create_failed( call_id, simple_voip::Failed::FAILED, "call ended unexpectedly" ) );
        switch_to_idle_and_cleanup();
        break;

    case skype_service::call_status_e::FAILED:
    case skype_service::call_status_e::VM_FAILED:
        callback_consume( simple_voip::create_failed( call_id, simple_voip::Failed::FAILED, "call failed" ) );
        switch_to_idle_and_cleanup();
        break;

    case skype_service::call_status_e::MISSED:
        callback_consume( simple_voip::create_failed( call_id, simple_voip::Failed::REFUSED, "call was missed" ) );
        switch_to_idle_and_cleanup();
        break;

    case skype_service::call_status_e::BUSY:
        callback_consume( simple_voip::create_failed( call_id, simple_voip::Failed::BUSY, "number is busy" ) );
        switch_to_idle_and_cleanup();

        break;
    case skype_service::call_status_e::REFUSED:
        callback_consume( simple_voip::create_failed( call_id, simple_voip::Failed::REFUSED, "call was refused" ) );
        switch_to_idle_and_cleanup();
        break;

    default:
        dummy_log_warn( MODULENAME, "unhandled status %s (%u)", skype_service::to_string( s ).c_str(), s );
        break;
    }
}

void Dialer::handle_in_connected( const skype_service::CallStatusEvent * e )
{
    uint32_t                        call_id = e->call_id;
    skype_service::call_status_e    s       = e->status;

    dummy_log_debug( MODULENAME, "call %u status %s", call_id, skype_service::to_string( s ).c_str() );

    switch( s )
    {
    case skype_service::call_status_e::CANCELLED:
        callback_consume( simple_voip::create_connection_lost( call_id, "cancelled by user" ) );
        switch_to_idle_and_cleanup();
        break;

    case skype_service::call_status_e::FINISHED:
        if( pstn_status_ != 0 )
            callback_consume( simple_voip::create_connection_lost( call_id, "PSTN: " + std::to_string( pstn_status_ ) + ", " + pstn_status_msg_ ) );
        else
            callback_consume( simple_voip::create_connection_lost( call_id, "cancelled by user" ) );

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
        callback_consume( simple_voip::create_connection_lost( call_id, "call ended unexpectedly" ) );
        switch_to_idle_and_cleanup();
        break;

    case skype_service::call_status_e::FAILED:
        callback_consume( simple_voip::create_connection_lost( call_id, "call failed" ) );
        switch_to_idle_and_cleanup();
        break;

    default:
        dummy_log_warn( MODULENAME, "unhandled status %s (%u)", skype_service::to_string( s ).c_str(), s );
        break;
    }
}

void Dialer::handle_in_w_drpr( const skype_service::CallStatusEvent * e )
{
    uint32_t                        call_id = e->call_id;
    skype_service::call_status_e    s       = e->status;

    dummy_log_debug( MODULENAME, "call %u status %s (%u)", call_id, skype_service::to_string( s ).c_str(), s );

    // ignore command response as it carries current status
    if( ignore_response( e ) )
    {
        return;
    }

    switch( s )
    {

    case skype_service::call_status_e::FINISHED:
    {
        callback_consume( simple_voip::create_drop_response( current_job_id_ ) );

        switch_to_idle_and_cleanup();
    }
        break;


    case skype_service::call_status_e::VM_SENT:
        callback_consume( simple_voip::create_drop_response( current_job_id_ ) );

        switch_to_idle_and_cleanup();
        break;


    case skype_service::call_status_e::NONE:
    case skype_service::call_status_e::FAILED:
    case skype_service::call_status_e::CANCELLED:
    case skype_service::call_status_e::ROUTING:
    case skype_service::call_status_e::RINGING:
    case skype_service::call_status_e::INPROGRESS:
    case skype_service::call_status_e::EARLYMEDIA:
    case skype_service::call_status_e::BUSY:
    case skype_service::call_status_e::REFUSED:
    case skype_service::call_status_e::MISSED:
    {
        dummy_log_error( MODULENAME, "handle_in_w_drpr: call %u, status %u, unexpected in state %s",
                call_id_, s, StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
    }
        break;

    default:
        dummy_log_warn( MODULENAME, "unhandled status %s (%u)", skype_service::to_string( s ).c_str(), s );
        break;
    }
}

void Dialer::handle_in_w_drpr_2( const skype_service::CallStatusEvent * e )
{
    uint32_t                        call_id = e->call_id;
    skype_service::call_status_e    s       = e->status;

    dummy_log_debug( MODULENAME, "call %u status %s (%u)", call_id, skype_service::to_string( s ).c_str(), s );

    // ignore command response as it carries current status
    if( ignore_response( e ) )
    {
        return;
    }

    switch( s )
    {

    case skype_service::call_status_e::CANCELLED:
    {
        /*
        if( ignore_non_response( e ) )
        {
            return;
        }
        */

        callback_consume( simple_voip::create_drop_response( current_job_id_ ) );

        switch_to_idle_and_cleanup();

    }
        break;

    case skype_service::call_status_e::INPROGRESS:
    case skype_service::call_status_e::EARLYMEDIA:
    case skype_service::call_status_e::ROUTING:
    case skype_service::call_status_e::RINGING:
    {
        if( ignore_non_expected_response( e ) )
        {
            return;
        }

        dummy_log_info( MODULENAME, "handle_in_w_drpr_2: call %u, status %u, ignoring in state %s",
                call_id_, s, StrHelper::to_string( state_ ).c_str() );
    }
    break;

    case skype_service::call_status_e::NONE:
    case skype_service::call_status_e::FAILED:
    case skype_service::call_status_e::FINISHED:
    case skype_service::call_status_e::BUSY:
    case skype_service::call_status_e::REFUSED:
    case skype_service::call_status_e::MISSED:
    {
        dummy_log_error( MODULENAME, "handle_in_w_drpr_2: call %u, status %u, unexpected in state %s",
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
    uint32_t n  = ev->call_id;
    uint32_t e  = ev->error_code;
    const std::string & descr = ev->descr;

    dummy_log_debug( MODULENAME, "call %u PSTN status %u '%s'", n, e, descr.c_str() );

    ASSERT( pstn_status_ == 0 );
    ASSERT( pstn_status_msg_.empty() );

    pstn_status_        = ev->error_code;
    pstn_status_msg_    = ev->descr;
}

void Dialer::handle( const skype_service::CallDurationEvent * e )
{
    dummy_log_debug( MODULENAME, "call %u dur %u", e->call_id, e->duration );
}

void Dialer::handle( const skype_service::VoicemailDurationEvent * e )
{
    dummy_log_debug( MODULENAME, "call %u voicemail dur %u", e->call_id, e->duration );
}

void Dialer::handle( const skype_service::CallFailureReasonEvent * e )
{
    dummy_log_info( MODULENAME, "call %u failure %u", e->call_id, e->reason );

    ASSERT( failure_reason_ == 0 );
    ASSERT( failure_reason_msg_.empty() );

    failure_reason_     = e->reason;
    failure_reason_msg_ = decode_failure_reason( failure_reason_ );
}

void Dialer::handle( const DetectedTone * e )
{
    dummy_log_info( MODULENAME, "detected tone %u", e->tone );

    switch( state_ )
    {
    case CONNECTED:
    {
        auto tone = decode_tone( e->tone );

        auto ev = simple_voip::create_dtmf_tone( call_id_, tone );

        callback_consume( ev );
    }
        break;
    default:
        dummy_log_error( MODULENAME, "handle: unexpected in state %s",
                StrHelper::to_string( state_ ).c_str() );
        break;
    }

}

const char* Dialer::decode_failure_reason( const uint32_t c )
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

simple_voip::DtmfTone::tone_e Dialer::decode_tone( dtmf::tone_e tone )
{
    static const simple_voip::DtmfTone::tone_e table[] =
    {
        simple_voip::DtmfTone::tone_e::TONE_0,
        simple_voip::DtmfTone::tone_e::TONE_1,
        simple_voip::DtmfTone::tone_e::TONE_2,
        simple_voip::DtmfTone::tone_e::TONE_3,
        simple_voip::DtmfTone::tone_e::TONE_4,
        simple_voip::DtmfTone::tone_e::TONE_5,
        simple_voip::DtmfTone::tone_e::TONE_6,
        simple_voip::DtmfTone::tone_e::TONE_7,
        simple_voip::DtmfTone::tone_e::TONE_8,
        simple_voip::DtmfTone::tone_e::TONE_9,
        simple_voip::DtmfTone::tone_e::TONE_A,
        simple_voip::DtmfTone::tone_e::TONE_B,
        simple_voip::DtmfTone::tone_e::TONE_C,
        simple_voip::DtmfTone::tone_e::TONE_D,
        simple_voip::DtmfTone::tone_e::TONE_STAR,
        simple_voip::DtmfTone::tone_e::TONE_HASH
    };

    if( tone >= dtmf::tone_e::TONE_0 && tone <= dtmf::tone_e::TONE_HASH )
    {
        return table[ static_cast<uint16_t>( tone ) ];
    }

    return simple_voip::DtmfTone::tone_e::TONE_A;
}

Dialer::party_e Dialer::get_party_type( const std::string & inp )
{
    if( regex_match( inp, "^\\+[1-9]+[0-9]*$") )
        return party_e::NUMBER;
    if ( regex_match( inp, "^[a-zA-Z][a-zA-Z0-9_]*$") )
        return party_e::SYMBOLIC;

    return party_e::UNKNOWN;
}

bool Dialer::transform_party( const std::string & inp, std::string & outp )
{
    auto party_type = get_party_type( inp );

    if( party_type == party_e::NUMBER )
    {
        outp = "00" + inp.substr( 1 );
        return true;
    }

    if( party_type == party_e::SYMBOLIC )
    {
        outp = inp;
        return true;
    }

    return false;
}

bool Dialer::is_call_id_valid( uint32_t call_id ) const
{
    return call_id == call_id_;
}

void Dialer::send_reject_response( uint32_t job_id, uint32_t errorcode, const std::string & descr )
{
    callback_consume( simple_voip::create_reject_response( job_id, errorcode, descr ) );
}

void Dialer::send_error_response( uint32_t job_id, uint32_t errorcode, const std::string & descr )
{
    callback_consume( simple_voip::create_error_response( job_id, errorcode, descr ) );
}

void Dialer::callback_consume( const simple_voip::CallbackObject * req )
{
    if( callback_ )
        callback_->consume( req );
}

void Dialer::send_reject_due_to_wrong_state( uint32_t job_id )
{
    // called from locked area

    dummy_log_error( MODULENAME, "cannot process request job_id %u in state %s", job_id, StrHelper::to_string( state_ ).c_str() );

    send_reject_response( job_id, 0,
            "cannot process in state " + StrHelper::to_string( state_ ) );
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
            "cannot process request id " + std::to_string( job_id ) +
            ", currently processing request " + std::to_string( current_job_id_ ) );

    return true;
}

bool Dialer::ignore_response( const skype_service::Event * ev )
{
    if( ev->req_id != 0 )
    {
        dummy_log_info( MODULENAME, "state %s, ignoring a response notification: %s, job_id %u",
                StrHelper::to_string( state_ ).c_str(),
                typeid( *ev ).name(),
                ev->req_id );

        return true;
    }

    return false;
}

bool Dialer::ignore_non_response( const skype_service::Event * ev )
{
    if( ev->req_id == 0 )
    {
        dummy_log_info( MODULENAME, "state %s, ignoring a non-response notification: %s", StrHelper::to_string( state_ ).c_str(), typeid( *ev ).name() );

        return true;
    }

    return false;
}

bool Dialer::ignore_non_expected_response( const skype_service::Event * ev )
{
    if( ignore_non_response( ev ) )
        return true;

    if( ev->req_id != current_job_id_ )
    {
        dummy_log_error( MODULENAME, "state %s, unexpected job_id: %u, expected %u, msg %s, ignoring",
                StrHelper::to_string( state_ ).c_str(),
                ev->req_id, current_job_id_,
                typeid( *ev ).name() );

        return true;
    }

    return false;
}

NAMESPACE_DIALER_END
