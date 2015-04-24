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

// $Revision: 1724 $ $Date:: 2015-04-24 #$ $Author: serge $

#ifndef DIALER_H
#define DIALER_H

#include <string>                   // std::string
#include <mutex>                    // std::mutex
#include "../utils/types.h"         // uint32

#include "../voip_io/i_voip_service_callback.h"     // IVoipServiceCallback
#include "../voip_io/objects.h"                     // VoipioInitiateCallResponse
#include "../threcon/i_controllable.h"              // IControllable
#include "../servt/server_t.h"      // ServerT
#include "objects.h"                // DialerObject
#include "i_dialer.h"               // IDialer
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

class IDialerCallback;
class Dialer;

typedef servt::ServerT< const servt::IObject*, Dialer> ServerBase;

class Dialer:
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
        WAITING_INITIATE_CALL_RESPONSE,
        WAITING_DIALLING,
        DIALLING,
        RINGING,
        CONNECTED,
        WAITING_DROP_RESPONSE,
    };

public:
    Dialer();
    ~Dialer();

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


private:
    void handle( const servt::IObject* req );

    // for interface IDialer
    void handle( const DialerInitiateCallRequest * req );
    void handle( const DialerDrop * req );
    void handle( const DialerPlayFile * req );
    void handle( const DialerRecordFile * req );

    // for interface IVoipServiceCallback
    void handle( const voip_service::VoipioInitiateCallResponse * r );
    void handle( const voip_service::VoipioErrorResponse * r );
    void handle( const voip_service::VoipioDropResponse * r );
    void handle( const voip_service::VoipioCallErrorResponse * r );
    void handle( const voip_service::VoipioCallEnd * r );
    void handle( const voip_service::VoipioDial * r );
    void handle( const voip_service::VoipioRing * r );
    void handle( const voip_service::VoipioConnect * r );
    void handle( const voip_service::VoipioCallDuration * r );
    void handle( const voip_service::VoipioPlayStarted * r );
    void handle( const voip_service::VoipioPlayStopped * r );

    bool is_inited__() const;
    bool is_call_id_valid( uint32 call_id ) const;

private:
    mutable std::mutex         mutex_;

    state_e                     state_;

    voip_service::IVoipService  * voips_;
    sched::IScheduler           * sched_;
    IDialerCallback             * callback_;

    uint32                      call_id_;
    PlayerSM                    player_;
};

NAMESPACE_DIALER_END

#endif  // DIALER_H
