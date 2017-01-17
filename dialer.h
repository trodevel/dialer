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

// $Revision: 5556 $ $Date:: 2017-01-16 #$ $Author: serge $

#ifndef DIALER_H
#define DIALER_H

#include <string>                   // std::string
#include <mutex>                    // std::mutex
#include <cstdint>                  // uint32_t

#include "../simple_voip/i_simple_voip.h"       // ISimpleVoip
#include "../simple_voip/i_simple_voip_callback.h" // ISimpleVoipCallback
#include "../simple_voip/objects.h"             // InitiateCallResponse
#include "../skype_service/i_callback.h"        // ICallback
#include "../skype_service/events.h"            // ConnStatusEvent, ...
#include "../workt/worker_t.h"                  // WorkerT
#include "../workt/i_object.h"                  // workt::IObject
#include "../threcon/i_controllable.h"          // IControllable
#include "../dtmf_detector/IDtmfDetectorCallback.hpp"   // IDtmfDetectorCallback
#include "player_sm.h"                          // PlayerSM


#include "namespace_lib.h"          // NAMESPACE_DIALER_START

namespace sched
{
class IScheduler;
}

NAMESPACE_DIALER_START

class Dialer;
class DetectedTone;
class ObjectWrap;
class SimpleVoipWrap;

typedef workt::WorkerT< const workt::IObject*, Dialer> WorkerBase;

class Dialer:
        public WorkerBase,
        virtual public simple_voip::ISimpleVoip,
        virtual public skype_service::ICallback,
        virtual public threcon::IControllable,
        virtual public dtmf::IDtmfDetectorCallback
{
    friend WorkerBase;

public:
    enum state_e
    {
        UNKNOWN = 0,
        IDLE,
        WAITING_INITIATE_CALL_RESPONSE,
        WAITING_CONNECTION,
        CONNECTED,
        CANCELED_IN_C,
        CANCELED_IN_WC,    // waiting drop response before connection
    };

public:
    Dialer();
    ~Dialer();

    bool init(
            skype_service::SkypeService * sw,
            sched::IScheduler           * sched,
            uint16_t                    data_port = 0 );

    bool register_callback( simple_voip::ISimpleVoipCallback * callback );

    bool is_inited() const;

    state_e get_state() const;

    // interface ISimpleVoip
    virtual void consume( const simple_voip::ForwardObject * req );

    // interface skype_service::ICallback
    virtual void consume( const skype_service::Event * e );

    // interface dtmf::IDtmfDetectorCallback
    virtual void on_detect( dtmf::tone_e button );

    void start();

    // interface IControllable
    bool shutdown();

private:
    void handle( const workt::IObject* req );

    // for interface ISimpleVoip
    void handle( const simple_voip::InitiateCallRequest * req );
    void handle( const simple_voip::DropRequest * req );
    void handle( const simple_voip::PlayFileRequest * req );
    void handle( const simple_voip::PlayFileStopRequest * req );
    void handle( const simple_voip::RecordFileRequest * req );
    void handle( const SimpleVoipWrap * req );
    void handle( const ObjectWrap * req );

    // interface skype_service::ICallback
    void handle( const skype_service::ConnStatusEvent * e );
    void handle( const skype_service::UserStatusEvent * e );
    void handle( const skype_service::CurrentUserHandleEvent * e );
    void handle( const skype_service::ErrorEvent * e );
    void handle_in_w_ical( const skype_service::CallStatusEvent * e );
    void handle_in_w_conn( const skype_service::CallStatusEvent * e );
    void handle_in_connected( const skype_service::CallStatusEvent * e );
    void handle_in_w_drpr( const skype_service::CallStatusEvent * e );
    void handle_in_w_drpr_2( const skype_service::CallStatusEvent * e );
    void handle( const skype_service::CallPstnStatusEvent * e );
    void handle( const skype_service::CallDurationEvent * e );
    void handle( const skype_service::VoicemailDurationEvent * e );
    void handle( const skype_service::CallFailureReasonEvent * e );

    void handle( const DetectedTone * ev );

    void on_unknown( const std::string & s );

    void handle_in_state_unknown( const skype_service::Event * ev );
    void handle_in_state_idle( const skype_service::Event * ev );
    void handle_in_state_w_ical( const skype_service::Event * ev );
    void handle_in_state_w_conn( const skype_service::Event * ev );
    void handle_in_state_connected( const skype_service::Event * ev );
    void handle_in_state_w_drpr( const skype_service::Event * ev );

    void forward_to_player( const skype_service::Event * ev );

    void send_reject_response( uint32_t job_id, uint32_t errorcode, const std::string & descr );
    void send_error_response( uint32_t job_id, uint32_t errorcode, const std::string & descr );

    bool is_inited__() const;
    bool is_call_id_valid( uint32_t call_id ) const;

    void callback_consume( const simple_voip::CallbackObject * req );

    void send_reject_due_to_wrong_state( uint32_t job_id );
    bool send_reject_if_in_request_processing( uint32_t job_id );
    bool ignore_response( const skype_service::Event * ev );
    bool ignore_non_response( const skype_service::Event * ev );
    bool ignore_non_expected_response( const skype_service::Event * ev );
    static const char* decode_failure_reason( uint32_t c );
    void switch_to_ready_if_possible();
    void switch_to_idle_and_cleanup();

    static simple_voip::DtmfTone::tone_e decode_tone( dtmf::tone_e tone );

    enum class party_e
    {
        UNKNOWN,
        NUMBER,
        SYMBOLIC
    };
    static party_e get_party_type( const std::string & inp );
    static bool transform_party( const std::string & inp, std::string & outp );

private:
    mutable std::mutex          mutex_;

    state_e                     state_;

    skype_service::SkypeService * sio_;
    sched::IScheduler           * sched_;
    simple_voip::ISimpleVoipCallback  * callback_;
    uint16_t                    data_port_;

    uint32_t                    current_job_id_;
    uint32_t                    call_id_;
    skype_service::conn_status_e   cs_;
    skype_service::user_status_e   us_;
    uint32_t                    failure_reason_;
    std::string                 failure_reason_msg_;
    uint32_t                    pstn_status_;
    std::string                 pstn_status_msg_;

    PlayerSM                    player_;
};

NAMESPACE_DIALER_END

#endif  // DIALER_H
