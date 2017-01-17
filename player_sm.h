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

#ifndef PLAYER_SM_H
#define PLAYER_SM_H

#include <string>                   // std::string
#include <cstdint>                  // uint32_t

#include <mutex>                    // std::mutex
#include "namespace_lib.h"          // NAMESPACE_DIALER_START

namespace sched
{
class IScheduler;
class IOneTimeJob;
}

namespace skype_service
{
class SkypeService;
}

namespace simple_voip
{
class ISimpleVoipCallback;
}

NAMESPACE_DIALER_START

class PlayerSM
{
public:
    enum state_e
    {
        IDLE    = 0,
        WAIT_PLAY_RESP,
        WAIT_PLAY_START,
        CANCELED_IN_WPS,
        PLAYING,
        CANCELED_IN_P
    };

public:
    PlayerSM();
    ~PlayerSM();

    bool init( skype_service::SkypeService * sw, sched::IScheduler * sched );

    bool register_callback( simple_voip::ISimpleVoipCallback  * callback );

    bool is_inited() const;

    // IPlayerSM
    void play_file( uint32_t job_id, uint32_t call_id, const std::string & filename );
    void stop( uint32_t job_id, uint32_t call_id );
    void on_loss();

    // ISimpleVoipCallback
    void on_play_file_response( uint32_t job_id );
    void on_error_response( uint32_t job_id );
    void on_play_start( uint32_t call_id );
    void on_play_stop( uint32_t call_id );

    // called by timer
    void on_play_failed( uint32_t job_id );

private:

private:
    mutable std::mutex          mutex_;

    state_e                     state_;
    uint32_t                    job_id_;

    skype_service::SkypeService * sio_;
    sched::IScheduler           * sched_;
    simple_voip::ISimpleVoipCallback  * callback_;

    //std::shared_ptr<sched::IOneTimeJob>     job_;
    sched::IOneTimeJob             * job_;
};

NAMESPACE_DIALER_END

#endif  // PLAYER_SM_H
