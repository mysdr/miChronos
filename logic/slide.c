// (C) Jacko edited to turn into a slide rule LINE2
// *************************************************************************************************
//
//      Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/
//
//
//        Redistribution and use in source and binary forms, with or without
//        modification, are permitted provided that the following conditions
//        are met:
//
//          Redistributions of source code must retain the above copyright
//          notice, this list of conditions and the following disclaimer.
//
//          Redistributions in binary form must reproduce the above copyright
//          notice, this list of conditions and the following disclaimer in the
//          documentation and/or other materials provided with the
//          distribution.
//
//          Neither the name of Texas Instruments Incorporated nor the names of
//          its contributors may be used to endorse or promote products derived
//          from this software without specific prior written permission.
//
//        THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//        "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//        LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//        A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//        OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//        SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//        LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//        DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//        THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//        (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//        OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// *************************************************************************************************
// Time functions.
// *************************************************************************************************

// *************************************************************************************************
// Include section

// system
#include "project.h"

// driver
#include "ports.h"
#include "display.h"

// logic
#include "menu.h"
#include "slide.h"
#include "user.h"

// *************************************************************************************************
// Prototypes section
void mx_slide();
void sx_slide();

// *************************************************************************************************
// Defines section

// *************************************************************************************************
#define INV_LOG_E100  3643126
#define FOUR_INV_PI		21361414

/* Functions available, scaled if necessary, and to 4 digits.

IR - Inverse Root 	1/sqrt(x)
SQ - Square			x*x
IV - Inverse 		1/x
RT - Root 		sqrt(x)
RL - Relativity	sqrt(1-x*x)
BD - Bend 		x/(1+sqrt(1+x*x))
LG - Log		log(x)		scaled to provide maximum at 100
AT - Arc Tangent	atan(x)		scaled to provide maximum for 45 degrees

*/

float square(float x) {
	return x * x;
}

/* use initial estimate and y'=y*(3-x*y*y)/2 with iterations */
float irt(float x) {
    	// Watchdog triggers after 16 seconds when not cleared
#ifdef USE_WATCHDOG
    	WDTCTL = WDTPW + WDTIS__512K + WDTSSEL__ACLK;
#else
    	WDTCTL = WDTPW + WDTHOLD;
#endif
	u32 i;
        float x2;
        const float threehalfs = 1.5F;
	x2 = x * 0.5F;
        i  = * ( long * ) &x;                       // evil floating point bit level hacking
        i  = 0x5f3759df - ( i >> 1 );
        x  = * ( float * ) &i;
	for(i = 0; i < 4; i++)
        	x  *= ( threehalfs - ( x2 * x * x ) );   //iteration
        return x;
}

float inv(float x) {
	x = irt(x);
	return square(x);
}

float sqrt(float x) {
	return x * irt(x);
}

float bend(float x) {		/* x/(1+sqrt(1+x*x)) */
	return x * inv(1.0F+sqrt(1.0F+square(x)));
}

float atanh(float x, s8 i2) {
	float acc = x;
	float mul = x;
	x = square(x);
	x = i2 < 0 ? -x : x;
	u8 i;
	for(i = 3;i < 64;i+=2) {
			mul = mul * x;
            acc += mul * inv(i);
        }
	return acc;
}

float log(float x) { //base e
	x = irt(irt(irt(x)));//symetry and double triple roots
	return -atanh((x-1.0F) * inv(x+1.0F), 1) * 16.0F;
}

float atan(float x) {
	return atanh(bend(x), -1) * 2.0F;
}

float rel(float x) {
	if(x <= 0) return 0;
	return sqrt(1.0F-square(x));
}

const u8 named_calc[][4] = { 	"SQRE", "INRT", " INV", "ROOT",
								" REL", "BEND", "LOGS", "ATAN" };

const float pre_scale[] = { 		0.0001F, 1.0F, 1.0F, 1.0F,
								0.0001F, 0.0001F, 1.0F, 0.0001F};

const float scale[] = { 			10000.0F, 10000.0F, 10000.0F, 100.0F,
								10000.0F, 10000.0F, 1.08573620476e+3F, 12732.3954474F };

u8 fn_calc = 6;
s32 in_calc = 5000;
s32 out_calc = 0;

void calc_slide() {
	float out = (float)in_calc;
	out *= pre_scale[fn_calc];//suitable range
	switch(fn_calc) {
	case 0: out = square(out); break;
	case 1: out = irt(out); break;
	case 2: out = inv(out); break;
	case 3: out = sqrt(out); break;
	case 4: out = rel(out); break;
	case 5: out = bend(out); break;
	case 6: out = log(out); break;
	case 7: out = atan(out); break;
	}
	out *= scale[fn_calc];// 0 to 10000
	out_calc = (u16)out;
	if(out - (float)out_calc >= 0.5F) out_calc++;
	if(out_calc > 9999) out_calc = 9999;
}

void mx_slide()
{
    clear_display_all();
    display_slide(0);//fill in
    // Loop values until all are set or user breaks set
    while (1)
    {
        // Idle timeout: exit
        if (sys.flag.idle_timeout)
        {
            break;
        }

        // Button STAR (short): exit
        if (button.flag.star)
        {
            break;
        }

        set_value(&in_calc, 4, 0, 1, 10000, SETVALUE_ROLLOVER_VALUE + SETVALUE_DISPLAY_VALUE +
                          SETVALUE_NEXT_VALUE + SETVALUE_FAST_MODE, LCD_SEG_L2_3_0,
                          display_value);
        calc_slide();
        //and the result
       	display_chars(LCD_SEG_L1_3_0, int_to_array(out_calc, 4, 0), SEG_ON);
    }

    // Clear button flags
    button.all_flags = 0;
}

void sx_slide()
{
    fn_calc = (++fn_calc)&7;//fixed
    display_slide(0);
}

void display_slide(u8 update)
{
	display_chars(LCD_SEG_L2_3_0, (u8 *)named_calc[fn_calc], SEG_ON);
}