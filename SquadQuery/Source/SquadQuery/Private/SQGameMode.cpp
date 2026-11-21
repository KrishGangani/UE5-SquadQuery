// P07 SquadQuery (demo) - the game mode.

#include "SQGameMode.h"

ASQGameMode::ASQGameMode()
{
	// DefaultPawnClass is a class property, not a config one (GameModeBase.h:106-108), hence this subclass.
	DefaultPawnClass = nullptr;
}
