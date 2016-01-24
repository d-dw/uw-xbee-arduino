#ifndef XBeeCom_h
#define XBeeCom_h

#define STATUS_HEADER_SIZE 11
#define MAX_NAME_LEN 20
#define MAX_MSG_LEN 69
#define MAX_PKT_SIZE 100
#define MAGIC_HEADER1 0x55
#define MAGIC_HEADER2 0x57
#define PKT_VERS 1

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <XBee.h>

// types of messages that can be sent
enum MsgType
{
	REGISTER_NODE = 1,
	REPORT_STATUS,
	SEND_TIME,
	SEND_ALERT,
	SEND_DATA
};

// device types attached to a room hub
enum DevType
{
	LIGHT_DEV = 101,
	TEMP_DEV,
	SOUND_DEV,
	MOTION_DEV,
	INTRUSION_DEV,
	ID_TAG_DEV,
	WINDOW_DEV,
	LAMP_DEV,
	CAMERA_DEV,
	ANNOUNCE_DEV,
	ALARM_DEV,
	APPLIANCE_DEV,
	SPRINKLER_DEV,

	OTHER_DEV = 900,

	// this device represents the room hub itself -- use it
	// if you need to signal something about the entire room
	ROOM_HUB_DEV = 901,

	// this device is for the home gateway and shouldn't be
	// used by an individual room hub
	HOME_GATEWAY_DEV = 999
};

// gateway-generated alert messages broadcast to all rooms
enum AlertType
{
	NO_ALERT = 0,
	FIRE_ALERT = 1001,
	SMOKE_ALERT,
	FLOOD_ALERT,
	INTRUDER_ALERT,
	EARTHQUAKE_ALERT,
	ZOMBIE_ALERT,
	APOCALYPSE_ALERT
};

// gateway-generated data stream messages
enum DataStreamType
{
	TIME_DATA = 10001,
	TEMP_DATA,
	POWER_DATA
};

typedef struct StatusTXPacket {
	byte h1;
	byte h2;
	byte vers;
	MsgType msgtype;
	DevType devtype;
	int devnum;
	byte namelen;
	byte msglen;
	const char * name;
	const char * msg;
} StatusTXPacket;

typedef struct StatusRXPacket {
	byte h1;
	byte h2;
	byte vers;
	MsgType msgtype;
	DevType devtype;
	int devnum;
	byte namelen;
	byte msglen;
	char chardata[MAX_NAME_LEN + MAX_MSG_LEN];
} StatusRXPacket;

typedef struct AlertTXPacket {
	byte h1;
	byte h2;
	byte vers;
	MsgType msgtype;
	AlertType alerttype;
} AlertTXPacket;

typedef struct AlertRXPacket {
	byte h1;
	byte h2;
	byte vers;
	MsgType msgtype;
	AlertType alerttype;
} AlertRXPacket;

typedef union alerttxpacket {
	AlertTXPacket alertPacket;
	uint8_t uint8ary[7];
}AlertTXPacketU;

typedef union xbrxpacket {
	StatusRXPacket statuspacket;
	AlertRXPacket alertpacket;
	uint8_t uint8ary[MAX_PKT_SIZE];
}XBRXPacket;

class XBeeCom
{
public:
	XBeeCom(String name,
			bool softSerial,
			long baud,
			int rxPin,
			int txPin);

	XBeeCom(String name);

	~XBeeCom();

	void begin();
	void sendStatus(DevType deviceType, int deviceNum, String msg);
	AlertType checkAlert(unsigned int runtime = 100);
	String alertName(AlertType alertCode);
	// New functions
	void sendAlert(AlertType alert, XBeeAddress64 &addr64);
	void sendAlertBroadcast(AlertType alert);
	int receiveAndConvertPacket(Rx16Response &resp, XBRXPacket* &pkt);

private:

	String _name;
	bool _softSerial;
	int _rxPin;
	int _txPin;
	long _baud;
	XBee _xbee;
	XBeeAddress64 _gwAddr;
	SoftwareSerial* _sserial;
	AtCommandRequest _atReq;
	AtCommandResponse _atResp;

	int getStatusPayloadSize(StatusTXPacket* packet);
	void marshalTXStatusPacket(uint8_t* payload, StatusTXPacket* packet);
	int send16(uint16_t address, uint8_t* payload, uint8_t size);
	int send64(XBeeAddress64 &addr64, uint8_t* payload, uint8_t size);
	int sendAtCommand();
	void setupTXStatusPacket(StatusTXPacket* packet);
	uint32_t swap_uint32(uint32_t i);
	int find64Addr(uint8_t ni[], uint8_t size, XBeeAddress64 &addr64);
};

#endif
