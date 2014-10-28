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

// $Id: i_dialer_callback.h 1216 2014-10-28 18:04:01Z serge $

#ifndef I_DIALER_CALLBACK_H
#define I_DIALER_CALLBACK_H

#include "../utils/types.h"         // uint32

#include "call_i.h"                 // CallIPtr

#include "namespace_lib.h"          // NAMESPACE_DIALER_START

NAMESPACE_DIALER_START

class IDialerCallback
{
public:

    virtual ~IDialerCallback() {}

    virtual void on_registered( bool b )                            = 0;
    virtual void on_call_initiate_response( bool is_initiated, uint32 status, CallIPtr call ) = 0;
    virtual void on_call_started()                                  = 0;
    virtual void on_ready()                                         = 0;
    virtual void on_error( uint32 errorcode )                       = 0;
};

NAMESPACE_DIALER_END

#endif  // I_DIALER_CALLBACK_H
