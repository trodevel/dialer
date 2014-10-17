/*

Dialer interface.

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

// $Id: i_dialer.h 1163 2014-10-17 18:02:06Z serge $

#ifndef I_DIALER_H
#define I_DIALER_H

#include <string>                   // std::string
#include "../utils/types.h"         // uint32

#include "namespace_lib.h"          // NAMESPACE_DIALER_START

NAMESPACE_DIALER_START

class CallI;
class IDialerCallback;

class IDialer
{
public:

public:
    virtual ~IDialer() {};

    virtual bool register_callback( IDialerCallback * callback )                                = 0;
    virtual void initiate_call( const std::string & party )                                     = 0;
    virtual void drop_all_calls()                                                               = 0;
    virtual boost::shared_ptr< CallI > get_call()                                               = 0;

    virtual bool shutdown()                                                                     = 0;
};

NAMESPACE_DIALER_END

#endif  // I_DIALER_H
