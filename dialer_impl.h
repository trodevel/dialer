/*

DialerImpl.

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

// $Id: dialer_impl.h 1186 2014-10-22 18:15:19Z serge $

#ifndef DIALER_IMPL_H
#define DIALER_IMPL_H

#include <string>                   // std::string
#include <boost/thread.hpp>         // boost::mutex
#include "../utils/types.h"         // uint32

#include "../voip_io/i_voip_service_callback.h"    // IVoipServiceCallback
#include "i_dialer_callback.h"      // IDialerCallback
#include "player_sm.h"              // PlayerSM
#include "call.h"                   // Call

#include "namespace_lib.h"          // NAMESPACE_DIALER_START

namespace sched
{
class IScheduler;
}

namespace voip_service
{
class IVoipService;
}

NAMESPACE_DIALER_START

class DialerImpl
{
public:
    enum state_e
    {
        UNKNOWN = 0,
        IDLE,
        BUSY,
    };

public:
    DialerImpl();
    ~DialerImpl();

    bool init(
            voip_service::IVoipService  * voips,
            sched::IScheduler           * sched );

    bool register_callback( IDialerCallback * callback );

    bool is_inited() const;

    state_e get_state() const;

    // IDialer
    void initiate_call( const std::string & party );
    void drop_all_calls();

    bool shutdown();


    // IVoipServiceCallback
    void on_ready( uint32 errorcode );
    void on_error( uint32 call_id, uint32 errorcode );
    void on_fatal_error( uint32 call_id, uint32 errorcode );
    void on_call_end( uint32 call_id, uint32 errorcode );
    void on_dial( uint32 call_id );
    void on_ring( uint32 call_id );
    void on_connect( uint32 call_id );
    void on_call_duration( uint32 call_id, uint32 t );
    void on_play_start( uint32 call_id );
    void on_play_stop( uint32 call_id );

private:
    bool is_inited__() const;
    bool is_call_id_valid( uint32 call_id ) const;

    void check_call_end( const char * event_name );

    boost::shared_ptr< CallI > get_call();

private:
    mutable boost::mutex        mutex_;

    state_e                     state_;

    voip_service::IVoipService  * voips_;
    sched::IScheduler           * sched_;
    IDialerCallback             * callback_;

    boost::shared_ptr< Call >   call_;
};

NAMESPACE_DIALER_END

#endif  // DIALER_IMPL_H
