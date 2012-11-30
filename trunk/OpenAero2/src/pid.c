//***********************************************************
//* pid.c
//***********************************************************

//***********************************************************
//* Includes
//***********************************************************

#include <avr/io.h>
#include <stdbool.h>
#include <util/delay.h>
#include "..\inc\io_cfg.h"
#include "..\inc\gyros.h"
#include "..\inc\main.h"
#include "..\inc\init.h"
#include "..\inc\acc.h"
#include "..\inc\imu.h"
#include "..\inc\rc.h"

//************************************************************
// Defines
//************************************************************

#define GYRO_DEADBAND	5			// Region where no gyro input is added to I-term

//************************************************************
// Notes
//************************************************************
//
// Servo output range is 2500 to 5000, centered on 3750.
// RC and PID values are added to this then rescaled at the the output stage to 1000 to 2000.
// As such, the maximum usable value that the PID section can output is +/-1250.
// So working backwards, prior to rescaling (/64) the max values are +/-80,000.
// Prior to this, PID_Gyro_I_actual has been divided by 32 so the values are now +/- 2,560,000
// however the I-term gain can be up to 127 which means the values are now limited to +/-20,157 for full scale authority.
// For reference, a constant gyro value of 50 would go full scale in about 1 second at max gain of 127 if incremented at 400Hz.
// This seems about right for heading hold usage.

//************************************************************
// Prototypes
//************************************************************

void Calculate_PID(void);

//************************************************************
// Code
//************************************************************

// PID globals
int16_t PID_Gyros[3];
int16_t PID_ACCs[3];
int32_t	IntegralGyro[3];					// PID I-terms (gyro) for each axis

