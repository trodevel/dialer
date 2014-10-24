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

// $Id: call.cpp 1196 2014-10-24 19:00:52Z serge $

#include "call.h"                       // self

#include <boost/bind.hpp>               // boost::bind

#include "../utils/assert.h"            // ASSERT

#include "../asyncp/event.h"            // utils::new_event
#include "../utils/wrap_mutex.h"        // SCOPE_LOCK

#include "call_impl.h"                  // CallImpl

#define MODULENAME      "Call"

NAMESPACE_DIALER_START

Call::Call(
        uint32                        call_id,
        voip_service::IVoipService    * voips,
        sched::IScheduler             * sched,
        asyncp::IAsyncProxy           * proxy ):
    proxy_( proxy )
{
    impl_   = new CallImpl( call_id, voips, sched );

    ASSERT( proxy );
}

Call::~Call()
{
    if( impl_ )
    {
        delete impl_;
        impl_   = nullptr;
    }
}

void Call::drop()
{
    SCOPE_LOCK( mutex_ );

    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &CallImpl::drop, impl_ ) ) ) );
}

void Call::set_input_file( const std::string & filename )
{
    SCOPE_LOCK( mutex_ );

    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &CallImpl::set_input_file, impl_, filename ) ) ) );
}
void Call::set_output_file( const std::string & filename )
{
    SCOPE_LOCK( mutex_ );

    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &CallImpl::set_output_file, impl_, filename ) ) ) );
}

bool Call::is_ended() const
{
    SCOPE_LOCK( mutex_ );

    return impl_->is_ended();
}

bool Call::is_active() const
{
    SCOPE_LOCK( mutex_ );

    return impl_->is_active();
}

bool Call::register_callback( ICallCallbackPtr callback )
{
    SCOPE_LOCK( mutex_ );

    return impl_->register_callback( callback );
}

void Call::on_error( uint32 errorcode )
{
    SCOPE_LOCK( mutex_ );

    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &CallImpl::on_error, impl_, errorcode ) ) ) );
}
void Call::on_fatal_error( uint32 errorcode )
{
    SCOPE_LOCK( mutex_ );

    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &CallImpl::on_fatal_error, impl_, errorcode ) ) ) );
}
void Call::on_dial()
{
    SCOPE_LOCK( mutex_ );

    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &CallImpl::on_dial, impl_ ) ) ) );
}
void Call::on_ring()
{
    SCOPE_LOCK( mutex_ );

    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &CallImpl::on_ring, impl_ ) ) ) );
}

void Call::on_connect()
{
    SCOPE_LOCK( mutex_ );

    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &CallImpl::on_connect, impl_ ) ) ) );
}

void Call::on_call_duration( uint32 t )
{
    SCOPE_LOCK( mutex_ );

    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &CallImpl::on_call_duration, impl_, t ) ) ) );
}

void Call::on_play_start()
{
    SCOPE_LOCK( mutex_ );

    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &CallImpl::on_play_start, impl_ ) ) ) );
}

void Call::on_play_stop()
{
    SCOPE_LOCK( mutex_ );

    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &CallImpl::on_play_stop, impl_ ) ) ) );
}


void Call::on_call_end( uint32 errorcode )
{
    SCOPE_LOCK( mutex_ );

    proxy_->add_event( asyncp::IEventPtr( asyncp::new_event( boost::bind( &CallImpl::on_call_end, impl_, errorcode ) ) ) );
}


NAMESPACE_DIALER_END
