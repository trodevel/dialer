/*

Example.

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

// $Revision: 5690 $ $Date:: 2017-02-06 #$ $Author: serge $

#include <iostream>         // cout
#include <typeinfo>
#include <sstream>          // stringstream
#include <atomic>           // std::atomic
#include <vector>           // std::vector

#include "dialer.h"                     // dialer::Dialer
#include "../simple_voip/object_factory.h"             // simple_voip::create_message_t

#include "../skype_service/skype_service.h"     // SkypeService
#include "../utils/dummy_logger.h"      // dummy_log_set_log_level
#include "../scheduler/scheduler.h"     // Scheduler


namespace sched
{
extern unsigned int MODULE_ID;
}

class Callback: virtual public simple_voip::ISimpleVoipCallback
{
public:
    Callback( simple_voip::ISimpleVoip * dialer, sched::Scheduler * sched ):
        dialer_( dialer ),
        sched_( sched ),
        call_id_( 0 ),
        last_req_id_( 0 )
    {
    }

    // interface ISimpleVoipCallback
    void consume( const simple_voip::CallbackObject * req )
    {
        std::cout << "got " << typeid( *req ).name() << std::endl;

        if( typeid( *req ) == typeid( simple_voip::InitiateCallResponse ) )
        {
            std::cout << "got InitiateCallResponse"
                    << " req_id " << dynamic_cast< const simple_voip::InitiateCallResponse *>( req )->req_id
                    << " call_id " << dynamic_cast< const simple_voip::InitiateCallResponse *>( req )->call_id
                    << std::endl;

            call_id_    = dynamic_cast< const simple_voip::InitiateCallResponse *>( req )->call_id;
        }
        else if( typeid( *req ) == typeid( simple_voip::ErrorResponse ) )
        {
            std::cout << "got ErrorResponse"
                    << " req_id " << dynamic_cast< const simple_voip::ErrorResponse *>( req )->req_id
                    << " " << dynamic_cast< const simple_voip::ErrorResponse *>( req )->descr
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( simple_voip::RejectResponse ) )
        {
            std::cout << "got RejectResponse"
                    << " req_id " << dynamic_cast< const simple_voip::RejectResponse *>( req )->req_id
                    << " " << dynamic_cast< const simple_voip::RejectResponse *>( req )->descr
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( simple_voip::DropResponse ) )
        {
            std::cout << "got DropResponse"
                    << " req_id " << dynamic_cast< const simple_voip::DropResponse *>( req )->req_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( simple_voip::Failed ) )
        {
            std::cout << "got Failed"
                    << " call_id " << dynamic_cast< const simple_voip::Failed *>( req )->call_id
                    << std::endl;

            call_id_    = 0;
        }
        else if( typeid( *req ) == typeid( simple_voip::ConnectionLost ) )
        {
            std::cout << "got ConnectionLost"
                    << " call_id " << dynamic_cast< const simple_voip::ConnectionLost *>( req )->call_id
                    << std::endl;

            call_id_    = 0;
        }
        else if( typeid( *req ) == typeid( simple_voip::Dialing ) )
        {
            std::cout << "got Dial"
                    << " call_id " << dynamic_cast< const simple_voip::Dialing *>( req )->call_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( simple_voip::Ringing ) )
        {
            std::cout << "got Ring"
                    << " call_id " << dynamic_cast< const simple_voip::Ringing *>( req )->call_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( simple_voip::Connected ) )
        {
            std::cout << "got Connected"
                    << " call_id " << dynamic_cast< const simple_voip::Connected *>( req )->call_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( simple_voip::CallDuration ) )
        {
            std::cout << "got CallDuration"
                    << " call_id " << dynamic_cast< const simple_voip::CallDuration *>( req )->call_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( simple_voip::DtmfTone ) )
        {
            std::cout << "got DtmfTone"
                    << " call_id " << dynamic_cast< const simple_voip::DtmfTone *>( req )->call_id
                    << " tone " << static_cast<uint16_t>(
                            dynamic_cast< const simple_voip::DtmfTone *>( req )->tone )
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( simple_voip::PlayFileResponse ) )
        {
            std::cout << "got PlayFileResponse"
                    << " req_id " << dynamic_cast< const simple_voip::PlayFileResponse *>( req )->req_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( simple_voip::RecordFileResponse ) )
        {
            std::cout << "got RecordFileResponse"
                    << " req_id " << dynamic_cast< const simple_voip::RecordFileResponse *>( req )->req_id
                    << std::endl;
        }
        else
        {
            std::cout << "got unknown event" << std::endl;
        }

        delete req;
    }

    void control_thread()
    {
        std::cout << "type exit or quit to quit: " << std::endl;
        std::cout << "call <party>" << std::endl;
        std::cout << "drop" << std::endl;
        std::cout << "play <file>" << std::endl;
        std::cout << "rec <file>" << std::endl;

        std::string input;

        while( true )
        {
            std::cout << "enter command: ";

            std::getline( std::cin, input );

            std::cout << "your input: " << input << std::endl;

            bool b = process_input( input );

            if( b == false )
                break;
        };

        std::cout << "exiting ..." << std::endl;

        sched_->shutdown();
    }

private:
    bool process_input( const std::string & input )
    {
        std::string cmd;

        std::stringstream stream( input );
        if( stream >> cmd )
        {
            if( cmd == "exit" || cmd == "quit" )
            {
                return false;
            }
            else if( cmd == "call" )
            {
                std::string s;
                stream >> s;

                last_req_id_++;

                dialer_->consume( simple_voip::create_initiate_call_request( last_req_id_, s ) );
            }
            else if( cmd == "drop" )
            {
                last_req_id_++;
                dialer_->consume( simple_voip::create_drop_request( last_req_id_, call_id_ ) );
            }
            else if( cmd == "play" )
            {
                std::string filename;
                stream >> filename;

                last_req_id_++;
                dialer_->consume( simple_voip::create_play_file_request( last_req_id_, call_id_, filename ) );
            }
            else if( cmd == "stop" )
            {
                last_req_id_++;
                dialer_->consume( simple_voip::create_play_file_stop_request( last_req_id_, call_id_ ) );
            }
            else if( cmd == "rec" )
            {
                std::string filename;
                stream >> filename;

                last_req_id_++;
                dialer_->consume( simple_voip::create_record_file_request( last_req_id_, call_id_, filename ) );
            }
            else
                std::cout << "ERROR: unknown command '" << cmd << "'" << std::endl;
        }
        else
        {
            std::cout << "ERROR: cannot read command" << std::endl;
        }
        return true;
    }

private:
    simple_voip::ISimpleVoip    * dialer_;
    sched::Scheduler            * sched_;

    std::atomic<int>            call_id_;

    uint32_t                    last_req_id_;

};

void scheduler_thread( sched::Scheduler * sched )
{
    sched->start( true );
}

int main()
{
    dummy_logger::set_log_level( log_levels_log4j::DEBUG );

    skype_service::SkypeService sio;
    dialer::Dialer              dialer;
    sched::Scheduler            sched;

    sched::MODULE_ID        = dummy_logger::register_module( "Scheduler" );
    dummy_logger::set_log_level( sched::MODULE_ID, log_levels_log4j::ERROR );

    sched.load_config();
    sched.init();

    {
        bool b = dialer.init( & sio, & sched );
        if( !b )
        {
            std::cout << "cannot initialize Dialer" << std::endl;
            return 0;
        }
    }

    {
        bool b = sio.init();

        if( !b )
        {
            std::cout << "cannot initialize SkypeService - " << sio.get_error_msg() << std::endl;
            return 0;
        }

        sio.register_callback( & dialer );
    }

    Callback test( & dialer, & sched );
    dialer.register_callback( &test );

    dialer.start();

    std::vector< std::thread > tg;

    tg.push_back( std::thread( std::bind( &Callback::control_thread, &test ) ) );
    tg.push_back( std::thread( std::bind( &scheduler_thread, &sched ) ) );

    for( auto & t : tg )
        t.join();

    sio.shutdown();
    dialer.Dialer::shutdown();

    std::cout << "Done! =)" << std::endl;

    return 0;
}
