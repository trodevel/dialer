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

// $Id: dialer.cpp 1234 2014-11-25 19:24:30Z serge $

#include "dialer.h"                     // self

#include "../utils/dummy_logger.h"      // dummy_log
#include "../asyncp/async_proxy.h"      // AsyncProxy
#include "../asyncp/event.h"            // new_event

#include "../utils/wrap_mutex.h"        // SCOPE_LOCK
#include "../utils/assert.h"            // ASSERT

#define MODULENAME      "Dialer"

NAMESPACE_DIALER_START

Dialer::Dialer()
{
    proxy_  = new asyncp::AsyncProxy;

    {
        asyncp::AsyncProxy::Config cfg;
        cfg.sleep_time_ms   = 1;
        cfg.name            = MODULENAME;

        ASSERT( proxy_->init( cfg ) );
    }
}

Dialer::~Dialer()
{
    if( proxy_ )
    {
        delete proxy_;
        proxy_  = nullptr;
    }
}

bool Dialer::init(
        voip_service::IVoipService  * voips,
        sched::IScheduler           * sched )
{
    return DialerImpl::init( voips, sched );
}

void Dialer::thread_func()
{
    dummy_log_debug( MODULENAME, "thread_func: started" );

    proxy_->thread_func();

    dummy_log_debug( MODULENAME, "thread_func: ended" );
}

bool Dialer::register_callback( IDialerCallback * callback )
{
    return DialerImpl::register_callback( callback );
}

bool Dialer::is_inited() const
{
    return DialerImpl::is_inited();
}

void Dialer::initiate_call( const std::string & party )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::initiate_call, this, party ) ) ) );
}
void Dialer::drop( uint32 call_id )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::drop, this, call_id ) ) ) );
}
void Dialer::set_input_file( uint32 call_id, const std::string & filename )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::set_input_file, this, call_id, filename ) ) ) );
}
void Dialer::set_output_file( uint32 call_id, const std::string & filename )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::set_output_file, this, call_id, filename  ) ) ) );
}

bool Dialer::shutdown()
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::shutdown, this ) ) ) );

    return proxy_->shutdown();
}

void Dialer::on_ready( uint32 errorcode )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_ready, this, errorcode ) ) ) );
}
void Dialer::on_error( uint32 call_id, uint32 errorcode )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_error, this, call_id, errorcode ) ) ) );
}
void Dialer::on_fatal_error( uint32 call_id, uint32 errorcode )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_fatal_error, this, call_id, errorcode ) ) ) );
}
void Dialer::on_dial( uint32 call_id )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_dial, this, call_id ) ) ) );
}
void Dialer::on_ring( uint32 call_id )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_ring, this, call_id ) ) ) );
}

void Dialer::on_connect( uint32 call_id )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_connect, this, call_id ) ) ) );
}

void Dialer::on_call_duration( uint32 call_id, uint32 t )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_call_duration, this, call_id, t ) ) ) );
}

void Dialer::on_play_start( uint32 call_id )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_play_start, this, call_id ) ) ) );
}

void Dialer::on_play_stop( uint32 call_id )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_play_stop, this, call_id ) ) ) );
}

void Dialer::on_call_end( uint32 call_id, uint32 errorcode )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_call_end, this, call_id, errorcode ) ) ) );
}


NAMESPACE_DIALER_END
