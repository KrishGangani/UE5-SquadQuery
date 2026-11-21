// P07 SquadQuery (demo) - one squad member: a character that walks where its brain tells it.

#pragma once

#include "GameFramework/Character.h"
#include "SQAgent.generated.h"

class USQAgentBrainComponent;
class UStaticMeshComponent;

/**
 * The agent of every configuration. Deliberately thin: movement comes from the engine (character movement + an AI
 * controller following navmesh paths), decisions come from the brain component. The same class in every configuration,
 * so movement cost is identical and only the decision method differs (02 section 9).
 */
UCLASS()
class ASQAgent : public ACharacter
{
	GENERATED_BODY()

public:
	ASQAgent();

	USQAgentBrainComponent* GetBrain() const { return Brain; }

	/** Walking speed (cm/s): a jog. Time-to-cover depends on it, so it is fixed for every configuration. */
	static constexpr float WalkSpeedCmPerSecond = 450.f;

private:
	/** A cylinder roughly the capsule's size, so the agent is visible; no collision (the capsule collides). */
	UPROPERTY(VisibleAnywhere, Category = "Agent")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Agent")
	TObjectPtr<USQAgentBrainComponent> Brain;
};
