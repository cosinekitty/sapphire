/* ========================================
 *  Galactic - Galactic.h
 *  Copyright (c) 2016 airwindows, Airwindows uses the MIT license
 * ======================================== */

#ifndef __Galactic_H
#include "Galactic.h"
#endif

Galactic::Galactic()
{
	A = 0.5;
	B = 0.5;
	C = 0.5;
	D = 1.0;
	E = 1.0;

	iirAL = 0.0; iirAR = 0.0;
	iirBL = 0.0; iirBR = 0.0;

	for(int count = 0; count < 6479; count++) {aIL[count] = 0.0;aIR[count] = 0.0;}
	for(int count = 0; count < 3659; count++) {aJL[count] = 0.0;aJR[count] = 0.0;}
	for(int count = 0; count < 1719; count++) {aKL[count] = 0.0;aKR[count] = 0.0;}
	for(int count = 0; count < 679; count++) {aLL[count] = 0.0;aLR[count] = 0.0;}

	for(int count = 0; count < 9699; count++) {aAL[count] = 0.0;aAR[count] = 0.0;}
	for(int count = 0; count < 5999; count++) {aBL[count] = 0.0;aBR[count] = 0.0;}
	for(int count = 0; count < 2319; count++) {aCL[count] = 0.0;aCR[count] = 0.0;}
	for(int count = 0; count < 939; count++) {aDL[count] = 0.0;aDR[count] = 0.0;}

	for(int count = 0; count < 15219; count++) {aEL[count] = 0.0;aER[count] = 0.0;}
	for(int count = 0; count < 8459; count++) {aFL[count] = 0.0;aFR[count] = 0.0;}
	for(int count = 0; count < 4539; count++) {aGL[count] = 0.0;aGR[count] = 0.0;}
	for(int count = 0; count < 3199; count++) {aHL[count] = 0.0;aHR[count] = 0.0;}

	for(int count = 0; count < 3110; count++) {aML[count] = aMR[count] = 0.0;}

	feedbackAL = 0.0; feedbackAR = 0.0;
	feedbackBL = 0.0; feedbackBR = 0.0;
	feedbackCL = 0.0; feedbackCR = 0.0;
	feedbackDL = 0.0; feedbackDR = 0.0;

	for(int count = 0; count < 6; count++) {lastRefL[count] = 0.0;lastRefR[count] = 0.0;}

	thunderL = 0; thunderR = 0;

	countI = 1;
	countJ = 1;
	countK = 1;
	countL = 1;

	countA = 1;
	countB = 1;
	countC = 1;
	countD = 1;

	countE = 1;
	countF = 1;
	countG = 1;
	countH = 1;
	countM = 1;
	//the predelay
	cycle = 0;

	vibM = 3.0;

	oldfpd = 429496.7295;

	// Don Cross: make deterministic for unit tests
	fpdL = 2756923396;
	fpdR = 2341963165;
}

void Galactic::setParameter(int index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Galactic::getParameter(int index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        case kParamE: return E; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}
