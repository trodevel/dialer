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

// $Revision: 5560 $ $Date:: 2017-01-16 #$ $Author: serge $

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

#include "../scheduler/i_scheduler.h"   // IScheduler
#include "../scheduler/timeout_job.h"   // new_timeout_job

#include "namespace_lib.h"          // NAMESPACE_DIALER_START

#define MODULENAME      "PlayerSM"

#define PLAY_TIMEOUT    ( 2 * sched::Time::MICROS_PER_SECOND )

NAMESPACE_DIALER_START

PlayerSM::PlayerSM():
    state_( IDLE ), job_id_( 0 ), sio_( 0L ), sched_( 0L ), callback_( nullptr ), job_( 0L )
{
}

PlayerSM::~PlayerSM()
{
    if( job_ )
    {
        delete job_;
    }
}

bool PlayerSM::init( skype_service::SkypeService * sw, sched::IScheduler * sched )
{
    if( !sw || !sched )
        return false;

    sio_    = sw;
    sched_  = sched;

    dummy_log_info( MODULENAME, "init: switching to IDLE" );

    state_  = IDLE;
    job_id_ = 0;

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


void PlayerSM::play_file( uint32_t job_id, uint32_t call_id, const std::string & filename )
{
    dummy_log_debug( MODULENAME, "play_file: job_id %u", job_id );

    MUTEX_SCOPE_LOCK( mutex_ );

    if( state_ != IDLE )
    {
        dummy_log_fatal( MODULENAME, "play_file: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( false );
        return;
    }

    ASSERT( is_inited() );

    bool b = sio_->alter_call_set_input_file( call_id, filename, job_id );

    if( b == false )
    {
        dummy_log_error( MODULENAME, "failed setting input file: %s", filename.c_str() );

        callback_->consume( simple_voip::create_error_response( job_id, 0, "failed setting input file: " + filename ) );

        return;
    }

    state_  = WAIT_PLAY_RESP;
    job_id_ = job_id;
}

void PlayerSM::stop( uint32_t job_id, uint32_t call_id )
{
    dummy_log_debug( MODULENAME, "stop: job_id %u", job_id );

    MUTEX_SCOPE_LOCK( mutex_ );

    if( state_ == IDLE )
    {
        dummy_log_warn( MODULENAME, "stop: ineffective in state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

    if( state_ == WAIT_PLAY_START )
    {
        if( job_ )
        {
            job_->cancel();
            job_    = nullptr;
        }

        state_      = IDLE;
        job_id_     = 0;
    }
    else if( state_ == PLAYING )
    {
        auto b = sio_->alter_call_set_input_soundcard( call_id, job_id );

        if( b == false )
        {
            dummy_log_error( MODULENAME, "failed input soundcard" );

            callback_->consume( simple_voip::create_error_response( job_id, 0, "failed setting input soundcard" ) );

            return;
        }

        state_  = CANCELED_IN_P;
    }

    dummy_log_debug( MODULENAME, "stop: ok" );

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
        if( job_ )
        {
            job_->cancel();
            job_    = nullptr;
        }
    }

    dummy_log_debug( MODULENAME, "on_loss: ok" );

    ASSERT( is_inited() );

    state_      = IDLE;
    job_id_     = 0;
}

void PlayerSM::on_play_file_response( uint32_t job_id )
{
    dummy_log_debug( MODULENAME, "on_play_file_response: job_id %u", job_id );

    MUTEX_SCOPE_LOCK( mutex_ );

    if( state_ != WAIT_PLAY_RESP )
    {
        dummy_log_fatal( MODULENAME, "on_play_file_response: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( false );
        return;
    }

    dummy_log_debug( MODULENAME, "on_play_file_response: ok" );

    if( job_ )
    {
        delete job_;
        job_    = nullptr;
    }
    job_    = sched::new_timeout_job( sched_, PLAY_TIMEOUT, std::bind( &PlayerSM::on_play_failed, this, job_id ) );

    state_  = WAIT_PLAY_START;
    job_id_ = job_id;

}

void PlayerSM::on_error_response( uint32_t job_id )
{
    dummy_log_debug( MODULENAME, "on_error_response: %u", job_id );

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

    callback_->consume( simple_voip::create_play_file_response( job_id_ ) );

    state_  = PLAYING;
    job_->cancel();     // cancel timeout job as replay was successfully started
    job_    = nullptr;
    job_id_ = 0;
}

void PlayerSM::on_play_stop( uint32_t call_id )
{
    dummy_log_debug( MODULENAME, "on_play_stop: %u", call_id );

    MUTEX_SCOPE_LOCK( mutex_ );

    if( state_ != PLAYING && state_ != CANCELED_IN_P )
    {
        dummy_log_fatal( MODULENAME, "on_play_stop: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( false );
        return;
    }

    dummy_log_debug( MODULENAME, "on_play_stop: ok" );

    state_  = IDLE;
    job_id_ = 0;
}

void PlayerSM::on_play_failed( uint32_t job_id )
{
    dummy_log_debug( MODULENAME, "on_play_failed: job_id %u", job_id );

    MUTEX_SCOPE_LOCK( mutex_ );

    if( state_ != WAIT_PLAY_START )
    {
        dummy_log_fatal( MODULENAME, "on_play_failed: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( false );
        return;
    }

    dummy_log_debug( MODULENAME, "on_play_failed: ok" );

    callback_->consume( simple_voip::create_error_response( job_id_, 0, "play failed" ) );

    state_  = IDLE;
    job_    = nullptr;       // job_ is not valid after call of invoke()
    job_id_ = 0;
}


NAMESPACE_DIALER_END
