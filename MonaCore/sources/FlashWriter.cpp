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

#include "Mona/FlashWriter.h"
#include "Mona/Util.h"
#include "Mona/Session.h"
#include "Mona/AVC.h"
#include "Mona/HEVC.h"
#include "Mona/FLVWriter.h"
#include "Mona/StringWriter.h"


using namespace std;


namespace Mona {


FlashWriter::FlashWriter() : _callbackHandleOnAbort(0),_callbackHandle(0), amf0(false), isMain(false), _lastTime(0) {
}

void FlashWriter::closing(Int32 error, const char* reason) {
	if (!isMain || error <= 0)
		return;
	switch(error) {
		case Session::ERROR_SERVER:
			writeAMFMainError("NetConnection.Connect.AppShutdown", reason ? reason : "Server shutdown");
			break;
		case Session::ERROR_REJECTED:
			writeAMFMainError("NetConnection.Connect.Rejected", reason ? reason : "Client rejected");
			break;
		case Session::ERROR_IDLE:
			writeAMFMainError("NetConnection.Connect.IdleTimeout", reason ? reason : "Client idle timeout");
			break;
		case Session::ERROR_PROTOCOL:
			writeAMFMainError("NetConnection.Connect.Failed", reason ? reason : "Protocol error");
			break;
		case Session::ERROR_UNEXPECTED:
			writeAMFMainError("NetConnection.Connect.Failed", reason ? reason : "Unknown error");
			break;
		default: // User code
			writeAMFMainError("NetConnection.Connect.Failed", reason ? reason : String("Error ", error));
	}
}

AMFWriter& FlashWriter::writeMessage() {
	AMFWriter& writer(writeInvocation("_result",_callbackHandleOnAbort = _callbackHandle));
	_callbackHandle = 0;
	return writer;
}

void FlashWriter::writeRaw(DataReader& reader) {
	UInt8 type;
	UInt32 time;
	if (!reader.readNumber(type)) {
		ERROR("RTMP content required at less a AMF number type (second param can be optionally timestamp number)");
		return;
	}
	StringWriter<> writer(write(AMF::Type(type), reader.readNumber(time) ? time : 0)->buffer());
	reader.read(writer);
}

AMFWriter& FlashWriter::writeInvocation(const char* name, double callback) {
	AMFWriter& writer(write(AMF::TYPE_INVOCATION));
	writer.amf0 = true;
	writer.writeString(name, strlen(name));
	writer.writeNumber(callback);
	writer->write8(AMF::AMF0_NULL); // for RTMP compatibility! (requiere it)
	writer.amf0 = amf0;
	return writer;
}

AMFWriter& FlashWriter::writeAMFState(const char* name,const char* code, bool isError, const string& description,bool withoutClosing) {
	AMFWriter& writer = (AMFWriter&)writeInvocation(name, _callbackHandleOnAbort = _callbackHandle);
	_callbackHandle = 0;
	writer.amf0 = true;
	writer.beginObject();
	writer.writeStringProperty("level",isError ? "error" : "status");
	writer.writeStringProperty("code",code);
	writer.writeStringProperty("description", description);
	writer.amf0 = amf0;
	if(!withoutClosing)
		writer.endObject();
	return writer;
}

AMFWriter& FlashWriter::writeAMFData(const string& name) {
	AMFWriter& writer(write(AMF::TYPE_DATA));
	writer.amf0 = true;
	writer.writeString(name.data(),name.size());
	writer.amf0 = amf0;
	return writer;
}

bool FlashWriter::beginMedia(const string& name) {
	_pPublicationName = &name;
	writeAMFStatus("NetStream.Play.PublishNotify", name + " is now published");
	_firstAV = true;
	return true;
}

bool FlashWriter::writeAudio(const Media::Audio::Tag& tag, const Packet& packet, bool reliable) {
	_time = tag.time + _lastTime;
	_firstAV = false;
	BinaryWriter& writer(*write(AMF::TYPE_AUDIO, _time, packet, reliable));
	if (!packet)
		return true; // "end audio signal" => detected by FP and works just if no packet content!
	writer.write8(FLVWriter::ToCodecs(tag));
	if (tag.codec == Media::Audio::CODEC_AAC)
		writer.write8(tag.isConfig ? 0 : 1);
	return true;
}

bool FlashWriter::writeVideo(const Media::Video::Tag& tag, const Packet& packet, bool reliable) {
	_time = tag.time + _lastTime;
	if (_firstAV) {
		_firstAV = false;
		// patch for flash, to avoid to wait audio, an empty audio packet has to be sent ( = "end audio signal")
		write(AMF::TYPE_AUDIO, _time, reliable);
	}
	bool isAVCConfig(tag.frame == Media::Video::FRAME_CONFIG && ((tag.codec == Media::Video::CODEC_H264 && AVC::ParseVideoConfig(packet, _sps, _pps)) || (tag.codec == Media::Video::CODEC_HEVC && HEVC::ParseVideoConfig(packet, _vps, _sps, _pps))));
	BinaryWriter& writer(*write(AMF::TYPE_VIDEO, _time, isAVCConfig ? Packet::Null() : packet, reliable));
	writer.write8(FLVWriter::ToCodecs(tag));
	if (tag.codec != Media::Video::CODEC_H264 && tag.codec != Media::Video::CODEC_HEVC)
		return true;
	if (isAVCConfig) {
		if (tag.codec == Media::Video::CODEC_HEVC)
			HEVC::WriteVideoConfig(writer.write8(0).write24(tag.compositionOffset), _vps, _sps, _pps);
		else
			AVC::WriteVideoConfig(writer.write8(0).write24(tag.compositionOffset), _sps, _pps);
	} else
		writer.write8(1).write24(tag.compositionOffset);
	return true;
}

bool FlashWriter::writeData(Media::Data::Type type, const Packet& packet, bool reliable) {
	// Always give 0 here for time, otherwise RTMP or RTMFP can't receive the data (tested..)
	AMFWriter& writer(write(AMF::TYPE_DATA, 0, type, packet, reliable));
	if (type == Media::Data::TYPE_AMF && packet && (*packet.data() == AMF::AMF0_STRING || *packet.data() == AMF::AMF0_LONG_STRING))
		return true; // has already correct (AMF0) handler!
	// Handler required (else can't be received in flash)
	writer.amf0 = true;
	if(type==Media::Data::TYPE_TEXT)
		writer.writeString(EXPAND("onTextData"));
	else if (type == Media::Data::TYPE_MEDIA)
		writer.writeString(EXPAND("onMedia"));
	else {
		unique_ptr<DataReader> pReader(Media::Data::NewReader(type, packet));
		if (!pReader->read(DataReader::STRING, writer))
			writer.writeString(EXPAND("onData")); // onData(data as ByteArray)
	}
	writer.amf0 = amf0;
	return true;
}

bool FlashWriter::writeProperties(const Media::Properties& properties) {
	// Always give 0 here for time, otherwise RTMP or RTMFP can't receive the data (tested..)
	AMFWriter& writer(write(AMF::TYPE_DATA, 0, properties[amf0 ? Media::Data::TYPE_AMF0 : Media::Data::TYPE_AMF], true));
	// Handler required (else can't be received in flash)
	writer.amf0 = true;
	writer.writeString(EXPAND("onMetaData"));
	writer.amf0 = amf0;
	return true;
}

void FlashWriter::endMedia() {
	_lastTime = _time;
	writeAMFStatus("NetStream.Play.UnpublishNotify", *_pPublicationName + " is now unpublished");
}


} // namespace Mona
