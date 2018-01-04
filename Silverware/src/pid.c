/*
The MIT License (MIT)

Copyright (c) 2016 silverx

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/


//#define RECTANGULAR_RULE_INTEGRAL
//#define MIDPOINT_RULE_INTEGRAL
#define SIMPSON_RULE_INTEGRAL


//#define NORMAL_DTERM
//#define NEW_DTERM
#define MAX_FLAT_LPF_DIFF_DTERM

//#define ANTI_WINDUP_DISABLE

#include <stdbool.h>
#include "pid.h"
#include "util.h"
#include "config.h"

#include "defines.h"

// current pid tuning by NotFastEnuf
// for eachine E011

// NotFastEnuf Note:  I feel like P range is acceptable from 19.5 to 26.5
//																D range is acceptable from 5.7 to 11.3. 
//																If you get a random bumpy FPV ride without wind - try lowering D First then consider lowering P in balance


// Kp	                  ROLL       PITCH     YAW
float pidkp[PIDNUMBER] = { 13.0e-2 , 13.0e-2  , 6e-1 }; 

// Ki		              ROLL       PITCH     YAW
float pidki[PIDNUMBER] = { 8.8e-1  , 8.8e-1 , 3e-1 };	

// Kd			          ROLL       PITCH     YAW
float pidkd[PIDNUMBER] = { 5.5e-1 , 5.5e-1  , 5.0e-1 };	


// NotFastEnuf Note:  Increasing setpoint weight slows the craft's response to the sticks during short "manuvers" or stick inputs like a flip or a roll.
//										Increasing this will slow the initiation of rotation on stick command but will also decelerate faster towards termination of stick command.
//										Slowing the termination of stick command means the rotation rate will slow down at the end of a flip faster and help
//										prevent bounceback without needing very high D gains.
//										AT CURRENT TUNING:  I consider the adjustable reange of this to be from 0 to .1
//										where 0 to .5 feels like a racer, and approaching .1 feels more freestyle
// "setpoint weighting" 0.0 - 1.0 where 0.0 = normal pid
//						ROLL   PITCH   YAW
float b[3] = { 0.0 , 0.0 , 0.0};




// output limit			
const float outlimit[PIDNUMBER] = { 0.8 , 0.8 , 0.5 };

// limit of integral term (abs)
const float integrallimit[PIDNUMBER] = { 0.8 , 0.8 , 0.5 };





// non changable things below
float ierror[PIDNUMBER] = { 0 , 0 , 0};	

float pidoutput[PIDNUMBER];

extern float error[PIDNUMBER];
static float lasterror[PIDNUMBER];

extern float looptime;
extern float gyro[3];
extern int onground;

extern float looptime;


#ifdef NORMAL_DTERM
static float lastrate[PIDNUMBER];
#endif

#ifdef NEW_DTERM
static float lastratexx[PIDNUMBER][2];
#endif

#ifdef MAX_FLAT_LPF_DIFF_DTERM
static float lastratexx[PIDNUMBER][4];
#endif

#ifdef SIMPSON_RULE_INTEGRAL
static float lasterror2[PIDNUMBER];
#endif

float timefactor;

void pid_precalc()
{
	timefactor = 0.0032f / looptime;
}

float pid(int x )
{ 

        if (onground) 
				{
           ierror[x] *= 0.8f;
				}
	
				int iwindup = 0;
				if (( pidoutput[x] == outlimit[x] )&& ( error[x] > 0) )
				{
					iwindup = 1;		
				}
				if (( pidoutput[x] == -outlimit[x])&& ( error[x] < 0) )
				{
					iwindup = 1;				
				}              
                #ifdef ANTI_WINDUP_DISABLE
                iwindup = 0;
                #endif
        if ( !iwindup)
				{
				#ifdef MIDPOINT_RULE_INTEGRAL
				 // trapezoidal rule instead of rectangular
         ierror[x] = ierror[x] + (error[x] + lasterror[x]) * 0.5f *  pidki[x] * looptime;
				 lasterror[x] = error[x];
				#endif
					
				#ifdef RECTANGULAR_RULE_INTEGRAL
				 ierror[x] = ierror[x] + error[x] *  pidki[x] * looptime;
				 lasterror[x] = error[x];					
				#endif
					
				#ifdef SIMPSON_RULE_INTEGRAL
					// assuming similar time intervals
				 ierror[x] = ierror[x] + 0.166666f* (lasterror2[x] + 4*lasterror[x] + error[x]) *  pidki[x] * looptime;	
					lasterror2[x] = lasterror[x];
					lasterror[x] = error[x];
					#endif
					
				}
				
				limitf( &ierror[x] , integrallimit[x] );
				
				// P term
                pidoutput[x] = error[x] * ( 1 - b[x])* pidkp[x] ;
				
				// b
                pidoutput[x] +=  - ( b[x])* pidkp[x] * gyro[x]  ;
				
				
				// I term	
				pidoutput[x] += ierror[x];
			
				// D term		  

				#ifdef NORMAL_DTERM
					pidoutput[x] = pidoutput[x] - (gyro[x] - lastrate[x]) * pidkd[x] * timefactor  ;
					lastrate[x] = gyro[x];
				#endif

                #ifdef NEW_DTERM
                    pidoutput[x] = pidoutput[x] - ( ( 0.5f) *gyro[x] 
                                - (0.5f) * lastratexx[x][1] ) * pidkd[x] * timefactor  ;
                                    
                    lastratexx[x][1] = lastratexx[x][0];
                    lastratexx[x][0] = gyro[x];
               #endif
			
               #ifdef MAX_FLAT_LPF_DIFF_DTERM 
					pidoutput[x] = pidoutput[x] - ( + 0.125f *gyro[x] + 0.250f * lastratexx[x][0]
								- 0.250f * lastratexx[x][2] - ( 0.125f) * lastratexx[x][3]) * pidkd[x] * timefactor 						;
				
					lastratexx[x][3] = lastratexx[x][2];
					lastratexx[x][2] = lastratexx[x][1];
					lastratexx[x][1] = lastratexx[x][0];
					lastratexx[x][0] = gyro[x];
				#endif            

				  limitf(  &pidoutput[x] , outlimit[x]);



return pidoutput[x];		 		
}


