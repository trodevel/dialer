/*

Dialer object factory.

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


// $Id: object_factory.h 1278 2014-12-23 18:23:45Z serge $

#ifndef DIALER_OBJECT_FACTORY_H
#define DIALER_OBJECT_FACTORY_H

#include "objects.h"    // DialerObject...

NAMESPACE_DIALER_START

inline void init_call_id( DialerCallObject * obj, uint32 call_id )
{
    obj->call_id = call_id;
}

inline void init_call_id( DialerCallbackCallObject * obj, uint32 call_id )
{
    obj->call_id = call_id;
}

template <class _T>
_T *create_message_t( uint32 call_id )
{
    _T *res = new _T;

    init_call_id( res, call_id );

    return res;
}

inline DialerErrorResponse *create_error_response( uint32 errorcode, const std::string & descr )
{
    DialerErrorResponse *res = new DialerErrorResponse;

    res->errorcode  = errorcode;
    res->descr      = descr;

    return res;
}

inline DialerRejectResponse *create_reject_response( uint32 errorcode, const std::string & descr )
{
    DialerRejectResponse *res = new DialerRejectResponse;

    res->errorcode  = errorcode;
    res->descr      = descr;

    return res;
}

inline DialerInitiateCallResponse *create_initiate_call_response( uint32 call_id, uint32 status )
{
    DialerInitiateCallResponse *res = new DialerInitiateCallResponse;

    res->call_id    = call_id;
    res->status     = status;

    return res;
}

inline DialerCallDuration *create_call_duration( uint32 call_id, uint32 t )
{
    DialerCallDuration *res = create_message_t<DialerCallDuration>( call_id );

    res->t  = t;

    return res;
}

inline DialerCallEnd *create_call_end( uint32 call_id, uint32 errorcode )
{
    DialerCallEnd *res = create_message_t<DialerCallEnd>( call_id );

    res->errorcode  = errorcode;

    return res;
}

inline DialerError *create_error( uint32 call_id, const std::string & error )
{
    DialerError *res = create_message_t<DialerError>( call_id );

    res->error = error;

    return res;
}

inline DialerFatalError *create_fatal_error( uint32 call_id, const std::string & error )
{
    DialerFatalError *res = create_message_t<DialerFatalError>( call_id );

    res->error = error;

    return res;
}

NAMESPACE_DIALER_END

#endif  // DIALER_OBJECT_FACTORY_H