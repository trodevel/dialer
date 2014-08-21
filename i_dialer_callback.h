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

// $Id: i_dialer_callback.h 742 2014-07-11 16:57:13Z serge $

#ifndef I_DIALER_CALLBACK_H
#define I_DIALER_CALLBACK_H

#include "../utils/types.h"         // uint32

#include "namespace_lib.h"          // NAMESPACE_DIALER_START

NAMESPACE_DIALER_START

class IDialerCallback
{
public:

    virtual ~IDialerCallback() {}

    virtual void on_registered( bool b )                            = 0;
    virtual void on_ready()                                         = 0;
    virtual void on_busy()                                          = 0;
    virtual void on_error( uint32 errorcode )                       = 0;
};

NAMESPACE_DIALER_END

#endif  // I_DIALER_CALLBACK_H
