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

// $Revision: 6016 $ $Date:: 2017-03-14 #$ $Author: serge $

#include "str_helper.h"             // self

#include <map>                      // std::map

NAMESPACE_DIALER_START

#define TUPLE_VAL_STR(_x_)  _x_,#_x_

#define MAP_INSERT_VAL( _m, _val )      _m.insert( Map::value_type( _val ) )

const std::string & StrHelper::to_string( const Dialer::state_e & l )
{
    typedef std::map< Dialer::state_e, std::string > Map;
    static Map m;
    if( m.empty() )
    {
        MAP_INSERT_VAL( m, Dialer:: TUPLE_VAL_STR( UNKNOWN ) );
        MAP_INSERT_VAL( m, Dialer:: TUPLE_VAL_STR( IDLE ) );
        MAP_INSERT_VAL( m, Dialer:: TUPLE_VAL_STR( WAITING_INITIATE_CALL_RESPONSE ) );
        MAP_INSERT_VAL( m, Dialer:: TUPLE_VAL_STR( WAITING_CONNECTION ) );
        MAP_INSERT_VAL( m, Dialer:: TUPLE_VAL_STR( CONNECTED ) );
        MAP_INSERT_VAL( m, Dialer:: TUPLE_VAL_STR( CANCELED_IN_C ) );
        MAP_INSERT_VAL( m, Dialer:: TUPLE_VAL_STR( CANCELED_IN_WC ) );
    }

    static const std::string def("???");

    if( 0 == m.count( l ) )
        return def;

    return m[l];
}

const std::string & StrHelper::to_string( const PlayerSM::state_e & l )
{
    typedef std::map< PlayerSM::state_e, std::string > Map;
    static Map m =
    {
        { PlayerSM:: TUPLE_VAL_STR( IDLE ) },
        { PlayerSM:: TUPLE_VAL_STR( WAIT_PLAY_RESP ) },
        { PlayerSM:: TUPLE_VAL_STR( WAIT_PLAY_START ) },
        { PlayerSM:: TUPLE_VAL_STR( PLAYING ) },
        { PlayerSM:: TUPLE_VAL_STR( CANCELED_IN_WPS ) },
        { PlayerSM:: TUPLE_VAL_STR( PLAYING_ALREADY_STOPPED ) },
        { PlayerSM:: TUPLE_VAL_STR( CANCELED_IN_P ) },
    };

    auto it = m.find( l );

    static const std::string undef( "???" );

    if( it == m.end() )
        return undef;

    return it->second;
}


NAMESPACE_DIALER_END

