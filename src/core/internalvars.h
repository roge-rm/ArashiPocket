/* Variables the game reads from the Arashi Script, from InternalVars.h by
   Juri Munkki. The values are the tokens the prescript (TEXT 2128) gives
   them by naming them in this order, so the order must not change. */
#ifndef CORE_INTERNALVARS_H
#define CORE_INTERNALVARS_H

#include "parser.h"

enum {
	kConstDEPTH = firstVariable,
	kVarLevelNumber,
	kVarLevelField,
	kVarLevelColor,
	kVarLevelBonus,
	kVarLevelSTBonus,
	kVarLevelNext,

	kVarPlayerMaxMove,
	kVarPlayerSuperZaps,

	kVarShotSpeed,
	kVarShotPower,
	kVarShotColor_0_,
	kVarShotColor_1_,
	kVarShotColor_2_,
	kVarShotColor_3_,
	kVarShotColor_4_,

	kVarFlipperPoints,
	kVarFlipperCount,
	kVarFlipperProb,
	kVarFlipperSpeed,
	kVarFlipperRot,
	kVarFlipperColor,

	kVarTank_0_points,
	kVarTank_0_count,
	kVarTank_0_prob,
	kVarTank_0_speed,
	kVarTank_0_color,

	kVarTank_1_points,
	kVarTank_1_count,
	kVarTank_1_prob,
	kVarTank_1_speed,
	kVarTank_1_color,

	kVarTank_2_points,
	kVarTank_2_count,
	kVarTank_2_prob,
	kVarTank_2_speed,
	kVarTank_2_color,

	kVarFusePoints,
	kVarFuseBullseye,
	kVarFuseCount,
	kVarFuseProb,
	kVarFuseWarpP,
	kVarFusePlayerPlus,
	kVarFuseColor_0_,
	kVarFuseColor_1_,
	kVarFuseColor_2_,
	kVarFuseColor_3_,
	kVarFuseColor_4_,

	kVarPulsarPoints,
	kVarPulsarCount,
	kVarPulsarProb,
	kVarPulsarSpeed,
	kVarPulsarRot,
	kVarPulsarColor,
	kVarPulsarTime,
	kVarPulsarPulsDepth,

	kVarSpikerPoints,
	kVarSpikerProb,
	kVarSpikerSpeed,
	kVarSpikerStart,
	kVarSpikerTop,
	kVarSpikerPlasma,
	kVarSpikerPlSpeed,
	kVarSpikerPlPoints,
	kVarSpikerSpikePoints,
	kVarSpikerColor_0_,
	kVarSpikerColor_1_,
	kVarSpikerColor_2_,

	kVarBoredomCount,
	kVarBoredProb,
	kVarEndTimer,

	kVarAllowPracticeRestart,
	kVarAllowArcadeRestart,
	kVarShowSpikesMsg,
	kVarDoomsday,

	kVarLast = kVarDoomsday
};

#endif
