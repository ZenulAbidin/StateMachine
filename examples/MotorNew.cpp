#include "MotorNew.h"
#include <iostream>

using namespace std;

MotorNew::MotorNew() :
	StateMachine(ST_MAX_STATES),
	m_currentSpeed(0)
{
}
	
// set motor speed external event
void MotorNew::SetSpeed(MotorNewData* data)
{
	// For implementing an event using the new transitions,
	// you must fill it with only TRANSITION_BODY like this:
	TRANSITION_BODY(SetSpeed, data);
}

bool MotorNew::SetSpeedAllowed()
{
	// To implement the event legality checks using the new transitions,
	// you must fill it with only TRANSITION_CHECK_BODY like this:
	TRANSITION_CHECK_BODY(SetSpeed);
}

// halt motor external event
void MotorNew::Halt()
{
	// As with the old transitions, if there is no event data, simply
	// fill the second parameter with NULL:
	TRANSITION_BODY(Halt, NULL);
}

bool MotorNew::HaltAllowed()
{
	// The TRANSITION_CHECK_BODY must take only the event name as the
	// parameter.
	TRANSITION_CHECK_BODY(Halt);
}


// state machine sits here when motor is not running
STATE_DEFINE(MotorNew, Idle, NoEventData)
{
	cout << "MotorNew::ST_Idle" << endl;
}

// stop the motor 
STATE_DEFINE(MotorNew, Stop, NoEventData)
{
	cout << "MotorNew::ST_Stop" << endl;
	m_currentSpeed = 0; 

	// The transition legality checks are ordinary functions and
	// can be calld within the state body as well as outside of it.
	cout << "Is \"Halt\" Allowed here? " << HaltAllowed() << endl;
	cout << "Is \"SetSpeed\" Allowed here? " << SetSpeedAllowed() << endl;

	// perform the stop motor processing here
	// transition to Idle via an internal event
	InternalEvent(ST_IDLE);
}

// start the motor going
STATE_DEFINE(MotorNew, Start, MotorNewData)
{
	cout << "MotorNew::ST_Start : Speed is " << data->speed << endl;
	m_currentSpeed = data->speed;

	// set initial motor speed processing here
}

// changes the motor speed once the motor is moving
STATE_DEFINE(MotorNew, ChangeSpeed, MotorNewData)
{
	cout << "MotorNew::ST_ChangeSpeed : Speed is " << data->speed << endl;
	m_currentSpeed = data->speed;

	// perform the change motor speed to data->speed here
}
