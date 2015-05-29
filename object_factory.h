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


// $Revision: 1778 $ $Date:: 2015-05-29 #$ $Author: serge $

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

inline DialerInitiateCallRequest *create_initiate_call_request( const std::string & party )
{
    DialerInitiateCallRequest *res = new DialerInitiateCallRequest;

    res->party      = party;

    return res;
}

inline DialerPlayFile *create_play_file( uint32 call_id, const std::string & filename )
{
    DialerPlayFile *res = new DialerPlayFile;

    res->call_id    = call_id;
    res->filename   = filename;

    return res;
}

inline DialerRecordFile *create_record_file( uint32 call_id, const std::string & filename )
{
    DialerRecordFile *res = new DialerRecordFile;

    res->call_id    = call_id;
    res->filename   = filename;

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

inline DialerCallEnd *create_call_end( uint32 call_id, uint32 errorcode, const std::string & descr )
{
    DialerCallEnd *res = create_message_t<DialerCallEnd>( call_id );

    res->errorcode  = errorcode;
    res->descr      = descr;

    return res;
}

NAMESPACE_DIALER_END

#endif  // DIALER_OBJECT_FACTORY_H
