/*

Dialer objects.

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


// $Revision: 1404 $ $Date:: 2015-01-16 #$ $Author: serge $

#ifndef DIALER_OBJECTS_H
#define DIALER_OBJECTS_H

#include <string>                   // std::string
#include "../utils/types.h"         // uint32

#include "../servt/i_object.h"      // IObject

#include "namespace_lib.h"          // NAMESPACE_DIALER_START

NAMESPACE_DIALER_START

struct DialerObject: public servt::IObject
{
};

struct DialerCallObject: public DialerObject
{
    uint32          call_id;
};

struct DialerInitiateCallRequest: public DialerObject
{
    std::string     party;
};

struct DialerPlayFile: public DialerCallObject
{
    std::string     filename;
};

struct DialerRecordFile: public DialerCallObject
{
    std::string     filename;
};

struct DialerDrop: public DialerCallObject
{
};

// ******************* CALLBACKS *******************

struct DialerCallbackObject: public DialerObject
{
};

struct DialerErrorResponse: public DialerCallbackObject
{
    uint32 errorcode;
    std::string descr;
};

struct DialerRejectResponse: public DialerCallbackObject
{
    uint32 errorcode;
    std::string descr;
};

struct DialerInitiateCallResponse: public DialerCallbackObject
{
    uint32 call_id;
    uint32 status;
};

struct DialerCallbackCallObject: public DialerCallbackObject
{
    uint32          call_id;
};

struct DialerDropResponse: public DialerCallbackCallObject
{
};

struct DialerDial: public DialerCallbackCallObject
{
};

struct DialerRing: public DialerCallbackCallObject
{
};

struct DialerConnect: public DialerCallbackCallObject
{
};

struct DialerCallDuration: public DialerCallbackCallObject
{
    uint32 t;
};

struct DialerCallEnd: public DialerCallbackCallObject
{
    uint32          errorcode;
    std::string     descr;
};

struct DialerPlayStarted: public DialerCallbackCallObject
{
};

struct DialerPlayStopped: public DialerCallbackCallObject
{
};

struct DialerPlayFailed: public DialerCallbackCallObject
{
};

NAMESPACE_DIALER_END

#endif  // DIALER_OBJECTS_H
