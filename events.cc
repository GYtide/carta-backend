#include "events.h"
#include "rapidjson/writer.h"

using namespace uWS;
using namespace rapidjson;
using namespace std;

void sendEvent(WebSocket<SERVER>* ws, Document& document)
{
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	document.Accept(writer);
	string jsonPayload = buffer.GetString();
	ws->send(jsonPayload.c_str(), jsonPayload.size(), uWS::TEXT);
}

void sendEventBinaryPayload(WebSocket<SERVER>* ws, Document& document, void* payload, uint32_t length)
{
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	document.Accept(writer);
	string jsonPayload = buffer.GetString();
	auto rawData = new char[jsonPayload.size() + length + sizeof(length)];
	memcpy(rawData, &length, sizeof(length));
	memcpy(rawData + sizeof(length), payload, length);
	memcpy(rawData + length + sizeof(length), jsonPayload.c_str(), jsonPayload.size());
	ws->send(rawData, jsonPayload.size() + length + sizeof(length), uWS::BINARY);
	delete[] rawData;
}