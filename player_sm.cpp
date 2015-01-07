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

// $Id: player_sm.cpp 1324 2015-01-06 18:02:57Z serge $

#include "player_sm.h"              // self

#include <functional>                   // std::bind

#include "../voip_io/i_voip_service.h"  // IVoipService
#include "../voip_io/object_factory.h"  // voip_service::create_play_file
#include "../utils/dummy_logger.h"      // dummy_log
#include "../utils/wrap_mutex.h"        // SCOPE_LOCK
#include "../utils/assert.h"            // ASSERT
#include "str_helper.h"                 // StrHelper
#include "object_factory.h"             // create_message_t
#include "i_dialer_callback.h"          // IDialerCallback

#include "../scheduler/i_scheduler.h"   // IScheduler
#include "../scheduler/timeout_job.h"   // new_timeout_job

#include "namespace_lib.h"          // NAMESPACE_DIALER_START

#define MODULENAME      "PlayerSM"

#define PLAY_TIMEOUT    ( 2 * sched::Time::MICROS_PER_SECOND )

NAMESPACE_DIALER_START

PlayerSM::PlayerSM():
    state_( UNKNOWN ), voips_( 0L ), sched_( 0L ), callback_( nullptr ), job_( 0L )
{
}

PlayerSM::~PlayerSM()
{
    if( job_ )
    {
        delete job_;
    }
}

bool PlayerSM::init( voip_service::IVoipService  * voips, sched::IScheduler * sched )
{
    if( !voips || !sched )
        return false;

    voips_  = voips;
    sched_  = sched;

    dummy_log_info( MODULENAME, "init: switching to IDLE" );

    state_  = IDLE;

    return true;
}


bool PlayerSM::register_callback( IDialerCallback * callback )
{
    if( callback == 0L )
        return false;

    SCOPE_LOCK( mutex_ );

    if( callback_ != 0L )
        return false;

    callback_ = callback;

    return true;
}

bool PlayerSM::is_inited() const
{
    if( !voips_ || sched_ == 0L )
        return false;

    return true;
}


void PlayerSM::play_file( uint32 call_id, const std::string & filename )
{
    SCOPE_LOCK( mutex_ );

    if( state_ != IDLE )
    {
        dummy_log_fatal( MODULENAME, "play_file: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( false );
        return;
    }

    ASSERT( is_inited() );

    voips_->consume( voip_service::create_play_file( call_id, filename ) );

//    sched::IOneTimeJob* job = sched::new_timeout_job( sched_, PLAY_TIMEOUT, std::bind( &PlayerSM::on_play_failed, this, call_id ) );
//
//    boost::shared_ptr<sched::IOneTimeJob>     job_p( job );

    //job_.reset( job );
//    job_    = job_p;
    if( job_ )
    {
        delete job_;
        job_    = 0L;
    }
    job_    = sched::new_timeout_job( sched_, PLAY_TIMEOUT, std::bind( &PlayerSM::on_play_failed, this, call_id ) );

    //job_.reset( sched::new_timeout_job( sched_, PLAY_TIMEOUT, std::bind( &PlayerSM::on_play_failed, this, call_id ) ) );

    state_  = WAITING;
}

void PlayerSM::stop()
{
    dummy_log_debug( MODULENAME, "stop" );

    SCOPE_LOCK( mutex_ );

    if( state_ != WAITING && state_ != PLAYING )
    {
        dummy_log_fatal( MODULENAME, "stop: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( false );
        return;
    }

    dummy_log_debug( MODULENAME, "stop: ok" );

    ASSERT( is_inited() );

    state_      = IDLE;
}

bool PlayerSM::is_playing() const
{
    SCOPE_LOCK( mutex_ );

    return state_ == PLAYING;
}

void PlayerSM::on_play_start( uint32 call_id )
{
    dummy_log_debug( MODULENAME, "on_play_start: %u", call_id );

    SCOPE_LOCK( mutex_ );

    if( state_ != WAITING )
    {
        dummy_log_fatal( MODULENAME, "on_play_start: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( false );
        return;
    }

    dummy_log_debug( MODULENAME, "on_play_start: ok" );

    callback_->consume( create_message_t<DialerPlayStarted>( call_id ) );

    state_   = PLAYING;
    job_->cancel();     // cancel timeout job as replay was successfully started
    job_    = 0L;
}

void PlayerSM::on_play_stop( uint32 call_id )
{
    dummy_log_debug( MODULENAME, "on_play_stop: %u", call_id );

    SCOPE_LOCK( mutex_ );

    if( state_ != PLAYING )
    {
        dummy_log_fatal( MODULENAME, "on_play_stop: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( false );
        return;
    }

    dummy_log_debug( MODULENAME, "on_play_stop: ok" );

    callback_->consume( create_message_t<DialerPlayStopped>( call_id ) );

    state_  = IDLE;
}

void PlayerSM::on_play_failed( uint32 call_id )
{
    dummy_log_debug( MODULENAME, "on_play_failed: %u", call_id );

    SCOPE_LOCK( mutex_ );

    if( state_ != WAITING )
    {
        dummy_log_fatal( MODULENAME, "on_play_failed: unexpected in state %s", StrHelper::to_string( state_ ).c_str() );
        ASSERT( false );
        return;
    }

    dummy_log_debug( MODULENAME, "on_play_failed: ok" );

    callback_->consume( create_message_t<DialerPlayFailed>( call_id ) );

    state_  = IDLE;
    job_    = 0L;       // job_ is not valid after call of invoke()
}


NAMESPACE_DIALER_END
