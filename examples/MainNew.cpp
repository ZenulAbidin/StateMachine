#include "stdafx.h"
#define USE_NEW_TRANSITIONS
#include "MotorNew.h"

using namespace std;

int main(void)
{
	// @see StateMachine.h comments
#if EXTERNAL_EVENT_NO_HEAP_DATA
	// Create Motor object with new transitions
	MotorNew motor;

	MotorNewData data;
	data.speed = 100;
	motor.SetSpeed(&data);

	MotorNewData data2;
	data2.speed = 200;
	motor.SetSpeed(&data2);

	motor.Halt();
	motor.Halt();
#else
	// Create Motor object with new transitions
	MotorNew motor;

	MotorNewData* data = new MotorNewData();
	data->speed = 100;
	motor.SetSpeed(data);

	MotorNewData* data2 = new MotorNewData();
	data2->speed = 200;
	motor.SetSpeed(data2);

	motor.Halt();
	motor.Halt();
#endif

}

