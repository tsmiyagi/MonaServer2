/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

*/

#pragma once

#include "Mona/Mona.h"
#include "Mona/Runner.h"
#include "Mona/JSONWriter.h"
#include "Mona/StringWriter.h"
#include "Mona/Socket.h"
#include "Mona/WS/WS.h"


namespace Mona {

struct WSSender : Runner, virtual Object {
	WSSender(const shared<Socket>& pSocket, WS::Type type, const Packet& packet);
	~WSSender() { delete &writer; }
	
	DataWriter&		writer;

	const WS::Type  type;

protected:
	virtual bool run(Exception&);
	Packet	_packet;

private:
	bool send(const Packet& packet);

	shared<Buffer>	_pBuffer;
	shared<Socket>	_pSocket;
};

struct WSDataSender : WSSender, virtual Object {
	WSDataSender(const shared<Socket>& pSocket, Media::Data::Type packetType, const Packet& packet) : _packetType(packetType), WSSender(pSocket, WS::TYPE_NIL, packet) {}
private:
	bool run(Exception&);

	Media::Data::Type _packetType;
};


} // namespace Mona
