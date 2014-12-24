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

// $Id: dialer_impl.h 1279 2014-12-23 18:24:10Z serge $

#ifndef DIALER_IMPL_H
#define DIALER_IMPL_H

#include <string>                   // std::string
#include <boost/thread.hpp>         // boost::mutex
#include "../utils/types.h"         // uint32

#include "../voip_io/i_voip_service_callback.h"    // IVoipServiceCallback
#include "../threcon/i_controllable.h"      // IControllable
#include "../servt/server_t.h"          // ServerT
#include "i_dialer.h"               // IDialer
#include "i_dialer_callback.h"      // IDialerCallback
#include "player_sm.h"              // PlayerSM

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

class DialerImpl;

typedef servt::ServerT< const servt::IObject*, DialerImpl> ServerBase;

class DialerImpl:
        public ServerBase,
        virtual public IDialer,
        virtual public voip_service::IVoipServiceCallback,
        virtual public threcon::IControllable
{
    friend ServerBase;

public:
    enum state_e
    {
        UNKNOWN = 0,
        IDLE,
        WAITING_VOIP_RESPONSE,
        DIALLING,
        RINGING,
        CONNECTED,
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

    // interface IDialer
    void consume( const DialerObject * req );

    // interface IVoipServiceCallback
    virtual void consume( const voip_service::VoipioCallbackObject * req );

    // interface IControllable
    bool shutdown();

    // interface IVoipServiceCallback
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
    // IVoipService interface
    void handle( const servt::IObject* req );

    void handle( const DialerInitiateCallRequest * req );
    void handle( const DialerDrop * req );
    void handle( const DialerPlayFile * req );
    void handle( const DialerRecordFile * req );


    bool is_inited__() const;
    bool is_call_id_valid( uint32 call_id ) const;

private:
    mutable boost::mutex        mutex_;

    state_e                     state_;

    voip_service::IVoipService  * voips_;
    sched::IScheduler           * sched_;
    IDialerCallback             * callback_;

    uint32                      call_id_;
    PlayerSM                    player_;
};

NAMESPACE_DIALER_END

#endif  // DIALER_IMPL_H
