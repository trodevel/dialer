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

// $Revision: 1785 $ $Date:: 2015-05-29 #$ $Author: serge $

#include <iostream>         // cout
#include <typeinfo>
#include <sstream>          // stringstream
#include <atomic>           // std::atomic
#include <vector>           // std::vector

#include "i_dialer_callback.h"          // dialer::IDialerCallback
#include "dialer.h"                     // dialer::Dialer
#include "object_factory.h"             // dialer::create_message_t

#include "../voip_io/voip_service.h"    // voip_service::VoipService
#include "../skype_io/skype_io.h"       // SkypeIo
#include "../utils/dummy_logger.h"      // dummy_log_set_log_level
#include "../scheduler/scheduler.h"     // Scheduler

namespace sched
{
extern unsigned int MODULE_ID;
}

class Callback: virtual public dialer::IDialerCallback
{
public:
    Callback( dialer::IDialer * dialer, sched::Scheduler * sched ):
        dialer_( dialer ),
        sched_( sched ),
        call_id_( 0 )
    {
    }

    // interface IVoipServiceCallback
    void consume( const dialer::DialerCallbackObject * req )
    {
        if( typeid( *req ) == typeid( dialer::DialerInitiateCallResponse ) )
        {
            std::cout << "got DialerInitiateCallResponse "
                    << dynamic_cast< const dialer::DialerInitiateCallResponse *>( req )->call_id
                    << std::endl;

            call_id_    = dynamic_cast< const dialer::DialerInitiateCallResponse *>( req )->call_id;
        }
        else if( typeid( *req ) == typeid( dialer::DialerErrorResponse ) )
        {
            std::cout << "got DialerErrorResponse "
                    << dynamic_cast< const dialer::DialerErrorResponse *>( req )->descr
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( dialer::DialerRejectResponse ) )
        {
            std::cout << "got DialerRejectResponse "
                    << dynamic_cast< const dialer::DialerRejectResponse *>( req )->descr
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( dialer::DialerDropResponse ) )
        {
            std::cout << "got DialerDropResponse "
                    << dynamic_cast< const dialer::DialerDropResponse *>( req )->call_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( dialer::DialerCallEnd ) )
        {
            std::cout << "got DialerCallEnd "
                    << dynamic_cast< const dialer::DialerCallEnd *>( req )->call_id
                    << std::endl;

            call_id_    = 0;
        }
        else if( typeid( *req ) == typeid( dialer::DialerDial ) )
        {
            std::cout << "got DialerDial "
                    << dynamic_cast< const dialer::DialerDial *>( req )->call_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( dialer::DialerRing ) )
        {
            std::cout << "got DialerRing "
                    << dynamic_cast< const dialer::DialerRing *>( req )->call_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( dialer::DialerConnect ) )
        {
            std::cout << "got DialerConnect "
                    << dynamic_cast< const dialer::DialerConnect *>( req )->call_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( dialer::DialerCallDuration ) )
        {
            std::cout << "got DialerCallDuration "
                    << dynamic_cast< const dialer::DialerCallDuration *>( req )->call_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( dialer::DialerPlayStarted ) )
        {
            std::cout << "got DialerPlayStarted "
                    << dynamic_cast< const dialer::DialerPlayStarted *>( req )->call_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( dialer::DialerPlayStopped ) )
        {
            std::cout << "got DialerPlayStopped "
                    << dynamic_cast< const dialer::DialerPlayStopped *>( req )->call_id
                    << std::endl;
        }
        else if( typeid( *req ) == typeid( dialer::DialerPlayFailed ) )
        {
            std::cout << "got DialerPlayFailed "
                    << dynamic_cast< const dialer::DialerPlayFailed *>( req )->call_id
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

                dialer_->consume( dialer::create_initiate_call_request( s ) );
            }
            else if( cmd == "drop" )
            {
                if( call_id_ != 0 )
                    dialer_->consume( dialer::create_message_t<dialer::DialerDrop>( call_id_ ) );
            }
            else if( cmd == "play" )
            {
                if( call_id_ != 0 )
                {
                    std::string filename;
                    stream >> filename;

                    dialer_->consume( dialer::create_play_file( call_id_, filename ) );
                }
            }
            else if( cmd == "rec" )
            {
                if( call_id_ != 0 )
                {
                    std::string filename;
                    stream >> filename;

                    dialer_->consume( dialer::create_record_file( call_id_, filename ) );
                }
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
    dialer::IDialer     * dialer_;
    sched::Scheduler    * sched_;

    std::atomic<int>    call_id_;

};

void scheduler_thread( sched::Scheduler * sched )
{
    sched->start( true );
}

int main()
{
    dummy_logger::set_log_level( log_levels_log4j::DEBUG );

    skype_wrap::SkypeIo         sio;
    voip_service::VoipService   voips;
    dialer::Dialer              dialer;
    sched::Scheduler            sched;

    dummy_logger::set_log_level( sched::MODULE_ID, log_levels_log4j::ERROR );

    sched.load_config();
    sched.init();

    {
        bool b = dialer.init( & voips, & sched );
        if( !b )
        {
            std::cout << "cannot initialize Dialer" << std::endl;
            return 0;
        }
    }

    {
        bool b = voips.init( & sio );
        if( !b )
        {
            std::cout << "cannot initialize VoipService" << std::endl;
            return 0;
        }

        voips.register_callback( & dialer );
    }

    {
        bool b = sio.init();

        if( !b )
        {
            std::cout << "cannot initialize SkypeIo - " << sio.get_error_msg() << std::endl;
            return 0;
        }

        sio.register_callback( & voips );
    }


    Callback test( & dialer, & sched );
    dialer.register_callback( &test );

    voips.start();
    dialer.start();

    std::vector< std::thread > tg;

    tg.push_back( std::thread( std::bind( &Callback::control_thread, &test ) ) );
    tg.push_back( std::thread( std::bind( &scheduler_thread, &sched ) ) );

    for( auto & t : tg )
        t.join();

    sio.shutdown();
    voips.VoipService::shutdown();
    dialer.Dialer::shutdown();

    std::cout << "Done! =)" << std::endl;

    return 0;
}
