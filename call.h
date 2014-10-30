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

// $Id: call.h 1226 2014-10-29 23:34:06Z serge $

#ifndef CALL_H
#define CALL_H

#include <string>                   // std::string
#include <boost/thread.hpp>         // boost::mutex
#include "../utils/types.h"         // uint32

#include "call_i.h"                 // CallI
#include "call_impl.h"              // CallImpl

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

class Dialer;

class Call: public CallImpl, virtual public CallI
{
public:
    Call( uint32                        call_id,
          voip_service::IVoipService    * voips,
          sched::IScheduler             * sched,
          asyncp::IAsyncProxy           * proxy );
    ~Call();

    void register_callback_on_ended( Dialer * callback );

    // CallI
    void drop();
    void set_input_file( const std::string & filename );
    void set_output_file( const std::string & filename );
    bool register_callback( ICallCallbackPtr callback );

    // forwarded IVoipServiceCallback
    void on_error( uint32 errorcode );
    void on_fatal_error( uint32 errorcode );
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

    asyncp::IAsyncProxy         * proxy_;

};

NAMESPACE_DIALER_END

#endif  // CALL_H