void Calculate_PID(void)
{
	static 	int32_t currentError[4];		// Used with lastError to keep track of D-Terms in PID calculations
	static	int32_t lastError[4];

	int16_t DifferentialGyro;				// Holds difference between last two errors (angular acceleration)
	int32_t PID_gyro_temp;
	int32_t PID_acc_temp;
	int32_t PID_Gyro_I_temp = 0;			// Temporary i-terms bound to max throw
	int32_t PID_Gyro_I_actual = 0;			// Actual unbound i-terms
	int8_t	axis;
	int8_t	RCinputsAxis[3] = {AILERON, ELEVATOR, RUDDER}; 	// Cross-ref for actual RCinput elements
	int8_t	RCinputsFlap[3] = {Config.FlapChan, NOCHAN, NOCHAN}; // Offset for handling flaperon

	// Initialise arrays with gain values. Cludgy fix to reduce code space
	int8_t 	P_gain[3] = {Config.Roll.P_mult, Config.Pitch.P_mult, Config.Yaw.P_mult};
	int8_t 	I_gain[3] = {Config.Roll.I_mult, Config.Pitch.I_mult, Config.Yaw.I_mult};
	int8_t 	D_gain[3] = {Config.Roll.D_mult, Config.Pitch.D_mult, Config.Yaw.D_mult};
	int8_t 	L_gain[2] = {Config.A_Roll_P_mult, Config.A_Pitch_P_mult};

	int16_t	RCinputOffset = 0;

	//************************************************************
	// Increment and limit I-terms, handle heading hold modes
	//************************************************************

	for (axis = 0; axis <= YAW; axis ++)
	{
		if (Stability)
		{
			// For 3D mode, change neutral with sticks
			if (Config.AutoCenter == FIXED)
			{
				// Reset the I-terms when you need to set the I-term with RC
				// Note that the I-term is not constrained when no RC input is present.
				if (RCinputs[RCinputsAxis[axis]] != 0)
				{
					if (IntegralGyro[axis] > Config.Raw_I_Constrain[axis])
					{
						IntegralGyro[axis] = Config.Raw_I_Constrain[axis];
					}
					if (IntegralGyro[axis] < -Config.Raw_I_Constrain[axis])
					{
						IntegralGyro[axis] = -Config.Raw_I_Constrain[axis];
					}

					// Adjust I-term with RC input (scaled down by 8)
					IntegralGyro[axis] += (RCinputs[RCinputsAxis[axis]] >> 3); 
				}
			}	

			// Reduce Gyro drift noise before adding into I-term
			if ((gyroADC[axis] > GYRO_DEADBAND) || (gyroADC[axis] < -GYRO_DEADBAND)) 
			{
				IntegralGyro[axis] += gyroADC[axis]; 
			}

			// Handle auto-centering of I-terms in Auto mode
			// If no significant gyro input and IntegralGyro[axis] is non-zero, pull it back slowly.
			else if (Config.AutoCenter == AUTO)
			{
				if (IntegralGyro[axis] > 0)
				{
					IntegralGyro[axis] --;
				}
				else if (IntegralGyro[axis] < 0)
				{	
					IntegralGyro[axis] ++;
				}
			}		
		}

		//************************************************************
		// Calculate PID
		//************************************************************
		// Error calculation - gyro
		// We have to make sure fixed offsets (like flaperons) are taken into account
		// and not seen as error info into the PID loop
		if (RCinputsFlap[axis] != NOCHAN)
		{
			// Check to see if there is an offset for this axis
			RCinputOffset = RCinputs[RCinputsAxis[axis]];
		}
		else
		{
			RCinputOffset = 0;
		}

		// Note that gyro polarity always opposes RC so we add here to get the difference 
		// Also, remove any fixed offset (flaperon channel)
		// When also in Autolevel, don't use RC input as it will fight that of the Autolevel
		if (!AutoLevel)
		{
			currentError[axis] = gyroADC[axis] + ((RCinputs[RCinputsAxis[axis]] + RCinputOffset) >> 1);	
		}
		else
		{
			currentError[axis] = gyroADC[axis];
		}

		// Gyro P-term
		PID_gyro_temp = currentError[axis] * P_gain[axis];			// Multiply P-term (Max gain of 127)
		PID_gyro_temp = PID_gyro_temp * (int32_t)3;					// Multiply by 3

		// Gyro I-term
		PID_Gyro_I_actual = IntegralGyro[axis] * I_gain[axis];		// Multiply I-term (Max gain of 127)
		PID_Gyro_I_actual = PID_Gyro_I_actual >> 5;					// Divide by 32

		// Gyro D-term
		DifferentialGyro = (int16_t)(currentError[axis] - lastError[axis]);
		lastError[axis] = currentError[axis];
		DifferentialGyro *= D_gain[axis];							// Multiply D-term by up to 127
		DifferentialGyro = DifferentialGyro << 4;					// Multiply by 16

		// Autolevel mode (Use IMU to calculate attitude) for roll and pitch only
		if (AutoLevel && (axis < YAW)) 
		{
			// Process requested angle. angle[] is in degrees, but RC is +/-1000 steps
			PID_acc_temp = ((RCinputs[RCinputsAxis[axis]] + RCinputOffset) >> 4); 	// 1000 = 62.5 degrees max

			// Limit maximum angle to that stored in Config.A_Limits ("Max:" in Autolevel menu)
			if (PID_acc_temp > Config.A_Limits)
			{
				PID_acc_temp = Config.A_Limits;
			}
			if (PID_acc_temp < -Config.A_Limits)
			{
				PID_acc_temp = -Config.A_Limits;
			}

			// Acc P terms and error calculation
			PID_acc_temp = PID_acc_temp + angle[axis];
			PID_acc_temp *= L_gain[axis];							// P-term of accelerometer (Max gain of 127)

			PID_ACCs[axis] = (int16_t)(PID_acc_temp >> 2);			// Accs need much less scaling
		}

		// I-term output limits. Maximum 125% limit is full servo throw 
		if (PID_Gyro_I_actual > Config.Raw_I_Limits[axis]) 
		{
			PID_Gyro_I_temp = Config.Raw_I_Limits[axis];
		}
		else if (PID_Gyro_I_actual < -Config.Raw_I_Limits[axis]) 
		{
			PID_Gyro_I_temp = -Config.Raw_I_Limits[axis];	
		}
		else
		{
			PID_Gyro_I_temp = PID_Gyro_I_actual;
		}

		// Sum Gyro P and D terms and rescale	
		PID_Gyros[axis] = (int16_t)((PID_gyro_temp + PID_Gyro_I_temp + DifferentialGyro) >> 6);
	}
}