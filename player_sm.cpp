/*

Player state machine.

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

// $Revision: 7092 $ $Date:: 2017-07-06 #$ $Author: serge $

#include "player_sm.h"              // self

#include <functional>                   // std::bind

#include "../simple_voip/i_simple_voip.h"  // IVoipService
#include "../simple_voip/i_simple_voip_callback.h" // ISimpleVoipCallback
#include "../skype_service/skype_service.h"     // skype_service::SkypeService
#include "../simple_voip/object_factory.h"  // simple_voip::create_play_file
#include "../utils/dummy_logger.h"      // dummy_log
#include "../utils/mutex_helper.h"      // MUTEX_SCOPE_LOCK
#include "../utils/assert.h"            // ASSERT
#include "str_helper.h"                 // StrHelper

#include "../scheduler/i_scheduler.h"       // IScheduler
#include "../scheduler/timeout_job_aux.h"   // create_timeout_job

#include "namespace_lib.h"          // NAMESPACE_DIALER_START

#define MODULENAME      "PlayerSM"

#define PLAY_TIMEOUT    ( 2 )

NAMESPACE_DIALER_START

PlayerSM::PlayerSM():
    state_( IDLE ), req_id_( 0 ), sio_( 0L ), sched_( 0L ), callback_( nullptr ), job_id_( 0 )
{
}

PlayerSM::~PlayerSM()
{
//    if( job_id_ )
//    {
//        delete job_id_;
//    }
}

bool PlayerSM::init( skype_service::SkypeService * sw, scheduler::IScheduler * sched )
{
    if( !sw || !sched )
        return false;

    sio_    = sw;
    sched_  = sched;

    dummy_log_info( MODULENAME, "init: switching to IDLE" );

    state_  = IDLE;
    req_id_ = 0;

    return true;
}


bool PlayerSM::register_callback( simple_voip::ISimpleVoipCallback  * callback )
{
    if( callback == 0L )
        return false;

    MUTEX_SCOPE_LOCK( mutex_ );

    if( callback_ != 0L )
        return false;

    callback_ = callback;

    return true;
}

bool PlayerSM::is_inited() const
{
    if( !sio_ || sched_ == 0L )
        return false;

    return true;
}


void PlayerSM::play_file( uint32_t req_id, uint32_t call_id, const std::string & filename )
{
    dummy_log_debug( MODULENAME, "play_file: req_id %u", req_id );

    MUTEX_SCOPE_LOCK( mutex_ );

    if( state_ != IDLE )
    {
        dummy_log_fatal( MODULENAME, "play_file: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( false );
        return;
    }

    ASSERT( is_inited() );

    bool b = sio_->alter_call_set_input_file( call_id, filename, req_id );

    if( b == false )
    {
        dummy_log_error( MODULENAME, "failed setting input file: %s", filename.c_str() );

        callback_->consume( simple_voip::create_error_response( req_id, 0, "failed setting input file: " + filename ) );

        return;
    }

    ASSERT( req_id_ == 0 );

    req_id_ = req_id;

    next_state( WAIT_PLAY_RESP );
}

void PlayerSM::stop( uint32_t req_id, uint32_t call_id )
{
    dummy_log_debug( MODULENAME, "stop: req_id %u", req_id );

    MUTEX_SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case IDLE:
    {
        dummy_log_warn( MODULENAME, "stop: ineffective in state %s", StrHelper::to_string( state_ ).c_str() );
        break;
    }

    case WAIT_PLAY_START:
    {
        if( job_id_ )
        {
            std::string error_msg;
            sched_->delete_job( & error_msg, job_id_ );     // cancel timeout job as replay was successfully started
            job_id_     = 0;
        }

        dummy_log_debug( MODULENAME, "stop: ok" );

        callback_->consume( simple_voip::create_play_file_stop_response( req_id ) );

        req_id_     = 0;
        next_state( IDLE );
    }
        break;

    case PLAYING_ALREADY_STOPPED:
    {
        dummy_log_debug( MODULENAME, "stop: ok" );

        callback_->consume( simple_voip::create_play_file_stop_response( req_id ) );

        req_id_     = 0;
        next_state( IDLE );
    }
        break;

    case PLAYING:
    {
        auto b = sio_->alter_call_set_input_soundcard( call_id, req_id );

        if( b == false )
        {
            dummy_log_error( MODULENAME, "failed input soundcard" );

            callback_->consume( simple_voip::create_error_response( req_id, 0, "failed setting input soundcard" ) );

            return;
        }

        ASSERT( req_id_ == 0 );

        req_id_ = req_id;

        next_state( CANCELED_IN_P );
    }
    break;

    default:
        dummy_log_error( MODULENAME, "stop: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( 0 );
        break;
    }

    ASSERT( is_inited() );
}

void PlayerSM::on_loss()
{
    dummy_log_debug( MODULENAME, "on_loss" );

    MUTEX_SCOPE_LOCK( mutex_ );

    if( state_ == IDLE )
    {
        dummy_log_warn( MODULENAME, "on_loss: ineffective in state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

    if( state_ == WAIT_PLAY_START )
    {
        if( job_id_ )
        {
            std::string error_msg;
            sched_->delete_job( & error_msg, job_id_ );     // cancel timeout job as replay was successfully started
            job_id_     = 0;
        }
    }

    dummy_log_debug( MODULENAME, "on_loss: ok" );

    ASSERT( is_inited() );

    req_id_     = 0;
    next_state( IDLE );
}

void PlayerSM::on_play_file_response( uint32_t req_id )
{
    dummy_log_debug( MODULENAME, "on_play_file_response: req_id %u", req_id );

    MUTEX_SCOPE_LOCK( mutex_ );

    if( state_ != WAIT_PLAY_RESP )
    {
        dummy_log_fatal( MODULENAME, "on_play_file_response: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( false );
        return;
    }

    dummy_log_debug( MODULENAME, "on_play_file_response: ok" );

    if( job_id_ )
    {
        std::string error_msg;
        sched_->delete_job( & error_msg, job_id_ );     // cancel timeout job as replay was successfully started
        job_id_     = 0;
    }

    std::string err_msg;
    scheduler::create_and_insert_timeout_job( & job_id_, & err_msg, * sched_, "timeout", PLAY_TIMEOUT, std::bind( &PlayerSM::on_play_failed, this, req_id ) );

    req_id_ = req_id;
    next_state( WAIT_PLAY_START );
}

void PlayerSM::on_error_response( uint32_t req_id )
{
    dummy_log_debug( MODULENAME, "on_error_response: %u", req_id );

    MUTEX_SCOPE_LOCK( mutex_ );

    if( state_ != WAIT_PLAY_RESP )
    {
        dummy_log_fatal( MODULENAME, "on_play_file_response: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( false );
        return;
    }
}

void PlayerSM::on_play_start( uint32_t call_id )
{
    dummy_log_debug( MODULENAME, "on_play_start: %u", call_id );

    MUTEX_SCOPE_LOCK( mutex_ );

    if( state_ != WAIT_PLAY_START )
    {
        dummy_log_fatal( MODULENAME, "on_play_start: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( false );
        return;
    }

    dummy_log_debug( MODULENAME, "on_play_start: ok" );

    callback_->consume( simple_voip::create_play_file_response( req_id_ ) );

    std::string error_msg;
    sched_->delete_job( & error_msg, job_id_ );     // cancel timeout job as replay was successfully started
    job_id_     = 0;
    req_id_ = 0;
    next_state( PLAYING );
}

void PlayerSM::on_play_stop( uint32_t call_id )
{
    dummy_log_debug( MODULENAME, "on_play_stop: %u", call_id );

    MUTEX_SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case PLAYING:
    {
        req_id_ = 0;
        next_state( PLAYING_ALREADY_STOPPED );
    }
        break;

    case CANCELED_IN_P:
    {
        ASSERT( req_id_ );

        callback_->consume( simple_voip::create_play_file_stop_response( req_id_ ) );

        dummy_log_debug( MODULENAME, "on_play_stop: ok" );

        req_id_ = 0;
        next_state( IDLE );
    }
        break;

    default:
        dummy_log_fatal( MODULENAME, "on_play_stop: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( false );
        break;
    }
}

void PlayerSM::on_play_failed( uint32_t req_id )
{
    dummy_log_debug( MODULENAME, "on_play_failed: req_id %u", req_id );

    MUTEX_SCOPE_LOCK( mutex_ );

    if( state_ != WAIT_PLAY_START )
    {
        dummy_log_fatal( MODULENAME, "on_play_failed: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( false );
        return;
    }

    dummy_log_debug( MODULENAME, "on_play_failed: ok" );

    callback_->consume( simple_voip::create_error_response( req_id_, 0, "play failed" ) );

    job_id_     = 0;       // job_id_ is not valid after call of invoke()
    req_id_ = 0;
    next_state( IDLE );
}

void PlayerSM::next_state( state_e state )
{
    state_  = state;

    trace_state_switch();
}

void PlayerSM::trace_state_switch() const
{
    dummy_log_debug( MODULENAME, "switched to %s", StrHelper::to_string( state_ ).c_str() );
}

NAMESPACE_DIALER_END
