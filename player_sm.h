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

// $Revision: 1724 $ $Date:: 2015-04-24 #$ $Author: serge $

#ifndef PLAYER_SM_H
#define PLAYER_SM_H

#include <string>                   // std::string
#include "../utils/types.h"         // uint32

#include <mutex>                    // std::mutex
#include "namespace_lib.h"          // NAMESPACE_DIALER_START

namespace sched
{
class IScheduler;
class IOneTimeJob;
}

namespace voip_service
{
class IVoipService;
}

NAMESPACE_DIALER_START

class IDialerCallback;

class PlayerSM
{
public:
    enum state_e
    {
        UNKNOWN = 0,
        IDLE,
        WAITING,
        PLAYING,
    };

public:
    PlayerSM();
    ~PlayerSM();

    bool init( voip_service::IVoipService  * voips, sched::IScheduler * sched );

    bool register_callback( IDialerCallback * callback );

    bool is_inited() const;

    // IPlayerSM
    void play_file( uint32 call_id, const std::string & filename );
    void stop();
    bool is_playing() const;

    // IVoipServiceCallback
    void on_play_start( uint32 call_id );
    void on_play_stop( uint32 call_id );

    // called by timer
    void on_play_failed( uint32 call_id );

private:

private:
    mutable std::mutex         mutex_;

    state_e                     state_;

    voip_service::IVoipService  * voips_;
    sched::IScheduler           * sched_;
    IDialerCallback             * callback_;

    //std::shared_ptr<sched::IOneTimeJob>     job_;
    sched::IOneTimeJob             * job_;
};

NAMESPACE_DIALER_END

#endif  // PLAYER_SM_H
