// P07 SquadQuery (demo) - the game mode: the default game mode without a pawn for the player.

#pragma once

#include "GameFramework/GameModeBase.h"
#include "SQGameMode.generated.h"

/**
 * AGameModeBase with no default pawn. The player only watches through the director's fixed camera; the engine default
 * (ADefaultPawn) would put a colliding pawn at the map's PlayerStart, among the agents, and possessing it would take the
 * view target back. With no pawn class the engine skips the spawn without logging anything (GameModeBase.cpp:1292-1305;
 * Controller.cpp:115-118). Set as GlobalDefaultGameMode in DefaultEngine.ini.
 */
UCLASS()
class ASQGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASQGameMode();
};
