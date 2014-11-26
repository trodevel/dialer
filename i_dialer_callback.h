/*

Dialer callback interface.

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

// $Id: i_dialer_callback.h 1234 2014-11-25 19:24:30Z serge $

#ifndef I_DIALER_CALLBACK_H
#define I_DIALER_CALLBACK_H

#include "../utils/types.h"         // uint32

#include "namespace_lib.h"          // NAMESPACE_DIALER_START

NAMESPACE_DIALER_START

class IDialerCallback
{
public:

    virtual ~IDialerCallback() {}

    virtual void on_call_initiate_response( uint32 call_id, uint32 status )     = 0;
    virtual void on_error_response( uint32 error, const std::string & descr )   = 0;
    virtual void on_dial( uint32 call_id )                                      = 0;
    virtual void on_ring( uint32 call_id )                                      = 0;
    virtual void on_call_started( uint32 call_id )                              = 0;
    virtual void on_call_duration( uint32 call_id, uint32 t )                   = 0;
    virtual void on_call_end( uint32 call_id, uint32 errorcode )                = 0;
    virtual void on_ready()                                                     = 0;
    virtual void on_error( uint32 call_id, uint32 errorcode )                   = 0;
    virtual void on_fatal_error( uint32 call_id, uint32 errorcode )             = 0;
};

NAMESPACE_DIALER_END

#endif  // I_DIALER_CALLBACK_H
