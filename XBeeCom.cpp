#include <Arduino.h>
#include "XBeeCom.h"

XBeeCom::XBeeCom(String name,
		bool softSerial,
		long baud,
		int rxPin,
		int txPin) {
	// Ensure name is less than MAX_NAME_LEN - null character
	name.remove(MAX_NAME_LEN - 1);
	_name = name;
	_softSerial = softSerial;
	_rxPin = rxPin;
	_txPin = txPin;
	_baud = baud;
	_xbee = XBee();
	if (_softSerial) {
		_sserial = new SoftwareSerial(_rxPin, _txPin);
		_hserial = NULL;
	} else {
		_sserial = NULL;
		if (rxPin == 0)
			_hserial = &Serial;
# if defined(HAVE_HWSERIAL1)
		if (rxPin == 1)
			_hserial = &Serial1;
# endif
# if defined(HAVE_HWSERIAL2)
		if (rxPin == 2)
			_hserial = &Serial2;
# endif
# if defined(HAVE_HWSERIAL3)
		if (rxPin == 3)
			_hserial = &Serial3;
# endif
	}
	_atResp = AtCommandResponse();
}

XBeeCom::XBeeCom(String name): XBeeCom(name, true, 57600, 2, 3) {
}

XBeeCom::~XBeeCom() {
	_sserial->end();
	delete _sserial;
}

void XBeeCom::begin() {
	if (_softSerial) {
		_sserial->begin(_baud);
		_xbee.setSerial(*_sserial);
	} else {
		_hserial->begin(_baud);
		_xbee.setSerial(*_hserial);
	}
	// Wait until radio is associated with coordinator
	uint8_t aiCmd[] = {'A', 'I'};
	_atReq = AtCommandRequest(aiCmd);
	do {
		delay(1000);
		int ret = sendAtCommand();
		if (ret != 0) {
			//Serial.print(ret);
			continue;
		}
		//Serial.println(_atResp.getValue()[0], HEX);
	} while (_atResp.getValueLength() != 1 || _atResp.getValue()[0] != 0);
	// TODO: Perhaps gwNI shouldn't be hard coded, but for convenience, it is.
	uint8_t gwNI[] = "GW1";
	find64Addr(gwNI, 3, _gwAddr);
	/*
	Serial.println(_gwAddr.getMsb(), HEX);
	Serial.println(_gwAddr.getLsb(), HEX);
	*/
}

int XBeeCom::find64Addr(uint8_t ni[], uint8_t size, XBeeAddress64 &addr64) {
	uint8_t ndCmd[] = {'N', 'D'};
	_atReq = AtCommandRequest(ndCmd, ni, size);
	int ret = sendAtCommand();
	/*
	for (int i = 0; i < _atResp.getValueLength(); i++) {
		Serial.print(_atResp.getValue()[i], HEX);
		Serial.print(" ");
	}
	*/
	addr64.setMsb(swap_uint32(*reinterpret_cast<uint32_t*>(_atResp.getValue() + 2)));
	addr64.setLsb(swap_uint32(*reinterpret_cast<uint32_t*>(_atResp.getValue() + 6)));
	return ret;
}

uint32_t XBeeCom::swap_uint32(uint32_t i) {
	i = ((i << 8) & 0xFF00FF00) | ((i >> 8) & 0xFF00FF);
	return (i << 16) | (i >> 16);
}

