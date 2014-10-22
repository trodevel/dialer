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

// $Id: dialer.h 1186 2014-10-22 18:15:19Z serge $

#ifndef DIALER_H
#define DIALER_H

#include <string>                   // std::string
#include <boost/thread.hpp>         // boost::mutex
#include "../utils/types.h"         // uint32

#include "../voip_io/i_voip_service_callback.h"    // IVoipServiceCallback
#include "../threcon/i_controllable.h"      // IControllable
#include "i_dialer.h"               // IDialer
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

namespace asyncp
{
class AsyncProxy;
}

NAMESPACE_DIALER_START

class DialerImpl;

class Dialer: virtual public IDialer, virtual public voip_service::IVoipServiceCallback, public virtual threcon::IControllable
{
public:
    Dialer();
    ~Dialer();

    bool init(
            voip_service::IVoipService  * voips,
            sched::IScheduler           * sched );

    void thread_func();

    bool register_callback( IDialerCallback * callback );

    bool is_inited() const;

    //state_e get_state() const;

    // IDialer
    virtual void initiate_call( const std::string & party );
    virtual void drop_all_calls();

    virtual bool shutdown();


    // IVoipServiceCallback
    virtual void on_ready( uint32 errorcode );
    virtual void on_error( uint32 call_id, uint32 errorcode );
    virtual void on_fatal_error( uint32 call_id, uint32 errorcode );
    virtual void on_call_end( uint32 call_id, uint32 errorcode );
    virtual void on_dial( uint32 call_id );
    virtual void on_ring( uint32 call_id );
    virtual void on_connect( uint32 call_id );
    virtual void on_call_duration( uint32 call_id, uint32 t );
    virtual void on_play_start( uint32 call_id );
    virtual void on_play_stop( uint32 call_id );

private:

    asyncp::AsyncProxy          * proxy_;
    DialerImpl                  * impl_;
};

NAMESPACE_DIALER_END

#endif  // DIALER_H
