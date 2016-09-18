/*

Wrapper for boost::regex_match()

Copyright (C) 2016 Sergey Kolevatov

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

// $Revision: 4429 $ $Date:: 2016-09-19 #$ $Author: serge $

#include "regex_match.h"                // self

#include <boost/regex.hpp>              // boost::regex_match()

NAMESPACE_DIALER_START

bool regex_match( const std::string & s, const std::string & regex )
{
    boost::regex e( regex );
    return boost::regex_match( s, e );
}

NAMESPACE_DIALER_END
