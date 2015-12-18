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

// $Revision: 3004 $ $Date:: 2015-12-17 #$ $Author: serge $

#ifndef DIALER_H
#define DIALER_H

#include <string>                   // std::string
#include <mutex>                    // std::mutex
#include "../utils/types.h"         // uint32

#include "../voip_io/i_voip_service_callback.h" // IVoipServiceCallback
#include "../voip_io/objects.h"                 // InitiateCallResponse
#include "../threcon/i_controllable.h"          // IControllable
#include "../skype_service/i_callback.h"        // ICallback
#include "../skype_service/events.h"            // ConnStatusEvent, ...
#include "../servt/server_t.h"                  // ServerT
#include "../voip_io/i_voip_service.h"          // IVoipService
#include "player_sm.h"                          // PlayerSM



#include "namespace_lib.h"          // NAMESPACE_DIALER_START

namespace sched
{
class IScheduler;
}

NAMESPACE_DIALER_START

class Dialer;

typedef servt::ServerT< const servt::IObject*, Dialer> ServerBase;

class Dialer:
        public ServerBase,
        virtual public voip_service::IVoipService,
        virtual public skype_service::ICallback,
        virtual public threcon::IControllable
{
    friend ServerBase;

public:
    enum state_e
    {
        UNKNOWN = 0,
        IDLE,
        WAITING_INITIATE_CALL_RESPONSE,
        WAITING_CONNECTION,
        CONNECTED,
        WAITING_DROP_RESPONSE,
    };

public:
    Dialer();
    ~Dialer();

    bool init(
            skype_service::SkypeService * sw,
            sched::IScheduler           * sched );

    bool register_callback( voip_service::IVoipServiceCallback * callback );

    bool is_inited() const;

    state_e get_state() const;

    // interface IVoipService
    virtual void consume( const voip_service::Object * req );

    // interface skype_service::ICallback
    virtual void consume( const skype_service::Event * e );

    // interface IControllable
    bool shutdown();


private:
    void handle( const servt::IObject* req );

    // for interface IVoipService
    void handle( const voip_service::InitiateCallRequest * req );
    void handle( const voip_service::DropRequest * req );
    void handle( const voip_service::PlayFileRequest * req );
    void handle( const voip_service::RecordFileRequest * req );
    void handle( const voip_service::ObjectWrap * req );

    // interface skype_service::ICallback
    void handle( const skype_service::ConnStatusEvent * e );
    void handle( const skype_service::UserStatusEvent * e );
    void handle( const skype_service::CurrentUserHandleEvent * e );
    void handle( const skype_service::ErrorEvent * e );
    void handle( const skype_service::CallStatusEvent * e );
    void handle( const skype_service::CallPstnStatusEvent * e );
    void handle( const skype_service::CallDurationEvent * e );
    void handle( const skype_service::CallFailureReasonEvent * e );
    void handle( const skype_service::CallVaaInputStatusEvent * e );

    void on_unknown( const std::string & s );

    void handle_in_state_unknown( const skype_service::Event * ev );
    void handle_in_state_idle( const skype_service::Event * ev );
    void handle_in_state_w_ical( const skype_service::Event * ev );
    void handle_in_state_w_drpr( const skype_service::Event * ev );
    void handle_in_state_w_conn( const skype_service::Event * ev );
    void handle_in_state_connected( const skype_service::Event * ev );

    void send_reject_response( uint32_t job_id, uint32_t errorcode, const std::string & descr );

    bool is_inited__() const;
    bool is_call_id_valid( uint32 call_id ) const;

    void callback_consume( const voip_service::ResponseObject * req );

    bool send_reject_if_in_request_processing( uint32_t job_id );
    bool ignore_non_response( const skype_service::Event * ev );
    static const char* decode_failure_reason( uint32 c );
    void switch_to_ready_if_possible();

private:
    mutable std::mutex          mutex_;

    state_e                     state_;

    skype_service::SkypeService * sio_;
    sched::IScheduler           * sched_;
    voip_service::IVoipServiceCallback  * callback_;

    uint32_t                    current_job_id_;
    uint32                      call_id_;
    skype_service::conn_status_e   cs_;
    skype_service::user_status_e   us_;
    PlayerSM                    player_;
};

NAMESPACE_DIALER_END

#endif  // DIALER_H