void XBeeCom::sendStatus(DevType deviceType,
		int deviceNum,
		String msg) {
	// Ensure msg is less than MAX_MSG_LEN - null character
	msg.remove(MAX_MSG_LEN - 1);
	StatusTXPacket packet = StatusTXPacket();
	setupTXStatusPacket(&packet);
	packet.devtype = deviceType;
	packet.devnum = deviceNum;
	// Sets length as length of string + null character
	packet.namelen = _name.length() + 1;
	packet.msglen = msg.length() + 1;
	packet.name = _name.c_str();
	packet.msg = msg.c_str();
	uint8_t payload[getStatusPayloadSize(&packet)];
	//Serial.println(sizeof(payload));
	marshalTXStatusPacket(payload, &packet);
	int status = send64(_gwAddr, payload, sizeof(payload));
	//Serial.println(status);
}
// Determines the size of payload
int XBeeCom::getStatusPayloadSize(StatusTXPacket *packet) {
	return STATUS_HEADER_SIZE + packet->namelen + packet->msglen;
}
// Marshall variable size payload
void XBeeCom::marshalTXStatusPacket(uint8_t* payload, StatusTXPacket* packet) {
	memcpy(payload, packet, STATUS_HEADER_SIZE);
	memcpy(payload + STATUS_HEADER_SIZE, packet->name, packet->namelen);
	memcpy(payload + STATUS_HEADER_SIZE + packet->namelen, packet->msg, packet->msglen);
}

void XBeeCom::setupTXStatusPacket(StatusTXPacket* packet) {
	packet->h1 = MAGIC_HEADER1;
	packet->h2 = MAGIC_HEADER2;
	packet->vers = PKT_VERS;
	packet->msgtype = REPORT_STATUS;
}

int XBeeCom::send16(uint16_t address, uint8_t *payload, uint8_t size) {
	/*Serial.println(size);
	for (int i = 0; i < size; i++) {
		Serial.print(payload[i], HEX);
		Serial.print(" ");
	}
	Serial.print("\n");
	Serial.println(address);
	Serial.println("Sending packet");
	delay(500);
	*/
	Tx16Request tx = Tx16Request(address, payload, size);
	_xbee.send(tx);
	if (_xbee.readPacket(2000)) {
		if (_xbee.getResponse().getApiId() == TX_STATUS_RESPONSE) {
			TxStatusResponse txStatus = TxStatusResponse();
			_xbee.getResponse().getTxStatusResponse(txStatus);

			if (txStatus.getStatus() == SUCCESS) {
				return 0;
			} else {
				return txStatus.getStatus();
			}
		}
	}
	return -1;
}

int XBeeCom::send64(XBeeAddress64 &addr64, uint8_t* payload, uint8_t size) {
	/*
	Serial.println(size);
	for (int i = 0; i < size; i++) {
		Serial.print(payload[i], HEX);
		Serial.print(" ");
	}
	Serial.print("\n");
	Serial.println("Sending packet");
	delay(500);
	*/
	Tx64Request tx = Tx64Request(addr64, payload, size);
	_xbee.send(tx);
	if (_xbee.readPacket(2000)) {
		if (_xbee.getResponse().getApiId() == TX_STATUS_RESPONSE) {
			TxStatusResponse txStatus = TxStatusResponse();
			_xbee.getResponse().getTxStatusResponse(txStatus);

			if (txStatus.getStatus() == SUCCESS) {
				return 0;
			} else {
				return txStatus.getStatus();
			}
		}
	}
	return -1;
}

AlertType XBeeCom::checkAlert(unsigned int maxRuntime) {
	unsigned long startT = millis();
	while (millis() - startT < maxRuntime) {
		_xbee.readPacket();
		if (_xbee.getResponse().isAvailable()) {
			//Serial.println(_xbee.getResponse().getApiId(), HEX);
			if (_xbee.getResponse().getApiId() == RX_16_RESPONSE) {
				Rx16Response rx16 = Rx16Response();
				_xbee.getResponse().getRx16Response(rx16);
				XBRXPacket * packet = reinterpret_cast<XBRXPacket *>(rx16.getData());
				/*
				Serial.println(packet->alertpacket.h1, HEX);
				Serial.println(packet->alertpacket.h2, HEX);
				Serial.println((int)packet->alertpacket.msgtype, HEX);
				Serial.println((int)packet->alertpacket.alerttype, HEX);
				*/
				if (packet->alertpacket.h1 == MAGIC_HEADER1 &&
						packet->alertpacket.h2 == MAGIC_HEADER2 &&
						packet->alertpacket.msgtype == SEND_ALERT) {
					return packet->alertpacket.alerttype;
				} else {
					continue;
				}
			}
			if (_xbee.getResponse().getApiId() == RX_64_RESPONSE) {
				Rx64Response rx64 = Rx64Response();
				_xbee.getResponse().getRx64Response(rx64);
				XBRXPacket * packet = reinterpret_cast<XBRXPacket *>(rx64.getData());
				if (packet->alertpacket.h1 == MAGIC_HEADER1 &&
						packet->alertpacket.h2 == MAGIC_HEADER2 &&
						packet->alertpacket.msgtype == SEND_ALERT) {
					return packet->alertpacket.alerttype;
				} else {
					continue;
				}
			}

		} else {
			return NO_ALERT;
		}
	}
	return NO_ALERT;
}

