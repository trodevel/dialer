/*

Call.

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

// $Id: call.h 765 2014-07-11 16:57:13Z serge $

#ifndef CALL_H
#define CALL_H

#include <string>                   // std::string
#include <boost/thread.hpp>         // boost::mutex
#include "../utils/types.h"         // uint32

#include "../voip_io/i_voip_service_callback.h"    // IVoipServiceCallback
#include "call_i.h"                 // CallI
#include "player_sm.h"              // PlayerSM

#include "namespace_lib.h"          // NAMESPACE_DIALER_START

namespace asyncp
{
class IAsyncProxy;
}

namespace sched
{
class IScheduler;
}

namespace voip_service
{
class IVoipService;
}

NAMESPACE_DIALER_START

class Call: virtual public CallI
{
public:
    enum state_e
    {
        UNKNOWN = 0,
        IDLE,
        DIALLING,
        RINGING,
        CONNECTED,
        ENDED
    };

public:
    Call( uint32                        call_id,
          voip_service::IVoipService    * voips,
          sched::IScheduler             * sched,
          asyncp::IAsyncProxy           * proxy );
    ~Call();

    state_e get_state() const;
    uint32 get_id() const;

    // CallI
    bool drop();
    bool set_input_file( const std::string & filename );
    bool set_output_file( const std::string & filename );
    bool is_ended() const;
    bool is_active() const;
    bool register_callback( ICallCallbackPtr callback );

    // forwarded IVoipServiceCallback
    void on_error( uint32 errorcode );
    void on_call_end( uint32 errorcode );
    void on_dial();
    void on_ring();
    void on_connect();
    void on_call_duration( uint32 t );
    void on_play_start();
    void on_play_stop();

private:

private:
    mutable boost::mutex        mutex_;

    state_e                     state_;

    voip_service::IVoipService  * voips_;
    sched::IScheduler           * sched_;
    asyncp::IAsyncProxy         * proxy_;

    uint32                      call_id_;

    PlayerSM                    player_;

    ICallCallbackPtr            callback_;

};

NAMESPACE_DIALER_END

#endif  // CALL_H
