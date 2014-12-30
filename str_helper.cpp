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

// $Id: str_helper.cpp 1288 2014-12-29 18:18:07Z serge $

#include "str_helper.h"             // self

#include <map>                      // std::map

NAMESPACE_DIALER_START

#define TUPLE_VAL_STR(_x_)  _x_,#_x_

#define MAP_INSERT_VAL( _m, _val )      _m.insert( Map::value_type( _val ) )

std::string StrHelper::to_string( const DialerImpl::state_e & l )
{
    typedef std::map< DialerImpl::state_e, std::string > Map;
    static Map m;
    if( m.empty() )
    {
        MAP_INSERT_VAL( m, DialerImpl:: TUPLE_VAL_STR( UNKNOWN ) );
        MAP_INSERT_VAL( m, DialerImpl:: TUPLE_VAL_STR( IDLE ) );
        MAP_INSERT_VAL( m, DialerImpl:: TUPLE_VAL_STR( WAITING_VOIP_RESPONSE ) );
        MAP_INSERT_VAL( m, DialerImpl:: TUPLE_VAL_STR( WAITING_DIALLING ) );
        MAP_INSERT_VAL( m, DialerImpl:: TUPLE_VAL_STR( DIALLING ) );
        MAP_INSERT_VAL( m, DialerImpl:: TUPLE_VAL_STR( RINGING ) );
        MAP_INSERT_VAL( m, DialerImpl:: TUPLE_VAL_STR( CONNECTED ) );
        MAP_INSERT_VAL( m, DialerImpl:: TUPLE_VAL_STR( WAITING_DROP_RESPONSE ) );
    }

    if( 0 == m.count( l ) )
        return "???";

    return m[l];
}

std::string StrHelper::to_string( const PlayerSM::state_e & l )
{
    typedef std::map< PlayerSM::state_e, std::string > Map;
    static Map m;
    if( m.empty() )
    {
        MAP_INSERT_VAL( m, PlayerSM:: TUPLE_VAL_STR( UNKNOWN ) );
        MAP_INSERT_VAL( m, PlayerSM:: TUPLE_VAL_STR( IDLE ) );
        MAP_INSERT_VAL( m, PlayerSM:: TUPLE_VAL_STR( WAITING ) );
        MAP_INSERT_VAL( m, PlayerSM:: TUPLE_VAL_STR( PLAYING ) );
    }

    if( 0 == m.count( l ) )
        return "UNKNOWN";

    return m[l];
}


NAMESPACE_DIALER_END

