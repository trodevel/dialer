/*

Call interface.

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

// $Id: call_i.h 1226 2014-10-29 23:34:06Z serge $

#ifndef CALL_I_H
#define CALL_I_H

#include <string>                   // std::string
#include "../utils/types.h"         // uint32
#include <boost/shared_ptr.hpp>

#include "i_call_callback.h"        // ICallCallbackPtr

#include "namespace_lib.h"          // NAMESPACE_DIALER_START

NAMESPACE_DIALER_START

class CallI
{
public:

public:
    virtual ~CallI() {};

    virtual void drop()                                                                         = 0;
    virtual void set_input_file( const std::string & filename )                                 = 0;
    virtual void set_output_file( const std::string & filename )                                = 0;
    virtual bool register_callback( ICallCallbackPtr callback )                                 = 0;
};

typedef boost::shared_ptr< CallI > CallIPtr;

NAMESPACE_DIALER_END

#endif  // CALL_I_H
