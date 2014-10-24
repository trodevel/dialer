/*

String helper.

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

// $Id: str_helper.h 1193 2014-10-24 18:17:16Z serge $

#include "namespace_lib.h"      // NAMESPACE_DIALER_START
#include "dialer_impl.h"        // enums
#include "player_sm.h"          // enums
#include "call_impl.h"          // enums

NAMESPACE_DIALER_START

class StrHelper
{
public:
    static std::string to_string( const DialerImpl::state_e & l );
    static std::string to_string( const PlayerSM::state_e & l );
    static std::string to_string( const CallImpl::state_e & l );
};

NAMESPACE_DIALER_END
