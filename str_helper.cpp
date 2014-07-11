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

// $Id: str_helper.cpp 763 2014-07-11 16:52:14Z serge $

#include "str_helper.h"             // self

#include <map>                      // std::map

NAMESPACE_DIALER_START


#define GLUE( _a, _b )  _a ## _b

#define TUPLE_VAL_STR(_x_)  _x_,#_x_
#define TUPLE_STR_VAL(_x_)  #_x_,_x_

#define TUPLE_STR_VAL_PREF(_pref,_x_)  #_x_,GLUE(_pref,_x_)
#define TUPLE_VAL_STR_PREF(_pref,_x_)  GLUE(_pref,_x_),#_x_

#define MAP_INSERT_VS( _m, _val )              _m.insert( Map::value_type( TUPLE_VAL_STR( _val ) ) )
#define MAP_INSERT_VS_PREF( _m, _pref, _val )  _m.insert( Map::value_type( TUPLE_VAL_STR_PREF( _pref, _val ) ) )

#define MAP_INSERT( _m, _val )              _m.insert( Map::value_type( TUPLE_STR_VAL( _val ) ) )
#define MAP_INSERT_PREF( _m, _pref, _val )  _m.insert( Map::value_type( TUPLE_STR_VAL_PREF( _pref, _val ) ) )

std::string StrHelper::to_string( const Dialer::state_e & l )
{
    typedef std::map< Dialer::state_e, std::string > Map;
    static Map m;
    if( m.empty() )
    {
        MAP_INSERT_VS( m, Dialer::UNKNOWN );
        MAP_INSERT_VS( m, Dialer::IDLE );
        MAP_INSERT_VS( m, Dialer::BUSY );
    }

    if( 0 == m.count( l ) )
        return "UNKNOWN";

    return m[l];
}

std::string StrHelper::to_string( const Call::state_e & l )
{
    typedef std::map< Call::state_e, std::string > Map;
    static Map m;
    if( m.empty() )
    {
        MAP_INSERT_VS( m, Call::UNKNOWN );
        MAP_INSERT_VS( m, Call::IDLE );
        MAP_INSERT_VS( m, Call::DIALLING );
        MAP_INSERT_VS( m, Call::RINGING );
        MAP_INSERT_VS( m, Call::CONNECTED );
    }

    if( 0 == m.count( l ) )
        return "UNKNOWN";

    return m[l];
}

std::string StrHelper::to_string( const PlayerSM::state_e & l )
{
    typedef std::map< PlayerSM::state_e, std::string > Map;
    static Map m;
    if( m.empty() )
    {
        MAP_INSERT_VS( m, PlayerSM::UNKNOWN );
        MAP_INSERT_VS( m, PlayerSM::IDLE );
        MAP_INSERT_VS( m, PlayerSM::WAITING );
        MAP_INSERT_VS( m, PlayerSM::PLAYING );
    }

    if( 0 == m.count( l ) )
        return "UNKNOWN";

    return m[l];
}


NAMESPACE_DIALER_END

