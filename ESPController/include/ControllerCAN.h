#ifndef DIYBMS_CONTROLLER_CANBUS_H_
#define DIYBMS_CONTROLLER_CANBUS_H_

#pragma once

#include "defines.h"
#include "Rules.h"
#include <driver/twai.h>
//#include "queue.h"



class ControllerCAN
{
public:
	ControllerCAN()
	{
		memset(&data,0,sizeof(data));

		online_controller_count = 1; 
		master = 0;
		network_status = 0;

		memset(&DIYBMS_TIMESTAMP,0,sizeof(DIYBMS_TIMESTAMP));

		ControllerIsolated = 0;
		uint16_t aggregate_SOC = 0;
		uint16_t aggregate_Voltage = 0;
	}


	void c2c_DVCC();
	void c2c_ALARMS();
	void c2c_DIYBMS_MSGS();
	void c2c_MODULES();
	void c2c_SOC();
	void c2c_CAP();
	void c2c_HOST();
	void c2c_MINMAX_CELL_V_T();
	void c2c_CELL_IDS();
	void c2c_VIT();

	void clearvalues();

	void who_is_master();

	uint8_t master;

	uint8_t network_status;

	uint8_t controllerNetwork_status();

	// this array holds all the CAN identifiers
	const static uint32_t id[MAX_CAN_PARAMETERS][MAX_NUM_CONTROLLERS];

	// storage array for each parameter  FIRST 2 DIMENSIONS MUST MATCH DIMENSIONS OF id[] ARRAY!
	static uint8_t data[MAX_CAN_PARAMETERS][MAX_NUM_CONTROLLERS][TWAI_FRAME_MAX_DLC];

	// storage array for CANBUS timestamp (microseconds). Stores timestamp of the last received BITMSGS can frame
	// for each controller
	static int64_t DIYBMS_TIMESTAMP[MAX_NUM_CONTROLLERS];

	// # of controllers currently online
	static uint8_t online_controller_count;

	// returns the heartbeat status of a networked controller
	bool controller_heartbeat(uint8_t controllerAddress);

	// getter function
	bool IsControllerIsolated() const {return ControllerIsolated;}

	// Reconnect checks
	bool SOC_Permit_Reconnect();
	bool Voltage_Permit_Reconnect();	

	uint16_t AggregateSOC;
	int16_t AggregateVoltage;

private:
	void SetBankAndModuleText(char* buffer, uint8_t cellid);
	
	// Controller is Isolated
	bool ControllerIsolated;

};


extern uint8_t TotalNumberOfCells();
extern Rules rules;
extern currentmonitoring_struct currentMonitor;
extern diybms_eeprom_settings mysettings;
extern std::string hostname;
extern ControllerState _controller_state;
extern QueueHandle_t CANtx_q_handle;

#endif