String XBeeCom::alertName(AlertType alertCode)
{
	switch (alertCode) {
	case NO_ALERT:
		return(F("NO ALERT"));
		break;
	case FIRE_ALERT:
		return(F("FIRE"));
		break;
	case SMOKE_ALERT:
		return(F("SMOKE"));
		break;
	case FLOOD_ALERT:
		return(F("FLOOD"));
		break;
	case INTRUDER_ALERT:
		return(F("INTRUDER"));
		break;
	case EARTHQUAKE_ALERT:
		return(F("EARTHQUAKE"));
		break;
	case ZOMBIE_ALERT:
		return(F("ZOMBIE"));
		break;
	case APOCALYPSE_ALERT:
		return(F("APOCALYPSE"));
		break;
	default:
		return(F("???"));
	}
}

void XBeeCom::sendAlert(AlertType alert, XBeeAddress64 &addr64) {
	AlertTXPacketU aPacketU;
	aPacketU.alertPacket.h1 = MAGIC_HEADER1;
	aPacketU.alertPacket.h2 = MAGIC_HEADER2;
	aPacketU.alertPacket.vers = PKT_VERS;
	aPacketU.alertPacket.msgtype = SEND_ALERT;
	aPacketU.alertPacket.alerttype = alert;
	send64(addr64, aPacketU.uint8ary, sizeof(aPacketU.uint8ary));
}

void XBeeCom::sendAlertBroadcast(AlertType alert) {
	XBeeAddress64 addr64 = XBeeAddress64(0x0, 0xFFFF);
	sendAlert(alert, addr64);
}
// Receives a XBee packet to 'resp' and casts the data to a XBRXPacket pointer 'pkt'
int XBeeCom::receiveAndConvertPacket(Rx64Response &resp, XBRXPacket* &pkt) {
	_xbee.readPacket();
	if (_xbee.getResponse().isAvailable()) {
		if (_xbee.getResponse().getApiId() == RX_64_RESPONSE) {
			_xbee.getResponse().getRx64Response(resp);
			pkt = reinterpret_cast<XBRXPacket*>(resp.getData());
			return 0;
		}
		if (_xbee.getResponse().getApiId() == RX_16_RESPONSE) {
			_xbee.getResponse().getRx16Response(resp);
			pkt = reinterpret_cast<XBRXPacket*>(resp.getData());
			return 0;
		}
		return 2;
	} else {
		return 1;
	}
}

int XBeeCom::sendAtCommand() {
	// send the command
	_xbee.send(_atReq);

	// wait up to 5 seconds for the status response
	if (_xbee.readPacket(5000)) {
		// should be an AT command response
		if (_xbee.getResponse().getApiId() == AT_COMMAND_RESPONSE) {
			_xbee.getResponse().getAtCommandResponse(_atResp);
			return 0;
		}
	} else {
		// at command failed
		if (_xbee.getResponse().isError()) {
			return 1;
		}
		else {
			return 2;
		}
	}
	return -1;
}
