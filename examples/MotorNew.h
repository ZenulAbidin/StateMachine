#ifndef _MOTORNEW_H
#define _MOTORNEW_H

// You must define this macro to use the new transitions.
#define USE_NEW_TRANSITIONS

#include "StateMachine.h"

class MotorNewData : public EventData
{
public:
	int speed;
};

class MotorNew : public StateMachine
{
public:
	MotorNew();

	// External events taken by this state machine
	void SetSpeed(MotorNewData* data);
	void Halt();

	// These functions return whether the event is allowed
	// for this state, and can have any name.
	bool SetSpeedAllowed();
	bool HaltAllowed();

private:
	int m_currentSpeed; 

	// State enumeration order must match the order of state method entries
	// in the state map.
	enum States
	{
		ST_IDLE,
		ST_STOP,
		ST_START,
		ST_CHANGE_SPEED,
		ST_MAX_STATES
	};

	// For each event, the state machine has been moved into the
	// class definition.
	// You must declare the list of states before the transition map,
	// because the transition map macros depend on ST_MAX_STATES being
	// defined.
	// Each transition map takes a single parameter which is the name of
	// the event. This is used to initialize the maps, and the name must
	// be used consistently with all of the new transition macros.
	// It is recommended for the event name to correspond
	// to the event function names that you define listed above, as well
	// as the event legality check functions you also define above,
	// but this is not strictly required.
	BEGIN_TRANSITION_MAP(SetSpeed)			              		// - Current State -
		TRANSITION_MAP_ENTRY (ST_START)					// ST_IDLE
		TRANSITION_MAP_ENTRY (CANNOT_HAPPEN)				// ST_STOP
		TRANSITION_MAP_ENTRY (ST_CHANGE_SPEED)				// ST_START
		TRANSITION_MAP_ENTRY (ST_CHANGE_SPEED)				// ST_CHANGE_SPEED
	END_TRANSITION_MAP

	BEGIN_TRANSITION_MAP(Halt)			              		// - Current State -
		TRANSITION_MAP_ENTRY (EVENT_IGNORED)				// ST_IDLE
		TRANSITION_MAP_ENTRY (CANNOT_HAPPEN)				// ST_STOP
		TRANSITION_MAP_ENTRY (ST_STOP)					// ST_START
		TRANSITION_MAP_ENTRY (ST_STOP)					// ST_CHANGE_SPEED
	END_TRANSITION_MAP

	// Define the state machine state functions with event data type
	STATE_DECLARE(MotorNew, 	Idle,			NoEventData)
	STATE_DECLARE(MotorNew, 	Stop,			NoEventData)
	STATE_DECLARE(MotorNew, 	Start,			MotorNewData)
	STATE_DECLARE(MotorNew, 	ChangeSpeed,		MotorNewData)

	// State map to define state object order. Each state map entry defines a
	// state object.
	BEGIN_STATE_MAP
		STATE_MAP_ENTRY(&Idle)
		STATE_MAP_ENTRY(&Stop)
		STATE_MAP_ENTRY(&Start)
		STATE_MAP_ENTRY(&ChangeSpeed)
	END_STATE_MAP	
};

#endif
