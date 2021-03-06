/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/

#pragma once

#include "Mona/Mona.h"
#include "Mona/Path.h"
#include "Mona/ThreadQueue.h"
#include "Mona/Event.h"

namespace Mona {

/*!
File is a Path file with read and write operation 
/!\ Direct access to disk cache, no buffering */
struct File : virtual Object {
	typedef Event<void(shared<Buffer>& pBuffer, bool end)>	OnReaden;
	typedef Event<void(const Exception&)>					OnError;
	typedef Event<void()>									OnFlush;
	NULLABLE

	/*!
	Decoder offers to decode data in the reception thread when file is used with IOFile,
	If pBuffer is reseted, no onReaden is callen (data captured),
	If returns > 0 it continue reading operation (reads returned size) */
	struct Decoder : virtual Object { virtual UInt32 decode(shared<Buffer>& pBuffer, bool end) = 0; };

	// A mode R+W has no sense at this system level, because there is just one reading/writing shared header (R and W position)
	enum Mode {
		MODE_READ = 0, // To allow to test WRITE with a simple "if"
		MODE_WRITE,
		MODE_APPEND
	};
	File(const Path& path, Mode mode);
	~File();

	const Mode  mode;

	explicit operator bool() const { return _handle != -1; }
	operator const Path&() const { return _path; }

	// properties
	const std::string&  path() const { return _path; }
	const std::string&	name() const { return _path.name(); }
	const std::string&	baseName() const { return _path.baseName(); }
	const std::string&	extension() const { return _path.extension(); }
	const std::string&  parent() const { return _path.parent(); }
	bool				isAbsolute() const { return _path.isAbsolute(); }

	bool		exists(bool refresh = false) const { return _path.exists(refresh); }
	UInt64		size(bool refresh = false) const;
	Int64		lastAccess(bool refresh = false) const { return _path.lastAccess(refresh); }
	Int64		lastChange(bool refresh = false) const { return _path.lastChange(refresh); }

	bool		loaded() const { return _handle != -1; }

	UInt64		readen() const { return _readen; }
	UInt64		written() const { return _written; }

	UInt64		queueing() const;

	// File operation
	/*!
	Load the file, as expensive as a FileSystem::GetAttributes!
	If reading error => Ex::Permission || Ex::Unfound || Ex::Intern */
	virtual bool		load(Exception& ex); // virtual to allow to detect loading error with IOFile (see HTTPFileSender sample)
	/*!
	If reading error returns -1 => Ex::System::File || Ex::Intern */
	int					read(Exception& ex, void* data, UInt32 size);
	/*!
	If writing error => Ex::System::File || Ex::Intern */
	bool				write(Exception& ex, const void* data, UInt32 size);

	void				reset();

private:
	Path				_path;
	long				_handle;
	std::atomic<UInt64>	_readen;
	std::atomic<UInt64>	_written;

	//// Used by IOFile /////////////////////
	Decoder*					pDecoder;
	bool						externDecoder;
	OnReaden					onReaden;
	OnFlush						onFlush;
	OnError						onError;

	std::atomic<UInt64>			_queueing;
	UInt16						_ioTrack;
	UInt16						_decodingTrack;
	friend struct IOFile;
};


} // namespace Mona
