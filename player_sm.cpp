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

// $Id: player_sm.cpp 867 2014-07-30 17:49:26Z serge $

#include "player_sm.h"              // self

#include <boost/bind.hpp>           // boost::bind

#include "../voip_io/i_voip_service.h"  // IVoipService
#include "../utils/dummy_logger.h"      // dummy_log
#include "../utils/wrap_mutex.h"        // SCOPE_LOCK
#include "str_helper.h"                 // StrHelper

#include "../scheduler/i_scheduler.h"   // IScheduler
#include "../scheduler/timeout_job.h"   // new_timeout_job

#include "namespace_lib.h"          // NAMESPACE_DIALER_START

#define MODULENAME      "PlayerSM"

#define PLAY_TIMEOUT    ( 2 * sched::Time::MICROS_PER_SECOND )

NAMESPACE_DIALER_START

PlayerSM::PlayerSM():
    state_( UNKNOWN ), voips_( 0L ), sched_( 0L ), job_( 0L )
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

    dummy_log( 0, MODULENAME, "init: switching to IDLE" );

    state_  = IDLE;

    return true;
}

bool PlayerSM::is_inited() const
{
    if( !voips_ || sched_ == 0L )
        return false;

    return true;
}


bool PlayerSM::play_file( uint32 call_id, const std::string & filename )
{
    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case PLAYING:
        dummy_log( 0, MODULENAME, "play_file: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return false;

    case IDLE:
        break;

    default:
        dummy_log( 0, MODULENAME, "play_file: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return false;
    }

    if( !is_inited() )
        return false;

    bool b = voips_->set_input_file( call_id, filename );

    if( !b )
    {
        dummy_log( 0, MODULENAME, "play_file: voip service failed" );
        return false;
    }

//    sched::IShotJob* job = sched::new_timeout_job( sched_, PLAY_TIMEOUT, boost::bind( &PlayerSM::on_play_failed, this, call_id ) );
//
//    boost::shared_ptr<sched::IShotJob>     job_p( job );

    //job_.reset( job );
//    job_    = job_p;
    if( job_ )
    {
        delete job_;
        job_    = 0L;
    }
    job_    = sched::new_timeout_job( sched_, PLAY_TIMEOUT, boost::bind( &PlayerSM::on_play_failed, this, call_id ) );

    //job_.reset( sched::new_timeout_job( sched_, PLAY_TIMEOUT, boost::bind( &PlayerSM::on_play_failed, this, call_id ) ) );

    state_  = WAITING;

    return true;
}

void PlayerSM::stop()
{
    dummy_log( 0, MODULENAME, "stop" );

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
        dummy_log( 0, MODULENAME, "stop: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case WAITING:
    case PLAYING:
        dummy_log( 0, MODULENAME, "stop: ok" );
        break;

    default:
        dummy_log( 0, MODULENAME, "stop: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

    if( !is_inited() )
        return;

    state_      = IDLE;
}

void PlayerSM::on_play_start( uint32 call_id )
{
    dummy_log( 0, MODULENAME, "on_play_start: %u", call_id );

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case PLAYING:
        dummy_log( 0, MODULENAME, "on_play_start: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case WAITING:
        dummy_log( 0, MODULENAME, "on_play_start: ok" );
        break;

    default:
        dummy_log( 0, MODULENAME, "on_play_start: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

    state_   = PLAYING;
    job_->cancel();     // cancel timeout job as replay was successfully started
    job_    = 0L;
}

void PlayerSM::on_play_stop( uint32 call_id )
{
    dummy_log( 0, MODULENAME, "on_play_stop: %u", call_id );

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case WAITING:
        dummy_log( 0, MODULENAME, "on_play_stop: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case PLAYING:
        dummy_log( 0, MODULENAME, "on_play_stop: ok" );
        break;

    default:
        dummy_log( 0, MODULENAME, "on_play_stop: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

    state_  = IDLE;
}

void PlayerSM::on_play_failed( uint32 call_id )
{
    dummy_log( 0, MODULENAME, "on_play_failed: %u", call_id );

    SCOPE_LOCK( mutex_ );

    switch( state_ )
    {
    case UNKNOWN:
    case IDLE:
    case PLAYING:
        dummy_log( 0, MODULENAME, "on_play_failed: ignored in state %s", StrHelper::to_string( state_ ).c_str() );
        return;

    case WAITING:
        dummy_log( 0, MODULENAME, "on_play_failed: ok" );
        break;

    default:
        dummy_log( 0, MODULENAME, "on_play_failed: invalid state %s", StrHelper::to_string( state_ ).c_str() );
        return;
    }

    state_  = IDLE;
    job_    = 0L;       // job_ is not valid after call of invoke()
}


NAMESPACE_DIALER_END
