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

// $Revision: 3040 $ $Date:: 2015-12-23 #$ $Author: serge $

#include <iostream>         // cout
#include <typeinfo>
#include <sstream>          // stringstream
#include <atomic>           // std::atomic
#include <vector>           // std::vector

#include "dialer.h"                     // dialer::Dialer
#include "../voip_io/object_factory.h"             // voip_service::create_message_t

#include "../skype_service/skype_service.h"     // SkypeService
#include "../utils/dummy_logger.h"      // dummy_log_set_log_level
#include "../scheduler/scheduler.h"     // Scheduler

namespace sched
{
extern unsigned int MODULE_ID;
}

class Callback: virtual public voip_service::IVoipServiceCallback
{
public:
    Callback( voip_service::IVoipService * dialer, sched::Scheduler * sched ):
        dialer_( dialer ),
        sched_( sched ),
        call_id_( 0 ),
        last_job_id_( 0 )
    {
    }

    // interface IVoipServiceCallback
    void consume( const voip_service::CallbackObject * req )
    {
        std::cout << "got " << typeid( *req ).name() << std::endl;

        if( typeid( *req ) == typeid( voip_service::InitiateCallResponse ) )
        {
            std::cout << "got InitiateCallResponse"
                    << " job_id " << dynamic_cast< const voip_service::InitiateCallResponse *>( req )->job_id
                    << " call_id " << dynamic_cast< const voip_service::InitiateCallResponse *>( req )->call_id
                    << std::endl;

            call_id_    = dynamic_cast< const voip_service::InitiateCallResponse *>( req )->call_id;
        }
        else if( typeid( *req ) == typeid( voip_service::ErrorResponse ) )
        {
            std::cout << "got ErrorResponse"
                    << " job_id " << dynamic_cast< const voip_service::ErrorResponse *>( req )->job_id
                    << " call_id " << dynamic_cast< const voip_service::ErrorResponse *>( req )->descr
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( voip_service::RejectResponse ) )
        {
            std::cout << "got RejectResponse"
                    << " job_id " << dynamic_cast< const voip_service::RejectResponse *>( req )->job_id
                    << " " << dynamic_cast< const voip_service::RejectResponse *>( req )->descr
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( voip_service::DropResponse ) )
        {
            std::cout << "got DropResponse"
                    << " job_id " << dynamic_cast< const voip_service::DropResponse *>( req )->job_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( voip_service::Failed ) )
        {
            std::cout << "got Failed"
                    << " call_id " << dynamic_cast< const voip_service::Failed *>( req )->call_id
                    << std::endl;

            call_id_    = 0;
        }
        else if( typeid( *req ) == typeid( voip_service::ConnectionLost ) )
        {
            std::cout << "got ConnectionLost"
                    << " call_id " << dynamic_cast< const voip_service::ConnectionLost *>( req )->call_id
                    << std::endl;

            call_id_    = 0;
        }
        else if( typeid( *req ) == typeid( voip_service::Dial ) )
        {
            std::cout << "got Dial"
                    << " call_id " << dynamic_cast< const voip_service::Dial *>( req )->call_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( voip_service::Ring ) )
        {
            std::cout << "got Ring"
                    << " call_id " << dynamic_cast< const voip_service::Ring *>( req )->call_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( voip_service::Connected ) )
        {
            std::cout << "got Connected"
                    << " call_id " << dynamic_cast< const voip_service::Connected *>( req )->call_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( voip_service::CallDuration ) )
        {
            std::cout << "got CallDuration"
                    << " call_id " << dynamic_cast< const voip_service::CallDuration *>( req )->call_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( voip_service::PlayFileResponse ) )
        {
            std::cout << "got PlayFileResponse"
                    << " job_id " << dynamic_cast< const voip_service::PlayFileResponse *>( req )->job_id
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

                last_job_id_++;

                dialer_->consume( voip_service::create_initiate_call_request( last_job_id_, s ) );
            }
            else if( cmd == "drop" )
            {
                last_job_id_++;
                dialer_->consume( voip_service::create_drop_request( last_job_id_, call_id_ ) );
            }
            else if( cmd == "play" )
            {
                std::string filename;
                stream >> filename;

                last_job_id_++;
                dialer_->consume( voip_service::create_play_file_request( last_job_id_, call_id_, filename ) );
            }
            else if( cmd == "rec" )
            {
                std::string filename;
                stream >> filename;

                last_job_id_++;
                dialer_->consume( voip_service::create_record_file_request( last_job_id_, call_id_, filename ) );
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
    voip_service::IVoipService  * dialer_;
    sched::Scheduler            * sched_;

    std::atomic<int>            call_id_;

    uint32_t                    last_job_id_;

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
