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

// $Id: dialer.cpp 1186 2014-10-22 18:15:19Z serge $

#include "dialer.h"                     // self

#include "../utils/dummy_logger.h"      // dummy_log
#include "../asyncp/async_proxy.h"      // AsyncProxy
#include "../asyncp/event.h"            // new_event

#include "dialer_impl.h"                // DialerImpl

#include "../utils/wrap_mutex.h"        // SCOPE_LOCK
#include "../utils/assert.h"            // ASSERT

#include "namespace_lib.h"          // NAMESPACE_DIALER_START

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

    impl_   = new DialerImpl;

}

Dialer::~Dialer()
{
    if( proxy_ )
    {
        delete proxy_;
        proxy_  = nullptr;
    }

    if( impl_ )
    {
        delete impl_;
        impl_   = nullptr;
    }
}

bool Dialer::init(
        voip_service::IVoipService  * voips,
        sched::IScheduler           * sched )
{
    return impl_->init( voips, sched );
}

void Dialer::thread_func()
{
    dummy_log_debug( MODULENAME, "thread_func: started" );

    proxy_->thread_func();

    dummy_log_debug( MODULENAME, "thread_func: ended" );
}

bool Dialer::register_callback( IDialerCallback * callback )
{
    return impl_->register_callback( callback );
}

bool Dialer::is_inited() const
{
    return impl_->is_inited();
}

/*
Dialer::state_e Dialer::get_state() const
{
    return impl_->get_state();
}
*/

void Dialer::initiate_call( const std::string & party )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::initiate_call, impl_, party ) ) ) );
}
void Dialer::drop_all_calls()
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::drop_all_calls, impl_ ) ) ) );
}

bool Dialer::shutdown()
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::shutdown, impl_ ) ) ) );

    return proxy_->shutdown();
}

void Dialer::on_ready( uint32 errorcode )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_ready, impl_, errorcode ) ) ) );
}
void Dialer::on_error( uint32 call_id, uint32 errorcode )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_error, impl_, call_id, errorcode ) ) ) );
}
void Dialer::on_fatal_error( uint32 call_id, uint32 errorcode )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_fatal_error, impl_, call_id, errorcode ) ) ) );
}
void Dialer::on_dial( uint32 call_id )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_dial, impl_, call_id ) ) ) );
}
void Dialer::on_ring( uint32 call_id )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_ring, impl_, call_id ) ) ) );
}

void Dialer::on_connect( uint32 call_id )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_connect, impl_, call_id ) ) ) );
}

void Dialer::on_call_duration( uint32 call_id, uint32 t )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_call_duration, impl_, call_id, t ) ) ) );
}

void Dialer::on_play_start( uint32 call_id )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_play_start, impl_, call_id ) ) ) );
}

void Dialer::on_play_stop( uint32 call_id )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_play_stop, impl_, call_id ) ) ) );
}

void Dialer::on_call_end( uint32 call_id, uint32 errorcode )
{
    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &DialerImpl::on_call_end, impl_, call_id, errorcode ) ) ) );
}


NAMESPACE_DIALER_END
