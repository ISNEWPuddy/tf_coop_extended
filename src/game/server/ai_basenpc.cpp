//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"

#include "ai_basenpc.h"
#include "fmtstr.h"
#include "activitylist.h"
#include "animation.h"
#include "basecombatweapon.h"
#include "soundent.h"
#include "decals.h"
#include "entitylist.h"
#include "eventqueue.h"
#include "entityapi.h"
#include "bitstring.h"
#include "gamerules.h"		// For g_pGameRules
#include "scripted.h"
#include "worldsize.h"
#include "game.h"
#include "shot_manipulator.h"
#include "ai_interactions.h"
#include "ai_network.h"
#include "ai_networkmanager.h"
#include "ai_pathfinder.h"
#include "ai_node.h"
#include "ai_default.h"
#include "ai_schedule.h"
#include "ai_task.h"
#include "ai_hull.h"
#include "ai_moveprobe.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_senses.h"
#include "ai_squadslot.h"
#include "ai_memory.h"
#include "ai_squad.h"
#include "ai_localnavigator.h"
#include "ai_tacticalservices.h"
#include "ai_behavior.h"
#include "ai_dynamiclink.h"
#include "AI_Criteria.h"
#include "basegrenade_shared.h"
#include "ammodef.h"
#include "player.h"
#include "sceneentity.h"
#include "ndebugoverlay.h"
#include "mathlib/mathlib.h"
#include "bone_setup.h"
#include "IEffects.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "tier1/strtools.h"
#include "doors.h"
#include "BasePropDoor.h"
#include "saverestore_utlvector.h"
#include "npcevent.h"
#include "movevars_shared.h"
#include "te_effect_dispatch.h"
#include "globals.h"
#include "saverestore_bitstring.h"
#include "checksum_crc.h"
#include "iservervehicle.h"
#include "filters.h"
#if defined (HL2_DLL) || defined (TF_CLASSIC)
#include "npc_bullseye.h"
//#include "hl2_player.h"
#include "weapon_physcannon.h"
#endif
#include "waterbullet.h"
#include "in_buttons.h"
#include "eventlist.h"
#include "globalstate.h"
#include "physics_prop_ragdoll.h"
#include "vphysics/friction.h"
#include "physics_npc_solver.h"
#include "tier0/vcrmode.h"
#include "death_pose.h"
#include "datacache/imdlcache.h"
#include "vstdlib/jobthread.h"

#ifdef HL2_EPISODIC
#include "npc_alyx_episodic.h"
#endif

#ifdef PORTAL
	#include "prop_portal_shared.h"
	#include "portal_util_shared.h"
#endif

#ifdef USE_NAV_MESH
#include "nav_mesh.h"
#endif // USE_NAV_MESH

#include "env_debughistory.h"
#include "collisionutils.h"

extern ConVar sk_healthkit;

#ifdef TF_CLASSIC
extern ConVar tf_boost_drain_time;
extern ConVar tf_invuln_time;
extern ConVar tf_damage_disablespread;
extern ConVar tf_damage_range;
extern ConVar tf_damage_lineardist;
extern ConVar tf_damage_events_track_for;
extern ConVar tf_airblast_cray_ground_minz;
extern ConVar tf_airblast_cray_ground_reflect;
#endif

// dvs: for opening doors -- these should probably not be here
#include "ai_route.h"
#include "ai_waypoint.h"

#include "utlbuffer.h"
#include "gamestats.h"

#ifdef TF_CLASSIC
#include "tf_weaponbase.h"
#include "tf_team.h"
#include "tf_obj.h"
#include "tf_gamerules.h"
#include "tf_weapon_medigun.h"
#include "triggers.h"
#include "tf_gamestats.h"
#include "tf_shareddefs.h"
#include "entity_capture_flag.h"
#include "NavMeshEntities/func_nav_prerequisite.h"
#include "tf_weaponbase_decapitation.h"
#include "dt_utlvector_send.h"
#include "ilagcompensationmanager.h"
#include "func_capture_zone.h"
#include "nav_mesh/tf_nav_mesh.h"
#include "tf_weapon_buff_item.h"
#include "tf_wearable.h"
#include "tf_ammo_pack.h"
#include "entity_rune.h"
#include "gib.h"
#include "tf_weapon_sniperrifle.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef __clang__
	// These clang 3.1 warnings don't seem very useful, and cannot easily be
	// avoided in this file.
	#pragma GCC diagnostic ignored "-Wdangling-else"	// warning: add explicit braces to avoid dangling else [-Wdangling-else]
#endif

//#define DEBUG_LOOK

bool RagdollManager_SaveImportant( CAI_BaseNPC *pNPC );

#define	MIN_PHYSICS_FLINCH_DAMAGE	5.0f

#define	NPC_GRENADE_FEAR_DIST		200
#define	MAX_GLASS_PENETRATION_DEPTH	16.0f

#define FINDNAMEDENTITY_MAX_ENTITIES	32		// max number of entities to be considered for random entity selection in FindNamedEntity

#ifdef TF_CLASSIC
#define NPC_DAMAGE_FORCE_SCALE_SELF				9
#define MAX_NPC_DAMAGE_EVENTS		128
extern ConVar tf_damageforcescale_other;
extern ConVar	tf_double_donk_window;

#endif

extern bool			g_fDrawLines;
extern short		g_sModelIndexLaser;		// holds the index for the laser beam
extern short		g_sModelIndexLaserDot;	// holds the index for the laser beam dot

// Debugging tools
ConVar	ai_no_select_box( "ai_no_select_box", "0" );

ConVar	ai_show_think_tolerance( "ai_show_think_tolerance", "0" );
ConVar	ai_debug_think_ticks( "ai_debug_think_ticks", "0" );
ConVar	ai_debug_doors( "ai_debug_doors", "0" );
ConVar  ai_debug_enemies( "ai_debug_enemies", "0" );

ConVar	ai_rebalance_thinks( "ai_rebalance_thinks", "1" );
ConVar	ai_use_efficiency( "ai_use_efficiency", "1" );
ConVar	ai_use_frame_think_limits( "ai_use_frame_think_limits", "1" );
ConVar	ai_default_efficient( "ai_default_efficient", ( IsX360() ) ? "1" : "0" );
ConVar	ai_efficiency_override( "ai_efficiency_override", "0" );
ConVar	ai_debug_efficiency( "ai_debug_efficiency", "0" );
ConVar	ai_debug_dyninteractions( "ai_debug_dyninteractions", "0", FCVAR_NONE, "Debug the NPC dynamic interaction system." );
ConVar	ai_frametime_limit( "ai_frametime_limit", "50", FCVAR_NONE, "frametime limit for min efficiency AIE_NORMAL (in sec's)." );

ConVar	ai_use_think_optimizations( "ai_use_think_optimizations", "1" );

ConVar	ai_test_moveprobe_ignoresmall( "ai_test_moveprobe_ignoresmall", "0" );

#ifdef HL2_EPISODIC
extern ConVar ai_vehicle_avoidance;
#endif // HL2_EPISODIC

#ifndef _RETAIL
#define ShouldUseEfficiency()			( ai_use_think_optimizations.GetBool() && ai_use_efficiency.GetBool() )
#define ShouldUseFrameThinkLimits()		( ai_use_think_optimizations.GetBool() && ai_use_frame_think_limits.GetBool() )
#define ShouldRebalanceThinks()			( ai_use_think_optimizations.GetBool() && ai_rebalance_thinks.GetBool() )
#define ShouldDefaultEfficient()		( ai_use_think_optimizations.GetBool() && ai_default_efficient.GetBool() )
#else
#define ShouldUseEfficiency()			( true )
#define ShouldUseFrameThinkLimits()		( true )
#define ShouldRebalanceThinks()			( true )
#define ShouldDefaultEfficient()		( true )
#endif

#ifndef _RETAIL
#define DbgEnemyMsg if ( !ai_debug_enemies.GetBool() ) ; else DevMsg
#else
#define DbgEnemyMsg if ( 0 ) ; else DevMsg
#endif

#ifdef DEBUG_AI_FRAME_THINK_LIMITS
#define DbgFrameLimitMsg DevMsg
#else
#define DbgFrameLimitMsg (void)
#endif

// NPC damage adjusters
ConVar	sk_npc_head( "sk_npc_head","2" );
ConVar	sk_npc_chest( "sk_npc_chest","1" );
ConVar	sk_npc_stomach( "sk_npc_stomach","1" );
ConVar	sk_npc_arm( "sk_npc_arm","1" );
ConVar	sk_npc_leg( "sk_npc_leg","1" );
ConVar	showhitlocation( "showhitlocation", "0" );

// Squad debugging
ConVar  ai_debug_squads( "ai_debug_squads", "0" );
ConVar  ai_debug_loners( "ai_debug_loners", "0" );

// Shoot trajectory
ConVar	ai_lead_time( "ai_lead_time","0.0" );
ConVar	ai_shot_stats( "ai_shot_stats", "0" );
ConVar	ai_shot_stats_term( "ai_shot_stats_term", "1000" );
ConVar	ai_shot_bias( "ai_shot_bias", "1.0" );

ConVar	ai_spread_defocused_cone_multiplier( "ai_spread_defocused_cone_multiplier","3.0" );
ConVar	ai_spread_cone_focus_time( "ai_spread_cone_focus_time","0.6" );
ConVar	ai_spread_pattern_focus_time( "ai_spread_pattern_focus_time","0.8" );

ConVar	ai_reaction_delay_idle( "ai_reaction_delay_idle","0.3" );
ConVar	ai_reaction_delay_alert( "ai_reaction_delay_alert", "0.1" );

ConVar ai_strong_optimizations( "ai_strong_optimizations", ( IsX360() ) ? "1" : "0" );
bool AIStrongOpt( void )
{
	return ai_strong_optimizations.GetBool();
}

#ifdef TF_CLASSIC
// Make sure you keep this in sync with the class table in basentity.h!
int g_TFClassTeams[] =
{
	TEAM_UNASSIGNED,	//CLASS_NONE,				
	TEAM_UNASSIGNED,	//CLASS_PLAYER, (Not used)		
	TF_TEAM_RED,	//CLASS_PLAYER_ALLY,
	TF_TEAM_RED,	//CLASS_PLAYER_ALLY_VITAL,
	TF_TEAM_YELLOW,	//CLASS_ANTLION,
	TEAM_UNASSIGNED,	//CLASS_BARNACLE,
	TEAM_UNASSIGNED,	//CLASS_BULLSEYE,
	TEAM_UNASSIGNED,	////CLASS_BULLSQUID,	
	TF_TEAM_RED,	//CLASS_CITIZEN_PASSIVE,	
	TF_TEAM_RED,	//CLASS_CITIZEN_REBEL,
	TF_TEAM_BLUE,	//CLASS_COMBINE,
	TF_TEAM_BLUE,	//CLASS_COMBINE_GUNSHIP,
	TF_TEAM_RED,	//CLASS_CONSCRIPT,
	TF_TEAM_GREEN,	//CLASS_HEADCRAB,
	//TEAM_UNASSIGNED,	////CLASS_HOUNDEYE,
	TF_TEAM_BLUE,	//CLASS_MANHACK,
	TF_TEAM_BLUE,	//CLASS_METROPOLICE,		
	TF_TEAM_BLUE,	//CLASS_MILITARY,		
	TF_TEAM_BLUE,	//CLASS_SCANNER,		
	TF_TEAM_BLUE,	//CLASS_STALKER,		
	TF_TEAM_RED,	//CLASS_VORTIGAUNT,
	TF_TEAM_GREEN,	//CLASS_ZOMBIE,
	TF_TEAM_BLUE,	//CLASS_PROTOSNIPER,
	TEAM_UNASSIGNED,	//CLASS_MISSILE,
	TEAM_UNASSIGNED,	//CLASS_FLARE,
	TEAM_UNASSIGNED,	//CLASS_EARTH_FAUNA,
	TF_TEAM_RED,	//CLASS_HACKED_ROLLERMINE,
	TF_TEAM_BLUE,	//CLASS_COMBINE_HUNTER,
	TF_TEAM_BLUE,	//CLASS_MACHINE_HL1,
	TF_TEAM_RED,	//CLASS_HUMAN_PASSIVE,
	TF_TEAM_BLUE,	//CLASS_HUMAN_MILITARY,
	TF_TEAM_GREEN,	//CLASS_ALIEN_MILITARY,
	TF_TEAM_YELLOW,	//CLASS_ALIEN_MONSTER,
	TF_TEAM_YELLOW,	//CLASS_ALIEN_PREY,
	TF_TEAM_YELLOW,	//CLASS_ALIEN_PREDATOR,
	TEAM_UNASSIGNED,	//CLASS_INSECT,
	TF_TEAM_RED,	//CLASS_PLAYER_BIOWEAPON,
	TF_TEAM_GREEN,	//CLASS_ALIEN_BIOWEAPON,
};
#endif

//-----------------------------------------------------------------------------
//
// Crude frame timings
//

CFastTimer g_AIRunTimer;
CFastTimer g_AIPostRunTimer;
CFastTimer g_AIMoveTimer;

CFastTimer g_AIConditionsTimer;
CFastTimer g_AIPrescheduleThinkTimer;
CFastTimer g_AIMaintainScheduleTimer;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CAI_Manager g_AI_Manager;

//-------------------------------------

CAI_Manager::CAI_Manager()
{
	m_AIs.EnsureCapacity( MAX_AIS );
}

//-------------------------------------

CAI_BaseNPC **CAI_Manager::AccessAIs()
{
	if (m_AIs.Count())
		return &m_AIs[0];
	return NULL;
}

//-------------------------------------

int CAI_Manager::NumAIs()
{
	return m_AIs.Count();
}

//-------------------------------------

void CAI_Manager::AddAI( CAI_BaseNPC *pAI )
{
	m_AIs.AddToTail( pAI );
}

//-------------------------------------

void CAI_Manager::RemoveAI( CAI_BaseNPC *pAI )
{
	int i = m_AIs.Find( pAI );

	if ( i != -1 )
		m_AIs.FastRemove( i );
}


//-----------------------------------------------------------------------------

// ================================================================
//  Init static data
// ================================================================
int					CAI_BaseNPC::m_nDebugBits		= 0;
CAI_BaseNPC*		CAI_BaseNPC::m_pDebugNPC		= NULL;
int					CAI_BaseNPC::m_nDebugPauseIndex	= -1;

CAI_ClassScheduleIdSpace	CAI_BaseNPC::gm_ClassScheduleIdSpace( true );
CAI_GlobalScheduleNamespace CAI_BaseNPC::gm_SchedulingSymbols;
CAI_LocalIdSpace			CAI_BaseNPC::gm_SquadSlotIdSpace( true );

string_t CAI_BaseNPC::gm_iszPlayerSquad;

int		CAI_BaseNPC::gm_iNextThinkRebalanceTick;
float	CAI_BaseNPC::gm_flTimeLastSpawn;
int		CAI_BaseNPC::gm_nSpawnedThisFrame;

CSimpleSimTimer CAI_BaseNPC::m_AnyUpdateEnemyPosTimer;

//
//	Deferred Navigation calls go here
//

CPostFrameNavigationHook g_PostFrameNavigationHook;
CPostFrameNavigationHook *PostFrameNavigationSystem( void )
{
	return &g_PostFrameNavigationHook;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPostFrameNavigationHook::Init( void )
{
	m_Functors.Purge();
	m_bGameFrameRunning = false;
	return true;
}

// Main query job
CJob *g_pQueuedNavigationQueryJob = NULL;

static void ProcessNavigationQueries( CFunctor **pData, unsigned int nCount )
{
	// Run all queued navigation on a separate thread
	for ( int i = 0; i < nCount; i++ )
	{
		(*pData[i])();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPostFrameNavigationHook::FrameUpdatePreEntityThink( void )
{ 
	// If the thread is executing, then wait for it to finish
	if ( g_pQueuedNavigationQueryJob )
	{
		g_pQueuedNavigationQueryJob->WaitForFinishAndRelease();
		g_pQueuedNavigationQueryJob = NULL;
		m_Functors.Purge();
	}
	
	if ( ai_post_frame_navigation.GetBool() == false )
		return;

	SetGrameFrameRunning( true ); 
}

//-----------------------------------------------------------------------------
// Purpose: Now that the game frame has collected all the navigation queries, service them
//-----------------------------------------------------------------------------
void CPostFrameNavigationHook::FrameUpdatePostEntityThink( void )
{
	if ( ai_post_frame_navigation.GetBool() == false )
		return;

	// The guts of the NPC will check against this to decide whether or not to queue its navigation calls
	SetGrameFrameRunning( false );

	// Throw this off to a thread job
	g_pQueuedNavigationQueryJob = ThreadExecute( &ProcessNavigationQueries, m_Functors.Base(), m_Functors.Count() );
}

//-----------------------------------------------------------------------------
// Purpose: Queue up our navigation call
//-----------------------------------------------------------------------------
void CPostFrameNavigationHook::EnqueueEntityNavigationQuery( CAI_BaseNPC *pNPC, CFunctor *pFunctor )
{
	if ( ai_post_frame_navigation.GetBool() == false )
		return;

	m_Functors.AddToTail( pFunctor );
	pNPC->SetNavigationDeferred( true );
}

//
//	Deferred Navigation calls go here
//


// ================================================================
//  Class Methods
// ================================================================

//-----------------------------------------------------------------------------
// Purpose: Static debug function to clear schedules for all NPCS
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CAI_BaseNPC::ClearAllSchedules(void)
{
	CAI_BaseNPC *npc = gEntList.NextEntByClass( (CAI_BaseNPC *)NULL );

	while (npc)
	{
		npc->ClearSchedule( "CAI_BaseNPC::ClearAllSchedules" );
		npc->GetNavigator()->ClearGoal();
		npc = gEntList.NextEntByClass(npc);
	}
}

// ==============================================================================

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::Event_Gibbed( const CTakeDamageInfo &info )
{
	// CLFRagdoll takes care of gibbing.
	//return false;

	bool gibbed = CorpseGib( info );

	if ( gibbed )
	{
		SpawnBlood( EyePosition(), g_vecAttackDir, BloodColor(), info.GetDamage() );
		RemoveDeferred();

		CTFWeaponBase *pKillerWeapon = NULL;
		if ( info.GetWeapon() )
		{
			pKillerWeapon = dynamic_cast<CTFWeaponBase *>( info.GetWeapon() );
		}
		else if ( info.GetAttacker() && info.GetAttacker()->IsPlayer() )
		{
			// Assume that player used his currently active weapon.
			pKillerWeapon = ToTFPlayer( info.GetAttacker() )->GetActiveTFWeapon();
		}

		if ( pKillerWeapon && pKillerWeapon->GetWeaponID() == TF_WEAPON_SNIPERRIFLE_CLASSIC )
		{
			CEffectData	data;
			data.m_nHitBox = GetParticleSystemIndex( "tfc_sniper_mist" );
			data.m_vOrigin = WorldSpaceCenter();
			data.m_vAngles = vec3_angle;
			data.m_nEntIndex = 0;

			CPVSFilter filter( WorldSpaceCenter() );
			te->DispatchEffect( filter, 0.0, data.m_vOrigin, "ParticleEffect", data );
		}
	}
	else
	{
		CorpseFade();
	}

	return gibbed;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::BecomeRagdoll( const CTakeDamageInfo &info, const Vector &forceVector )
{
	if ( ( m_iDamageCustomDeath == TF_DMG_CUSTOM_DECAPITATION ) && HasHumanGibs() )
	{
		CGib::SpawnHeadGib( this );
	}

	return BaseClass::BecomeRagdoll( info, forceVector );

	/*if ( CanBecomeRagdoll() )
	{
		VPhysicsDestroyObject();
		AddSolidFlags( FSOLID_NOT_SOLID );
		m_nRenderFX = kRenderFxRagdoll;

		// Have to do this dance because m_vecForce is a network vector
		// and can't be sent to ClampRagdollForce as a Vector *
		Vector vecClampedForce;
		ClampRagdollForce( forceVector, &vecClampedForce );
		m_vecForce = vecClampedForce;

		SetParent( NULL );

		AddFlag( FL_TRANSRAGDOLL );

		SetMoveType( MOVETYPE_NONE );

		return true;
	}

	return false;*/
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_BaseNPC::CreateRagdollEntity( void )
{
	CreateRagdollEntity( false, false, false, 0.0f, 0, false );
}

//-----------------------------------------------------------------------------
// Purpose: Create a ragdoll entity to pass to the client.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::CreateRagdollEntity( bool bGib, bool bBurning, bool bOnGround, float flInvisLevel, int iDamageCustom, bool bFake )
{
}

//-----------------------------------------------------------------------------
// Purpose: Destroy's a ragdoll, called with a player is disconnecting.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::DestroyRagdoll( void )
{
}

//=========================================================
// GetFlinchActivity - determines the best type of flinch
// anim to play.
//=========================================================
Activity CAI_BaseNPC::GetFlinchActivity( bool bHeavyDamage, bool bGesture )
{
	Activity	flinchActivity;

	switch ( LastHitGroup() )
	{
		// pick a region-specific flinch
	case HITGROUP_HEAD:
		flinchActivity = bGesture ? ACT_GESTURE_FLINCH_HEAD : ACT_FLINCH_HEAD;
		break;
	case HITGROUP_STOMACH:
		flinchActivity = bGesture ? ACT_GESTURE_FLINCH_STOMACH : ACT_FLINCH_STOMACH;
		break;
	case HITGROUP_LEFTARM:
		flinchActivity = bGesture ? ACT_GESTURE_FLINCH_LEFTARM : ACT_FLINCH_LEFTARM;
		break;
	case HITGROUP_RIGHTARM:
		flinchActivity = bGesture ? ACT_GESTURE_FLINCH_RIGHTARM : ACT_FLINCH_RIGHTARM;
		break;
	case HITGROUP_LEFTLEG:
		flinchActivity = bGesture ? ACT_GESTURE_FLINCH_LEFTLEG : ACT_FLINCH_LEFTLEG;
		break;
	case HITGROUP_RIGHTLEG:
		flinchActivity = bGesture ? ACT_GESTURE_FLINCH_RIGHTLEG : ACT_FLINCH_RIGHTLEG;
		break;
	case HITGROUP_CHEST:
		flinchActivity = bGesture ? ACT_GESTURE_FLINCH_CHEST : ACT_FLINCH_CHEST;
		break;
	case HITGROUP_GEAR:
	case HITGROUP_GENERIC:
	default:
		// just get a generic flinch.
		if ( bHeavyDamage )
		{
			flinchActivity = bGesture ? ACT_GESTURE_BIG_FLINCH : ACT_BIG_FLINCH;
		}
		else
		{
			flinchActivity = bGesture ? ACT_GESTURE_SMALL_FLINCH : ACT_SMALL_FLINCH;
		}
		break;
	}

	// do we have a sequence for the ideal activity?
	if ( SelectWeightedSequence ( flinchActivity ) == ACTIVITY_NOT_AVAILABLE )
	{
		if ( bHeavyDamage )
		{
			flinchActivity = bGesture ? ACT_GESTURE_BIG_FLINCH : ACT_BIG_FLINCH;

			// If we fail at finding a big flinch, resort to a small one
			if ( SelectWeightedSequence ( flinchActivity ) == ACTIVITY_NOT_AVAILABLE )
			{
				flinchActivity = bGesture ? ACT_GESTURE_SMALL_FLINCH : ACT_SMALL_FLINCH;
			}
		}
		else
		{
			flinchActivity = bGesture ? ACT_GESTURE_SMALL_FLINCH : ACT_SMALL_FLINCH;
		}
	}

	return flinchActivity;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
//-----------------------------------------------------------------------------
void CAI_BaseNPC::CleanupOnDeath( CBaseEntity *pCulprit, bool bFireDeathOutput )
{
	if ( !m_bDidDeathCleanup )
	{
		m_bDidDeathCleanup = true;

		if ( m_NPCState == NPC_STATE_SCRIPT && m_hCine )
		{
			// bail out of this script here
			m_hCine->CancelScript();
			// now keep going with the death code
		}

		if ( GetHintNode() )
		{
			GetHintNode()->Unlock();
			SetHintNode( NULL );
		}

		if( bFireDeathOutput )
		{
			m_OnDeath.FireOutput( pCulprit, this );
		}

		// Vacate any strategy slot I might have
		VacateStrategySlot();

		// Remove from squad if in one
		if (m_pSquad)
		{
			// If I'm in idle it means that I didn't see who killed me
			// and my squad is still in idle state. Tell squad we have
			// an enemy to wake them up and put the enemy position at
			// my death position
			if ( m_NPCState == NPC_STATE_IDLE && pCulprit)
			{
				// If we already have some danger memory, don't do this cheat
				if ( GetEnemies()->GetDangerMemory() == NULL )
				{
					UpdateEnemyMemory( pCulprit, GetAbsOrigin() );
				}
			}

			// Remove from squad
			m_pSquad->RemoveFromSquad(this, true);
			m_pSquad = NULL;
		}

		RemoveActorFromScriptedScenes( this, false /*all scenes*/ );

#ifdef TF_CLASSIC
		RemoveAllCond();

		// Remove from team
		CTFTeam *pTFTeam = ( CTFTeam * )GetTeam();
		if ( pTFTeam )
		{
			pTFTeam->RemoveNPC( this );
		}
#endif
	}
	else
		DevMsg( "Unexpected double-death-cleanup\n" );
}

void CAI_BaseNPC::SelectDeathPose( const CTakeDamageInfo &info )
{
	if ( !GetModelPtr() || (info.GetDamageType() & DMG_PREVENT_PHYSICS_FORCE) )
		return;

	if ( ShouldPickADeathPose() == false )
		return;

	Activity aActivity = ACT_INVALID;
	int iDeathFrame = 0;

	SelectDeathPoseActivityAndFrame( this, info, LastHitGroup(), aActivity, iDeathFrame );
	if ( aActivity == ACT_INVALID )
	{
		SetDeathPose( ACT_INVALID );
		SetDeathPoseFrame( 0 );
		return;
	}

	SetDeathPose( SelectWeightedSequence( aActivity ) );
	SetDeathPoseFrame( iDeathFrame );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::Event_KilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	BaseClass::Event_KilledOther( pVictim, info );

	CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon *>( info.GetWeapon() );

	if ( pVictim->IsPlayer() )
	{
		if ( IsAlive() )
		{
			// Apply on-kill effects.
			if ( pWeapon )
			{
				// Apply on-kill effects.
				float flCritOnKill = 0.0f;
				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flCritOnKill, add_onkill_critboost_time );
				if ( flCritOnKill )
					AddCond( TF_COND_CRITBOOSTED_ON_KILL, flCritOnKill );

				float flMiniCritOnKill = 0.0f;
				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flMiniCritOnKill, add_onkill_minicritboost_time );
				if ( flMiniCritOnKill )
					AddCond( TF_COND_MINICRITBOOSTED_ON_KILL, flMiniCritOnKill );

				float flSpeedBoostOnKill = 0.0f;
				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flSpeedBoostOnKill, speed_boost_on_kill );
				if ( flSpeedBoostOnKill )
					AddCond( TF_COND_SPEED_BOOST, flSpeedBoostOnKill );

				float flHealthOnKill = 0.0f;
				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flHealthOnKill, heal_on_kill );
				if( flHealthOnKill )
				{
					int iHealthRestored = TakeHealth( flHealthOnKill, DMG_GENERIC );
					if ( iHealthRestored )
					{
						IGameEvent *event = gameeventmanager->CreateEvent( "npc_healonhit" );
				
						if ( event )
						{
							event->SetInt( "amount", iHealthRestored );
							event->SetInt( "entindex", entindex() );
					
							gameeventmanager->FireEvent( event );
						}
					}
				}

				float flRestoreHealthOnKill = 0.0f;
				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flRestoreHealthOnKill, restore_health_on_kill );
				if( flRestoreHealthOnKill )
				{
					int iHealthRestored = TakeHealth( ( ( flRestoreHealthOnKill/100 ) * GetMaxHealth() ), DMG_IGNORE_MAXHEALTH );
					if ( iHealthRestored )
					{
						IGameEvent *event = gameeventmanager->CreateEvent( "npc_healonhit" );
				
						if ( event )
						{
							event->SetInt( "amount", iHealthRestored );
							event->SetInt( "entindex", entindex() );
					
							gameeventmanager->FireEvent( event );
						}
					}
				}
			}
		}
	}
	else if ( pVictim->IsNPC() )
	{
		if ( IsAlive() )
		{
			// Apply on-kill effects.
			if ( pWeapon )
			{
				// Apply on-kill effects.
				float flCritOnKill = 0.0f;
				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flCritOnKill, add_onkill_critboost_time );
				if ( flCritOnKill )
					AddCond( TF_COND_CRITBOOSTED_ON_KILL, flCritOnKill );

				float flMiniCritOnKill = 0.0f;
				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flMiniCritOnKill, add_onkill_minicritboost_time );
				if ( flMiniCritOnKill )
					AddCond( TF_COND_MINICRITBOOSTED_ON_KILL, flMiniCritOnKill );

				float flSpeedBoostOnKill = 0.0f;
				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flSpeedBoostOnKill, speed_boost_on_kill );
				if ( flSpeedBoostOnKill )
					AddCond( TF_COND_SPEED_BOOST, flSpeedBoostOnKill );

				float flHealthOnKill = 0.0f;
				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flHealthOnKill, heal_on_kill );
				if( flHealthOnKill )
				{
					int iHealthRestored = TakeHealth( flHealthOnKill, DMG_GENERIC );
					if (iHealthRestored)
					{
						IGameEvent *event = gameeventmanager->CreateEvent("npc_healonhit");
				
						if ( event )
						{
							event->SetInt( "amount", iHealthRestored );
							event->SetInt( "entindex", entindex() );
					
							gameeventmanager->FireEvent( event );
						}
					}
				}

				float flRestoreHealthOnKill = 0.0f;
				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flRestoreHealthOnKill, restore_health_on_kill );
				if( flRestoreHealthOnKill )
				{
					int iHealthRestored = TakeHealth( ( ( flRestoreHealthOnKill/100 ) * GetMaxHealth() ), DMG_IGNORE_MAXHEALTH );
					if ( iHealthRestored )
					{
						IGameEvent *event = gameeventmanager->CreateEvent( "npc_healonhit" );
				
						if ( event )
						{
							event->SetInt( "amount", iHealthRestored );
							event->SetInt( "entindex", entindex() );
					
							gameeventmanager->FireEvent( event );
						}
					}
				}
			}
		}
 	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
//-----------------------------------------------------------------------------
void CAI_BaseNPC::Event_Killed( const CTakeDamageInfo &info )
{
	if (IsCurSchedule(SCHED_NPC_FREEZE))
	{
		// We're frozen; don't die.
		return;
	}

	Wake( false );

	RemoveAllCond();

	if ( FClassnameIs( this, "npc_turret_floor" ) || FClassnameIs( this, "npc_portal_turret_floor" ) )
	{
	}
	else if ( info.GetAttacker()->GetTeamNumber() == TF_TEAM_RED && (TFGameRules()->IsAnyCoOp() || TFGameRules()->IsVersus()) && 
			  sv_dynamicnpcs.GetBool() && (GetTeamNumber() == TF_TEAM_BLUE || GetTeamNumber() == TF_TEAM_YELLOW || GetTeamNumber() == TF_TEAM_GREEN))
	{
		TFGameRules()->m_iDirectorAnger = TFGameRules()->m_iDirectorAnger + 2;
	}

	int killer_index = 0;

	// Find the killer & the scorer
	CAI_BaseNPC *pVictim = this;
	CBaseEntity *pInflictor = info.GetInflictor();
	CBaseEntity *pKiller = info.GetAttacker();
	CTFPlayer *pTFAttacker = ToTFPlayer( pKiller );
	CTFPlayer *pScorer = ToTFPlayer( TFGameRules()->GetDeathScorer( pKiller, pInflictor, pVictim ) );
	CBaseEntity *pAssister = TFGameRules()->GetAssister( pVictim, pKiller, pInflictor );

	int iWeaponID = TF_WEAPON_NONE;
	// Work out what killed the player, and send a message to all clients about it
	const char *killer_weapon_name = TFGameRules()->GetKillingWeaponName( info, NULL, iWeaponID );
	const char *killer_weapon_log_name = NULL;

	const char* killer_Name = 0;
	const char* assister_Name = 0;
	const char* victim_Name = 0;


	CTFWeaponBase *pKillerWeapon = NULL;
	if ( info.GetWeapon() )
	{
		pKillerWeapon = dynamic_cast<CTFWeaponBase *>( info.GetWeapon() );
	}
	else if ( pKiller && pKiller->IsPlayer() )
	{
		// Assume that player used his currently active weapon.
		pKillerWeapon = ToTFPlayer( pKiller )->GetActiveTFWeapon();
	}

	if ( iWeaponID && pScorer )
	{
		CTFWeaponBase *pWeapon = pScorer->Weapon_OwnsThisID( iWeaponID );
		if ( pWeapon )
		{
			CEconItemDefinition *pItemDef = pWeapon->GetItem()->GetStaticData();
			if ( pItemDef )
			{
				if ( pItemDef->item_iconname[0] )
					killer_weapon_name = pItemDef->item_iconname;

				if ( pItemDef->item_logname[0] )
					killer_weapon_log_name = pItemDef->item_logname;
			}
		}
	}

	if ( pScorer )	// Is the killer a client?
	{
		killer_index = pScorer->entindex();
	}
	else if ( pKiller && pKiller->IsNPC() )
	{
		killer_index = pKiller->entindex();
	}

	if ( pVictim )
	{
		victim_Name = pVictim->GetLocalizeName();
		if ( !victim_Name )
			victim_Name = pVictim->GetClassname();
	}

	if ( pKiller )
	{
		if ( pKiller->IsNPC() )
			killer_Name = pKiller->MyNPCPointer()->GetLocalizeName();
		if ( !killer_Name )
			killer_Name = pKiller->GetClassname();
	}

	if ( pAssister )
	{
		if ( pAssister->IsNPC() )
			assister_Name = pAssister->MyNPCPointer()->GetLocalizeName();
		if ( !assister_Name )
			assister_Name = pAssister->GetClassname();
	}

	// are we allowed to make deathnotice on THIS npc?
	if ( AllowDeathNotice() )
	{
		//DeathNotice( info );

		IGameEvent *event = gameeventmanager->CreateEvent( "npc_death" );
		if ( event )
		{
			event->SetInt( "victim_index", pVictim->entindex() );
			event->SetString( "victim_name", victim_Name );
			event->SetString( "victim_entname", STRING( pVictim->GetEntityName() ) );
			event->SetInt( "victim_team", pVictim->GetTeamNumber() );
			event->SetInt( "attacker_index", killer_index );
			event->SetString( "attacker_name", pKiller ? killer_Name : NULL );
			event->SetInt( "attacker_team", pKiller ? pKiller->GetTeamNumber() : 0 );
			event->SetInt( "assister_index", pAssister ? pAssister->entindex() : -1 );
			event->SetString( "assister_name", pAssister ? assister_Name : NULL );
			event->SetInt( "assister_team", pAssister ? pAssister->GetTeamNumber() : 0 );
			event->SetString( "weapon", killer_weapon_name );
			event->SetInt( "weaponid", iWeaponID );
			event->SetString( "weapon_logclassname", killer_weapon_log_name );
			event->SetInt( "damagebits", info.GetDamageType() );
			event->SetInt( "customkill", info.GetDamageCustom() );
			event->SetInt( "priority", 7 );	// HLTV event priority, not transmitted

			gameeventmanager->FireEvent( event );
		}

		if ( TFGameRules()->IsHolidayActive( kHoliday_AprilFools ) )
		{
			const float flRand = rand() / VALVE_RAND_MAX;
			if ( flRand < 0.4f )
			{
				CReliableBroadcastRecipientFilter filter;
				UTIL_SayText2Filter( filter, NULL, false, "#game_player_left_game", pVictim->GetLocalizeName() );
			}
		}
	}

	// the workaround
	if ( pKillerWeapon )
	{
		CTFDecapitationMeleeWeaponBase *pSword = dynamic_cast<CTFDecapitationMeleeWeaponBase *>( pKillerWeapon );
		if ( pSword )
			pSword->OnDecapitation( pVictim );
	}

	// Ragdoll should burn if NPC burned to death.
	m_bBurningDeath = IsOnFire() || ( info.GetDamageType() & (DMG_BURN | DMG_IGNITE) );

	if ( TFGameRules() )
		TFGameRules()->NPCKilled( this, info );

	/*CTFPlayer *pPlayerScorer = ToTFPlayer( pScorer );
	if ( pPlayerScorer && NoReward() )
	{
		pPlayerScorer->Event_KilledOther( this, info );
	}*/

	CTakeDamageInfo info_modified = info;
	int iDamageCustom = info.GetDamageCustom();

	bool bCritOnHardHit = false, bTurnToAsh = false;
	if ( pKillerWeapon && pKiller != this )
	{
		int nRagdollsBecomeAsh = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pKillerWeapon, nRagdollsBecomeAsh, ragdolls_become_ash );
		bTurnToAsh = nRagdollsBecomeAsh != 0;

		int nRagdollPlasmaEffect = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pKillerWeapon, nRagdollPlasmaEffect, ragdolls_plasma_effect );
		if ( nRagdollPlasmaEffect != 0 )
			iDamageCustom = TF_DMG_CUSTOM_PLASMA;

		int nCritOnHardHit = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pKillerWeapon, nCritOnHardHit, crit_on_hard_hit );
		bCritOnHardHit = nCritOnHardHit != 0;

		int nTurnToGold = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pKillerWeapon, nTurnToGold, set_turn_to_gold );
		if ( nTurnToGold != 0 )
			iDamageCustom = TF_DMG_CUSTOM_GOLD_WRENCH;
	}

	m_bAshDeath = bTurnToAsh;

	if ( iDamageCustom == TF_DMG_CUSTOM_SUICIDE )
	{
		// if this was suicide, recalculate attacker to see if we want to award the kill to a recent damager
		info_modified.SetAttacker( TFGameRules()->GetDeathScorer( info.GetAttacker(), pInflictor, this ) );
	}
	else if ( !pKiller || pKiller == this || pKiller->IsBSPModel() )
	{
		// Recalculate attacker if player killed himself or this was environmental death.
		CBaseEntity *pDamager = TFGameRules()->GetRecentDamager( this, 0, TF_TIME_ENV_DEATH_KILL_CREDIT );
		if ( pDamager )
		{
			info_modified.SetAttacker( pDamager );
			info_modified.SetInflictor( NULL );
			info_modified.SetWeapon( NULL );
			info_modified.SetDamageType( DMG_GENERIC );
			iDamageCustom = TF_DMG_CUSTOM_SUICIDE;
		}
	}

	bool bDisguiseOnStab = false, bTurnToIce = false;
	if ( pTFAttacker )
	{
		if ( TF_DMG_CUSTOM_HEADSHOT == iDamageCustom )
		{
			CTF_GameStats.Event_Headshot( pTFAttacker );
		}
		else if ( TF_DMG_CUSTOM_BACKSTAB == iDamageCustom )
		{
			CTF_GameStats.Event_Backstab( pTFAttacker );
			if ( pKillerWeapon && pKiller != this )
			{
				int nTurnToIce = 0;
				CALL_ATTRIB_HOOK_INT_ON_OTHER( pKillerWeapon, nTurnToIce, freeze_backstab_victim );
				bTurnToIce = nTurnToIce != 0;

				int nDisguiseOnBackstab = 0;
				CALL_ATTRIB_HOOK_INT_ON_OTHER( pKillerWeapon, nDisguiseOnBackstab, set_disguise_on_backstab );
				bDisguiseOnStab = nDisguiseOnBackstab != 0;
			}
		}
	}

	m_bIceDeath = bTurnToIce;
	if ( m_bIceDeath )
		EmitSound( "Icicle.TurnToIce" );

	m_iDamageCustomDeath = iDamageCustom;

	if ( m_iDamageCustomDeath == TF_DMG_CUSTOM_GOLD_WRENCH )
		EmitSound( "Saxxy.TurnGold" );
	else if ( m_iDamageCustomDeath == TF_DMG_CUSTOM_DECAPITATION )
		EmitSound( "TFPlayer.Decapitated" );

	info_modified.SetDamageCustom( iDamageCustom );

	if ( pKiller && pKiller == pInflictor && pKiller->IsBSPModel() )
	{
		CBaseEntity *pDamager = TFGameRules()->GetRecentDamager( this, 0, TF_TIME_ENV_DEATH_KILL_CREDIT );
		CTFPlayer *pTFDamager = ToTFPlayer( pDamager );

		if ( pTFDamager )
		{
			if ( AllowDeathNotice() )
			{
				IGameEvent *event = gameeventmanager->CreateEvent( "npc_environmental_death" );

				if ( event )
				{
					event->SetInt( "killer", pTFDamager->GetUserID() );
					event->SetInt( "victim", entindex() );
					event->SetInt( "priority", 9 ); // HLTV event priority, not transmitted
				
					gameeventmanager->FireEvent( event );
				}
			}
		}
	}

	if ( GetCarryingRuneType() != TF_RUNE_NONE && !TFGameRules()->IsSkillLevel( SKILL_UNUSUAL ) )
		DropRune();

	if ( pTFAttacker )
	{
		int nDropHealthOnKill = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pTFAttacker, nDropHealthOnKill, drop_health_pack_on_kill );
		if ( nDropHealthOnKill != 0 )
			DropHealthPack( info );
	}

	// Handle on-kill effects.
	if ( pKillerWeapon && pKiller != this )
	{
		// Notify the damaging weapon.
		pKillerWeapon->OnPlayerKill( this, info );
	}

	// body is gonna be around for a while, so have it stink for a bit.
	CSoundEnt::InsertSound ( SOUND_CARCASS, GetAbsOrigin(), 384, 15 );

	// If the npc has a capture flag and was killed by a player, award that player a defense
	if ( HasTheFlag() && pKiller )
	{
		CCaptureFlag *pCaptureFlag = dynamic_cast<CCaptureFlag *>( GetItem() );
		if ( pCaptureFlag )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "teamplay_flag_event" );
			if ( event )
			{
				event->SetInt( "player", pKiller->entindex() );
				event->SetString( "player_name", pKiller->IsPlayer() ? pTFAttacker->GetPlayerName() : pKiller->GetClassname() );
				event->SetInt( "carrier", entindex() );
				event->SetInt( "eventtype", TF_FLAGEVENT_DEFEND );
				event->SetInt( "home", 0 ); // whether or not the flag was home (only set for TF_FLAGEVENT_PICKUP) 
				event->SetInt( "team", GetItem()->GetTeamNumber() );
				event->SetInt( "priority", 8 );
				gameeventmanager->FireEvent( event );
			}

			if ( pTFAttacker )
				CTF_GameStats.Event_PlayerDefendedPoint( pTFAttacker );
		}
	}

	if ( HasTheFlag() )
		DropFlag();

	if ( GetParent() && GetParent()->ClassMatches( "obj_attachment_sapper" ) )
		UTIL_Remove( GetParent() );

	//Adrian: Select a death pose to extrapolate the ragdoll's velocity.
	SelectDeathPose( info );

	m_lifeState = LIFE_DYING;

	CleanupOnDeath( info.GetAttacker() );

	StopLoopingSounds();
	DeathSound(info);

	// Ragdoll, gib, or death animation.
	bool bRagdoll = false;
	bool bGib = false;

	// we want the rag doll to burn if the npc was burning and was not a pryo (who only burns momentarily)
	/*bool bBurning = InCond( TF_COND_BURNING );
	bool bOnGround = ( GetFlags() & FL_ONGROUND ) != 0;
	float flInvis = 0.0f;*/

	// See if we should rag.
	if ( CanBecomeRagdoll() || IsRagdoll() )
		bRagdoll = true;

	// See if we should gib.
	if ( ShouldGib( info ) )
	{
		bGib = true;
		bRagdoll = false;
	}

	// Create the ragdoll entity.
	if ( bGib || bRagdoll )
	{
		//CreateRagdollEntity( bGib, bBurning, bOnGround, flInvis, info.GetDamageCustom(), true );
		SetState( NPC_STATE_DEAD );
	}

	// Remove all wearables.
	for ( int i = 0; i < GetNumWearables(); i++ )
	{
		CEconWearable *pWearable = GetWearable( i );
		if ( !pWearable )
			continue;

		RemoveWearable( pWearable );
	}

	if ( ( GetFlags() & FL_NPC ) && ( ShouldGib( info ) == false ) )
	{
		SetTouch( NULL );
	}

	BaseClass::Event_Killed( info );

	if ( m_bFadeCorpse )
	{
		m_bImportanRagdoll = RagdollManager_SaveImportant( this );
	}
	
	// Make sure this condition is fired too (OnTakeDamage breaks out before this happens on death)
	SetCondition( COND_LIGHT_DAMAGE );
	SetIdealState( NPC_STATE_DEAD );

	// Some characters rely on getting a state transition, even to death.
	// zombies, for instance. When a character becomes a ragdoll, their
	// server entity ceases to think, so we have to set the dead state here
	// because the AI code isn't going to pick up the change on the next think
	// for us.

	// If the remove-no-ragdoll flag is set in the damage type, we're being
	// told to remove ourselves immediately on death. This is used when something
	// else has some special reason for us to vanish instead of creating a ragdoll.
	// i.e. The barnacle does this because it's already got a ragdoll for us.
	if ( info.GetDamageType() & DMG_REMOVENORAGDOLL )
	{
		if ( !IsEFlagSet( EFL_IS_BEING_LIFTED_BY_BARNACLE ) )
		{
			// Go away
			RemoveDeferred();
		}
	}
}

//-----------------------------------------------------------------------------

void CAI_BaseNPC::Ignite( float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner )
{
#ifdef TF_CLASSIC
	// Don't bother igniting NPCs who have just been killed by the fire damage.
	if ( !IsAlive() )
		return;

	if ( !AllowedToIgnite() && !bCalledByLevelDesigner )
		return;

	if ( !IsOnFire() )
	{
		CTakeDamageInfo info;
		CTFPlayer *pTFAttacker = ToTFPlayer( info.GetAttacker() );
		// Start burning
		Burn( pTFAttacker );
	}
	
	m_flFlameRemoveTime = gpGlobals->curtime + flFlameLifetime;
	// Default attacker to world, Ignite calls from TF2 code need to change this.
	m_hBurnAttacker = GetContainingEntity( INDEXENT(0) );
#else
	BaseClass::Ignite( flFlameLifetime, bNPCOnly, flSize, bCalledByLevelDesigner );
#endif

/*#ifdef TF_CLASSIC
	CBasePlayer *pPlayer = UTIL_GetNearestPlayer( GetAbsOrigin() ); 
	if ( pPlayer->IRelationType( this ) != D_LI )
	{
		CNPC_Alyx *alyx = CNPC_Alyx::GetAlyx();

		if ( alyx )
		{
			alyx->EnemyIgnited( this );
		}
	}
#endif*/
}

//-----------------------------------------------------------------------------

ConVar	ai_block_damage( "ai_block_damage","0" );

bool CAI_BaseNPC::PassesDamageFilter( const CTakeDamageInfo &info )
{
	if ( ai_block_damage.GetBool() )
		return false;
	// FIXME: hook a friendly damage filter to the npc instead?
	if ( (CapabilitiesGet() & bits_CAP_FRIENDLY_DMG_IMMUNE) && info.GetAttacker() && info.GetAttacker() != this )
	{
		// check attackers relationship with me
		CBaseCombatCharacter *npcEnemy = info.GetAttacker()->MyCombatCharacterPointer();
		bool bHitByVehicle = false;
		if ( !npcEnemy )
		{
			if ( info.GetAttacker()->GetServerVehicle() )
			{
				bHitByVehicle = true;
			}
		}

		if ( bHitByVehicle || (npcEnemy && npcEnemy->IRelationType( this ) == D_LI) )
		{
			m_fNoDamageDecal = true;

			if ( npcEnemy && npcEnemy->IsPlayer() && !InSameTeam(npcEnemy) )
			{
				m_OnDamagedByPlayer.FireOutput( info.GetAttacker(), this );
				// This also counts as being harmed by player's squad.
				m_OnDamagedByPlayerSquad.FireOutput( info.GetAttacker(), this );
			}

			return false;
		}
	}
	
	if ( !BaseClass::PassesDamageFilter( info ) )
	{
		m_fNoDamageDecal = true;
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CAI_BaseNPC::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;

	CBaseEntity *pAttacker = info.GetAttacker();
	//CBaseEntity *pInflictor = info.GetInflictor();
	CTFWeaponBase *pWeapon = NULL;
	bool bObject = false;

	if ( inputInfo.GetWeapon() )
	{
		pWeapon = dynamic_cast<CTFWeaponBase *>( inputInfo.GetWeapon() );
	}
	else if ( pAttacker && pAttacker->IsPlayer() )
	{
		// Assume that player used his currently active weapon.
		pWeapon = ToTFPlayer( pAttacker )->GetActiveTFWeapon();
	}

	// If this is a base object get the builder
	if ( pAttacker && pAttacker->IsBaseObject() )
	{
		CBaseObject *pObject = static_cast< CBaseObject * >( pAttacker );
		//pAttacker = pObject->GetBuilder();
		pAttacker = pObject;
		bObject = true;
	}

	if (pAttacker && pAttacker->IsNPC())
	{
		CAI_BaseNPC *pAttacker2 = static_cast<CAI_BaseNPC*>(pAttacker);
		if (pAttacker2->InCond(TF_COND_REGENONDAMAGEBUFF) || InCond(TF_COND_MAD_MILK))
		{
			if (!pAttacker2->InCond(TF_COND_REGENONDAMAGEBUFF))
			{
				pAttacker2->TakeHealth(info.GetDamage() / 2.0f, DMG_GENERIC);
			}
			else
			{
				pAttacker2->TakeHealth(info.GetDamage() / 6.0f, DMG_GENERIC);
			}
		}
	}

	AddDamagerToHistory( pAttacker );

	// Save damage force for ragdolls.
	m_vecTotalBulletForce = info.GetDamageForce();
	m_vecTotalBulletForce.x = clamp( m_vecTotalBulletForce.x, -15000.0f, 15000.0f );
	m_vecTotalBulletForce.y = clamp( m_vecTotalBulletForce.y, -15000.0f, 15000.0f );
	m_vecTotalBulletForce.z = clamp( m_vecTotalBulletForce.z, -15000.0f, 15000.0f );

	int bitsDamage = inputInfo.GetDamageType();

	// If we're invulnerable, force ourselves to only take damage events only, so we still get pushed
	if ( IsInvulnerable() || InCond( TF_COND_PHASE ) )
	{
		bool bAllowDamage = false;

		// check to see if our attacker is a trigger_hurt entity (and allow it to kill us even if we're invuln)
		if ( pAttacker && pAttacker->IsSolidFlagSet( FSOLID_TRIGGER ) )
		{
			CTriggerHurt *pTrigger = dynamic_cast<CTriggerHurt *>( pAttacker );
			if ( pTrigger )
			{
				bAllowDamage = true;
			}
		}

		// Does not save from telefrags.
		if ( info.GetDamageCustom() == TF_DMG_CUSTOM_TELEFRAG )
			bAllowDamage = true;

		// Does not save from crocodiles.
		if ( info.GetDamageCustom() == TF_DMG_CUSTOM_CROC )
			bAllowDamage = true;

		if ( !bAllowDamage )
		{
			int iOldTakeDamage = m_takedamage;
			m_takedamage = DAMAGE_EVENTS_ONLY;
			// NOTE: Deliberately skip base player OnTakeDamage, because we don't want all the stuff it does re: suit voice
			CBaseCombatCharacter::OnTakeDamage( info );
			m_takedamage = iOldTakeDamage;

			// Burn sounds are handled in ConditionThink()
			if ( !(bitsDamage & DMG_BURN ) )
				PainSound( info );

			if( InCond( TF_COND_PHASE ) )
			{
				CEffectData	data;
				data.m_nHitBox = GetParticleSystemIndex( "miss_text" );
				data.m_vOrigin = WorldSpaceCenter() + Vector(0,0,32);
				data.m_vAngles = vec3_angle;
				data.m_nEntIndex = 0;

				CSingleUserRecipientFilter filter( (CBasePlayer*)pAttacker );
				te->DispatchEffect( filter, 0.0, data.m_vOrigin, "ParticleEffect", data );
			}
			return 0;
		}
	}

	if ( InCond( TF_COND_URINE ) || InCond( TF_COND_MARKEDFORDEATH ) || InCond( TF_COND_MARKEDFORDEATH_SILENT ) )
	{
		// Jarated npcs take mini-crits
		bitsDamage |= DMG_MINICRITICAL;
		info.AddDamageType( DMG_MINICRITICAL );
	}

	CTFPlayer *pTFAttacker = ToTFPlayer( pAttacker );
	//CAI_BaseNPC *pNPCInflictor = dynamic_cast<CAI_BaseNPC *>( pInflictor );

	// Handle on-hit effects.
	if ( pWeapon && pAttacker != this )
	{
		int nCritOnCond = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, nCritOnCond, or_crit_vs_playercond );
		if ( nCritOnCond )
		{
			for ( int i = 0; condition_to_attribute_translation[i] != TF_COND_LAST; i++ )
			{
				int nCond = condition_to_attribute_translation[i];
				int nFlag = ( 1 << i );
				if ( ( nCritOnCond & nFlag ) && InCond( nCond ) )
				{
					bitsDamage |= DMG_CRITICAL;
					info.AddDamageType( DMG_CRITICAL );
					break;
				}
			}
		}

		int nMiniCritOnCond = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, nMiniCritOnCond, or_minicrit_vs_playercond_burning );
		if ( nMiniCritOnCond )
		{
			for ( int i = 0; condition_to_attribute_translation[i] != TF_COND_LAST; i++ )
			{
				int nCond = condition_to_attribute_translation[i];
				int nFlag = ( 1 << i );
				if ( ( nMiniCritOnCond & nFlag ) && InCond( nCond ) )
				{
					bitsDamage |= DMG_MINICRITICAL;
					info.AddDamageType( DMG_MINICRITICAL );
					break;
				}
			}
		}

		float flPenaltyNonBurning = info.GetDamage();
		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flPenaltyNonBurning, mult_dmg_vs_nonburning );
		if ( !InCond( TF_COND_BURNING ) )
			info.SetDamage( flPenaltyNonBurning );

		float flPenaltyHealth = info.GetDamage();
		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flPenaltyHealth, mult_dmg_penalty_while_half_alive );
		if ( pAttacker->GetHealth() >= ( pAttacker->GetMaxHealth() / 2 ) )
			info.SetDamage( flPenaltyHealth );

		float flBonusHealth = info.GetDamage();
		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flBonusHealth, mult_dmg_bonus_while_half_dead );
		if ( pAttacker->GetHealth() < ( pAttacker->GetMaxHealth() / 2 ) )
			info.SetDamage( flBonusHealth );

		if ( IsMech() )
		{
			float flPenaltyVsBuilding = info.GetDamage();
			CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flPenaltyVsBuilding, mult_dmg_vs_buildings );
			info.SetDamage( flPenaltyVsBuilding );

			int iEnergyWeaponNoDamage = 0;
			CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iEnergyWeaponNoDamage, energy_weapon_no_hurt_building );
			if ( iEnergyWeaponNoDamage != 0 )
				info.SetDamage( info.GetDamage() * 0.2f );
		}

		int iCritVsWet = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iCritVsWet, crit_vs_wet_players );
		if ( iCritVsWet > 0 )
		{
			if ( GetWaterLevel() >= WL_Feet )
			{
				bitsDamage |= DMG_CRITICAL;
				info.AddDamageType( DMG_CRITICAL );
			}
		}

		if ( pTFAttacker )
		{
			if ( pTFAttacker->m_Shared.InCond( TF_COND_ENERGY_BUFF ) )
			{
				bitsDamage |= DMG_MINICRITICAL;
				info.AddDamageType( DMG_MINICRITICAL );
			}

			int nMarkAttackerForDeath = 0;
			CALL_ATTRIB_HOOK_INT_ON_OTHER( pTFAttacker, nMarkAttackerForDeath, mod_mark_attacker_for_death );
			if ( nMarkAttackerForDeath && pTFAttacker->m_Shared.InCond( TF_COND_ENERGY_BUFF ) )
			{
				pTFAttacker->m_Shared.AddCond( TF_COND_MARKEDFORDEATH, 5 );
			}

			int nCritWhileAirborne = 0;
			CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, nCritWhileAirborne, crit_while_airborne );
			if ( nCritWhileAirborne && pTFAttacker && pTFAttacker->m_Shared.InCond( TF_COND_BLASTJUMPING ) )
			{
				bitsDamage |= DMG_CRITICAL;
				info.AddDamageType( DMG_CRITICAL );
			}

			if ( pTFAttacker->m_Shared.InCond( TF_COND_DISGUISED ) )
			{
				float flDisguisedMod = info.GetDamage();
				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flDisguisedMod, mult_dmg_disguised );
				info.SetDamage( flDisguisedMod );
			}

			if ( pTFAttacker->m_Shared.InCond( TF_COND_RUNE_KNOCKOUT ) && IsMech() )
				info.SetDamage( info.GetDamage() * 4.0f );

			// Notify the damaging weapon.
			pWeapon->ApplyOnHitAttributes( this, pTFAttacker, info );

			// Build rage
			bool bBannerHealed = false;
			CTFBuffItem *pBanner = ( CTFBuffItem * )pTFAttacker->Weapon_GetSlot( LOADOUT_POSITION_SECONDARY );
			if ( pBanner )
			{
				int iBannerMode = pBanner->GetBuffType();
				if ( iBannerMode == TF_BUFF_REGENONDAMAGE ) //conch
				{
					if ( pTFAttacker->m_Shared.InCond( TF_COND_REGENONDAMAGEBUFF ) )
					{
						pTFAttacker->TakeHealth( info.GetDamage() / 6.0f, DMG_GENERIC );
						bBannerHealed = true;
						IGameEvent *event = gameeventmanager->CreateEvent( "player_healonhit" );
						if ( event )
						{
							event->SetInt( "amount", info.GetDamage() / 6.0f );
							event->SetInt( "entindex", pTFAttacker->entindex() );

							gameeventmanager->FireEvent( event );
						}
					}
					else
					{
						pTFAttacker->m_Shared.SetRageMeter( info.GetDamage() / (480 / 100), iBannerMode );
					}
				}
				else //buff banner, battalions
				{
					pTFAttacker->m_Shared.SetRageMeter( info.GetDamage() / (600 / 100), iBannerMode );
				}
			}

			// build mmmph
			int nSurpriseMotherFucker = 0;
			CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, nSurpriseMotherFucker, burn_damage_earns_rage );
			if ( nSurpriseMotherFucker )
				pTFAttacker->m_Shared.SetRageMeter( info.GetDamage() / (600 / 100), TF_BUFF_CRITBOOSTED ); // live = 300, mvm = 1200

			if ( !bBannerHealed && InCond( TF_COND_MAD_MILK ) )
			{
				pTFAttacker->TakeHealth( info.GetDamage() / 2.0f, DMG_GENERIC );
				IGameEvent *event = gameeventmanager->CreateEvent( "player_healonhit" );
				if ( event )
				{
					event->SetInt( "amount", info.GetDamage() / 2.0f );
					event->SetInt( "entindex", pTFAttacker->entindex() );

					gameeventmanager->FireEvent(event);
				}
			}

			// build hype
			int nHypeOnDamage = 0;
			CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, nHypeOnDamage, hype_on_damage );
			if ( nHypeOnDamage && !pTFAttacker->m_Shared.InCond( TF_COND_SODAPOPPER_HYPE ) )
				pTFAttacker->m_Shared.SetScoutHypeMeter( info.GetDamage() / (350 / 100) );

			// build boost
			int nBoostOnDamage = 0;
			CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, nBoostOnDamage, boost_on_damage );
			if ( nBoostOnDamage )
				pTFAttacker->m_Shared.SetScoutHypeMeter( info.GetDamage() );

			float flAddChargeShield = 0.0f;
			CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pTFAttacker, flAddChargeShield, charge_meter_on_hit );
			if ( flAddChargeShield )
				pTFAttacker->m_Shared.SetShieldChargeMeter( min( ( pTFAttacker->m_Shared.GetShieldChargeMeter() + ( flAddChargeShield * 100 ) ), 100.0f ) );
		}

		int nMiniCritOnAirborne = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, nMiniCritOnAirborne, mini_crit_airborne );
		if ( nMiniCritOnAirborne && ( GetGroundEntity() == NULL ) )
		{
			bitsDamage |= DMG_MINICRITICAL;
			info.AddDamageType( DMG_MINICRITICAL );
		}

		int nMiniCritBecomeCrits = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, nMiniCritBecomeCrits, minicrits_become_crits );
		if ( nMiniCritBecomeCrits && ( bitsDamage & DMG_MINICRITICAL ) )
		{
			bitsDamage |= DMG_CRITICAL;
			info.AddDamageType( DMG_CRITICAL );
		}

		bool bAttackBehind = false;

		// Get the forward view vector of the target, ignore Z
		Vector vecVictimForward;
		AngleVectors( EyeAngles(), &vecVictimForward );
		vecVictimForward.z = 0.0f;
		vecVictimForward.NormalizeInPlace();

		// Get a vector from my origin to my targets origin
		Vector vecToTarget;
		vecToTarget = WorldSpaceCenter() - pAttacker->WorldSpaceCenter();
		vecToTarget.z = 0.0f;
		vecToTarget.NormalizeInPlace();

		// Get a forward vector of the attacker.
		Vector vecOwnerForward;
		AngleVectors( pAttacker->EyeAngles(), &vecOwnerForward );
		vecOwnerForward.z = 0.0f;
		vecOwnerForward.NormalizeInPlace();

		float flDotOwner = DotProduct( vecOwnerForward, vecToTarget );
		float flDotVictim = DotProduct( vecVictimForward, vecToTarget );

		// Make sure they're actually facing the target.
		// This needs to be done because lag compensation can place target slightly behind the attacker.
		if ( flDotOwner > 0.5 && ( flDotVictim > -0.1 ) )
		{
			bAttackBehind = true;
		}

		int nCritFromBehind = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, nCritFromBehind, crit_from_behind );
		if ( nCritFromBehind && bAttackBehind )
		{
			bitsDamage |= DMG_CRITICAL;
			info.AddDamageType( DMG_CRITICAL );
		}

		int nBackAttackMiniCrit = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, nBackAttackMiniCrit, closerange_backattack_minicrits );
		if ( nBackAttackMiniCrit && bAttackBehind )
		{
			bitsDamage |= DMG_MINICRITICAL;
			info.AddDamageType( DMG_MINICRITICAL );
		}

		int nCritForceLaugh = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, nCritForceLaugh, crit_forces_victim_to_laugh );
		if ( nCritForceLaugh && ( bitsDamage & DMG_CRITICAL ) )
		{
			bitsDamage &= ~( DMG_CRITICAL | DMG_MINICRITICAL | DMG_CLUB | DMG_SLASH );
			StunNPC( 3.0f, 0.0f, 0.0f, TF_STUNFLAGS_BIGBONK, pTFAttacker );// we don't laugh, just get a little confuse
			info.SetDamage( 0 );
		}

		int nCritNoDamage = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, nCritNoDamage, crit_does_no_damage );
		if ( nCritNoDamage && ( bitsDamage & DMG_CRITICAL ) )
		{
			bitsDamage &= ~( DMG_CRITICAL );
			info.SetDamage( 0 );
		}

		int iCritBecomeMiniCrits = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iCritBecomeMiniCrits, crits_become_minicrits );
		if ( iCritBecomeMiniCrits && ( bitsDamage & DMG_CRITICAL ) )
		{
			bitsDamage &= ~( DMG_CRITICAL );
			bitsDamage |= DMG_MINICRITICAL;
			info.AddDamageType( DMG_MINICRITICAL );
		}

		// Check if we're stunned and should have reduced damage taken
		/*if ( InCond( TF_COND_STUNNED ) && ( GetStunFlags() & TF_STUNFLAG_RESISTDAMAGE ) )
		{
			// Reduce our damage
			info.SetDamage( info.GetDamage() * m_flStunResistance );
		}*/
	}

	// If we're not damaging ourselves, apply randomness
	if ( pAttacker != this && !( bitsDamage & ( DMG_DROWN | DMG_FALL ) ) )
	{
		float flDamage = 0;
		if ( info.GetAmmoType() == TF_AMMO_PRIMARY && pWeapon && ( pWeapon->GetWeaponID() == TF_WEAPON_LASER_POINTER ) )
		{
			// Wrangled shots should have damage falloff
			bitsDamage |= DMG_USEDISTANCEMOD;

			// Distance should be calculated from sentry
			pAttacker = info.GetAttacker();
		}

		if ( ( bitsDamage & DMG_BLAST ) && ( pWeapon && ( pWeapon->GetWeaponID() == TF_WEAPON_CANNON ) ) )
		{
			float flTimeSinceDamage = gpGlobals->curtime - GetLastDamageTime();
			if ( flTimeSinceDamage < tf_double_donk_window.GetFloat() )
			{
				// Show the attacker, unless the target is a disguised spy
				if ( pAttacker && pAttacker->IsPlayer() && !InCond( TF_COND_DISGUISED ) )
				{
					CEffectData	data;
					data.m_nHitBox = GetParticleSystemIndex( "doubledonk_text" );
					data.m_vOrigin = WorldSpaceCenter() + Vector(0,0,32);
					data.m_vAngles = vec3_angle;
					data.m_nEntIndex = 0;

					CSingleUserRecipientFilter filter( (CBasePlayer*)pAttacker );
					te->DispatchEffect( filter, 0.0, data.m_vOrigin, "ParticleEffect", data );

					EmitSound_t params;
					params.m_flSoundTime = 0;
					params.m_pSoundName = "TFPlayer.DoubleDonk";
					EmitSound( filter, pAttacker->entindex(), params );

					bitsDamage |= DMG_MINICRITICAL;
				}
			}
		}

		if ( bitsDamage & DMG_CRITICAL )
		{
			if ( IsAlive() )
			{
				flDamage = info.GetDamage() * TF_DAMAGE_CRIT_MULTIPLIER;

				// Show the attacker
				if ( pAttacker && pAttacker->IsPlayer() )
				{
					CEffectData	data;
					data.m_nHitBox = GetParticleSystemIndex( "crit_text" );
					data.m_vOrigin = WorldSpaceCenter() + Vector(0,0,32);
					data.m_vAngles = vec3_angle;
					data.m_nEntIndex = 0;

					CSingleUserRecipientFilter filter( (CBasePlayer*) pAttacker );
					te->DispatchEffect( filter, 0.0, data.m_vOrigin, "ParticleEffect", data );

					EmitSound_t params;
					params.m_flSoundTime = 0;
					params.m_pSoundName = "TFPlayer.CritHit";
					EmitSound( filter, pAttacker->entindex(), params );
				}
			}
		}
		else if ( bitsDamage & DMG_MINICRITICAL )
		{
			if ( DMG_USEDISTANCEMOD )
			{
				// Do any calculations regarding bonus ramp up/falloff compensation.
				float flRandomDamage = info.GetDamage() * tf_damage_range.GetFloat();			
				if ( tf_damage_lineardist.GetBool() )
				{
					float flBaseDamage = info.GetDamage() - flRandomDamage;
					if ( pWeapon && ( pWeapon->GetWeaponID() == TF_WEAPON_CROSSBOW ) )
					{
						// If we're a crossbow, invert our damage formula.
						flDamage = flBaseDamage - RandomFloat( 0, flRandomDamage * 2 );
					}
					else
						flDamage = flBaseDamage + RandomFloat( 0, flRandomDamage * 2 );
					
					// Negate any potential damage falloff.
					if ( flDamage < info.GetDamage() )
						flDamage = info.GetDamage();
				}
				else  // If we have damage falloff, compensate falloff and add ramp up damage to our minicrit.
				{
					// Distance should be calculated from sentry
					pAttacker = info.GetAttacker();
					
					float flCenter = 0.5;
					float flCenVar = ( 10 / 100 ) ;
					float flMin = flCenter - flCenVar;
					float flMax = flCenter + flCenVar;


					float flDistance = Max( 1.0f, ( WorldSpaceCenter() - pAttacker->WorldSpaceCenter() ).Length() );
					float flOptimalDistance = 512.0;

					// We have damage ramp up, but no damage falloff for minicrit.
					flCenter = RemapValClamped( flDistance / flOptimalDistance, 0.0, 1.0, 1.0, 0.5 );
					if ( bitsDamage & DMG_NOCLOSEDISTANCEMOD )
					{
						if ( flCenter > 0.5 )
						{
							// Reduce the damage bonus at close range
							flCenter = RemapVal( flCenter, 0.5, 1.0, 0.5, 0.65 );
						}
					}
					
					if ( pWeapon && ( pWeapon->GetWeaponID() == TF_WEAPON_CROSSBOW ) )
					{
						// Unlike other weapons, we don't get any compensation here.
						flCenter = RemapVal( flDistance / flOptimalDistance, 0.0, 2.0, 0.0, 0.5 );
					}

					flMin = ( 0.0 > (flCenter + flCenVar) ? 0.0 : (flCenter + flCenVar) ); // Our version of MAX.
					flMax = ( 1.0 < (flCenter + flCenVar) ? 1.0 : (flCenter - flCenVar) ); // Our version of MIN.

					//Msg("Range: %.2f - %.2f\n", flMin, flMax );
					float flRandomVal;

					if ( tf_damage_disablespread.GetBool() )
					{
						flRandomVal = flCenter;
					}
					else
					{
						flRandomVal = RandomFloat( flMin, flMax );
					}

					if ( flRandomVal > 0.5 )
					{
						// Rocket launcher, Sticky launcher and Scattergun have different short range bonuses
						if ( pWeapon )
						{
							switch ( pWeapon->GetWeaponID() )
							{
								case TF_WEAPON_ROCKETLAUNCHER:
								case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
								case TF_WEAPON_PIPEBOMBLAUNCHER:
									// Rocket launcher and sticky launcher only have half the bonus of the other weapons at short range
									flRandomDamage *= 0.5;
									break;
								case TF_WEAPON_SCATTERGUN:
									// Scattergun gets 50% bonus of other weapons at short range
									flRandomDamage *= 1.5;
									break;

								case TF_WEAPON_SHOTGUN_PRIMARY:
								case TF_WEAPON_SHOTGUN_SOLDIER:
								case TF_WEAPON_SHOTGUN_HWG:
								case TF_WEAPON_SHOTGUN_PYRO:
								case TF_WEAPON_SENTRY_REVENGE:
									// Shotguns gets 10% bonus of other weapons at short range
									flRandomDamage *= 1.1;
									break;
							}
						}	
					}

					float flOut = SimpleSplineRemapValClamped( flRandomVal, 0, 1, -flRandomDamage, flRandomDamage );
					
					flDamage = ( info.GetDamage() + flOut );
				}
			}
			else
				flDamage = info.GetDamage(); // This is kind of anticlimatic.

			// Now we can finally add on the minicrit multiplier.
			flDamage *= TF_DAMAGE_MINICRIT_MULTIPLIER;

			// Show the attacker, unless the target is a disguised spy
			if ( pAttacker && pAttacker->IsPlayer() && !InCond( TF_COND_DISGUISED ) )
			{
				CEffectData	data;
				data.m_nHitBox = GetParticleSystemIndex( "minicrit_text" );
				data.m_vOrigin = WorldSpaceCenter() + Vector(0,0,32);
				data.m_vAngles = vec3_angle;
				data.m_nEntIndex = 0;

				CSingleUserRecipientFilter filter( (CBasePlayer*)pAttacker );
				te->DispatchEffect( filter, 0.0, data.m_vOrigin, "ParticleEffect", data );

				EmitSound_t params;
				params.m_flSoundTime = 0;
				params.m_pSoundName = "TFPlayer.CritHitMini";
				EmitSound( filter, pAttacker->entindex(), params );
			}
		}
		else if ( DMG_USEDISTANCEMOD )
		{
			float flRandomDamage = info.GetDamage() * tf_damage_range.GetFloat();
			if ( tf_damage_lineardist.GetBool() )
			{
				float flBaseDamage = info.GetDamage() - flRandomDamage;
				if ( pWeapon && ( pWeapon->GetWeaponID() == TF_WEAPON_CROSSBOW ) )
				{
					// If we're a crossbow, invert our damage formula.
					flDamage = flBaseDamage - RandomFloat( 0, flRandomDamage * 2 );
				}
				else
					flDamage = flBaseDamage + RandomFloat( 0, flRandomDamage * 2 );
			}
			else
			{
				float flCenter = 0.5;
				float flCenVar = ( 10 / 100 ) ;
				float flMin = flCenter - flCenVar;
				float flMax = flCenter + flCenVar;

				if ( bitsDamage & DMG_USEDISTANCEMOD )
				{
					float flDistance = Max( 1.0f, ( WorldSpaceCenter() - pAttacker->WorldSpaceCenter() ).Length() );
					float flOptimalDistance = 512.0;

					flCenter = RemapValClamped( flDistance / flOptimalDistance, 0.0, 2.0, 1.0, 0.0 );
					if ( bitsDamage & DMG_NOCLOSEDISTANCEMOD )
					{
						if ( flCenter > 0.5 )
						{
							// Reduce the damage bonus at close range
							flCenter = RemapVal( flCenter, 0.5, 1.0, 0.5, 0.65 );
						}
					}
					
					if ( pWeapon && ( pWeapon->GetWeaponID() == TF_WEAPON_CROSSBOW ) )
					{
						// If we're a crossbow, change our falloff band so that our 100% is at long range.
						flCenter = RemapVal( flDistance / flOptimalDistance, 0.0, 2.0, 0.0, 0.5 );
					}
					
					flMin = ( 0.0 > (flCenter + flCenVar) ? 0.0 : (flCenter + flCenVar) ); // Our version of MAX.
					flMax = ( 1.0 < (flCenter + flCenVar) ? 1.0 : (flCenter - flCenVar) ); // Our version of MIN.
				}

				//Msg("Range: %.2f - %.2f\n", flMin, flMax );
				float flRandomVal;

				if ( tf_damage_disablespread.GetBool() )
				{
					flRandomVal = flCenter;
				}
				else
				{
					flRandomVal = RandomFloat( flMin, flMax );
				}

				if ( flRandomVal > 0.5 )
				{
					// Rocket launcher, Sticky launcher and Scattergun have different short range bonuses
					if ( pWeapon )
					{
						switch ( pWeapon->GetWeaponID() )
						{
							case TF_WEAPON_ROCKETLAUNCHER:
							case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
							case TF_WEAPON_PIPEBOMBLAUNCHER:
								// Rocket launcher and sticky launcher only have half the bonus of the other weapons at short range
								flRandomDamage *= 0.5;
								break;
							case TF_WEAPON_SCATTERGUN:
								// Scattergun gets 50% bonus of other weapons at short range
								flRandomDamage *= 1.5;
								break;

							case TF_WEAPON_SHOTGUN_PRIMARY:
							case TF_WEAPON_SHOTGUN_SOLDIER:
							case TF_WEAPON_SHOTGUN_HWG:
							case TF_WEAPON_SHOTGUN_PYRO:
							case TF_WEAPON_SENTRY_REVENGE:
								// Shotguns gets 10% bonus of other weapons at short range
								flRandomDamage *= 1.1;
								break;
						}
					}
				}

				float flOut = SimpleSplineRemapValClamped( flRandomVal, 0, 1, -flRandomDamage, flRandomDamage );
				
				flDamage = info.GetDamage() + flOut;

				/*
				for ( float flVal = flMin; flVal <= flMax; flVal += 0.05 )
				{
					float flOut = SimpleSplineRemapValClamped( flVal, 0, 1, -flRandomDamage, flRandomDamage );
					Msg("Val: %.2f, Out: %.2f, Dmg: %.2f\n", flVal, flOut, info.GetDamage() + flOut );
				}
				*/
			}
		}
		else
			flDamage = info.GetDamage(); // This is kind of anticlimatic.

		info.SetDamage( flDamage );
	}

	if ( InCond( TF_COND_PREVENT_DEATH ) ) 
	{
		if ((m_iHealth - info.GetDamage()) <= 0)
		{
			m_iHealth = 1;
			RemoveCond( TF_COND_PREVENT_DEATH );
			return 0;
		}
	}

	if ( InCond( TF_COND_HALLOWEEN_IN_HELL ) ) 
	{
		if ( (m_iHealth - info.GetDamage()) <= 0 )
		{
			m_iHealth = 1;
			AddCond( TF_COND_HALLOWEEN_GHOST_MODE );
			return 0;
		}
	}

	if ( InCond( TF_COND_RUNE_WARLOCK ) )
	{
		if ( pAttacker && ( pAttacker->GetHealth() - ( info.GetDamage() / 1.2 ) ) >= 1 )
		{
			CTakeDamageInfo newInfo( this, this, info.GetDamage() / 1.2, DMG_GENERIC, TF_DMG_CUSTOM_RUNE_REFLECT );
			pAttacker->TakeDamage( info );
			EmitSound( "Powerup.Reflect.Reflect" );
		}
	}

	RunAnimation();

	if ( pWeapon && pTFAttacker )
	{
		int nAddCloakOnHit = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, nAddCloakOnHit, add_cloak_on_hit );
		if ( nAddCloakOnHit > 0 )
			pTFAttacker->m_Shared.AddToSpyCloakMeter( nAddCloakOnHit );
	}

	float flDamageTaken = info.GetDamage();
	// Vintage Battalion's Backup resists
	if ( InCond( TF_COND_DEFENSEBUFF ) )
	{
		// Battalion's Backup negates all crit damage
		bitsDamage &= ~( DMG_CRITICAL | DMG_MINICRITICAL );
		if ( bObject )
		{
			// 50% resistance to sentry damage
			info.SetDamage( flDamageTaken * 0.50f );
		}
		else
		{
			// 35% to all other sources
			info.SetDamage( flDamageTaken * 0.65f );
		}
	}

	if ( InCond( TF_COND_RUNE_RESIST ) )
	{
		bitsDamage &= ~( DMG_CRITICAL | DMG_MINICRITICAL );
		info.SetDamage( flDamageTaken * 0.50f );
		EmitSound( "Powerup.ReducedDamage" );
	}

	// Post modified weapon effects
	if ( pWeapon )
		pWeapon->ApplyPostHitEffects( inputInfo, this );

	int ret = BaseClass::OnTakeDamage( info );

	if ( !ret )
		return 0;

	return ret;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CAI_BaseNPC::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	int iOldHealth = m_iHealth;
	CBaseEntity *pAttacker = info.GetAttacker();
	CBaseEntity *pInflictor = info.GetInflictor();
	CBaseEntity *pWeapon = info.GetWeapon();
	CTFPlayer *pTFAttacker = ToTFPlayer( pAttacker );
	CTFWeaponBase *pTFWeapon = dynamic_cast<CTFWeaponBase *>( pWeapon );
	CTFPlayer *pScorer = ToTFPlayer( TFGameRules()->GetDeathScorer( pAttacker, pInflictor, this ) );
	CBaseEntity *pAssister =  TFGameRules()->GetAssister( this, pScorer, pInflictor );
	CTFPlayer *pTFAssister = ToTFPlayer( pAssister );
	float flDamage = info.GetDamage();

	Vector vecDir = vec3_origin;
	if ( pInflictor )
	{
		// Huntsman should not do too much knockback
		if ( pTFWeapon && ( pTFWeapon->GetWeaponID() == TF_WEAPON_COMPOUND_BOW || pTFWeapon->GetWeaponID() == TF_WEAPON_CROSSBOW ) )
		{
			vecDir = -info.GetDamageForce();
		}
		else
		{
			vecDir = pInflictor->WorldSpaceCenter() - Vector( 0.0f, 0.0f, 10.0f ) - WorldSpaceCenter();
		}
		VectorNormalize( vecDir );
	}
	g_vecAttackDir = vecDir;

	Forget( bits_MEMORY_INCOVER );

	if ( !BaseClass::OnTakeDamage_Alive( info ) )
		return 0;

	if ( GetSleepState() == AISS_WAITING_FOR_THREAT )
		Wake();

	// NOTE: This must happen after the base class is called; we need to reduce
	// health before the pain sound, since some NPCs use the final health
	// level as a modifier to determine which pain sound to use.

	// REVISIT: Combine soldiers shoot each other a lot and then talk about it
	// this improves that case a bunch, but it seems kind of harsh.
	if (!m_pSquad || !m_pSquad->SquadIsMember(info.GetAttacker()))
	{
		PainSound(info);// "Ouch!"
	}

	// See if we're running a dynamic interaction that should break when I am damaged.
	if ( IsActiveDynamicInteraction() )
	{
		ScriptedNPCInteraction_t *pInteraction = GetRunningDynamicInteraction();
		if ( pInteraction->iLoopBreakTriggerMethod & SNPCINT_LOOPBREAK_ON_DAMAGE )
		{
			// Can only break when we're in the action anim
			if ( m_hCine->IsPlayingAction() )
			{
				m_hCine->StopActionLoop( true );
			}
		}
	}

	// If we're not allowed to die, refuse to die
	// Allow my interaction partner to kill me though
	if ( m_iHealth <= 0 && HasInteractionCantDie() && pAttacker != m_hInteractionPartner )
	{
		m_iHealth = 1;
	}

	// -----------------------------------
	//  Fire outputs
 	// -----------------------------------
	if ( m_flLastDamageTime != gpGlobals->curtime && pAttacker )
	{
		// only fire once per frame
		m_OnDamaged.FireOutput( pAttacker, this);

		if( pAttacker->IsPlayer() || pAttacker->IsBaseObject() )
		{
			m_OnDamagedByPlayer.FireOutput( pAttacker, this );

			// This also counts as being harmed by player's squad.
			m_OnDamagedByPlayerSquad.FireOutput( pAttacker, this );
		}
		else if ( pAttacker->IsNPC() )
		{
			// See if the person that injured me is an NPC.
			CAI_BaseNPC *pNPCAttacker = dynamic_cast<CAI_BaseNPC *>( pAttacker );
			if( pNPCAttacker && pNPCAttacker->IsAlive() && UTIL_GetNearestPlayer( GetAbsOrigin() ) ) 
			{
				if( pNPCAttacker->GetSquad() != NULL && pNPCAttacker->IsInPlayerSquad() )
				{
					m_OnDamagedByPlayerSquad.FireOutput( pAttacker, this );
				}
			}
		}
	}

	if( (info.GetDamageType() & DMG_CRUSH) && !(info.GetDamageType() & DMG_PHYSGUN) && flDamage >= MIN_PHYSICS_FLINCH_DAMAGE )
	{
		SetCondition( COND_PHYSICS_DAMAGE );
	}

	if ( m_iHealth <= ( m_iMaxHealth / 2 ) )
	{
		m_OnHalfHealth.FireOutput( pAttacker, this );
	}

	// react to the damage (get mad)
	if ( ( (GetFlags() & FL_NPC) == 0 ) || !pAttacker )
		return 1;

	// If the attacker was an NPC or client update my position memory
	if ( pAttacker->GetFlags() & (FL_NPC | FL_CLIENT) )
	{
		// ------------------------------------------------------------------
		//				DO NOT CHANGE THIS CODE W/O CONSULTING
		// Only update information about my attacker I don't see my attacker
		// ------------------------------------------------------------------
		if ( !FInViewCone( pAttacker ) || !FVisible( pAttacker ) )
		{
			// -------------------------------------------------------------
			//  If I have an inflictor (enemy / grenade) update memory with
			//  position of inflictor, otherwise update with an position
			//  estimate for where the attack came from
			// ------------------------------------------------------
			Vector vAttackPos;
			if ( pInflictor )
			{
				vAttackPos = pInflictor->GetAbsOrigin();
			}
			else
			{
				vAttackPos = (GetAbsOrigin() + ( g_vecAttackDir * 64 ));
			}


			// ----------------------------------------------------------------
			//  If I already have an enemy, assume that the attack
			//  came from the enemy and update my enemy's position
			//  unless I already know about the attacker or I can see my enemy
			// ----------------------------------------------------------------
			if ( GetEnemy() != NULL							&&
				!GetEnemies()->HasMemory( pAttacker )			&&
				!HasCondition(COND_SEE_ENEMY)	)
			{
				UpdateEnemyMemory(GetEnemy(), vAttackPos, GetEnemy());
			}
			// ----------------------------------------------------------------
			//  If I already know about this enemy, update his position
			// ----------------------------------------------------------------
			else if (GetEnemies()->HasMemory( pAttacker ))
			{
				UpdateEnemyMemory( pAttacker, vAttackPos );
			}
			// -----------------------------------------------------------------
			//  Otherwise just note the position, but don't add enemy to my list
			// -----------------------------------------------------------------
			else
			{
				UpdateEnemyMemory(NULL, vAttackPos);
			}
		}

		// add pain to the conditions
		if ( IsLightDamage( info ) )
		{
			SetCondition( COND_LIGHT_DAMAGE );
		}
		if ( IsHeavyDamage( info ) )
		{
			SetCondition( COND_HEAVY_DAMAGE );
		}

		ForceGatherConditions();

		// Keep track of how much consecutive damage I have recieved
		if ((gpGlobals->curtime - m_flLastDamageTime) < 1.0)
		{
			m_flSumDamage += flDamage;
		}
		else
		{
			m_flSumDamage = flDamage;
		}
		m_flLastDamageTime = gpGlobals->curtime;
		if ( pAttacker && pAttacker->IsPlayer() )
			m_flLastPlayerDamageTime = gpGlobals->curtime;
		GetEnemies()->OnTookDamageFrom( pAttacker );

		if (m_flSumDamage > m_iMaxHealth*0.3)
		{
			SetCondition(COND_REPEATED_DAMAGE);
		}
	
		NotifyFriendsOfDamage( pAttacker );
	}

	// ---------------------------------------------------------------
	//  Insert a combat sound so that nearby NPCs know I've been hit
	// ---------------------------------------------------------------
	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 1024, 0.5, this, SOUNDENT_CHANNEL_INJURY );

	if ( flDamage == 0.0f )
		return 0;

	// Self-damage modifiers.
	if ( pAttacker == this )
	{
		if ( ( info.GetDamageType() & DMG_BLAST ) && !info.GetDamagedOtherPlayers() )
		{
			CALL_ATTRIB_HOOK_FLOAT( flDamage, rocket_jump_dmg_reduction );
		}

		if ( pWeapon )
		{
			int iNoSelfDmg = 0;
			CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iNoSelfDmg, no_self_blast_dmg );

			if ( iNoSelfDmg )
			{
				flDamage = 0.0f;
			}
			else
			{
				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flDamage, blast_dmg_to_self );
			}
		}
	}

	int nIgnoreResists = 0;
	if ( pAttacker != this && pWeapon )
	{
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, nIgnoreResists, mod_pierce_resists_absorbs );
		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flDamage, mult_dmg_vs_players );
	}

	if ( info.GetDamageType() & DMG_CRITICAL )
	{
		float flDamageCritMult = 1.0f;
		CALL_ATTRIB_HOOK_FLOAT( flDamageCritMult, mult_dmgtaken_from_crit );
		if ( (nIgnoreResists == 1) && ( flDamageCritMult < 1.0f ) )
			flDamageCritMult = 1.0f;
		flDamage *= flDamageCritMult;
	}

	if ( info.GetDamageType() & DMG_BLAST )
	{
		float flDamageBlastMult = 1.0f;
		CALL_ATTRIB_HOOK_FLOAT( flDamageBlastMult, mult_dmgtaken_from_explosions );

		if ( InCond( TF_COND_MEDIGUN_SMALL_BLAST_RESIST ) )
			CALL_ATTRIB_HOOK_FLOAT( flDamageBlastMult, medigun_blast_resist_passive );

		if ( InCond( TF_COND_MEDIGUN_UBER_BLAST_RESIST ) )
			CALL_ATTRIB_HOOK_FLOAT( flDamageBlastMult, medigun_blast_resist_deployed );

		if ( (nIgnoreResists == 1) && ( flDamageBlastMult < 1.0f ) )
			flDamageBlastMult = 1.0f;

		flDamage *= flDamageBlastMult;

		if ( InCond( TF_COND_BLAST_IMMUNE ) )
			flDamage = 0.0;
	}

	if ( info.GetDamageType() & DMG_BURN|DMG_IGNITE )
	{
		float flDamageFireMult = 1.0f;
		CALL_ATTRIB_HOOK_FLOAT( flDamageFireMult, mult_dmgtaken_from_fire );

		if ( InCond( TF_COND_MEDIGUN_SMALL_FIRE_RESIST ) )
			CALL_ATTRIB_HOOK_FLOAT( flDamageFireMult, medigun_fire_resist_passive );

		if ( InCond( TF_COND_MEDIGUN_UBER_FIRE_RESIST ) )
			CALL_ATTRIB_HOOK_FLOAT( flDamageFireMult, medigun_fire_resist_deployed );

		if ( (nIgnoreResists == 1) && ( flDamageFireMult < 1.0f ) )
			flDamageFireMult = 1.0f;

		flDamage *= flDamageFireMult;

		if ( InCond( TF_COND_FIRE_IMMUNE ) )
			flDamage = 0.0f;
	}

	if ( info.GetDamageType() & DMG_BULLET|DMG_BUCKSHOT )
	{
		float flDamageBulletMult = 1.0f;
		CALL_ATTRIB_HOOK_FLOAT( flDamageBulletMult, mult_dmgtaken_from_bullets );

		if ( InCond( TF_COND_MEDIGUN_SMALL_BULLET_RESIST ) )
			CALL_ATTRIB_HOOK_FLOAT( flDamageBulletMult, medigun_bullet_resist_passive );

		if ( InCond( TF_COND_MEDIGUN_UBER_BULLET_RESIST ) )
			CALL_ATTRIB_HOOK_FLOAT( flDamageBulletMult, medigun_bullet_resist_deployed );

		flDamage *= flDamageBulletMult;

		if ( InCond( TF_COND_BULLET_IMMUNE ) )
			flDamage = 0.0f;
	}

	if ( info.GetDamageType() & DMG_CLUB|DMG_SLASH )
	{
		float flDamageMeleeMult = 1.0f;
		CALL_ATTRIB_HOOK_FLOAT( flDamageMeleeMult, mult_dmgtaken_from_melee );
		if ( (nIgnoreResists == 1) && ( flDamageMeleeMult < 1.0f ) )
			flDamageMeleeMult = 1.0f;
		flDamage *= flDamageMeleeMult;
	}

	float flDamageAllMult = 1.0f;
	CALL_ATTRIB_HOOK_FLOAT( flDamageAllMult, mult_dmgtaken );
	if ( (nIgnoreResists == 1) && ( flDamageAllMult < 1.0f ) )
		flDamageAllMult = 1.0f;
	flDamage *= flDamageAllMult;

	if ( pInflictor && pInflictor->IsBaseObject() )
	{
		float flDamageSentryMult = 1.0f;
		CALL_ATTRIB_HOOK_FLOAT( flDamageSentryMult, dmg_from_sentry_reduced );
		if ( (nIgnoreResists == 1) && ( flDamageSentryMult < 1.0f ) )
			flDamageSentryMult = 1.0f;
		flDamage *= flDamageSentryMult;
	}

	if ( GetActiveWeapon() )
	{
		if ( info.GetDamageType() & DMG_CLUB )
		{
			float flDamageClubMult = 1.0f;
			CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( GetActiveWeapon(), flDamageClubMult, dmg_from_melee );
			if ( (nIgnoreResists == 1) && ( flDamageClubMult < 1.0f ) )
				flDamageClubMult = 1.0f;
			flDamage *= flDamageClubMult;
		}

		if ( info.GetDamageType() & DMG_BULLET|DMG_BUCKSHOT|DMG_IGNITE|DMG_BLAST|DMG_SONIC )
		{
			float flDamageRangedMult = 1.0f;
			CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( GetActiveWeapon(), flDamageRangedMult, dmg_from_ranged );
			if ( (nIgnoreResists == 1) && ( flDamageRangedMult < 1.0f ) )
				flDamageRangedMult = 1.0f;
			flDamage *= flDamageRangedMult;
		}
		
		if ( info.GetDamageType() & DMG_BURN|DMG_IGNITE )
		{
			float flDamageFireActiveMult = 1.0f;
			CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( GetActiveWeapon(), flDamageFireActiveMult, mult_dmgtaken_from_fire_active );
			if ( (nIgnoreResists == 1) && ( flDamageFireActiveMult < 1.0f ) )
				flDamageFireActiveMult = 1.0f;
			flDamage *= flDamageFireActiveMult;
		}

		if ( InCond( TF_COND_AIMING ) )
		{
			if ( ( GetHealth() < ( GetMaxHealth() / 2 ) ) )
			{
				float flDamageAimMult = 1.0f;
				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( GetActiveWeapon(), flDamageAimMult, spunup_damage_resistance );
				if ( (nIgnoreResists == 1) && ( flDamageAimMult < 1.0f ) )
					flDamageAimMult = 1.0f;
				flDamage *= flDamageAimMult;
			}
		}
	}

	bool bIgniting = false;
	float flBleeding = 0.0f;

	if ( m_takedamage != DAMAGE_EVENTS_ONLY )
	{
		// Start burning if we took ignition damage
		bIgniting = ( ( info.GetDamageType() & DMG_IGNITE ) && ( GetWaterLevel() < WL_Waist ) );
		if ( !bIgniting )
		{
			int iIgniting = 0;
			CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, iIgniting, set_dmgtype_ignite );
			bIgniting = ( iIgniting > 0 );

			if ( !bIgniting )
				bIgniting = InCond( TF_COND_GAS );
		}

		if ( InCond( TF_COND_GRAPPLINGHOOK_LATCHED ) )
			flBleeding = 0.5f;

		if ( info.GetDamageCustom() != TF_DMG_CUSTOM_BLEEDING )
			CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pTFWeapon, flBleeding, bleeding_duration );

		// Take damage - round to the nearest integer.
		m_iHealth -= ( flDamage + 0.5f );
	}

	m_flLastDamageTime = gpGlobals->curtime;

	// Apply a damage force.
	if ( !pAttacker )
		return 0;

	ApplyPushFromDamage( info, vecDir );

	if ( bIgniting )
		Burn( pTFAttacker, pTFWeapon );

	if ( flBleeding > 0.0f )
	{
		int flBleedDamage = TF_BLEEDING_DAMAGE;
		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pTFWeapon, flBleedDamage, mult_wpn_bleeddmg );
		MakeBleed( pTFAttacker, pTFWeapon, flBleeding, flBleedDamage );
	}

	if ( pTFWeapon )
	{
		// Special case for Sydney, we base it on charge
		if ( pTFWeapon->IsWeapon( TF_WEAPON_SNIPERRIFLE ) )
		{
			CTFSniperRifle *pSniper = assert_cast<CTFSniperRifle *>( pTFWeapon );

			float flJarateDuration = pSniper->GetJarateTime();
			if ( flJarateDuration > 0.0f )
				AddCond( TF_COND_URINE, flJarateDuration );
		}
		else
		{
			float flJarateDuration = 0.0f;
			CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pTFWeapon, flJarateDuration, jarate_duration );
			if ( flJarateDuration > 0.0f )
				AddCond( TF_COND_URINE, flJarateDuration );
		}
	}

	// Fire a global game event - "npc_hurt"
	IGameEvent *event = gameeventmanager->CreateEvent( "npc_hurt" );
	if ( event )
	{
		event->SetInt( "victim_index", entindex() );
		event->SetInt( "attacker_index", info.GetAttacker() ? info.GetAttacker()->entindex() : 0 );

		event->SetInt( "health", max( 0, m_iHealth ) );
		event->SetInt( "damageamount", ( iOldHealth - m_iHealth ) );
		event->SetBool( "crit", ( info.GetDamageType() & DMG_CRITICAL ) != 0 );
		event->SetBool( "minicrit", ( info.GetDamageType() & DMG_MINICRITICAL ) != 0 );

		CBasePlayer *pPlayer = ToBasePlayer( pAttacker );
		event->SetInt( "attacker", pPlayer ? pPlayer->GetUserID() : 0 );

		// HLTV event priority, not transmitted
		event->SetInt( "priority", 5 );

		gameeventmanager->FireEvent( event );
	}

	if ( pTFWeapon && pTFWeapon->IsWeapon( TF_WEAPON_BAT_FISH ) )
	{
		int iFishOverride = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pTFWeapon, iFishOverride, fish_damage_override );

		const char *pszFishEvent = "fish_notice";
		if ( iFishOverride == 1 )
			pszFishEvent = "fish_notice__arm";

		IGameEvent *eventfish = gameeventmanager->CreateEvent( pszFishEvent );
		if ( eventfish )
		{
			event->SetInt( "userid", 0 );
			event->SetInt( "victim_index", entindex() );
			event->SetInt( "attacker", pTFAttacker ? pTFAttacker->GetUserID() : 0 );
			event->SetInt( "attacker_index", pAttacker->entindex() );
			event->SetString( "attacker_name", pAttacker ? pAttacker->GetClassname() : NULL );
			event->SetInt( "attacker_team", pAttacker ? pAttacker->GetTeamNumber() : 0 );
			event->SetInt( "assister", pTFAssister ? pTFAssister->GetUserID() : -1 );
			event->SetInt( "assister_index", pAssister ? pAssister->entindex() : -1 );
			event->SetString( "assister_name", pAssister ? pAssister->GetClassname() : NULL );
			event->SetInt( "assister_team", pAssister ? pAssister->GetTeamNumber() : 0 );
			event->SetString( "weapon", ( iFishOverride == 1 ) ? "unarmed_combat" : "holymackerel" );
			event->SetInt( "weaponid", pTFWeapon->GetWeaponID() );
			event->SetString( "weapon_logclassname", ( iFishOverride == 1 ) ? "unarmed_combat" : "holymackerel" );
			event->SetInt( "damagebits", info.GetDamageType() );
			event->SetInt( "customkill", info.GetDamageCustom() );
			event->SetInt( "priority", 7 );	// HLTV event priority, not transmitted
			event->SetInt( "death_flags", 0 );

			gameeventmanager->FireEvent( event );
		}
	}

	if ( pTFAttacker )
		pTFAttacker->RecordDamageEvent( info, (m_iHealth <= 0) );

	return 1;
}

static float NPCDamageForce( const Vector &size, float damage, float scale )
{ 
	float force;

	force = damage * ((24 * 24 * 64.0) / (size.x * size.y * size.z)) * scale;
	
	if ( force > 1000.0) 
	{
		force = 1000.0;
	}

	return force;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_BaseNPC::ApplyPushFromDamage( const CTakeDamageInfo &info, Vector &vecDir )
{
	CBaseEntity *pAttacker = info.GetAttacker();
	CBaseEntity *pInflictor = info.GetInflictor();

	if ( info.GetDamageType() & DMG_PREVENT_PHYSICS_FORCE )
		return;

	if( !pInflictor ||
		( GetMoveType() != MOVETYPE_WALK ) ||
		( pAttacker->IsSolidFlagSet( FSOLID_TRIGGER ) ) ||
		( InCond( TF_COND_DISGUISED ) ) )
		return;

	float flDamage = info.GetDamage();

	float flForceMultiplier = 1.0f;
	Vector vecForce;
	vecForce.Init();
	if ( pAttacker == this )
	{
		vecForce = vecDir * -NPCDamageForce( WorldAlignSize(), flDamage, NPC_DAMAGE_FORCE_SCALE_SELF );

		// If we damaged ourselves, check the force modifier attribute.
		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pAttacker, flForceMultiplier, mult_dmgself_push_force );
		if ( flForceMultiplier != 1.0f )
			vecForce *= flForceMultiplier;
	}
	else
	{
		// Sentryguns push a lot harder
		if ( ( info.GetDamageType() & DMG_BULLET ) && pInflictor->IsBaseObject() )
		{
			vecForce = vecDir * -NPCDamageForce( WorldAlignSize(), flDamage, 16 );
		}
		else
		{
			vecForce = vecDir * -NPCDamageForce( WorldAlignSize(), flDamage, tf_damageforcescale_other.GetFloat() );
		}

		// If we were damaged by someone else, reduce the amount of force with an attribute.
		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pAttacker, flForceMultiplier, damage_force_reduction );
		if ( flForceMultiplier != 1.0f )
			vecForce *= flForceMultiplier;

		if ( InCond( TF_COND_MEGAHEAL ) )
			vecForce *= 0.25f;
	}

	ApplyAbsVelocityImpulse( vecForce );

	for ( int i = 0; i < m_aHealers.Count(); i++ )
	{
		CTFPlayer *pHealer = ToTFPlayer( static_cast<CBaseEntity *>( GetHealerByIndex( i ) ) );
		if ( pHealer && pHealer->GetMedigun() && ( pHealer->GetMedigun()->GetMedigunType() == TF_MEDIGUN_QUICKFIX ) )
			pHealer->ApplyAbsVelocityImpulse( vecForce );
	}
}

//=========================================================
// OnTakeDamage_Dying - takedamage function called when a npc's
// corpse is damaged.
//=========================================================
int CAI_BaseNPC::OnTakeDamage_Dying( const CTakeDamageInfo &info )
{
	if ( info.GetDamageType() & DMG_PLASMA )
	{
		if ( m_takedamage != DAMAGE_EVENTS_ONLY )
		{
			m_iHealth -= info.GetDamage();

			if (m_iHealth < -500)
			{
				UTIL_Remove(this);
			}
		}
	}
	return BaseClass::OnTakeDamage_Dying( info );
}

//=========================================================
// OnTakeDamage_Dead - takedamage function called when a npc's
// corpse is damaged.
//=========================================================
int CAI_BaseNPC::OnTakeDamage_Dead( const CTakeDamageInfo &info )
{
	Vector			vecDir;

	// grab the vector of the incoming attack. ( pretend that the inflictor is a little lower than it really is, so the body will tend to fly upward a bit).
	vecDir = vec3_origin;
	if ( info.GetInflictor() )
	{
		vecDir = info.GetInflictor()->WorldSpaceCenter() - Vector ( 0, 0, 10 ) - WorldSpaceCenter();
		VectorNormalize( vecDir );
		g_vecAttackDir = vecDir;
	}

#if 0// turn this back on when the bounding box issues are resolved.

	SetGroundEntity( NULL );
	GetLocalOrigin().z += 1;

	// let the damage scoot the corpse around a bit.
	if ( info.GetInflictor() && (info.GetAttacker()->GetSolid() != SOLID_TRIGGER) )
	{
		ApplyAbsVelocityImpulse( vecDir * -DamageForce( flDamage ) );
	}

#endif

	// kill the corpse if enough damage was done to destroy the corpse and the damage is of a type that is allowed to destroy the corpse.
	if ( g_pGameRules->Damage_ShouldGibCorpse( info.GetDamageType() ) )
	{
		// Accumulate corpse gibbing damage, so you can gib with multiple hits
		if ( m_takedamage != DAMAGE_EVENTS_ONLY )
		{
			m_iHealth -= info.GetDamage() * 0.1;
		}
	}

	if ( info.GetDamageType() & DMG_PLASMA )
	{
		if ( m_takedamage != DAMAGE_EVENTS_ONLY )
		{
			m_iHealth -= info.GetDamage();

			if (m_iHealth < -500)
			{
				UTIL_Remove(this);
			}
		}
	}

	return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_BaseNPC::NotifyFriendsOfDamage( CBaseEntity *pAttackerEntity )
{
	CAI_BaseNPC *pAttacker = pAttackerEntity->MyNPCPointer();
	if ( pAttacker )
	{
		const Vector &origin = GetAbsOrigin();
		for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
		{
			const float NEAR_Z		= 10*12;
			const float NEAR_XY_SQ	= Square( 50*12 );
			CAI_BaseNPC *pNpc = g_AI_Manager.AccessAIs()[i];
			if ( pNpc && pNpc != this )
			{
				const Vector &originNpc = pNpc->GetAbsOrigin();
				if ( fabsf( originNpc.z - origin.z ) < NEAR_Z )
				{
					if ( (originNpc.AsVector2D() - origin.AsVector2D()).LengthSqr() < NEAR_XY_SQ )
					{
						if ( pNpc->GetSquad() == GetSquad() || IRelationType( pNpc ) == D_LI )
							pNpc->OnFriendDamaged( this, pAttacker );
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_BaseNPC::OnFriendDamaged( CBaseCombatCharacter *pSquadmate, CBaseEntity *pAttacker )
{
	if ( GetSleepState() != AISS_WAITING_FOR_INPUT )
	{
		float distSqToThreat = ( GetAbsOrigin() - pAttacker->GetAbsOrigin() ).LengthSqr();

		if ( GetSleepState() != AISS_AWAKE && distSqToThreat < Square( 20 * 12 ) )
			Wake();

		if ( distSqToThreat < Square( 50 * 12 ) )
			ForceGatherConditions();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::IsLightDamage( const CTakeDamageInfo &info )
{
	// ALL nonzero damage is light damage! Mask off COND_LIGHT_DAMAGE if you want to ignore light damage.
	return ( info.GetDamage() >  0 );
}

bool CAI_BaseNPC::IsHeavyDamage( const CTakeDamageInfo &info )
{
	return ( info.GetDamage() >  20 );
}

void CAI_BaseNPC::DoRadiusDamage( const CTakeDamageInfo &info, int iClassIgnore, CBaseEntity *pEntityIgnore )
{
	RadiusDamage( info, GetAbsOrigin(), info.GetDamage() * 2.5, iClassIgnore, pEntityIgnore );
}


void CAI_BaseNPC::DoRadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrc, int iClassIgnore, CBaseEntity *pEntityIgnore )
{
	RadiusDamage( info, vecSrc, info.GetDamage() * 2.5, iClassIgnore, pEntityIgnore );
}


//-----------------------------------------------------------------------------
// Set the contents types that are solid by default to all NPCs
//-----------------------------------------------------------------------------
unsigned int CAI_BaseNPC::PhysicsSolidMaskForEntity( void ) const 
{ 
	int teamContents = 0;

	switch ( GetTeamNumber() )
	{
	case TF_TEAM_RED:
		teamContents = CONTENTS_BLUETEAM | CONTENTS_GREENTEAM | CONTENTS_YELLOWTEAM;
		break;

	case TF_TEAM_BLUE:
		teamContents = CONTENTS_REDTEAM | CONTENTS_GREENTEAM | CONTENTS_YELLOWTEAM;
		break;

	case TF_TEAM_GREEN:
		teamContents = CONTENTS_REDTEAM | CONTENTS_BLUETEAM | CONTENTS_YELLOWTEAM;
		break;

	case TF_TEAM_YELLOW:
		teamContents = CONTENTS_REDTEAM | CONTENTS_BLUETEAM | CONTENTS_GREENTEAM;
		break;

	default:
		teamContents = CONTENTS_REDTEAM | CONTENTS_BLUETEAM | CONTENTS_GREENTEAM | CONTENTS_YELLOWTEAM;
		break;
	}

	return GetAITraceMask() | teamContents;
}


//=========================================================

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::DecalTrace( trace_t *pTrace, char const *decalName )
{
	if ( m_fNoDamageDecal )
	{
		m_fNoDamageDecal = false;
		// @Note (toml 04-23-03): e3, don't decal face on damage if still alive
		return;
	}
	BaseClass::DecalTrace( pTrace, decalName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
{
	if ( m_fNoDamageDecal )
	{
		m_fNoDamageDecal = false;
		// @Note (toml 04-23-03): e3, don't decal face on damage if still alive
		return;
	}
	BaseClass::ImpactTrace( pTrace, iDamageType, pCustomImpactName );
}

//---------------------------------------------------------
// Return the number by which to multiply incoming damage
// based on the hitgroup it hits. This exists mainly so
// that individual NPC's can have more or less resistance
// to damage done to certain hitgroups.
//---------------------------------------------------------
float CAI_BaseNPC::GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info )
{
	/*
	switch( iHitGroup )
	{
	case HITGROUP_GENERIC:
		return 1.0f;

	case HITGROUP_HEAD:
		return sk_npc_head.GetFloat();

	case HITGROUP_CHEST:
		return sk_npc_chest.GetFloat();

	case HITGROUP_STOMACH:
		return sk_npc_stomach.GetFloat();

	case HITGROUP_LEFTARM:
	case HITGROUP_RIGHTARM:
		return sk_npc_arm.GetFloat();

	case HITGROUP_LEFTLEG:
	case HITGROUP_RIGHTLEG:
		return sk_npc_leg.GetFloat();

	default:
		return 1.0f;
	}
	*/
	return 1.0f;
}

//=========================================================
// TraceAttack
//=========================================================
void CAI_BaseNPC::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	m_fNoDamageDecal = false;
	if ( m_takedamage == DAMAGE_NO )
		return;

	CTakeDamageInfo subInfo = info;

	SetLastHitGroup( ptr->hitgroup );
	m_nForceBone = ptr->physicsbone;		// save this bone for physics forces

#ifdef TF_CLASSIC
	CTFPlayer *pTFAttacker = ToTFPlayer( info.GetAttacker() );
#endif

	Assert( m_nForceBone > -255 && m_nForceBone < 256 );

	bool bDebug = showhitlocation.GetBool();

	switch ( ptr->hitgroup )
	{
	case HITGROUP_GENERIC:
		if( bDebug ) DevMsg("Hit Location: Generic\n");
		break;

	// hit gear, react but don't bleed
	case HITGROUP_GEAR:
		subInfo.SetDamage( 0.01 );
		ptr->hitgroup = HITGROUP_GENERIC;
		if( bDebug ) DevMsg("Hit Location: Gear\n");
		break;

	case HITGROUP_HEAD:
		// If we're attacked by a TF2 player then only the sniper and spy can do headshot damage.
		if ( info.GetAttacker()->IsPlayer() && pTFAttacker )
		{
			if ( subInfo.GetDamageType() & DMG_USE_HITLOCATIONS )
			{
				CTFWeaponBase *pWpn = pTFAttacker->GetActiveTFWeapon();
	            float flDamage = subInfo.GetDamage();				
				bool bCritical = true;

				if ( pWpn && !pWpn->CanFireCriticalShot( true ) )
					bCritical = false;

				if ( bCritical )
				{
					subInfo.AddDamageType( DMG_CRITICAL );
					subInfo.SetDamageCustom( TF_DMG_CUSTOM_HEADSHOT );

					// play the critical shot sound to the shooter	
					if ( pWpn )
					{
						if ( pWpn->IsWeapon( TF_WEAPON_SNIPERRIFLE ) || pWpn->IsWeapon( TF_WEAPON_SNIPERRIFLE_DECAP ) || pWpn->IsWeapon( TF_WEAPON_SNIPERRIFLE_CLASSIC ) || pWpn->IsWeapon( TF_WEAPON_REVOLVER ) )
							pWpn->WeaponSound( BURST );
					}
				}
				subInfo.SetDamage( flDamage );
			}
		}
		break;

	case HITGROUP_CHEST:
	case HITGROUP_STOMACH:
	case HITGROUP_LEFTARM:
	case HITGROUP_RIGHTARM:
	case HITGROUP_LEFTLEG:
	case HITGROUP_RIGHTLEG:
		/*if ( pWpn )
		{
			float flBodyshotModifer = 1.0f;
			CALL_ATTRIB_HOOK_INT_ON_OTHER( pWpn, flBodyshotModifer, bodyshot_damage_modify );
			if ( flBodyshotModifer != 1.0f ) // We have an attribute changing damage, modify it.
			{
				float flDamage = subInfo.GetDamage();
				flDamage *= flBodyshotModifer;
				subInfo.SetDamage( flDamage );
			}
		}*/
        break;
	
	default:
		if( bDebug ) DevMsg("Hit Location: UNKNOWN\n");
		break;
	}

#ifdef TF_CLASSIC
	if ( IsInvulnerable() )
	{ 
		// Make bullet impacts
		g_pEffects->Ricochet( ptr->endpos - (vecDir * 8), -vecDir );
	}
	else
#endif
	if ( subInfo.GetDamage() >= 1.0 && !(subInfo.GetDamageType() & DMG_SHOCK ) )
	{
		CDisablePredictionFiltering dis(true);

		// NPC's always bleed. Players only bleed in multiplayer.
		SpawnBlood( ptr->endpos, vecDir, BloodColor(), subInfo.GetDamage() );// a little surface blood.

		TraceBleed( subInfo.GetDamage(), vecDir, ptr, subInfo.GetDamageType() );

		if ( ptr->hitgroup == HITGROUP_HEAD && m_iHealth - subInfo.GetDamage() > 0 )
		{
			m_fNoDamageDecal = true;
		}
	}

	// Airboat gun will impart major force if it's about to kill him....
	if ( info.GetDamageType() & DMG_AIRBOAT )
	{
		if ( subInfo.GetDamage() >= GetHealth() )
		{
			float flMagnitude = subInfo.GetDamageForce().Length();
			if ( (flMagnitude != 0.0f) && (flMagnitude < 400.0f * 65.0f) )
			{
				subInfo.ScaleDamageForce( 400.0f * 65.0f / flMagnitude );
			}
		}
	}

	if( info.GetInflictor() )
	{
		subInfo.SetInflictor( info.GetInflictor() );
	}
	else
	{
		subInfo.SetInflictor( info.GetAttacker() );
	}

	AddMultiDamage( subInfo, this );
}

//-----------------------------------------------------------------------------
// Purpose: Checks if point is in spread angle between source and target Pos
//			Used to prevent friendly fire
// Input  : Source of attack, target position, spread angle
// Output :
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::PointInSpread( CBaseCombatCharacter *pCheckEntity, const Vector &sourcePos, const Vector &targetPos, const Vector &testPoint, float flSpread, float maxDistOffCenter )
{
	float distOffLine = CalcDistanceToLine2D( testPoint.AsVector2D(), sourcePos.AsVector2D(), targetPos.AsVector2D() );
	if ( distOffLine < maxDistOffCenter )
	{
		Vector toTarget		= targetPos - sourcePos;
		float  distTarget	= VectorNormalize(toTarget);

		Vector toTest   = testPoint - sourcePos;
		float  distTest = VectorNormalize(toTest);
		// Only reject if target is on other side 
		if (distTarget > distTest)
		{
			toTarget.z = 0.0;
			toTest.z = 0.0;

			float dotProduct = DotProduct(toTarget,toTest);
			if (dotProduct > flSpread)
			{
				return true;
			}
			else if( dotProduct > 0.0f )
			{
				// If this guy is in front, do the hull/line test:
				if( pCheckEntity )
				{
					float flBBoxDist = NAI_Hull::Width( pCheckEntity->GetHullType() );
					flBBoxDist *= 1.414f; // sqrt(2)

					// !!!BUGBUG - this 2d check will stop a citizen shooting at a gunship or strider
					// if another citizen is between them, even though the strider or gunship may be
					// high up in the air (sjb)
					distOffLine = CalcDistanceToLine( testPoint, sourcePos, targetPos );
					if( distOffLine < flBBoxDist )
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Checks if player is in spread angle between source and target Pos
//			Used to prevent friendly fire
// Input  : Source of attack, target position, spread angle
// Output :
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::PlayerInSpread( const Vector &sourcePos, const Vector &targetPos, float flSpread, float maxDistOffCenter, bool ignoreHatedPlayers )
{
	// loop through all players
	for (int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( pPlayer && ( !ignoreHatedPlayers || IRelationType( pPlayer ) != D_HT ) )
		{
			if ( PointInSpread( pPlayer, sourcePos, targetPos, pPlayer->WorldSpaceCenter(), flSpread, maxDistOffCenter ) )
				return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Checks if player is in range of given location.  Used by NPCs
//			to prevent friendly fire
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseEntity *CAI_BaseNPC::PlayerInRange( const Vector &vecLocation, float flDist )
{
	// loop through all players
	for (int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if (pPlayer && (vecLocation - pPlayer->WorldSpaceCenter() ).Length2D() <= flDist)
		{
			return pPlayer;
		}
	}
	return NULL;
}


#define BULLET_WIZZDIST	80.0
#define SLOPE ( -1.0 / BULLET_WIZZDIST )

void BulletWizz( Vector vecSrc, Vector vecEndPos, edict_t *pShooter, bool isTracer )
{
	CBasePlayer *pPlayer;
	Vector vecBulletPath;
	Vector vecPlayerPath;
	Vector vecBulletDir;
	Vector vecNearestPoint;
	float flDist;
	float flBulletDist;

	vecBulletPath = vecEndPos - vecSrc;
	vecBulletDir = vecBulletPath;
	VectorNormalize(vecBulletDir);

	// see how near this bullet passed by player in a single player game
	// for multiplayer, we need to go through the list of clients.
	for (int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer )
			continue;

		// Don't hear one's own bullets
		if( pPlayer->edict() == pShooter )
			continue;

		vecPlayerPath = pPlayer->EarPosition() - vecSrc;
		flDist = DotProduct( vecPlayerPath, vecBulletDir );
		vecNearestPoint = vecSrc + vecBulletDir * flDist;
		// FIXME: minus m_vecViewOffset?
		flBulletDist = ( vecNearestPoint - pPlayer->EarPosition() ).Length();
	}
}

//-----------------------------------------------------------------------------
// Hits triggers with raycasts
//-----------------------------------------------------------------------------
class CTriggerTraceEnum : public IEntityEnumerator
{
public:
	CTriggerTraceEnum( Ray_t *pRay, const CTakeDamageInfo &info, const Vector& dir, int contentsMask ) :
		m_info( info ),	m_VecDir(dir), m_ContentsMask(contentsMask), m_pRay(pRay)
	{
	}

	virtual bool EnumEntity( IHandleEntity *pHandleEntity )
	{
		trace_t tr;

		CBaseEntity *pEnt = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );

		// Done to avoid hitting an entity that's both solid & a trigger.
		if ( pEnt->IsSolid() )
			return true;

		enginetrace->ClipRayToEntity( *m_pRay, m_ContentsMask, pHandleEntity, &tr );
		if (tr.fraction < 1.0f)
		{
			pEnt->DispatchTraceAttack( m_info, m_VecDir, &tr );
			ApplyMultiDamage();
		}

		return true;
	}

private:
	Vector m_VecDir;
	int m_ContentsMask;
	Ray_t *m_pRay;
	CTakeDamageInfo m_info;
};

void CBaseEntity::TraceAttackToTriggers( const CTakeDamageInfo &info, const Vector& start, const Vector& end, const Vector& dir )
{
	Ray_t ray;
	ray.Init( start, end );

	CTriggerTraceEnum triggerTraceEnum( &ray, info, dir, MASK_SHOT );
	enginetrace->EnumerateEntities( ray, true, &triggerTraceEnum );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CAI_BaseNPC::GetTracerType( void )
{
	if ( GetActiveWeapon() )
	{
		return GetActiveWeapon()->GetTracerType();
	}

	return BaseClass::GetTracerType();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecTracerSrc - 
//			&tr - 
//			iTracerType - 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::MakeTracer( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType )
{
	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->MakeTracer( vecTracerSrc, tr, iTracerType );
		return;
	}

	BaseClass::MakeTracer( vecTracerSrc, tr, iTracerType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::FireBullets( const FireBulletsInfo_t &info )
{
#if defined (HL2_DLL) || defined (TF_CLASSIC)
	// If we're shooting at a bullseye, become perfectly accurate if the bullseye demands it
	if ( GetEnemy() && GetEnemy()->Classify() == CLASS_BULLSEYE )
	{
		CNPC_Bullseye *pBullseye = dynamic_cast<CNPC_Bullseye*>(GetEnemy()); 
		if ( pBullseye && pBullseye->UsePerfectAccuracy() )
		{
			FireBulletsInfo_t accurateInfo = info;
			accurateInfo.m_vecSpread = vec3_origin;
			BaseClass::FireBullets( accurateInfo );
			return;
		}
	}
#endif

	BaseClass::FireBullets( info );
}


//-----------------------------------------------------------------------------
// Shot statistics
//-----------------------------------------------------------------------------
void CBaseEntity::UpdateShotStatistics( const trace_t &tr )
{
	if ( ai_shot_stats.GetBool() )
	{
		CAI_BaseNPC *pNpc = MyNPCPointer();
		if ( pNpc )
		{
			pNpc->m_TotalShots++;
			if ( tr.m_pEnt == pNpc->GetEnemy() )
			{
				pNpc->m_TotalHits++;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Handle shot entering water
//-----------------------------------------------------------------------------
void CBaseEntity::HandleShotImpactingGlass( const FireBulletsInfo_t &info, 
	const trace_t &tr, const Vector &vecDir, ITraceFilter *pTraceFilter )
{
	// Move through the glass until we're at the other side
	Vector	testPos = tr.endpos + ( vecDir * MAX_GLASS_PENETRATION_DEPTH );

	CEffectData	data;

	data.m_vNormal = tr.plane.normal;
	data.m_vOrigin = tr.endpos;

	DispatchEffect( "GlassImpact", data );

	trace_t	penetrationTrace;

	// Re-trace as if the bullet had passed right through
	UTIL_TraceLine( testPos, tr.endpos, MASK_SHOT, pTraceFilter, &penetrationTrace );

	// See if we found the surface again
	if ( penetrationTrace.startsolid || tr.fraction == 0.0f || penetrationTrace.fraction == 1.0f )
		return;

	//FIXME: This is technically frustrating MultiDamage, but multiple shots hitting multiple targets in one call
	//		 would do exactly the same anyway...

	// Impact the other side (will look like an exit effect)
	DoImpactEffect( penetrationTrace, GetAmmoDef()->DamageType(info.m_iAmmoType) );

	data.m_vNormal = penetrationTrace.plane.normal;
	data.m_vOrigin = penetrationTrace.endpos;
	
	DispatchEffect( "GlassImpact", data );

	// Refire the round, as if starting from behind the glass
	FireBulletsInfo_t behindGlassInfo;
	behindGlassInfo.m_iShots = 1;
	behindGlassInfo.m_vecSrc = penetrationTrace.endpos;
	behindGlassInfo.m_vecDirShooting = vecDir;
	behindGlassInfo.m_vecSpread = vec3_origin;
	behindGlassInfo.m_flDistance = info.m_flDistance*( 1.0f - tr.fraction );
	behindGlassInfo.m_iAmmoType = info.m_iAmmoType;
	behindGlassInfo.m_iTracerFreq = info.m_iTracerFreq;
	behindGlassInfo.m_flDamage = info.m_flDamage;
	behindGlassInfo.m_pAttacker = info.m_pAttacker ? info.m_pAttacker : this;
	behindGlassInfo.m_nFlags = info.m_nFlags;

	FireBullets( behindGlassInfo );
}


//-----------------------------------------------------------------------------
// Computes the tracer start position
//-----------------------------------------------------------------------------
#define SHOT_UNDERWATER_BUBBLE_DIST 400

void CBaseEntity::CreateBubbleTrailTracer( const Vector &vecShotSrc, const Vector &vecShotEnd, const Vector &vecShotDir )
{
	int nBubbles;
	Vector vecBubbleEnd;
	float flLengthSqr = vecShotSrc.DistToSqr( vecShotEnd );
	if ( flLengthSqr > SHOT_UNDERWATER_BUBBLE_DIST * SHOT_UNDERWATER_BUBBLE_DIST )
	{
		VectorMA( vecShotSrc, SHOT_UNDERWATER_BUBBLE_DIST, vecShotDir, vecBubbleEnd );
		nBubbles = WATER_BULLET_BUBBLES_PER_INCH * SHOT_UNDERWATER_BUBBLE_DIST;
	}
	else
	{
		float flLength = sqrt(flLengthSqr) - 0.1f;
		nBubbles = WATER_BULLET_BUBBLES_PER_INCH * flLength;
		VectorMA( vecShotSrc, flLength, vecShotDir, vecBubbleEnd );
	}

	Vector vecTracerSrc;
	ComputeTracerStartPosition( vecShotSrc, &vecTracerSrc );
	UTIL_BubbleTrail( vecTracerSrc, vecBubbleEnd, nBubbles );
}


//=========================================================
//=========================================================
void CAI_BaseNPC::MakeDamageBloodDecal ( int cCount, float flNoise, trace_t *ptr, Vector vecDir )
{
	// make blood decal on the wall!
	trace_t Bloodtr;
	Vector vecTraceDir;
	int i;

	if ( !IsAlive() )
	{
		// dealing with a dead npc.
		if ( m_iMaxHealth <= 0 )
		{
			// no blood decal for a npc that has already decalled its limit.
			return;
		}
		else
		{
			m_iMaxHealth -= 1;
		}
	}

	for ( i = 0 ; i < cCount ; i++ )
	{
		vecTraceDir = vecDir;

		vecTraceDir.x += random->RandomFloat( -flNoise, flNoise );
		vecTraceDir.y += random->RandomFloat( -flNoise, flNoise );
		vecTraceDir.z += random->RandomFloat( -flNoise, flNoise );

		UTIL_TraceLine( ptr->endpos, ptr->endpos + vecTraceDir * 172, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &Bloodtr);

		if ( Bloodtr.fraction != 1.0 )
		{
			UTIL_BloodDecalTrace( &Bloodtr, BloodColor() );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &tr - 
//			nDamageType - 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::DoImpactEffect( trace_t &tr, int nDamageType )
{
	if ( GetActiveWeapon() != NULL )
	{
		GetActiveWeapon()->DoImpactEffect( tr, nDamageType );
		return;
	}

	BaseClass::DoImpactEffect( tr, nDamageType );
}

//---------------------------------------------------------
//---------------------------------------------------------
#define InterruptFromCondition( iCondition ) \
	AI_RemapFromGlobal( ( AI_IdIsLocal( iCondition ) ? GetClassScheduleIdSpace()->ConditionLocalToGlobal( iCondition ) : iCondition ) )
	
void CAI_BaseNPC::SetCondition( int iCondition )
{
	int interrupt = InterruptFromCondition( iCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return;
	}
	
	m_Conditions.Set( interrupt );
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CAI_BaseNPC::HasCondition( int iCondition )
{
	int interrupt = InterruptFromCondition( iCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return false;
	}
	
	bool bReturn = m_Conditions.IsBitSet(interrupt);
	return (bReturn);
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CAI_BaseNPC::HasCondition( int iCondition, bool bUseIgnoreConditions )
{
	if ( bUseIgnoreConditions )
		return HasCondition( iCondition );
	
	int interrupt = InterruptFromCondition( iCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return false;
	}
	
	bool bReturn = m_ConditionsPreIgnore.IsBitSet(interrupt);
	return (bReturn);
}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_BaseNPC::ClearCondition( int iCondition )
{
	int interrupt = InterruptFromCondition( iCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return;
	}
	
	m_Conditions.Clear(interrupt);
}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_BaseNPC::ClearConditions( int *pConditions, int nConditions )
{
	for ( int i = 0; i < nConditions; ++i )
	{
		int iCondition = pConditions[i];
		int interrupt = InterruptFromCondition( iCondition );
		
		if ( interrupt == -1 )
		{
			Assert(0);
			continue;
		}
		
		m_Conditions.Clear( interrupt );
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_BaseNPC::SetIgnoreConditions( int *pConditions, int nConditions )
{
	for ( int i = 0; i < nConditions; ++i )
	{
		int iCondition = pConditions[i];
		int interrupt = InterruptFromCondition( iCondition );
		
		if ( interrupt == -1 )
		{
			Assert(0);
			continue;
		}
		
		m_InverseIgnoreConditions.Clear( interrupt ); // clear means ignore
	}
}

void CAI_BaseNPC::ClearIgnoreConditions( int *pConditions, int nConditions )
{
	for ( int i = 0; i < nConditions; ++i )
	{
		int iCondition = pConditions[i];
		int interrupt = InterruptFromCondition( iCondition );
		
		if ( interrupt == -1 )
		{
			Assert(0);
			continue;
		}
		
		m_InverseIgnoreConditions.Set( interrupt ); // set means don't ignore
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CAI_BaseNPC::HasInterruptCondition( int iCondition )
{
	if( !GetCurSchedule() )
	{
		return false;
	}

	int interrupt = InterruptFromCondition( iCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return false;
	}
	return ( m_Conditions.IsBitSet( interrupt ) && GetCurSchedule()->HasInterrupt( interrupt ) );
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CAI_BaseNPC::ConditionInterruptsCurSchedule( int iCondition )
{	
	if( !GetCurSchedule() )
	{
		return false;
	}

	int interrupt = InterruptFromCondition( iCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return false;
	}
	return ( GetCurSchedule()->HasInterrupt( interrupt ) );
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CAI_BaseNPC::ConditionInterruptsSchedule( int localScheduleID, int iCondition )
{
	CAI_Schedule *pSchedule = GetSchedule( localScheduleID );
	if ( !pSchedule )
		return false;

	int interrupt = InterruptFromCondition( iCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return false;
	}
	return ( pSchedule->HasInterrupt( interrupt ) );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the interrupt conditions for a scripted schedule
// Input  : interrupt - the level of interrupt we allow
//-----------------------------------------------------------------------------
void CAI_BaseNPC::SetScriptedScheduleIgnoreConditions( Interruptability_t interrupt )
{
	static int g_GeneralConditions[] = 
	{
		COND_CAN_MELEE_ATTACK1,
		COND_CAN_MELEE_ATTACK2,
		COND_CAN_RANGE_ATTACK1,
		COND_CAN_RANGE_ATTACK2,
		COND_ENEMY_DEAD,
		COND_HEAR_BULLET_IMPACT,
		COND_HEAR_COMBAT,
		COND_HEAR_DANGER,
		COND_HEAR_PHYSICS_DANGER,
		COND_NEW_ENEMY,
		COND_PROVOKED,
		COND_SEE_ENEMY,
		COND_SEE_FEAR,
		COND_SMELL,
	};

	static int g_DamageConditions[] = 
	{
		COND_HEAVY_DAMAGE,
		COND_LIGHT_DAMAGE,
		COND_RECEIVED_ORDERS,
	};

	ClearIgnoreConditions( g_GeneralConditions, ARRAYSIZE(g_GeneralConditions) );
	ClearIgnoreConditions( g_DamageConditions, ARRAYSIZE(g_DamageConditions) );

	if ( interrupt > GENERAL_INTERRUPTABILITY )
		SetIgnoreConditions( g_GeneralConditions, ARRAYSIZE(g_GeneralConditions) );

	if ( interrupt > DAMAGEORDEATH_INTERRUPTABILITY )
		SetIgnoreConditions( g_DamageConditions, ARRAYSIZE(g_DamageConditions) );
}

//-----------------------------------------------------------------------------
// Returns whether we currently have any interrupt conditions that would
// interrupt the given schedule.
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::HasConditionsToInterruptSchedule( int nLocalScheduleID )
{
	CAI_Schedule *pSchedule = GetSchedule( nLocalScheduleID );
	if ( !pSchedule )
		return false;

	CAI_ScheduleBits bitsMask;
	pSchedule->GetInterruptMask( &bitsMask );

	CAI_ScheduleBits bitsOut;
	AccessConditionBits().And( bitsMask, &bitsOut );
	
	return !bitsOut.IsAllClear();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::IsCustomInterruptConditionSet( int nCondition )
{
	int interrupt = InterruptFromCondition( nCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return false;
	}
	
	return m_CustomInterruptConditions.IsBitSet( interrupt );
}

//-----------------------------------------------------------------------------
// Purpose: Sets a flag in the custom interrupt flags, translating the condition
//			to the proper global space, if necessary
//-----------------------------------------------------------------------------
void CAI_BaseNPC::SetCustomInterruptCondition( int nCondition )
{
	int interrupt = InterruptFromCondition( nCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return;
	}
	
	m_CustomInterruptConditions.Set( interrupt );
}

//-----------------------------------------------------------------------------
// Purpose: Clears a flag in the custom interrupt flags, translating the condition
//			to the proper global space, if necessary
//-----------------------------------------------------------------------------
void CAI_BaseNPC::ClearCustomInterruptCondition( int nCondition )
{
	int interrupt = InterruptFromCondition( nCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return;
	}
	
	m_CustomInterruptConditions.Clear( interrupt );
}


//-----------------------------------------------------------------------------
// Purpose: Clears all the custom interrupt flags.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::ClearCustomInterruptConditions()
{
	m_CustomInterruptConditions.ClearAll();
}


//-----------------------------------------------------------------------------

void CAI_BaseNPC::SetDistLook( float flDistLook )
{
	m_pSenses->SetDistLook( flDistLook );
}

//-----------------------------------------------------------------------------

bool CAI_BaseNPC::QueryHearSound( CSound *pSound )
{
	if ( pSound->SoundContext() & SOUND_CONTEXT_COMBINE_ONLY )
		return false;

	if ( pSound->SoundContext() & SOUND_CONTEXT_ALLIES_ONLY )
	{
		if ( !IsPlayerAlly() )
			return false;
	}

	if ( pSound->IsSoundType( SOUND_PLAYER ) && GetState() == NPC_STATE_IDLE && !FVisible( pSound->GetSoundReactOrigin() ) )
	{
		// NPC's that are IDLE should disregard player movement sounds if they can't see them.
		// This does not affect them hearing the player's weapon.
		// !!!BUGBUG - this probably makes NPC's not hear doors opening, because doors opening put SOUND_PLAYER
		// in the world, but the door's model will block the FVisible() trace and this code will then
		// deduce that the sound can not be heard
		return false;
	}

	// Disregard footsteps from our own class type
	if ( pSound->IsSoundType( SOUND_COMBAT ) && pSound->SoundChannel() == SOUNDENT_CHANNEL_NPC_FOOTSTEP )
	{
		if ( pSound->m_hOwner && pSound->m_hOwner->ClassMatches( m_iClassname ) )
				return false;
	}

	if( ShouldIgnoreSound( pSound ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------

bool CAI_BaseNPC::QuerySeeEntity( CBaseEntity *pEntity, bool bOnlyHateOrFearIfNPC )
{
	if ( bOnlyHateOrFearIfNPC && pEntity->IsNPC() )
	{
		Disposition_t disposition = IRelationType( pEntity );
		return ( disposition <= D_FR );
	}
	return true;
}

//-----------------------------------------------------------------------------

void CAI_BaseNPC::OnLooked( int iDistance )
{
	// DON'T let visibility information from last frame sit around!
	static int conditionsToClear[] =
	{
		COND_SEE_HATE,
		COND_SEE_DISLIKE,
		COND_SEE_ENEMY,
		COND_SEE_FEAR,
		COND_SEE_NEMESIS,
		COND_SEE_PLAYER,
		COND_LOST_PLAYER,
		COND_ENEMY_WENT_NULL,
	};

	bool bHadSeePlayer = HasCondition(COND_SEE_PLAYER);

	ClearConditions( conditionsToClear, ARRAYSIZE( conditionsToClear ) );

	AISightIter_t iter;
	CBaseEntity *pSightEnt;

	pSightEnt = GetSenses()->GetFirstSeenEntity( &iter );

	while( pSightEnt )
	{
		if ( pSightEnt->IsPlayer() || pSightEnt->IsBaseObject() && !InSameTeam(pSightEnt) )
		{
			// if we see a client, remember that (mostly for scripted AI)
			SetCondition(COND_SEE_PLAYER);
			m_flLastSawPlayerTime = gpGlobals->curtime;
		}

		Disposition_t relation = IRelationType( pSightEnt );

		// the looker will want to consider this entity
		// don't check anything else about an entity that can't be seen, or an entity that you don't care about.
		if ( relation != D_NU )
		{
			if ( pSightEnt == GetEnemy() )
			{
				// we know this ent is visible, so if it also happens to be our enemy, store that now.
				SetCondition(COND_SEE_ENEMY);
			}

			// don't add the Enemy's relationship to the conditions. We only want to worry about conditions when
			// we see npcs other than the Enemy.
			switch ( relation )
			{
			case D_HT:
				{
					int priority = IRelationPriority( pSightEnt );
					if (priority < 0)
					{
						SetCondition(COND_SEE_DISLIKE);
					}
					else if (priority > 10)
					{
						SetCondition(COND_SEE_NEMESIS);
					}
					else
					{
						SetCondition(COND_SEE_HATE);
					}
					UpdateEnemyMemory(pSightEnt,pSightEnt->GetAbsOrigin());
					break;

				}
			case D_FR:
				UpdateEnemyMemory(pSightEnt,pSightEnt->GetAbsOrigin());
				SetCondition(COND_SEE_FEAR);
				break;
			case D_LI:
			case D_NU:
				break;
			default:
				DevWarning( 2, "%s can't assess %s\n", GetClassname(), pSightEnt->GetClassname() );
				break;
			}
		}

		pSightEnt = GetSenses()->GetNextSeenEntity( &iter );
	}

	// Did we lose the player?
	if ( bHadSeePlayer && !HasCondition(COND_SEE_PLAYER) )
	{
		SetCondition(COND_LOST_PLAYER);
	}
}

//-----------------------------------------------------------------------------

void CAI_BaseNPC::OnListened()
{
	AISoundIter_t iter;

	CSound *pCurrentSound;

	static int conditionsToClear[] =
	{
		COND_HEAR_DANGER,
		COND_HEAR_COMBAT,
		COND_HEAR_WORLD,
		COND_HEAR_PLAYER,
		COND_HEAR_THUMPER,
		COND_HEAR_BUGBAIT,
		COND_HEAR_PHYSICS_DANGER,
		COND_HEAR_BULLET_IMPACT,
		COND_HEAR_MOVE_AWAY,

		COND_NO_HEAR_DANGER,

		COND_SMELL,
	};

	ClearConditions( conditionsToClear, ARRAYSIZE( conditionsToClear ) );

	pCurrentSound = GetSenses()->GetFirstHeardSound( &iter );

	while ( pCurrentSound )
	{
		// the npc cares about this sound, and it's close enough to hear.
		int condition = COND_NONE;

		if ( pCurrentSound->FIsSound() )
		{
			// this is an audible sound.
			switch( pCurrentSound->SoundTypeNoContext() )
			{
				case SOUND_DANGER:			
					if( gpGlobals->curtime > m_flIgnoreDangerSoundsUntil)
						condition = COND_HEAR_DANGER;			
					break;

				case SOUND_THUMPER:			condition = COND_HEAR_THUMPER;			break;
				case SOUND_BUGBAIT:			condition = COND_HEAR_BUGBAIT;			break;
				case SOUND_COMBAT:
					if ( pCurrentSound->SoundChannel() == SOUNDENT_CHANNEL_SPOOKY_NOISE )
					{
						condition = COND_HEAR_SPOOKY;
					}
					else 
					{
						condition = COND_HEAR_COMBAT;
					}
					break;

				case SOUND_WORLD:			condition = COND_HEAR_WORLD;			break;
				case SOUND_PLAYER:			condition = COND_HEAR_PLAYER;			break;
				case SOUND_BULLET_IMPACT:	condition = COND_HEAR_BULLET_IMPACT;	break;
				case SOUND_PHYSICS_DANGER:	condition = COND_HEAR_PHYSICS_DANGER;	break;
				case SOUND_DANGER_SNIPERONLY:/* silence warning */					break;
				case SOUND_MOVE_AWAY:		condition = COND_HEAR_MOVE_AWAY;		break;
				case SOUND_PLAYER_VEHICLE:	condition = COND_HEAR_PLAYER;			break;

				default:
					DevMsg( "**ERROR: NPC %s hearing sound of unknown type %d!\n", GetClassname(), pCurrentSound->SoundType() );
					break;
			}
		}
		else
		{
			// if not a sound, must be a smell - determine if it's just a scent, or if it's a food scent
			condition = COND_SMELL;
		}

		if ( condition != COND_NONE )
		{
			SetCondition( condition );
		}

		pCurrentSound = GetSenses()->GetNextHeardSound( &iter );
	}

	if( !HasCondition( COND_HEAR_DANGER ) )
	{
		SetCondition( COND_NO_HEAR_DANGER );
	}

	// Sound outputs
	if ( HasCondition( COND_HEAR_WORLD ) )
	{
		m_OnHearWorld.FireOutput(this, this);
	}

	if ( HasCondition( COND_HEAR_PLAYER ) )
	{
		m_OnHearPlayer.FireOutput(this, this);
	}

	if ( HasCondition( COND_HEAR_COMBAT ) ||
		 HasCondition( COND_HEAR_BULLET_IMPACT ) ||
		 HasCondition( COND_HEAR_DANGER ) )
	{
		m_OnHearCombat.FireOutput(this, this);
	}
}

//=========================================================
// FValidateHintType - tells use whether or not the npc cares
// about the type of Hint Node given
//=========================================================
bool CAI_BaseNPC::FValidateHintType ( CAI_Hint *pHint )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Override in subclasses to associate specific hint types
//			with activities
//-----------------------------------------------------------------------------
Activity CAI_BaseNPC::GetHintActivity( short sHintType, Activity HintsActivity )
{
	if ( HintsActivity != ACT_INVALID )
		return HintsActivity;

	return ACT_IDLE;
}

//-----------------------------------------------------------------------------
// Purpose: Override in subclasses to give specific hint types delays
//			before they can be used again
// Input  :
// Output :
//-----------------------------------------------------------------------------
float	CAI_BaseNPC::GetHintDelay( short sHintType )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose:	Return incoming grenade if spotted
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseGrenade* CAI_BaseNPC::IncomingGrenade(void)
{
	int				iDist;

	AIEnemiesIter_t iter;
	for( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst(&iter); pEMemory != NULL; pEMemory = GetEnemies()->GetNext(&iter) )
	{
		CBaseGrenade* pBG = dynamic_cast<CBaseGrenade*>((CBaseEntity*)pEMemory->hEnemy);

		// Make sure this memory is for a grenade and grenade is not dead
		if ( !pBG || pBG->m_lifeState == LIFE_DEAD )
			continue;

		// Make sure it's visible
		if ( !FVisible( pBG ) )
			continue;

		// Make sure it's not in our team
		if ( InSameTeam( pBG ) )
			continue;

		// Check if it's near me
		iDist = ( pBG->GetAbsOrigin() - GetAbsOrigin() ).Length();
		if ( iDist <= NPC_GRENADE_FEAR_DIST )
			return pBG;

		// Check if it's headed towards me
		Vector	vGrenadeDir = GetAbsOrigin() - pBG->GetAbsOrigin();
		Vector  vGrenadeVel;
		pBG->GetVelocity( &vGrenadeVel, NULL );
		VectorNormalize(vGrenadeDir);
		VectorNormalize(vGrenadeVel);
		float	flDotPr		= DotProduct(vGrenadeDir, vGrenadeVel);
		if (flDotPr > 0.85)
			return pBG;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Check my physical state with the environment
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CAI_BaseNPC::TryRestoreHull(void)
{
	if ( IsUsingSmallHull() && GetCurSchedule() )
	{
		trace_t tr;
		Vector	vUpBit = GetAbsOrigin();
		vUpBit.z += 1;

		UTIL_TraceHull( GetAbsOrigin(), vUpBit, GetHullMins(),
			GetHullMaxs(), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
		if ( !tr.startsolid && (tr.fraction == 1.0) )
		{
			SetHullSizeNormal();
		}
	}
}

//=========================================================
//=========================================================
int CAI_BaseNPC::GetSoundInterests( void )
{
	return SOUND_WORLD | SOUND_COMBAT | SOUND_PLAYER | SOUND_PLAYER_VEHICLE | 
		SOUND_BULLET_IMPACT;
}

//---------------------------------------------------------
//---------------------------------------------------------
int CAI_BaseNPC::GetSoundPriority( CSound *pSound )
{
	int iSoundTypeNoContext = pSound->SoundTypeNoContext();
	int iSoundContext = pSound->SoundContext();

	if( iSoundTypeNoContext & SOUND_DANGER )
	{
		return SOUND_PRIORITY_HIGHEST;
	}

	if( iSoundTypeNoContext & SOUND_COMBAT )
	{
		if( iSoundContext & SOUND_CONTEXT_EXPLOSION )
		{
			return SOUND_PRIORITY_VERY_HIGH;
		}

		return SOUND_PRIORITY_HIGH;
	}
	
	return SOUND_PRIORITY_NORMAL;
}

//---------------------------------------------------------
// Return the loudest sound of this type in the sound list
//---------------------------------------------------------
CSound *CAI_BaseNPC::GetLoudestSoundOfType( int iType )
{
	return CSoundEnt::GetLoudestSoundOfType( iType, EarPosition() );
}

//=========================================================
// GetBestSound - returns a pointer to the sound the npc
// should react to. Right now responds only to nearest sound.
//=========================================================
CSound* CAI_BaseNPC::GetBestSound( int validTypes )
{
	if ( m_pLockedBestSound->m_iType != SOUND_NONE )
		return m_pLockedBestSound;
	CSound *pResult = GetSenses()->GetClosestSound( false, validTypes );
	if ( pResult == NULL)
		DevMsg( "Warning: NULL Return from GetBestSound\n" ); // condition previously set now no longer true. Have seen this when play too many sounds...
	return pResult;
}

//=========================================================
// PBestScent - returns a pointer to the scent the npc
// should react to. Right now responds only to nearest scent
//=========================================================
CSound* CAI_BaseNPC::GetBestScent( void )
{
	CSound *pResult = GetSenses()->GetClosestSound( true );
	if ( pResult == NULL)
		DevMsg("Warning: NULL Return from GetBestScent\n" );
	return pResult;
}

//-----------------------------------------------------------------------------
void CAI_BaseNPC::LockBestSound()
{
	UnlockBestSound();
	CSound *pBestSound = GetBestSound();
	if ( pBestSound )
		*m_pLockedBestSound = *pBestSound;
}

//-----------------------------------------------------------------------------
void CAI_BaseNPC::UnlockBestSound()
{
	if ( m_pLockedBestSound->m_iType != SOUND_NONE )
	{
		m_pLockedBestSound->m_iType = SOUND_NONE;
		OnListened(); // reset hearing conditions
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the specified sound is visible. Handles sounds generated by entities correctly.
// Input  : *pSound - 
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::SoundIsVisible( CSound *pSound )
{
	CBaseEntity *pBlocker = NULL;
	if ( !FVisible( pSound->GetSoundReactOrigin(), MASK_BLOCKLOS, &pBlocker ) )
	{
		// Is the blocker the sound owner?
		if ( pBlocker && pBlocker == pSound->m_hOwner )
			return true;

		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if target is in legal range of eye movements
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::ValidEyeTarget(const Vector &lookTargetPos)
{
	Vector vHeadDir = HeadDirection3D( );
	Vector lookTargetDir	= lookTargetPos - EyePosition();
	VectorNormalize(lookTargetDir);

	// Only look if it doesn't crank my eyeballs too far
	float dotPr = DotProduct(lookTargetDir, vHeadDir);
	if (dotPr > 0.7)
	{
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Integrate head turn over time
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CAI_BaseNPC::SetHeadDirection( const Vector &vTargetPos, float flInterval)
{
	if (!(CapabilitiesGet() & bits_CAP_TURN_HEAD))
		return;

#ifdef DEBUG_LOOK
	// Draw line in body, head, and eye directions
	Vector vEyePos = EyePosition();
	Vector vHeadDir;
	HeadDirection3D(&vHeadDir);
	Vector vBodyDir;
	BodyDirection2D(&vBodyDir);

	//UNDONE <<TODO>>
	// currently eye dir just returns head dir, so use vTargetPos for now
	//Vector vEyeDir;	w
	//EyeDirection3D(&vEyeDir);
	NDebugOverlay::Line( vEyePos, vEyePos+(50*vHeadDir), 255, 0, 0, false, 0.1 );
	NDebugOverlay::Line( vEyePos, vEyePos+(50*vBodyDir), 0, 255, 0, false, 0.1 );
	NDebugOverlay::Line( vEyePos, vTargetPos, 0, 0, 255, false, 0.1 );
#endif

	//--------------------------------------
	// Set head yaw
	//--------------------------------------
	float flDesiredYaw = VecToYaw(vTargetPos - GetLocalOrigin()) - GetLocalAngles().y;
	if (flDesiredYaw > 180)
		flDesiredYaw -= 360;
	if (flDesiredYaw < -180)
		flDesiredYaw += 360;

	float	iRate	 = 0.8;

	// Make frame rate independent
	float timeToUse = flInterval;
	while (timeToUse > 0)
	{
		m_flHeadYaw	   = (iRate * m_flHeadYaw) + (1-iRate)*flDesiredYaw;
		timeToUse -= 0.1;
	}
	if (m_flHeadYaw > 360) m_flHeadYaw = 0;

	m_flHeadYaw = SetBoneController( 0, m_flHeadYaw );


	//--------------------------------------
	// Set head pitch
	//--------------------------------------
	Vector	vEyePosition	= EyePosition();
	float	fTargetDist		= (vTargetPos - vEyePosition).Length();
	float	fVertDist		= vTargetPos.z - vEyePosition.z;
	float	flDesiredPitch	= -RAD2DEG(atan(fVertDist/fTargetDist));

	// Make frame rate independent
	timeToUse = flInterval;
	while (timeToUse > 0)
	{
		m_flHeadPitch	   = (iRate * m_flHeadPitch) + (1-iRate)*flDesiredPitch;
		timeToUse -= 0.1;
	}
	if (m_flHeadPitch > 360) m_flHeadPitch = 0;

	SetBoneController( 1, m_flHeadPitch );

}

//------------------------------------------------------------------------------
// Purpose : Calculate the NPC's eye direction in 2D world space
// Input   :
// Output  :
//------------------------------------------------------------------------------
Vector CAI_BaseNPC::EyeDirection2D( void )
{
	// UNDONE
	// For now just return head direction....
	return HeadDirection2D( );
}

//------------------------------------------------------------------------------
// Purpose : Calculate the NPC's eye direction in 2D world space
// Input   :
// Output  :
//------------------------------------------------------------------------------
Vector CAI_BaseNPC::EyeDirection3D( void )
{
	// UNDONE //<<TODO>>
	// For now just return head direction....
	return HeadDirection3D( );
}

//------------------------------------------------------------------------------
// Purpose : Calculate the NPC's head direction in 2D world space
// Input   :
// Output  :
//------------------------------------------------------------------------------
Vector CAI_BaseNPC::HeadDirection2D( void )
{
	// UNDONE <<TODO>>
	// This does not account for head rotations in the animation,
	// only those done via bone controllers

	// Head yaw is in local cooridnate so it must be added to the body's yaw
	QAngle bodyAngles = BodyAngles();
	float flWorldHeadYaw	= m_flHeadYaw + bodyAngles.y;

	// Convert head yaw into facing direction
	return UTIL_YawToVector( flWorldHeadYaw );
}

//------------------------------------------------------------------------------
// Purpose : Calculate the NPC's head direction in 3D world space
// Input   :
// Output  :
//------------------------------------------------------------------------------
Vector CAI_BaseNPC::HeadDirection3D( void )
{
	Vector vHeadDirection;

	// UNDONE <<TODO>>
	// This does not account for head rotations in the animation,
	// only those done via bone controllers

	// Head yaw is in local cooridnate so it must be added to the body's yaw
	QAngle bodyAngles = BodyAngles();
	float	flWorldHeadYaw	= m_flHeadYaw + bodyAngles.y;

	// Convert head yaw into facing direction
	AngleVectors( QAngle( m_flHeadPitch, flWorldHeadYaw, 0), &vHeadDirection );
	return vHeadDirection;
}


//-----------------------------------------------------------------------------
// Purpose: Look at other NPCs and clients from time to time
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseEntity *CAI_BaseNPC::EyeLookTarget( void )
{
	if (m_flNextEyeLookTime < gpGlobals->curtime)
	{
		CBaseEntity*	pBestEntity = NULL;
		float			fBestDist	= MAX_COORD_RANGE;
		float			fTestDist;

		CBaseEntity *pEntity = NULL;

		for ( CEntitySphereQuery sphere( GetAbsOrigin(), 1024, 0 ); (pEntity = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity() )
		{
			if (pEntity == this)
			{
				continue;
			}
			CAI_BaseNPC *pNPC = pEntity->MyNPCPointer();
			if (pNPC || (pEntity->GetFlags() & FL_CLIENT))
			{
				fTestDist = (GetAbsOrigin() - pEntity->EyePosition()).Length();
				if (fTestDist < fBestDist)
				{
					if (ValidEyeTarget(pEntity->EyePosition()))
					{
						fBestDist	= fTestDist;
						pBestEntity	= pEntity;
					}
				}
			}
		}
		if (pBestEntity)
		{
			m_flNextEyeLookTime = gpGlobals->curtime + random->RandomInt(1,5);
			m_hEyeLookTarget = pBestEntity;
		}
	}
	return m_hEyeLookTarget;
}


//-----------------------------------------------------------------------------
// Purpose: Set direction that the NPC aiming their gun
// returns true is the passed Vector is in
// the caller's forward aim cone. The dot product is performed
// in 2d, making the view cone infinitely tall. By default, the
// callers aim cone is assumed to be very narrow
//-----------------------------------------------------------------------------

bool CAI_BaseNPC::FInAimCone( const Vector &vecSpot )
{
	Vector los = ( vecSpot - GetAbsOrigin() );

	// do this in 2D
	los.z = 0;
	VectorNormalize( los );

	Vector facingDir = BodyDirection2D( );

	float flDot = DotProduct( los, facingDir );

	if (CapabilitiesGet() & bits_CAP_AIM_GUN && m_poseAim_Yaw != -1)
	{
		// FIXME: query current animation for ranges
		return ( flDot > DOT_30DEGREE );
	}

	if ( flDot > 0.994 )//!!!BUGBUG - magic number same as FacingIdeal(), what is this?
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Cache whatever pose parameters we intend to use
//-----------------------------------------------------------------------------
void	CAI_BaseNPC::PopulatePoseParameters( void )
{
	m_poseAim_Pitch = LookupPoseParameter( "aim_pitch" );
	m_poseAim_Yaw   = LookupPoseParameter( "aim_yaw"   );
	m_poseMove_Yaw  = LookupPoseParameter( "move_yaw"  );

	BaseClass::PopulatePoseParameters();
}

//-----------------------------------------------------------------------------
// Purpose: Set direction that the NPC aiming their gun
//-----------------------------------------------------------------------------

void CAI_BaseNPC::SetAim( const Vector &aimDir )
{
	QAngle angDir;
	VectorAngles( aimDir, angDir );
	float curPitch = GetPoseParameter( m_poseAim_Pitch );
	float curYaw = GetPoseParameter( m_poseAim_Yaw );

	float newPitch;
	float newYaw;

	if( GetEnemy() )
	{
		// clamp and dampen movement
		newPitch = curPitch + 0.8 * UTIL_AngleDiff( UTIL_ApproachAngle( angDir.x, curPitch, 20 ), curPitch );

		float flRelativeYaw = UTIL_AngleDiff( angDir.y, GetAbsAngles().y );
		// float flNewTargetYaw = UTIL_ApproachAngle( flRelativeYaw, curYaw, 20 );
		// float newYaw = curYaw + 0.8 * UTIL_AngleDiff( flNewTargetYaw, curYaw );
		newYaw = curYaw + UTIL_AngleDiff( flRelativeYaw, curYaw );
	}
	else
	{
		// Sweep your weapon more slowly if you're not fighting someone
		newPitch = curPitch + 0.6 * UTIL_AngleDiff( UTIL_ApproachAngle( angDir.x, curPitch, 20 ), curPitch );

		float flRelativeYaw = UTIL_AngleDiff( angDir.y, GetAbsAngles().y );
		newYaw = curYaw + 0.6 * UTIL_AngleDiff( flRelativeYaw, curYaw );
	}

	newPitch = AngleNormalize( newPitch );
	newYaw = AngleNormalize( newYaw );

	SetPoseParameter( m_poseAim_Pitch, newPitch );
	SetPoseParameter( m_poseAim_Yaw, newYaw );

	// Msg("yaw %.0f (%.0f %.0f)\n", newYaw, angDir.y, GetAbsAngles().y );

	// Calculate our interaction yaw.
	// If we've got a small adjustment off our abs yaw, use that. 
	// Otherwise, use our abs yaw.
	if ( fabs(newYaw) < 20 )
	{
		m_flInteractionYaw = angDir.y;
	}
	else
	{
 		m_flInteractionYaw = GetAbsAngles().y;
	}
}


void CAI_BaseNPC::RelaxAim( )
{
	float curPitch = GetPoseParameter( m_poseAim_Pitch );
	float curYaw = GetPoseParameter( m_poseAim_Yaw );

	// dampen existing aim
	float newPitch = AngleNormalize( UTIL_ApproachAngle( 0, curPitch, 3 ) );
	float newYaw = AngleNormalize( UTIL_ApproachAngle( 0, curYaw, 2 ) );

	SetPoseParameter( m_poseAim_Pitch, newPitch );
	SetPoseParameter( m_poseAim_Yaw, newYaw );
	// DevMsg("relax aim %.0f %0.f\n", newPitch, newYaw ); 
}

//-----------------------------------------------------------------------------
void CAI_BaseNPC::AimGun()
{
	if (GetEnemy())
	{
		Vector vecShootOrigin;

		vecShootOrigin = Weapon_ShootPosition();
		Vector vecShootDir = GetShootEnemyDir( vecShootOrigin, false );

		SetAim( vecShootDir );
	}
	else
	{
		RelaxAim( );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set direction that the NPC is looking
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CAI_BaseNPC::MaintainLookTargets ( float flInterval )
{
	// --------------------------------------------------------
	// Try to look at enemy if I have one
	// --------------------------------------------------------
	if ((CBaseEntity*)GetEnemy())
	{
		if ( ValidEyeTarget(GetEnemy()->EyePosition()) )
		{
			SetHeadDirection(GetEnemy()->EyePosition(),flInterval);
			SetViewtarget( GetEnemy()->EyePosition() );
			return;
		}
	}

#if 0
	// --------------------------------------------------------
	// First check if I've been assigned to look at an entity
	// --------------------------------------------------------
	CBaseEntity *lookTarget = EyeLookTarget();
	if (lookTarget && ValidEyeTarget(lookTarget->EyePosition()))
	{
		SetHeadDirection(lookTarget->EyePosition(),flInterval);
		SetViewtarget( lookTarget->EyePosition() );
		return;
	}
#endif

	// --------------------------------------------------------
	// If I'm moving, look at my target position
	// --------------------------------------------------------
	if (GetNavigator()->IsGoalActive() && ValidEyeTarget(GetNavigator()->GetCurWaypointPos()))
	{
		SetHeadDirection(GetNavigator()->GetCurWaypointPos(),flInterval);
		SetViewtarget( GetNavigator()->GetCurWaypointPos() );
		return;
	}


	// -------------------------------------
	// If I hear a combat sounds look there
	// -------------------------------------
	if ( HasCondition(COND_HEAR_COMBAT) || HasCondition(COND_HEAR_DANGER) )
	{
		CSound *pSound = GetBestSound();

		if ( pSound && pSound->IsSoundType(SOUND_COMBAT | SOUND_DANGER) )
		{
			if (ValidEyeTarget( pSound->GetSoundOrigin() ))
			{
				SetHeadDirection(pSound->GetSoundOrigin(),flInterval);
				SetViewtarget( pSound->GetSoundOrigin() );
				return;
			}
		}
	}

	// -------------------------------------
	// Otherwise just look around randomly
	// -------------------------------------

	// Check that look target position is still valid
	if (m_flNextEyeLookTime > gpGlobals->curtime)
	{
		if (!ValidEyeTarget(m_vEyeLookTarget))
		{
			// Force choosing of new look target
			m_flNextEyeLookTime = 0;
		}
	}

	if (m_flNextEyeLookTime < gpGlobals->curtime)
	{
		Vector	vBodyDir = BodyDirection2D( );

		/*
		Vector  lookSpread	= Vector(0.82,0.82,0.22);
		float	x			= random->RandomFloat(-0.5,0.5) + random->RandomFloat(-0.5,0.5);
		float	y			= random->RandomFloat(-0.5,0.5) + random->RandomFloat(-0.5,0.5);
		float	z			= random->RandomFloat(-0.5,0.5) + random->RandomFloat(-0.5,0.5);

		QAngle angles;
		VectorAngles( vBodyDir, angles );
		Vector forward, right, up;
		AngleVectors( angles, &forward, &right, &up );

		Vector vecDir		= vBodyDir + (x * lookSpread.x * forward) + (y * lookSpread.y * right) + (z * lookSpread.z * up);
		float  lookDist		= random->RandomInt(50,2000);
		m_vEyeLookTarget	= EyePosition() + lookDist*vecDir;
		*/
		m_vEyeLookTarget	= EyePosition() + 500*vBodyDir;
		m_flNextEyeLookTime = gpGlobals->curtime + 0.5; // random->RandomInt(1,5);
	}
	SetHeadDirection(m_vEyeLookTarget,flInterval);

	// ----------------------------------------------------
	// Integrate eye turn in frame rate independent manner
	// ----------------------------------------------------
	float timeToUse = flInterval;
	while (timeToUse > 0)
	{
		m_vCurEyeTarget = ((1 - m_flEyeIntegRate) * m_vCurEyeTarget + m_flEyeIntegRate * m_vEyeLookTarget);
		timeToUse -= 0.1;
	}
	SetViewtarget( m_vCurEyeTarget );
}


//-----------------------------------------------------------------------------
// Let the motor deal with turning presentation issues
//-----------------------------------------------------------------------------

void CAI_BaseNPC::MaintainTurnActivity( )
{
	// Don't bother if we're in a vehicle
	if ( IsInAVehicle() )
		return;

	AI_PROFILE_SCOPE( CAI_BaseNPC_MaintainTurnActivity );
	GetMotor()->MaintainTurnActivity( );
}


//-----------------------------------------------------------------------------
// Here's where all motion occurs
//-----------------------------------------------------------------------------
void CAI_BaseNPC::PerformMovement()
{
	// don't bother to move if the npc isn't alive
	if (!IsAlive())
		return;

	AI_PROFILE_SCOPE(CAI_BaseNPC_PerformMovement);
	g_AIMoveTimer.Start();

	float flInterval = ( m_flTimeLastMovement != FLT_MAX ) ? gpGlobals->curtime - m_flTimeLastMovement : 0.1;

	m_pNavigator->Move( ROUND_TO_TICKS( flInterval ) );
	m_flTimeLastMovement = gpGlobals->curtime;

	g_AIMoveTimer.End();

}

//-----------------------------------------------------------------------------
// Updates to npc after movement is completed
//-----------------------------------------------------------------------------

void CAI_BaseNPC::PostMovement()
{
	AI_PROFILE_SCOPE( CAI_BaseNPC_PostMovement );

	InvalidateBoneCache();

	if ( GetModelPtr() && GetModelPtr()->SequencesAvailable() )
	{
		float flInterval = GetAnimTimeInterval();

		if ( CapabilitiesGet() & bits_CAP_AIM_GUN )
		{
			AI_PROFILE_SCOPE( CAI_BaseNPC_PM_AimGun );
			AimGun();
		}
		else
		{
			// NPCs with bits_CAP_AIM_GUN update this in SetAim, called by AimGun.
			m_flInteractionYaw = GetAbsAngles().y;
		}

		// set look targets for npcs with animated faces
		if ( CapabilitiesGet() & bits_CAP_ANIMATEDFACE )
		{
			AI_PROFILE_SCOPE( CAI_BaseNPC_PM_MaintainLookTargets );
			MaintainLookTargets(flInterval);
		}
	}

	{
	AI_PROFILE_SCOPE( CAI_BaseNPC_PM_MaintainTurnActivity );
	// update turning as needed
	MaintainTurnActivity( );
	}
}


//-----------------------------------------------------------------------------

float g_AINextDisabledMessageTime;
extern bool IsInCommentaryMode( void );

bool CAI_BaseNPC::PreThink( void )
{
	// ----------------------------------------------------------
	// Skip AI if its been disabled or networks haven't been
	// loaded, and put a warning message on the screen
	//
	// Don't do this if the convar wants it hidden
	// ----------------------------------------------------------
#ifdef USE_NAV_MESH
	if ((CAI_BaseNPC::m_nDebugBits & bits_debugDisableAI || (!g_pAINetworkManager->NetworksLoaded() && TheNavMesh->GetNavAreaCount() <= 0)))
#else
	if ((CAI_BaseNPC::m_nDebugBits & bits_debugDisableAI || !g_pAINetworkManager->NetworksLoaded()))
#endif // USE_NAV_MESH
	{
		if ( gpGlobals->curtime >= g_AINextDisabledMessageTime && !IsInCommentaryMode() )
		{
			g_AINextDisabledMessageTime = gpGlobals->curtime + 0.5f;

			hudtextparms_s tTextParam;
			tTextParam.x			= 0.7;
			tTextParam.y			= 0.65;
			tTextParam.effect		= 0;
			tTextParam.r1			= 255;
			tTextParam.g1			= 255;
			tTextParam.b1			= 255;
			tTextParam.a1			= 255;
			tTextParam.r2			= 255;
			tTextParam.g2			= 255;
			tTextParam.b2			= 255;
			tTextParam.a2			= 255;
			tTextParam.fadeinTime	= 0;
			tTextParam.fadeoutTime	= 0;
			tTextParam.holdTime		= 0.6;
			tTextParam.fxTime		= 0;
			tTextParam.channel		= 1;
			UTIL_HudMessageAll( tTextParam, "A.I. Disabled...\n" );
		}
		SetActivity( ACT_IDLE );
		return false;
	}

	// --------------------------------------------------------
	//	If debug stepping
	// --------------------------------------------------------
	if (CAI_BaseNPC::m_nDebugBits & bits_debugStepAI)
	{
		if (m_nDebugCurIndex >= CAI_BaseNPC::m_nDebugPauseIndex)
		{
			if (!GetNavigator()->IsGoalActive())
			{
				m_flPlaybackRate = 0;
			}
			return false;
		}
		else
		{
			m_flPlaybackRate = 1;
		}
	}

	if ( m_hOpeningDoor.Get() && AIIsDebuggingDoors( this ) )
	{
		NDebugOverlay::Line( EyePosition(), m_hOpeningDoor->WorldSpaceCenter(), 255, 255, 255, false, .1 );
	}

#ifdef TF_CLASSIC
	// If we're not on the ground, we're falling. Update our falling velocity.
	if ( !( GetFlags() & FL_ONGROUND ) )
	{
		m_flFallVelocity = -GetAbsVelocity().z;
	}

	// Reset bullet force accumulator, only lasts one frame, for ragdoll forces from multiple shots.
	m_vecTotalBulletForce = vec3_origin;

	ConditionThink();
#endif
	return true;
}

//-----------------------------------------------------------------------------

void CAI_BaseNPC::RunAnimation( void )
{
	VPROF_BUDGET( "CAI_BaseNPC_RunAnimation", VPROF_BUDGETGROUP_SERVER_ANIM );

	if ( !GetModelPtr() )
		return;

	float flInterval = GetAnimTimeInterval();
		
	StudioFrameAdvance( ); // animate

	if ((CAI_BaseNPC::m_nDebugBits & bits_debugStepAI) && !GetNavigator()->IsGoalActive())
	{
		flInterval = 0;
	}

	// start or end a fidget
	// This needs a better home -- switching animations over time should be encapsulated on a per-activity basis
	// perhaps MaintainActivity() or a ShiftAnimationOverTime() or something.
	if ( m_NPCState != NPC_STATE_SCRIPT && m_NPCState != NPC_STATE_DEAD && m_Activity == ACT_IDLE && IsActivityFinished() )
	{
		int iSequence;

		// FIXME: this doesn't reissue a translation, so if the idle activity translation changes over time, it'll never get reset
		if ( SequenceLoops() )
		{
			// animation does loop, which means we're playing subtle idle. Might need to fidget.
			iSequence = SelectWeightedSequence ( m_translatedActivity );
		}
		else
		{
			// animation that just ended doesn't loop! That means we just finished a fidget
			// and should return to our heaviest weighted idle (the subtle one)
			iSequence = SelectHeaviestSequence ( m_translatedActivity );
		}
		if ( iSequence != ACTIVITY_NOT_AVAILABLE )
		{
			ResetSequence( iSequence ); // Set to new anim (if it's there)

			//Adrian: Basically everywhere else in the AI code this variable gets set to whatever our sequence is.
			//But here it doesn't and not setting it causes any animation set through here to be stomped by the 
			//ideal sequence before it has a chance of playing out (since there's code that reselects the ideal 
			//sequence if it doesn't match the current one).
			if ( hl2_episodic.GetBool() )
			{
				m_nIdealSequence = iSequence;
			}
		}
	}

#ifdef TF_CLASSIC
	float flPlaybackRate = 1.0f;

	CALL_ATTRIB_HOOK_FLOAT( flPlaybackRate, mult_player_movespeed );

	CBaseCombatWeapon *pWeapon = GetActiveWeapon();

	if ( IsAlive() && pWeapon )
		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flPlaybackRate, mult_player_movespeed_active );

	/*
	if ( ( InCond( TF_COND_STUNNED ) && ( GetStunFlags() & TF_STUNFLAG_SLOWDOWN ) ) )
		flPlaybackRate /= 2;*/

	if ( IsSpeedBoosted() )
		flPlaybackRate *= 1.5f;

	if ( InCond( TF_COND_STUNNED ) && ( GetStunFlags() & TF_STUNFLAG_BONKSTUCK ) )
		flPlaybackRate = 0.1f;

	if ( InCond( TF_COND_SHIELD_CHARGE ) )
		flPlaybackRate = 7.5f;

	// Second, see if any flags are slowing them down
	if ( HasItem() && GetItem()->GetItemID() == TF_ITEM_CAPTURE_FLAG )
	{
		CCaptureFlag *pFlag = dynamic_cast<CCaptureFlag*>( GetItem() );
		if ( pFlag )
		{
			if ( pFlag->GetGameType() == TF_FLAGTYPE_ATTACK_DEFEND || pFlag->GetGameType() == TF_FLAGTYPE_TERRITORY_CONTROL )
				flPlaybackRate *= 0.5f;
		}
	}

	// if we're in bonus time because a team has won, give the winners 110% speed and the losers 90% speed
	if ( TFGameRules()->State_Get() == GR_STATE_TEAM_WIN )
	{
		int iWinner = TFGameRules()->GetWinningTeam();

		if ( iWinner != TEAM_UNASSIGNED )
		{
			if ( iWinner == GetTeamNumber() )
			{
				flPlaybackRate *= 1.1f;
			}
			else
			{
				flPlaybackRate *= 0.9f;
			}
		}
	}

	if ( flPlaybackRate > 12.0f )
		flPlaybackRate = 12.0f;

	SetPlaybackRate( flPlaybackRate );
#endif

	DispatchAnimEvents( this );
}

//-----------------------------------------------------------------------------

void CAI_BaseNPC::PostRun( void )
{
	AI_PROFILE_SCOPE(CAI_BaseNPC_PostRun);

	g_AIPostRunTimer.Start();

	if ( !IsMoving() )
	{
		if ( GetIdealActivity() == ACT_WALK || 
			 GetIdealActivity() == ACT_RUN || 
			 GetIdealActivity() == ACT_WALK_AIM || 
			 GetIdealActivity() == ACT_RUN_AIM )
		{
			PostRunStopMoving();
		}
	}

	RunAnimation();

	// update slave items (weapons)
	Weapon_FrameUpdate();

	g_AIPostRunTimer.End();
}

//-----------------------------------------------------------------------------

void CAI_BaseNPC::PostRunStopMoving()
{
	DbgNavMsg1( this, "NPC %s failed to stop properly, slamming activity\n", GetDebugName() );
	if ( !GetNavigator()->SetGoalFromStoppingPath() )
		SetIdealActivity( GetStoppedActivity() ); // This is to prevent running in place
}

//-----------------------------------------------------------------------------

bool CAI_BaseNPC::ShouldAlwaysThink()
{
	// @TODO (toml 07-08-03): This needs to be beefed up. 
	// There are simple cases already seen where an AI can briefly leave
	// the PVS while navigating to the player. Perhaps should incorporate a heuristic taking into
	// account mode, enemy, last time saw player, player range etc. For example, if enemy is player,
	// and player is within 100 feet, and saw the player within the past 15 seconds, keep running...
	return HasSpawnFlags(SF_NPC_ALWAYSTHINK);
}


//-----------------------------------------------------------------------------
// Purpose: Return true if the Player should be running the auto-move-out-of-way
//			avoidance code, which also means that the NPC shouldn't care about running into the Player.
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::ShouldPlayerAvoid( void )
{
	if ( GetState() == NPC_STATE_SCRIPT )
		return true;

	if ( IsInAScript() )
		return true;

	if ( IsInLockedScene() == true )
		 return true;

	if ( HasSpawnFlags( SF_NPC_ALTCOLLISION ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------

void CAI_BaseNPC::UpdateEfficiency( bool bInPVS )
{
	// Sleeping NPCs always dormant
	if ( GetSleepState() != AISS_AWAKE )
	{
		SetEfficiency( AIE_DORMANT );
		return;
	}

	m_bInChoreo = ( GetState() == NPC_STATE_SCRIPT || IsCurSchedule( SCHED_SCENE_GENERIC, false ) );

	if ( !ShouldUseEfficiency() )
	{
		SetEfficiency( AIE_NORMAL );
		SetMoveEfficiency( AIME_NORMAL );
		return;
	}

	//---------------------------------

#ifdef SecobMod__Enable_Fixed_Multiplayer_AI
	CBasePlayer *pPlayer = UTIL_GetNearestPlayer( GetAbsOrigin() );  
#else
	CBasePlayer *pPlayer = AI_GetSinglePlayer(); 
#endif //SecobMod__Enable_Fixed_Multiplayer_AI
	static Vector vPlayerEyePosition;
	static Vector vPlayerForward;
	static int iPrevFrame = -1;
	if ( gpGlobals->framecount != iPrevFrame )
	{
		iPrevFrame = gpGlobals->framecount;
		if ( pPlayer )
		{
			pPlayer->EyePositionAndVectors( &vPlayerEyePosition, &vPlayerForward, NULL, NULL );
		}
	}

	Vector	vToNPC		= GetAbsOrigin() - vPlayerEyePosition;
	float	playerDist	= VectorNormalize( vToNPC );
	bool	bPlayerFacing;

	bool	bClientPVSExpanded = UTIL_ClientPVSIsExpanded();

	if ( pPlayer )
	{
		bPlayerFacing = ( bClientPVSExpanded || ( bInPVS && vPlayerForward.Dot( vToNPC ) > 0 ) );
	}
	else
	{
		playerDist = 0;
		bPlayerFacing = true;
	}

	//---------------------------------

	bool bInVisibilityPVS = ( bClientPVSExpanded && UTIL_FindClientInVisibilityPVS( edict() ) != NULL );

	//---------------------------------

	if ( ( bInPVS && ( bPlayerFacing || playerDist < 25*12 ) ) || bClientPVSExpanded )
	{
		SetMoveEfficiency( AIME_NORMAL );
	}
	else
	{
		SetMoveEfficiency( AIME_EFFICIENT );
	}

	//---------------------------------

	if ( !IsRetail() && ai_efficiency_override.GetInt() > AIE_NORMAL && ai_efficiency_override.GetInt() <= AIE_DORMANT )
	{
		SetEfficiency( (AI_Efficiency_t)ai_efficiency_override.GetInt() );
		return;
	}

	//---------------------------------

	// Some conditions will always force normal
	if ( gpGlobals->curtime - GetLastAttackTime() < .15 )
	{
		SetEfficiency( AIE_NORMAL );
		return;
	}

	bool bFramerateOk = ( gpGlobals->frametime < ai_frametime_limit.GetFloat() );

	if ( m_bForceConditionsGather || 
		 gpGlobals->curtime - GetLastAttackTime() < .2 ||
		 gpGlobals->curtime - m_flLastDamageTime < .2 ||
		 ( GetState() < NPC_STATE_IDLE || GetState() > NPC_STATE_SCRIPT ) ||
		 ( ( bInPVS || bInVisibilityPVS ) && 
		   ( ( GetTask() && !TaskIsRunning() ) ||
			 GetTaskInterrupt() > 0 ||
			 m_bInChoreo ) ) )
	{
		SetEfficiency( ( bFramerateOk ) ? AIE_NORMAL : AIE_EFFICIENT );
		return;
	}

	AI_Efficiency_t minEfficiency;

	if ( !ShouldDefaultEfficient() )
	{
		minEfficiency = ( bFramerateOk ) ? AIE_NORMAL : AIE_EFFICIENT;
	}
	else
	{
		minEfficiency = ( bFramerateOk ) ? AIE_EFFICIENT : AIE_VERY_EFFICIENT;
	}

	// Stay normal if there's any chance of a relevant danger sound
	bool bPotentialDanger = false;
	
	if ( GetSoundInterests() & SOUND_DANGER )
	{
		int	iSound = CSoundEnt::ActiveList();
		
		while ( iSound != SOUNDLIST_EMPTY )
		{
			CSound *pCurrentSound = CSoundEnt::SoundPointerForIndex( iSound );

			float hearingSensitivity = HearingSensitivity();
			Vector vEarPosition = EarPosition();

			if ( pCurrentSound && (SOUND_DANGER & pCurrentSound->SoundType()) )
			{
				float flHearDistanceSq = pCurrentSound->Volume() * hearingSensitivity;
				flHearDistanceSq *= flHearDistanceSq;
				if ( pCurrentSound->GetSoundOrigin().DistToSqr( vEarPosition ) <= flHearDistanceSq )
				{
					bPotentialDanger = true;
					break;
				}
			}
			
			if ( pCurrentSound )
				iSound = pCurrentSound->NextSound();
			else
				break;
		}
	}

	if ( bPotentialDanger )
	{
		SetEfficiency( minEfficiency );
		return;
	}

	//---------------------------------

	if ( !pPlayer )
	{
		// No heuristic currently for dedicated servers
		SetEfficiency( minEfficiency );
		return;
	}

	enum
	{
		DIST_NEAR,
		DIST_MID,
		DIST_FAR
	};

	int	range;
	if ( bInPVS )
	{
		if ( playerDist < 15*12 )
		{
			SetEfficiency( minEfficiency );
			return;
		}

		range = ( playerDist < 50*12 ) ? DIST_NEAR : 
				( playerDist < 200*12 ) ? DIST_MID : DIST_FAR;
	}
	else
	{
		range = ( playerDist < 25*12 ) ? DIST_NEAR : 
				( playerDist < 100*12 ) ? DIST_MID : DIST_FAR;
	}

	// Efficiency mappings
	int state = GetState();
	if (state == NPC_STATE_SCRIPT ) // Treat script as alert. Already confirmed not in PVS
		state = NPC_STATE_ALERT;

	static AI_Efficiency_t mappings[] =
	{
		// Idle
			// In PVS
				// Facing
					AIE_NORMAL,
					AIE_EFFICIENT,
					AIE_EFFICIENT,
				// Not facing
					AIE_EFFICIENT,
					AIE_EFFICIENT,
					AIE_VERY_EFFICIENT,
			// Not in PVS
					AIE_VERY_EFFICIENT,
					AIE_SUPER_EFFICIENT,
					AIE_SUPER_EFFICIENT,
		// Alert
			// In PVS
				// Facing
					AIE_NORMAL,
					AIE_EFFICIENT,
					AIE_EFFICIENT,
				// Not facing
					AIE_NORMAL,
					AIE_EFFICIENT,
					AIE_EFFICIENT,
			// Not in PVS
					AIE_EFFICIENT,
					AIE_VERY_EFFICIENT,
					AIE_SUPER_EFFICIENT,
		// Combat
			// In PVS
				// Facing
					AIE_NORMAL,
					AIE_NORMAL,
					AIE_EFFICIENT,
				// Not facing
					AIE_NORMAL,
					AIE_EFFICIENT,
					AIE_EFFICIENT,
			// Not in PVS
					AIE_NORMAL,
					AIE_EFFICIENT,
					AIE_VERY_EFFICIENT,	
	};

	static const int stateBase[] = { 0, 9, 18 };
	const int NOT_FACING_OFFSET = 3;
	const int NO_PVS_OFFSET = 6;

	int iStateOffset = stateBase[state - NPC_STATE_IDLE] ;
	int iFacingOffset = (!bInPVS || bPlayerFacing) ? 0 : NOT_FACING_OFFSET;
	int iPVSOffset = (bInPVS) ? 0 : NO_PVS_OFFSET;
	int iMapping = iStateOffset + iPVSOffset + iFacingOffset + range;

	Assert( iMapping < ARRAYSIZE( mappings ) );

	AI_Efficiency_t efficiency = mappings[iMapping];

	//---------------------------------

	AI_Efficiency_t maxEfficiency = AIE_SUPER_EFFICIENT;
	if ( bInVisibilityPVS && state >= NPC_STATE_ALERT )
	{
		maxEfficiency = AIE_EFFICIENT;
	}
	else if ( bInVisibilityPVS || HasCondition( COND_SEE_PLAYER ) )
	{
		maxEfficiency = AIE_VERY_EFFICIENT;
	}
	
	//---------------------------------

	SetEfficiency( clamp( efficiency, minEfficiency, maxEfficiency ) );
}


//-----------------------------------------------------------------------------

void CAI_BaseNPC::UpdateSleepState( bool bInPVS )
{
	if ( GetSleepState() > AISS_AWAKE )
	{
#ifdef SecobMod__Enable_Fixed_Multiplayer_AI
		CBasePlayer *pLocalPlayer = UTIL_GetNearestPlayer( GetAbsOrigin() ); 
#else
		CBasePlayer *pLocalPlayer = AI_GetSinglePlayer();
#endif //SecobMod__Enable_Fixed_Multiplayer_AI
		if ( !pLocalPlayer )
		{
			if ( gpGlobals->maxClients > 1 )
			{
				Wake();
			}
			else
			{
				Warning( "CAI_BaseNPC::UpdateSleepState called with NULL pLocalPlayer\n" );
			}
			return;
		}

		if ( m_flWakeRadius > .1 && !(pLocalPlayer->GetFlags() & FL_NOTARGET) && ( pLocalPlayer->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr() <= Square(m_flWakeRadius) )
			Wake();
		else if ( GetSleepState() == AISS_WAITING_FOR_PVS )
		{
			if ( bInPVS )
				Wake();
		}
		else if ( GetSleepState() == AISS_WAITING_FOR_THREAT )
		{
			if ( HasCondition( COND_LIGHT_DAMAGE ) || HasCondition( COND_HEAVY_DAMAGE ) )
				Wake();
			else
			{
				if ( bInPVS )
				{
					for (int i = 1; i <= gpGlobals->maxClients; i++ )
					{
						CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
						if ( pPlayer && !(pPlayer->GetFlags() & FL_NOTARGET) && pPlayer->FVisible( this ) )
							Wake();
					}
				}
			
				// Should check for visible danger sounds
				if ( (GetSoundInterests() & SOUND_DANGER) && !(HasSpawnFlags(SF_NPC_WAIT_TILL_SEEN)) )
				{
					int	iSound = CSoundEnt::ActiveList();
					
					while ( iSound != SOUNDLIST_EMPTY )
					{
						CSound *pCurrentSound = CSoundEnt::SoundPointerForIndex( iSound );
						Assert( pCurrentSound );

						if ( (pCurrentSound->SoundType() & SOUND_DANGER) && 
							 GetSenses()->CanHearSound( pCurrentSound ) &&
							 SoundIsVisible( pCurrentSound ))
						{
							Wake();
							break;
						}

						if ( pCurrentSound )
							iSound = pCurrentSound->NextSound();
						else
							break;
					}
				}
			}
		}
	}
	else
	{
		// NPC is awake
		// Don't let an NPC sleep if they're running a script!
		if( !IsInAScript() && m_NPCState != NPC_STATE_SCRIPT )
		{
			if( HasSleepFlags(AI_SLEEP_FLAG_AUTO_PVS) )
			{
				if( !HasCondition(COND_IN_PVS) )
				{
					SetSleepState( AISS_WAITING_FOR_PVS );
					Sleep();
				}
			}
			if( HasSleepFlags(AI_SLEEP_FLAG_AUTO_PVS_AFTER_PVS) )
			{
				if( HasCondition(COND_IN_PVS) )
				{
					// OK, we're in the player's PVS. Now switch to regular old AUTO_PVS sleep rules.
					AddSleepFlags(AI_SLEEP_FLAG_AUTO_PVS);
					RemoveSleepFlags(AI_SLEEP_FLAG_AUTO_PVS_AFTER_PVS);
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------

struct AIRebalanceInfo_t
{
	CAI_BaseNPC *	pNPC;
	int				iNextThinkTick;
	bool			bInPVS;
	float			dotPlayer;
	float			distPlayer;
};

int __cdecl ThinkRebalanceCompare( const AIRebalanceInfo_t *pLeft, const AIRebalanceInfo_t *pRight )
{
	int base = pLeft->iNextThinkTick - pRight->iNextThinkTick;
	if ( base != 0 )
		return base;

	if ( !pLeft->bInPVS && !pRight->bInPVS )
		return 0;

	if ( !pLeft->bInPVS )
		return 1;

	if ( !pRight->bInPVS )
		return -1;

	if ( pLeft->dotPlayer < 0 && pRight->dotPlayer < 0 )
		return 0;

	if ( pLeft->dotPlayer < 0 )
		return 1;

	if ( pRight->dotPlayer < 0 )
		return -1;

	const float NEAR_PLAYER = 50*12;

	if ( pLeft->distPlayer < NEAR_PLAYER && pRight->distPlayer >= NEAR_PLAYER )
		return -1;

	if ( pRight->distPlayer < NEAR_PLAYER && pLeft->distPlayer >= NEAR_PLAYER )
		return 1;

	if ( pLeft->dotPlayer > pRight->dotPlayer )
		return -1;

	if ( pLeft->dotPlayer < pRight->dotPlayer )
		return 1;

	return 0;
}

inline bool CAI_BaseNPC::CanThinkRebalance()
{
	if ( m_pfnThink != (BASEPTR)&CAI_BaseNPC::CallNPCThink )
	{
		return false;
	}

	if ( m_bInChoreo )
	{
		return false;
	}

	if ( m_NPCState == NPC_STATE_DEAD )
	{
		return false;
	}

	if ( GetSleepState() != AISS_AWAKE )
	{
		return false;
	}

	if ( !m_bUsingStandardThinkTime /*&& m_iFrameBlocked == -1 */ )
	{
		return false;
	}

	return true;
}

void CAI_BaseNPC::RebalanceThinks() 
{
	bool bDebugThinkTicks = ai_debug_think_ticks.GetBool();
	if ( bDebugThinkTicks )
	{
		static int iPrevTick;
		static int nThinksInTick;
		static int nRebalanceableThinksInTick;

		if ( gpGlobals->tickcount != iPrevTick )
		{
			DevMsg( "NPC per tick is %d [%d] (tick %d, frame %d)\n", nRebalanceableThinksInTick, nThinksInTick, iPrevTick, gpGlobals->framecount );
			iPrevTick = gpGlobals->tickcount;
			nThinksInTick = 0;
			nRebalanceableThinksInTick = 0;
		}
		nThinksInTick++;
		if ( CanThinkRebalance() )
			nRebalanceableThinksInTick++;
	}

	if ( ShouldRebalanceThinks() && gpGlobals->tickcount >= gm_iNextThinkRebalanceTick )
	{
		AI_PROFILE_SCOPE(AI_Think_Rebalance );

		static CUtlVector<AIRebalanceInfo_t> rebalanceCandidates( 16, 64 );
		gm_iNextThinkRebalanceTick = gpGlobals->tickcount + TIME_TO_TICKS( random->RandomFloat( 3, 5) );

		int i;

#ifdef SecobMod__Enable_Fixed_Multiplayer_AI
		CBasePlayer *pPlayer = UTIL_GetNearestPlayer( GetAbsOrigin() ); 
#else
		CBasePlayer *pPlayer = AI_GetSinglePlayer();
#endif //SecobMod__Enable_Fixed_Multiplayer_AI
		Vector vPlayerForward;
		Vector vPlayerEyePosition;

		vPlayerForward.Init();
		vPlayerEyePosition.Init();

		if ( pPlayer )
		{
			pPlayer->EyePositionAndVectors( &vPlayerEyePosition, &vPlayerForward, NULL, NULL );
		}

		int iTicksPer10Hz = TIME_TO_TICKS( .1 );
		int iMinTickRebalance = gpGlobals->tickcount - 1; // -1 needed for alternate ticks
		int iMaxTickRebalance = gpGlobals->tickcount + iTicksPer10Hz;

		for ( i = 0; i < g_AI_Manager.NumAIs(); i++ )
		{
			CAI_BaseNPC *pCandidate = g_AI_Manager.AccessAIs()[i];
			if ( pCandidate->CanThinkRebalance() &&
				( pCandidate->GetNextThinkTick() >= iMinTickRebalance && 
				pCandidate->GetNextThinkTick() < iMaxTickRebalance ) )
			{
				int iInfo = rebalanceCandidates.AddToTail();

				rebalanceCandidates[iInfo].pNPC = pCandidate;
				rebalanceCandidates[iInfo].iNextThinkTick = pCandidate->GetNextThinkTick();

				if ( pCandidate->IsFlaggedEfficient() )
				{
					rebalanceCandidates[iInfo].bInPVS = false;
				}
				else if ( pPlayer )
				{
					Vector vToCandidate = pCandidate->EyePosition() - vPlayerEyePosition;
					rebalanceCandidates[iInfo].bInPVS = ( UTIL_FindClientInPVS( pCandidate->edict() ) != NULL );
					rebalanceCandidates[iInfo].distPlayer = VectorNormalize( vToCandidate );
					rebalanceCandidates[iInfo].dotPlayer = vPlayerForward.Dot( vToCandidate );
				}
				else
				{
					rebalanceCandidates[iInfo].bInPVS = true;
					rebalanceCandidates[iInfo].dotPlayer = 1;
					rebalanceCandidates[iInfo].distPlayer = 0;
				}
			}
			else if ( bDebugThinkTicks )
				DevMsg( "   Ignoring %d\n", pCandidate->GetNextThinkTick() );
		}

		if ( rebalanceCandidates.Count() )
		{
			rebalanceCandidates.Sort( ThinkRebalanceCompare );

			int iMaxThinkersPerTick = ceil( (float)( rebalanceCandidates.Count() + 1 ) / (float)iTicksPer10Hz ); // +1 to account for "this"

			int iCurTickDistributing = MIN( gpGlobals->tickcount, rebalanceCandidates[0].iNextThinkTick );
			int iRemainingThinksToDistribute = iMaxThinkersPerTick - 1; // Start with one fewer first time because "this" is 
			// always gets a slot in the current tick to avoid complications
			// in the calculation of "last think"

			if ( bDebugThinkTicks )
			{
				DevMsg( "Rebalance %d!\n", rebalanceCandidates.Count() + 1 );
				DevMsg( "   Distributing %d\n", iCurTickDistributing );
			}

			for ( i = 0; i < rebalanceCandidates.Count(); i++ )
			{
				if ( iRemainingThinksToDistribute == 0 || rebalanceCandidates[i].iNextThinkTick > iCurTickDistributing )
				{
					if ( rebalanceCandidates[i].iNextThinkTick <= iCurTickDistributing )
					{
						iCurTickDistributing = iCurTickDistributing + 1;
					}
					else
					{
						iCurTickDistributing = rebalanceCandidates[i].iNextThinkTick;
					}

					if ( bDebugThinkTicks )
						DevMsg( "   Distributing %d\n", iCurTickDistributing );

					iRemainingThinksToDistribute = iMaxThinkersPerTick;
				}

				if ( rebalanceCandidates[i].pNPC->GetNextThinkTick() != iCurTickDistributing )
				{
					if ( bDebugThinkTicks )
						DevMsg( "      Bumping %d to %d\n", rebalanceCandidates[i].pNPC->GetNextThinkTick(), iCurTickDistributing );

					rebalanceCandidates[i].pNPC->SetNextThink( TICKS_TO_TIME( iCurTickDistributing ) );
				}
				else if ( bDebugThinkTicks )
				{
					DevMsg( "      Leaving %d\n", rebalanceCandidates[i].pNPC->GetNextThinkTick() );
				}

				iRemainingThinksToDistribute--;
			}
		}

		rebalanceCandidates.RemoveAll();

		if ( bDebugThinkTicks )
		{
			DevMsg( "New distribution is:\n");
			for ( i = 0; i < g_AI_Manager.NumAIs(); i++ )
			{
				DevMsg( "   %d\n", g_AI_Manager.AccessAIs()[i]->GetNextThinkTick() );
			}
		}

		Assert( GetNextThinkTick() == TICK_NEVER_THINK ); // never change this objects tick
	}
}

static float g_NpcTimeThisFrame;
static float g_StartTimeCurThink;

bool CAI_BaseNPC::PreNPCThink()
{
	static int iPrevFrame = -1;
	static float frameTimeLimit = FLT_MAX;
	static const ConVar *pHostTimescale;

	if ( frameTimeLimit == FLT_MAX )
	{
		pHostTimescale = cvar->FindVar( "host_timescale" );
	}

	bool bUseThinkLimits = ( !m_bInChoreo && ShouldUseFrameThinkLimits() );

#ifdef _DEBUG
	const float NPC_THINK_LIMIT = 30.0 / 1000.0;
#else
	const float NPC_THINK_LIMIT = ( !IsXbox() ) ? (10.0 / 1000.0) : (12.5 / 1000.0);
#endif

	g_StartTimeCurThink = 0;

	if ( bUseThinkLimits && VCRGetMode() == VCR_Disabled )
	{
		if ( m_iFrameBlocked == gpGlobals->framecount )
		{
			DbgFrameLimitMsg( "Stalled %d (%d)\n", this, gpGlobals->framecount );
			SetNextThink( gpGlobals->curtime );
			return false;
		}
		else if ( gpGlobals->framecount != iPrevFrame )
		{
			DbgFrameLimitMsg( "--- FRAME: %d (%d)\n", this, gpGlobals->framecount );
			float timescale = pHostTimescale->GetFloat();
			if ( timescale < 1 )
				timescale = 1;

			iPrevFrame = gpGlobals->framecount;
			frameTimeLimit = NPC_THINK_LIMIT * timescale;
			g_NpcTimeThisFrame = 0;
		}
		else
		{
			if ( g_NpcTimeThisFrame > NPC_THINK_LIMIT )
			{
				float timeSinceLastRealThink = gpGlobals->curtime - m_flLastRealThinkTime;
				// Don't bump anyone more that a quarter second
				if ( timeSinceLastRealThink <= .25 )
				{
					DbgFrameLimitMsg( "Bumped %d (%d)\n", this, gpGlobals->framecount );
					m_iFrameBlocked = gpGlobals->framecount;
					SetNextThink( gpGlobals->curtime );
					return false;
				}
				else
				{
					DbgFrameLimitMsg( "(Over %d )\n", this );
				}
			}
		}

		DbgFrameLimitMsg( "Running %d (%d)\n", this, gpGlobals->framecount );
		g_StartTimeCurThink = engine->Time();

		m_iFrameBlocked = -1;
		m_nLastThinkTick = TIME_TO_TICKS( m_flLastRealThinkTime );
	}

	return true;
}

void CAI_BaseNPC::PostNPCThink( void ) 
{ 
	ProcessSceneEvents();

	if ( g_StartTimeCurThink != 0.0 && VCRGetMode() == VCR_Disabled )
	{
		g_NpcTimeThisFrame += engine->Time() - g_StartTimeCurThink;
	}
}

void CAI_BaseNPC::CallNPCThink( void ) 
{ 
	RebalanceThinks();

	//---------------------------------

	m_bUsingStandardThinkTime = false;

	//---------------------------------

	if ( !PreNPCThink() )
	{
		return;
	}

	// reduce cache queries by locking model in memory
	MDLCACHE_CRITICAL_SECTION();

	this->NPCThink(); 

	m_flLastRealThinkTime = gpGlobals->curtime;

	PostNPCThink();
} 

bool NPC_CheckBrushExclude( CBaseEntity *pEntity, CBaseEntity *pBrush )
{
	CAI_BaseNPC *pNPC = pEntity->MyNPCPointer();

	if ( pNPC )
	{
		return pNPC->GetMoveProbe()->ShouldBrushBeIgnored( pBrush );
	}

	return false;
}

class CTraceFilterPlayerAvoidance : public CTraceFilterEntitiesOnly
{
public:
	CTraceFilterPlayerAvoidance( const CBaseEntity *pEntity ) { m_pIgnore = pEntity; }

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );

		if ( m_pIgnore == pEntity )
			return false;

		if ( pEntity->IsPlayer() )
			return true;

		return false;
	}
private:

	const CBaseEntity		*m_pIgnore;
};

void CAI_BaseNPC::GetPlayerAvoidBounds( Vector *pMins, Vector *pMaxs )
{
	*pMins = WorldAlignMins();
	*pMaxs = WorldAlignMaxs();
}

ConVar ai_debug_avoidancebounds( "ai_debug_avoidancebounds", "0" );

void CAI_BaseNPC::SetPlayerAvoidState( void )
{
	bool bShouldPlayerAvoid = false;
	Vector vNothing;

	GetSequenceLinearMotion( GetSequence(), &vNothing );
	bool bIsMoving = ( IsMoving() || ( vNothing != vec3_origin ) );

	//If we are coming out of a script, check if we are stuck inside the player.
	if ( m_bPerformAvoidance || ( ShouldPlayerAvoid() && bIsMoving ) )
	{
		trace_t trace;
		Vector vMins, vMaxs;

		GetPlayerAvoidBounds( &vMins, &vMaxs );

#ifdef SecobMod__Enable_Fixed_Multiplayer_AI
		CBasePlayer *pLocalPlayer = UTIL_GetNearestPlayer( GetAbsOrigin() ); 
#else
		CBasePlayer *pLocalPlayer = AI_GetSinglePlayer();
#endif //SecobMod__Enable_Fixed_Multiplayer_AI

		if ( pLocalPlayer )
		{
			bShouldPlayerAvoid = IsBoxIntersectingBox( GetAbsOrigin() + vMins, GetAbsOrigin() + vMaxs, 
				pLocalPlayer->GetAbsOrigin() + pLocalPlayer->WorldAlignMins(), pLocalPlayer->GetAbsOrigin() + pLocalPlayer->WorldAlignMaxs() );
		}

		if ( ai_debug_avoidancebounds.GetBool() )
		{
			int iRed = ( bShouldPlayerAvoid == true ) ? 255 : 0;

			NDebugOverlay::Box( GetAbsOrigin(), vMins, vMaxs, iRed, 0, 255, 64, 0.1 );
		}
	}

	m_bPlayerAvoidState = ShouldPlayerAvoid();
	m_bPerformAvoidance = bShouldPlayerAvoid;

	if ( GetCollisionGroup() == COLLISION_GROUP_NPC || GetCollisionGroup() == COLLISION_GROUP_NPC_ACTOR )
	{
		if ( m_bPerformAvoidance == true )
		{
			SetCollisionGroup( COLLISION_GROUP_NPC_ACTOR );
		}
		else
		{
			SetCollisionGroup( COLLISION_GROUP_NPC );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Enables player avoidance when the player's vphysics shadow penetrates our vphysics shadow.  This can
// happen when the player is hit by a combine ball, which pushes them into an adjacent npc.  Subclasses should
// override this if it causes problems, but in general this will solve cases of the player getting stuck in
// the NPC from being pushed.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::PlayerPenetratingVPhysics( void )
{
	m_bPerformAvoidance = true;
}

//-----------------------------------------------------------------------------

bool CAI_BaseNPC::CheckPVSCondition()
{
	bool bInPVS = ( UTIL_FindClientInPVS( edict() ) != NULL ) || (UTIL_ClientPVSIsExpanded() && UTIL_FindClientInVisibilityPVS( edict() ));

	if ( bInPVS )
		SetCondition( COND_IN_PVS );
	else
		ClearCondition( COND_IN_PVS );

	return bInPVS;
}


//-----------------------------------------------------------------------------
// NPC Think - calls out to core AI functions and handles this
// npc's specific animation events
//

void CAI_BaseNPC::NPCThink( void )
{
	if ( m_bCheckContacts )
	{
		CheckPhysicsContacts();
	}

	Assert( !(m_NPCState == NPC_STATE_DEAD && m_lifeState == LIFE_ALIVE) );

	//---------------------------------

	SetNextThink( TICK_NEVER_THINK );

	//---------------------------------

	bool bInPVS = CheckPVSCondition();
		
	//---------------------------------

	UpdateSleepState( bInPVS );

	//---------------------------------

	bool bRanDecision = false;

	if ( GetEfficiency() < AIE_DORMANT && GetSleepState() == AISS_AWAKE )
	{
		static CFastTimer timer;
		float thinkLimit = ai_show_think_tolerance.GetFloat();

		if ( thinkLimit > 0 )
			timer.Start();

		if ( g_pAINetworkManager && g_pAINetworkManager->IsInitialized() )
		{
			VPROF_BUDGET( "NPCs", VPROF_BUDGETGROUP_NPCS );

			AI_PROFILE_SCOPE_BEGIN_( GetClassScheduleIdSpace()->GetClassName() ); // need to use a string stable from map load to map load

			SetPlayerAvoidState();

			if ( PreThink() )
			{
				UpdateLastKnownArea();

				if ( m_flNextDecisionTime <= gpGlobals->curtime )
				{
					bRanDecision = true;
					m_ScheduleState.bTaskRanAutomovement = false;
					m_ScheduleState.bTaskUpdatedYaw = false;
					RunAI();
				}
				else
				{
					if ( m_ScheduleState.bTaskRanAutomovement )
						AutoMovement();
					if ( m_ScheduleState.bTaskUpdatedYaw )
						GetMotor()->UpdateYaw();
				}

				PostRun();

				PerformMovement();

				m_bIsMoving = IsMoving();

				PostMovement();

				SetSimulationTime( gpGlobals->curtime );
			}
			else
				m_flTimeLastMovement = FLT_MAX;

			AI_PROFILE_SCOPE_END();
		}

		if ( thinkLimit > 0 )
		{
			timer.End();

			float thinkTime = g_AIRunTimer.GetDuration().GetMillisecondsF();

			if ( thinkTime > thinkLimit )
			{
				int color = (int)RemapVal( thinkTime, thinkLimit, thinkLimit * 3, 96.0, 255.0 );
				if ( color > 255 )
					color = 255;
				else if ( color < 96 )
					color = 96;

				Vector right;
				Vector vecPoint;

				vecPoint = EyePosition() + Vector( 0, 0, 12 );
				GetVectors( NULL, &right, NULL );
				NDebugOverlay::Line( vecPoint, vecPoint + Vector( 0, 0, 64 ), color, 0, 0, false , 1.0 );
				NDebugOverlay::Line( vecPoint, vecPoint + Vector( 0, 0, 16 ) + right * 16, color, 0, 0, false , 1.0 );
				NDebugOverlay::Line( vecPoint, vecPoint + Vector( 0, 0, 16 ) - right * 16, color, 0, 0, false , 1.0 );
			}
		}
	}

	m_bUsingStandardThinkTime = ( GetNextThinkTick() == TICK_NEVER_THINK );

	UpdateEfficiency( bInPVS );

	if ( m_bUsingStandardThinkTime )
	{
		static const char *ppszEfficiencies[] =
		{
			"AIE_NORMAL",
			"AIE_EFFICIENT",
			"AIE_VERY_EFFICIENT",
			"AIE_SUPER_EFFICIENT",
			"AIE_DORMANT",
		};

		static const char *ppszMoveEfficiencies[] = 
		{
			"AIME_NORMAL",
			"AIME_EFFICIENT",
		};

		if ( ai_debug_efficiency.GetBool() )
			DevMsg( this, "Eff: %s, Move: %s\n", ppszEfficiencies[GetEfficiency()], ppszMoveEfficiencies[GetMoveEfficiency()] );

		static float g_DecisionIntervals[] = 
		{
			.1,	//	AIE_NORMAL
			.2, //	AIE_EFFICIENT
			.4, //	AIE_VERY_EFFICIENT
			.6, //	AIE_SUPER_EFFICIENT
		};

		if ( bRanDecision )
		{
			m_flNextDecisionTime = gpGlobals->curtime + g_DecisionIntervals[GetEfficiency()];
		}

		if ( GetMoveEfficiency() == AIME_NORMAL || GetEfficiency() == AIE_NORMAL )
		{
			SetNextThink( gpGlobals->curtime + .1 );
		}
		else
		{
			SetNextThink( gpGlobals->curtime + .2 );
		}
	}
	else
	{
		m_flNextDecisionTime = 0;
	}
}

//=========================================================
// CAI_BaseNPC - USE - will make a npc angry at whomever
// activated it.
//=========================================================
void CAI_BaseNPC::NPCUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	return;

	// Can't +USE NPCs running scripts
	if ( GetState() == NPC_STATE_SCRIPT )
		return;

	if ( IsInAScript() )
		return;

	SetIdealState( NPC_STATE_ALERT );
}

//-----------------------------------------------------------------------------
// Purpose: Virtual function that allows us to have any npc ignore a set of
// shared conditions.
//
//-----------------------------------------------------------------------------
void CAI_BaseNPC::RemoveIgnoredConditions( void )
{
	m_ConditionsPreIgnore = m_Conditions;
	m_Conditions.And( m_InverseIgnoreConditions, &m_Conditions );

	if ( m_NPCState == NPC_STATE_SCRIPT && m_hCine )
		m_hCine->RemoveIgnoredConditions();
}

//=========================================================
// RangeAttack1Conditions
//=========================================================
int CAI_BaseNPC::RangeAttack1Conditions ( float flDot, float flDist )
{
	if ( flDist < 64)
	{
		return COND_TOO_CLOSE_TO_ATTACK;
	}
	else if (flDist > 784)
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if (flDot < 0.5)
	{
		return COND_NOT_FACING_ATTACK;
	}

	return COND_CAN_RANGE_ATTACK1;
}

//=========================================================
// RangeAttack2Conditions
//=========================================================
int CAI_BaseNPC::RangeAttack2Conditions ( float flDot, float flDist )
{
	if ( flDist < 64)
	{
		return COND_TOO_CLOSE_TO_ATTACK;
	}
	else if (flDist > 512)
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if (flDot < 0.5)
	{
		return COND_NOT_FACING_ATTACK;
	}

	return COND_CAN_RANGE_ATTACK2;
}

//=========================================================
// MeleeAttack1Conditions
//=========================================================
int CAI_BaseNPC::MeleeAttack1Conditions ( float flDot, float flDist )
{
	if ( flDist > 64)
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if (flDot < 0.7)
	{
		return 0;
	}
	else if (GetEnemy() == NULL)
	{
		return 0;
	}

	// Decent fix to keep folks from kicking/punching hornets and snarks is to check the onground flag(sjb)
	if ( GetEnemy()->GetFlags() & FL_ONGROUND )
	{
		return COND_CAN_MELEE_ATTACK1;
	}
	return 0;
}

//=========================================================
// MeleeAttack2Conditions
//=========================================================
int CAI_BaseNPC::MeleeAttack2Conditions ( float flDot, float flDist )
{
	if ( flDist > 64)
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if (flDot < 0.7)
	{
		return 0;
	}
	return COND_CAN_MELEE_ATTACK2;
}

// Get capability mask
int CAI_BaseNPC::CapabilitiesGet( void ) const
{
	int capability = m_afCapability;
	if ( GetActiveWeapon() )
	{
		capability |= GetActiveWeapon()->CapabilitiesGet();
	}
	return capability;
}

// Set capability mask
int CAI_BaseNPC::CapabilitiesAdd( int capability )
{
	m_afCapability |= capability;

	return m_afCapability;
}

// Set capability mask
int CAI_BaseNPC::CapabilitiesRemove( int capability )
{
	m_afCapability &= ~capability;

	return m_afCapability;
}

// Clear capability mask
void CAI_BaseNPC::CapabilitiesClear( void )
{
	m_afCapability = 0;
}


//=========================================================
// ClearAttacks - clear out all attack conditions
//=========================================================
void CAI_BaseNPC::ClearAttackConditions( )
{
	// Clear all attack conditions
	ClearCondition( COND_CAN_RANGE_ATTACK1 );
	ClearCondition( COND_CAN_RANGE_ATTACK2 );
	ClearCondition( COND_CAN_MELEE_ATTACK1 );
	ClearCondition( COND_CAN_MELEE_ATTACK2 );
	ClearCondition( COND_WEAPON_HAS_LOS );
	ClearCondition( COND_WEAPON_BLOCKED_BY_FRIEND );
	ClearCondition( COND_WEAPON_PLAYER_IN_SPREAD );		// Player in shooting direction
	ClearCondition( COND_WEAPON_PLAYER_NEAR_TARGET );	// Player near shooting position
	ClearCondition( COND_WEAPON_SIGHT_OCCLUDED );
}

//=========================================================
// GatherAttackConditions - sets all of the bits for attacks that the
// npc is capable of carrying out on the passed entity.
//=========================================================

void CAI_BaseNPC::GatherAttackConditions( CBaseEntity *pTarget, float flDist )
{
	AI_PROFILE_SCOPE(CAI_BaseNPC_GatherAttackConditions);
	
	Vector vecLOS = ( pTarget->GetAbsOrigin() - GetAbsOrigin() );
	vecLOS.z = 0;
	VectorNormalize( vecLOS );

	Vector vBodyDir = BodyDirection2D( );
	float  flDot	= DotProduct( vecLOS, vBodyDir );

	// we know the enemy is in front now. We'll find which attacks the npc is capable of by
	// checking for corresponding Activities in the model file, then do the simple checks to validate
	// those attack types.

	int		capability;
	Vector  targetPos;
	bool	bWeaponHasLOS;
	int		condition;

	capability		= CapabilitiesGet();

	// Clear all attack conditions
	AI_PROFILE_SCOPE_BEGIN( CAI_BaseNPC_GatherAttackConditions_PrimaryWeaponLOS );

	// @TODO (toml 06-15-03):  There are simple cases where
	// the upper torso of the enemy is visible, and the NPC is at an angle below
	// them, but the above test fails because BodyTarget returns the center of
	// the target. This needs some better handling/closer evaluation

	// Try the eyes first, as likely to succeed (because can see or else wouldn't be here) thus reducing
	// the odds of the need for a second trace
	ClearAttackConditions();
	targetPos = pTarget->EyePosition();
	bWeaponHasLOS = CurrentWeaponLOSCondition( targetPos, true );

	AI_PROFILE_SCOPE_END();

	if ( !bWeaponHasLOS )
	{
		AI_PROFILE_SCOPE( CAI_BaseNPC_GatherAttackConditions_SecondaryWeaponLOS );
		ClearAttackConditions( );
		targetPos		= pTarget->BodyTarget( GetAbsOrigin() );
		bWeaponHasLOS	= CurrentWeaponLOSCondition( targetPos, true );
	}
	else
	{
		SetCondition( COND_WEAPON_HAS_LOS );
	}

	bool bWeaponIsReady = (GetActiveWeapon() && !IsWeaponStateChanging());

	// FIXME: move this out of here
	if ( (capability & bits_CAP_WEAPON_RANGE_ATTACK1) && bWeaponIsReady )
	{
		AI_PROFILE_SCOPE( CAI_BaseNPC_GatherAttackConditions_WeaponRangeAttack1Condition );

		condition = GetActiveWeapon()->WeaponRangeAttack1Condition(flDot, flDist);

		if ( condition == COND_NOT_FACING_ATTACK && FInAimCone( targetPos ) )
			DevMsg( "Warning: COND_NOT_FACING_ATTACK set but FInAimCone is true\n" );

		if (condition != COND_CAN_RANGE_ATTACK1 || bWeaponHasLOS)
		{
			SetCondition(condition);
		}
	}
	else if ( capability & bits_CAP_INNATE_RANGE_ATTACK1 )
	{
		AI_PROFILE_SCOPE( CAI_BaseNPC_GatherAttackConditions_RangeAttack1Condition );

		condition = RangeAttack1Conditions( flDot, flDist );
		if (condition != COND_CAN_RANGE_ATTACK1 || bWeaponHasLOS)
		{
			SetCondition(condition);
		}
	}

	if ( (capability & bits_CAP_WEAPON_RANGE_ATTACK2) && bWeaponIsReady && ( GetActiveWeapon()->CapabilitiesGet() & bits_CAP_WEAPON_RANGE_ATTACK2 ) )
	{
		AI_PROFILE_SCOPE( CAI_BaseNPC_GatherAttackConditions_WeaponRangeAttack2Condition );

		condition = GetActiveWeapon()->WeaponRangeAttack2Condition(flDot, flDist);
		if (condition != COND_CAN_RANGE_ATTACK2 || bWeaponHasLOS)
		{
			SetCondition(condition);
		}
	}
	else if ( capability & bits_CAP_INNATE_RANGE_ATTACK2 )
	{
		AI_PROFILE_SCOPE( CAI_BaseNPC_GatherAttackConditions_RangeAttack2Condition );

		condition = RangeAttack2Conditions( flDot, flDist );
		if (condition != COND_CAN_RANGE_ATTACK2 || bWeaponHasLOS)
		{
			SetCondition(condition);
		}
	}

	if ( (capability & bits_CAP_WEAPON_MELEE_ATTACK1) && bWeaponIsReady)
	{
		AI_PROFILE_SCOPE( CAI_BaseNPC_GatherAttackConditions_WeaponMeleeAttack1Condition );
		SetCondition(GetActiveWeapon()->WeaponMeleeAttack1Condition(flDot, flDist));
	}
	else if ( capability & bits_CAP_INNATE_MELEE_ATTACK1 )
	{
		AI_PROFILE_SCOPE( CAI_BaseNPC_GatherAttackConditions_MeleeAttack1Condition );
		SetCondition(MeleeAttack1Conditions ( flDot, flDist ));
	}

	if ( (capability & bits_CAP_WEAPON_MELEE_ATTACK2) && bWeaponIsReady)
	{
		AI_PROFILE_SCOPE( CAI_BaseNPC_GatherAttackConditions_WeaponMeleeAttack2Condition );
		SetCondition(GetActiveWeapon()->WeaponMeleeAttack2Condition(flDot, flDist));
	}
	else if ( capability & bits_CAP_INNATE_MELEE_ATTACK2 )
	{
		AI_PROFILE_SCOPE( CAI_BaseNPC_GatherAttackConditions_MeleeAttack2Condition );
		SetCondition(MeleeAttack2Conditions ( flDot, flDist ));
	}

	// -----------------------------------------------------------------
	// If any attacks are possible clear attack specific bits
	// -----------------------------------------------------------------
	if (HasCondition(COND_CAN_RANGE_ATTACK2) ||
		HasCondition(COND_CAN_RANGE_ATTACK1) ||
		HasCondition(COND_CAN_MELEE_ATTACK2) ||
		HasCondition(COND_CAN_MELEE_ATTACK1) )
	{
		ClearCondition(COND_TOO_CLOSE_TO_ATTACK);
		ClearCondition(COND_TOO_FAR_TO_ATTACK);
		ClearCondition(COND_WEAPON_BLOCKED_BY_FRIEND);
	}
}


//=========================================================
// SetState
//=========================================================
void CAI_BaseNPC::SetState( NPC_STATE State )
{
	NPC_STATE OldState;

	OldState = m_NPCState;

	if ( State != m_NPCState )
	{
		m_flLastStateChangeTime = gpGlobals->curtime;
	}

	switch( State )
	{
	// Drop enemy pointers when going to idle
	case NPC_STATE_IDLE:

		if ( GetEnemy() != NULL )
		{
			SetEnemy( NULL ); // not allowed to have an enemy anymore.
			DevMsg( 2, "Stripped\n" );
		}
		break;
	}

	bool fNotifyChange = false;

	if( m_NPCState != State )
	{
		// Don't notify if we're changing to a state we're already in!
		fNotifyChange = true;
	}

	m_NPCState = State;
	SetIdealState( State );

	// Notify the character that its state has changed.
	if( fNotifyChange )
	{
		OnStateChange( OldState, m_NPCState );
	}
}

bool CAI_BaseNPC::WokeThisTick() const
{
	return m_nWakeTick == gpGlobals->tickcount ? true : false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_BaseNPC::Wake( bool bFireOutput )
{
	if ( GetSleepState() != AISS_AWAKE )
	{
		m_nWakeTick = gpGlobals->tickcount;
		SetSleepState( AISS_AWAKE );
		RemoveEffects( EF_NODRAW );
		if ( bFireOutput )
			m_OnWake.FireOutput( this, this );

		if ( m_bWakeSquad && GetSquad() )
		{
			AISquadIter_t iter;
			for ( CAI_BaseNPC *pSquadMember = GetSquad()->GetFirstMember( &iter ); pSquadMember; pSquadMember = GetSquad()->GetNextMember( &iter ) )
			{
				if ( pSquadMember->IsAlive() && pSquadMember != this )
				{
					pSquadMember->m_bWakeSquad = false;
					pSquadMember->Wake();
				}
			}

		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_BaseNPC::Sleep()
{
	// Don't render.
	AddEffects( EF_NODRAW );

	if( GetState() == NPC_STATE_SCRIPT )
	{
		Warning( "%s put to sleep while in Scripted state!\n", GetClassname() );
	}

	VacateStrategySlot();

	// Slam my schedule.
	SetSchedule( SCHED_SLEEP );

	m_OnSleep.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Sets all sensing-related conditions
//-----------------------------------------------------------------------------
void CAI_BaseNPC::PerformSensing( void )
{
	GetSenses()->PerformSensing();
}


//-----------------------------------------------------------------------------

void CAI_BaseNPC::ClearSenseConditions( void )
{
	static int conditionsToClear[] =
	{
		COND_SEE_HATE,
		COND_SEE_DISLIKE,
		COND_SEE_ENEMY,
		COND_SEE_FEAR,
		COND_SEE_NEMESIS,
		COND_SEE_PLAYER,
		COND_HEAR_DANGER,
		COND_HEAR_COMBAT,
		COND_HEAR_WORLD,
		COND_HEAR_PLAYER,
		COND_HEAR_THUMPER,
		COND_HEAR_BUGBAIT,
		COND_HEAR_PHYSICS_DANGER,
		COND_HEAR_MOVE_AWAY,
		COND_SMELL,
	};

	ClearConditions( conditionsToClear, ARRAYSIZE( conditionsToClear ) );
}

//-----------------------------------------------------------------------------

void CAI_BaseNPC::CheckOnGround( void )
{
	bool bScriptedWait = ( IsCurSchedule( SCHED_WAIT_FOR_SCRIPT ) || ( m_hCine && m_scriptState == CAI_BaseNPC::SCRIPT_WAIT ) );
	if ( !bScriptedWait && !HasCondition( COND_FLOATING_OFF_GROUND ) )
	{
		// parented objects are never floating
		if (GetMoveParent() != NULL)
			return;
			
		// NPCs in scripts with the fly flag shouldn't fall.
		// FIXME: should NPCS with FL_FLY ever fall? Doesn't seem like they should.
		if ( ( GetState() == NPC_STATE_SCRIPT ) && ( GetFlags() & FL_FLY ) )
			return;

		if ( ( GetNavType() == NAV_GROUND ) && ( GetMoveType() != MOVETYPE_VPHYSICS ) && ( GetMoveType() != MOVETYPE_NONE ) )
		{
			if ( m_CheckOnGroundTimer.Expired() )
			{
				m_CheckOnGroundTimer.Set(0.5);

				// check a shrunk box centered around the foot
				Vector maxs = WorldAlignMaxs();
				Vector mins = WorldAlignMins();

				if ( mins != maxs ) // some NPCs have no hull, so mins == maxs == vec3_origin
				{
					maxs -= Vector( 0.0f, 0.0f, 0.2f );

					Vector vecStart	= GetAbsOrigin() + Vector( 0, 0, .1f );
					Vector vecDown	= GetAbsOrigin();
					vecDown.z -= 4.0;

					trace_t trace;
					m_pMoveProbe->TraceHull( vecStart, vecDown, mins, maxs, GetAITraceMask(), &trace );

					if (trace.fraction == 1.0)
					{
						SetCondition( COND_FLOATING_OFF_GROUND );
						SetGroundEntity( NULL );
					}
					else
					{
						if ( trace.startsolid && trace.m_pEnt->GetMoveType() == MOVETYPE_VPHYSICS && 
							trace.m_pEnt->VPhysicsGetObject() && trace.m_pEnt->VPhysicsGetObject()->GetMass() < VPHYSICS_LARGE_OBJECT_MASS )
						{
							// stuck inside a small physics object?  
							m_CheckOnGroundTimer.Set(0.1f);
							NPCPhysics_CreateSolver( this, trace.m_pEnt, true, 0.25f );
							if ( VPhysicsGetObject() )
							{
								VPhysicsGetObject()->RecheckContactPoints();
							}
						}
						// Check to see if someone changed the ground on us...
						if ( trace.m_pEnt && trace.m_pEnt != GetGroundEntity() )
						{
							SetGroundEntity( trace.m_pEnt );
						}
					}
				}
			}
		}
	}
	else
	{
		// parented objects are never floating
		if ( bScriptedWait || GetMoveParent() != NULL || (GetFlags() & FL_ONGROUND ) || GetNavType() != NAV_GROUND )
		{
			ClearCondition( COND_FLOATING_OFF_GROUND );
			ClearAirblastState();

			// Clear the fall velocity so the impact doesn't happen again.
			m_flFallVelocity = 0;
		}
	}

}

void CAI_BaseNPC::NotifyPushMove()
{
	// don't recheck ground when I'm being push-moved
	m_CheckOnGroundTimer.Set( 0.5f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::CanFlinch( void )
{
	if ( IsCurSchedule( SCHED_BIG_FLINCH ) )
		return false;

	if ( m_flNextFlinchTime >= gpGlobals->curtime )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::CheckFlinches( void )
{
	// If we're currently flinching, don't allow gesture flinches to be overlaid
	if ( IsCurSchedule( SCHED_BIG_FLINCH ) )
	{
		ClearCondition( COND_LIGHT_DAMAGE );
		ClearCondition( COND_HEAVY_DAMAGE );
	}

	// If we've taken heavy damage, try to do a full schedule flinch
	if ( HasCondition(COND_HEAVY_DAMAGE) )
 	{
		// If we've already flinched recently, gesture flinch instead.
		if ( HasMemory(bits_MEMORY_FLINCHED) )
		{
			// Clear the heavy damage condition so we don't interrupt schedules
			// when we play a gesture flinch because we recently did a full flinch.
			// Prevents the player from stun-locking enemies, even though they don't full flinch.
			ClearCondition( COND_HEAVY_DAMAGE );
		}
		else if ( !HasInterruptCondition(COND_HEAVY_DAMAGE) )
		{
			// If we have taken heavy damage, but the current schedule doesn't 
			// break on that, resort to just playing a gesture flinch.
			PlayFlinchGesture();
		}

		// Otherwise, do nothing. The heavy damage will interrupt our schedule and we'll flinch.
	}
	else if ( HasCondition( COND_LIGHT_DAMAGE ) )
	{
		// If we have taken light damage play gesture flinches
		PlayFlinchGesture();
	}

	// If it's been a while since we did a full flinch, forget that we flinched so we'll flinch fully again
	if ( HasMemory(bits_MEMORY_FLINCHED) && gpGlobals->curtime > m_flNextFlinchTime )
	{
		Forget(bits_MEMORY_FLINCHED);
	}
}

//-----------------------------------------------------------------------------

void CAI_BaseNPC::GatherConditions( void )
{
	m_bConditionsGathered = true;
	g_AIConditionsTimer.Start();

	if( gpGlobals->curtime > m_flTimePingEffect && m_flTimePingEffect > 0.0f )
	{
		// Turn off the pinging.
		DispatchUpdateTransmitState();
		m_flTimePingEffect = 0.0f;
	}

	if ( m_NPCState != NPC_STATE_NONE && m_NPCState != NPC_STATE_DEAD )
	{
		if ( FacingIdeal() )
			Forget( bits_MEMORY_TURNING );

		bool bForcedGather = m_bForceConditionsGather;
		m_bForceConditionsGather = false;

		if ( m_pfnThink != (BASEPTR)&CAI_BaseNPC::CallNPCThink )
		{
			if ( UTIL_FindClientInPVS( edict() ) != NULL )
				SetCondition( COND_IN_PVS );
			else
				ClearCondition( COND_IN_PVS );
		}

		// Sample the environment. Do this unconditionally if there is a player in this
		// npc's PVS. NPCs in COMBAT state are allowed to simulate when there is no player in
		// the same PVS. This is so that any fights in progress will continue even if the player leaves the PVS.
		if ( !IsFlaggedEfficient() &&
			 ( bForcedGather || 
			   HasCondition( COND_IN_PVS ) ||
			   ShouldAlwaysThink() || 
			   m_NPCState == NPC_STATE_COMBAT ) )
		{
			CheckOnGround();

			if ( ShouldPlayIdleSound() )
			{
				AI_PROFILE_SCOPE(CAI_BaseNPC_IdleSound);
				IdleSound();
			}

			PerformSensing();

			GetEnemies()->RefreshMemories();
			ChooseEnemy();

			// Check to see if there is a better weapon available
			if (Weapon_IsBetterAvailable())
			{
				SetCondition(COND_BETTER_WEAPON_AVAILABLE);
			}

			if ( GetCurSchedule() &&
				( m_NPCState == NPC_STATE_IDLE || m_NPCState == NPC_STATE_ALERT) &&
				 GetEnemy() &&
				 !HasCondition( COND_NEW_ENEMY ) && 
				 GetCurSchedule()->HasInterrupt( COND_NEW_ENEMY ) )
			{
				// @Note (toml 05-05-04): There seems to be a case where an NPC can not respond
				//						  to COND_NEW_ENEMY. Only evidence right now is save
				//						  games after the fact, so for now, just patching it up
				DevMsg( 2, "Had to force COND_NEW_ENEMY\n" );
				SetCondition(COND_NEW_ENEMY);
			}
		}
		else
		{
			// if not done, can have problems if leave PVS in same frame heard/saw things, 
			// since only PerformSensing clears conditions
			ClearSenseConditions();
		}

		// do these calculations if npc has an enemy.
		if ( GetEnemy() != NULL )
		{
			if ( !IsFlaggedEfficient() )
			{
				GatherEnemyConditions( GetEnemy() );
				m_flLastEnemyTime = gpGlobals->curtime;
			}
			else
			{
				SetEnemy( NULL );
			}
		}

		// do these calculations if npc has a target
		if ( GetTarget() != NULL )
		{
			CheckTarget( GetTarget() );
		}

		CheckAmmo();

		CheckFlinches();

		CheckSquad();
	}
	else
		ClearCondition( COND_IN_PVS );

	g_AIConditionsTimer.End();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::PrescheduleThink( void )
{
#ifdef HL2_EPISODIC
	CheckForScriptedNPCInteractions();
#endif

	// If we use weapons, and our desired weapon state is not the current, fix it
	if( (CapabilitiesGet() & bits_CAP_USE_WEAPONS) && (m_iDesiredWeaponState == DESIREDWEAPONSTATE_HOLSTERED || m_iDesiredWeaponState == DESIREDWEAPONSTATE_UNHOLSTERED || m_iDesiredWeaponState == DESIREDWEAPONSTATE_HOLSTERED_DESTROYED ) )
	{
		if ( IsAlive() && !IsInAScript() )
		{
			if ( !IsCurSchedule( SCHED_MELEE_ATTACK1, false ) && !IsCurSchedule( SCHED_MELEE_ATTACK2, false ) &&
				 !IsCurSchedule( SCHED_RANGE_ATTACK1, false ) && !IsCurSchedule( SCHED_RANGE_ATTACK2, false ) )
			{
				if ( m_iDesiredWeaponState == DESIREDWEAPONSTATE_HOLSTERED || m_iDesiredWeaponState == DESIREDWEAPONSTATE_HOLSTERED_DESTROYED )
				{
					HolsterWeapon();
				}
				else if ( m_iDesiredWeaponState == DESIREDWEAPONSTATE_UNHOLSTERED )
				{
					UnholsterWeapon();
				}
			}
		}
		else
		{
			// Throw away the request
			m_iDesiredWeaponState = DESIREDWEAPONSTATE_IGNORE;
		}
	}
}

//-----------------------------------------------------------------------------
// Main entry point for processing AI
//-----------------------------------------------------------------------------

void CAI_BaseNPC::RunAI( void )
{
	AI_PROFILE_SCOPE(CAI_BaseNPC_RunAI);
	g_AIRunTimer.Start();

	if( ai_debug_squads.GetBool() )
	{
		if( IsInSquad() && GetSquad() && !CAI_Squad::IsSilentMember(this ) && ( GetSquad()->IsLeader( this ) || GetSquad()->NumMembers() == 1 ) )
		{
			AISquadIter_t iter;
			CAI_Squad *pSquad = GetSquad();

			Vector right;
			Vector vecPoint;

			vecPoint = EyePosition() + Vector( 0, 0, 12 );
			GetVectors( NULL, &right, NULL );
			NDebugOverlay::Line( vecPoint, vecPoint + Vector( 0, 0, 64 ), 0, 255, 0, false , 0.1 );
			NDebugOverlay::Line( vecPoint, vecPoint + Vector( 0, 0, 32 ) + right * 32, 0, 255, 0, false , 0.1 );
			NDebugOverlay::Line( vecPoint, vecPoint + Vector( 0, 0, 32 ) - right * 32, 0, 255, 0, false , 0.1 );

			for ( CAI_BaseNPC *pSquadMember = pSquad->GetFirstMember( &iter, false ); pSquadMember; pSquadMember = pSquad->GetNextMember( &iter, false ) )
			{
				if ( pSquadMember != this )
					NDebugOverlay::Line( EyePosition(), pSquadMember->EyePosition(), 0, 
										 CAI_Squad::IsSilentMember(pSquadMember) ? 127 : 255, 0, false , 0.1 );
			}
		}
	}

	if( ai_debug_loners.GetBool() && !IsInSquad() && AI_IsSinglePlayer() )
	{
		Vector right;
		Vector vecPoint;

		vecPoint = EyePosition() + Vector( 0, 0, 12 );

		UTIL_GetLocalPlayer()->GetVectors( NULL, &right, NULL );

		NDebugOverlay::Line( vecPoint, vecPoint + Vector( 0, 0, 64 ), 255, 0, 0, false , 0.1 );
		NDebugOverlay::Line( vecPoint, vecPoint + Vector( 0, 0, 32 ) + right * 32, 255, 0, 0, false , 0.1 );
		NDebugOverlay::Line( vecPoint, vecPoint + Vector( 0, 0, 32 ) - right * 32, 255, 0, 0, false , 0.1 );
	}
	
#ifdef _DEBUG
	m_bSelected = ( (m_debugOverlays & OVERLAY_NPC_SELECTED_BIT) != 0 );
#endif

	m_bConditionsGathered = false;
	m_bSkippedChooseEnemy = false;

	if ( g_pDeveloper->GetInt() && !GetNavigator()->IsOnNetwork() )
	{
		AddTimedOverlay( "NPC w/no reachable nodes!", 5 );
	}

	AI_PROFILE_SCOPE_BEGIN(CAI_BaseNPC_RunAI_GatherConditions);
	GatherConditions();
	RemoveIgnoredConditions();
	AI_PROFILE_SCOPE_END();

	if ( !m_bConditionsGathered )
		m_bConditionsGathered = true; // derived class didn't call to base

	TryRestoreHull();

	g_AIPrescheduleThinkTimer.Start();

	AI_PROFILE_SCOPE_BEGIN(CAI_RunAI_PrescheduleThink);
	PrescheduleThink();
	AI_PROFILE_SCOPE_END();

	g_AIPrescheduleThinkTimer.End();
	
	MaintainSchedule();

	PostscheduleThink();
				  
	ClearTransientConditions();

	g_AIRunTimer.End();
}

//-----------------------------------------------------------------------------
void CAI_BaseNPC::ClearTransientConditions()
{
	// if the npc didn't use these conditions during the above call to MaintainSchedule()
	// we throw them out cause we don't want them sitting around through the lifespan of a schedule
	// that doesn't use them.
	ClearCondition( COND_LIGHT_DAMAGE  );
	ClearCondition( COND_HEAVY_DAMAGE );
	ClearCondition( COND_PHYSICS_DAMAGE );
	ClearCondition( COND_PLAYER_PUSHING );
}


//-----------------------------------------------------------------------------
// Selecting the idle ideal state
//-----------------------------------------------------------------------------
NPC_STATE CAI_BaseNPC::SelectIdleIdealState()
{
	// IDLE goes to ALERT upon hearing a sound
	// IDLE goes to ALERT upon being injured
	// IDLE goes to ALERT upon seeing food
	// IDLE goes to COMBAT upon sighting an enemy
	if ( HasCondition(COND_NEW_ENEMY) ||
		 HasCondition(COND_SEE_ENEMY) )
	{
		// new enemy! This means an idle npc has seen someone it dislikes, or
		// that a npc in combat has found a more suitable target to attack
		return NPC_STATE_COMBAT;
	}
	
	// Set our ideal yaw if we've taken damage
	if ( HasCondition(COND_LIGHT_DAMAGE) || 
		 HasCondition(COND_HEAVY_DAMAGE) ||
	   (!GetEnemy() && gpGlobals->curtime - GetEnemies()->LastTimeSeen( AI_UNKNOWN_ENEMY ) < TIME_CARE_ABOUT_DAMAGE ) )
	{
		Vector vecEnemyLKP;

		// Fill in where we're trying to look
		if ( GetEnemy() )
		{
			vecEnemyLKP = GetEnemyLKP();
		}
		else
		{
			if ( GetEnemies()->Find( AI_UNKNOWN_ENEMY ) )
			{
				vecEnemyLKP = GetEnemies()->LastKnownPosition( AI_UNKNOWN_ENEMY );
			}
			else
			{
				// Don't have an enemy, so face the direction the last attack came from (don't face north)
				vecEnemyLKP = WorldSpaceCenter() + ( g_vecAttackDir * 128 );
			}
		}

		// Set the ideal
		GetMotor()->SetIdealYawToTarget( vecEnemyLKP );

		return NPC_STATE_ALERT;
	}

	if ( HasCondition(COND_HEAR_DANGER)  ||
		  HasCondition(COND_HEAR_COMBAT)  ||
		  HasCondition(COND_HEAR_WORLD)   ||
		  HasCondition(COND_HEAR_PLAYER)  ||
		  HasCondition(COND_HEAR_THUMPER) ||
		  HasCondition(COND_HEAR_BULLET_IMPACT) )
	{
		// Interrupted by a sound. So make our ideal yaw the
		// source of the sound!
		CSound *pSound;

		pSound = GetBestSound();
		Assert( pSound != NULL );
		if ( pSound )
		{
			// BRJ 1/7/04: This code used to set the ideal yaw.
			// It's really side-effecty to set the yaw here.
			// That is now done by the FACE_BESTSOUND schedule.
			// Revert this change if it causes problems.
			GetMotor()->SetIdealYawToTarget( pSound->GetSoundReactOrigin() );
			if ( pSound->IsSoundType( SOUND_COMBAT | SOUND_DANGER | SOUND_BULLET_IMPACT ) )
			{
				return NPC_STATE_ALERT;
			}
		}
	}
	
	if ( HasInterruptCondition(COND_SMELL) )
	{
		return NPC_STATE_ALERT;
	}
	
	return NPC_STATE_INVALID;
}


//-----------------------------------------------------------------------------
// Selecting the alert ideal state
//-----------------------------------------------------------------------------
NPC_STATE CAI_BaseNPC::SelectAlertIdealState()
{
	// ALERT goes to IDLE upon becoming bored
	// ALERT goes to COMBAT upon sighting an enemy
	if ( HasCondition(COND_NEW_ENEMY) ||
		 HasCondition(COND_SEE_ENEMY) ||
		 GetEnemy() != NULL )
	{
		return NPC_STATE_COMBAT;
	}
	
	// Set our ideal yaw if we've taken damage
	if ( HasCondition(COND_LIGHT_DAMAGE) ||
		 HasCondition(COND_HEAVY_DAMAGE) ||
		(!GetEnemy() && gpGlobals->curtime - GetEnemies()->LastTimeSeen( AI_UNKNOWN_ENEMY ) < TIME_CARE_ABOUT_DAMAGE ) )
	{
		Vector vecEnemyLKP;

		// Fill in where we're trying to look
		if ( GetEnemy() )
		{
			vecEnemyLKP = GetEnemyLKP();
		}
		else
		{
			if ( GetEnemies()->Find( AI_UNKNOWN_ENEMY ) )
			{
				vecEnemyLKP = GetEnemies()->LastKnownPosition( AI_UNKNOWN_ENEMY );
			}
			else
			{
				// Don't have an enemy, so face the direction the last attack came from (don't face north)
				vecEnemyLKP = WorldSpaceCenter() + ( g_vecAttackDir * 128 );
			}
		}

		// Set the ideal
		GetMotor()->SetIdealYawToTarget( vecEnemyLKP );

		return NPC_STATE_ALERT;
	}

	if ( HasCondition(COND_HEAR_DANGER) ||
		 HasCondition(COND_HEAR_COMBAT) )
	{
		CSound *pSound = GetBestSound();
		AssertOnce( pSound != NULL );

		if ( pSound )
		{
			GetMotor()->SetIdealYawToTarget( pSound->GetSoundReactOrigin() );
		}

		return NPC_STATE_ALERT;
	}
	
	if ( ShouldGoToIdleState() )
	{
		return NPC_STATE_IDLE;
	}
	
	return NPC_STATE_INVALID;
}


//-----------------------------------------------------------------------------
// Selecting the alert ideal state
//-----------------------------------------------------------------------------
NPC_STATE CAI_BaseNPC::SelectScriptIdealState()
{
	if ( HasCondition(COND_TASK_FAILED)  ||
		 HasCondition(COND_LIGHT_DAMAGE) ||
		 HasCondition(COND_HEAVY_DAMAGE) )
	{
		ExitScriptedSequence();	// This will set the ideal state
	}

	if ( m_IdealNPCState == NPC_STATE_IDLE )
	{
		// Exiting a script. Select the ideal state assuming we were idle now.
		m_NPCState = NPC_STATE_IDLE;
		NPC_STATE eIdealState = SelectIdealState();
		m_NPCState = NPC_STATE_SCRIPT;
		return eIdealState;
	}

	return NPC_STATE_INVALID;
}


//-----------------------------------------------------------------------------
// Purpose: Surveys the Conditions information available and finds the best
// new state for a npc.
//
// NOTICE the CAI_BaseNPC implementation of this function does not care about
// private conditions!
//
// Output : NPC_STATE - the suggested ideal state based on current conditions.
//-----------------------------------------------------------------------------
NPC_STATE CAI_BaseNPC::SelectIdealState( void )
{
	// dvs: FIXME: lots of side effecty code in here!! this function should ONLY return an ideal state!

	// ---------------------------
	// Do some squad stuff first
	// ---------------------------
	if (m_pSquad)
	{
		switch( m_NPCState )
		{
		case NPC_STATE_IDLE:
		case NPC_STATE_ALERT:
			if ( HasCondition ( COND_NEW_ENEMY )  )
			{
				m_pSquad->SquadNewEnemy( GetEnemy() );
			}
			break;
		}
	}

	// ---------------------------
	//  Set ideal state
	// ---------------------------
	switch ( m_NPCState )
	{
	case NPC_STATE_IDLE:
		{
			NPC_STATE nState = SelectIdleIdealState();
			if ( nState != NPC_STATE_INVALID )
				return nState;
		}
		break;

	case NPC_STATE_ALERT:
		{
			NPC_STATE nState = SelectAlertIdealState();
			if ( nState != NPC_STATE_INVALID )
				return nState;
		}
		break;

	case NPC_STATE_COMBAT:
		// COMBAT goes to ALERT upon death of enemy
		{
			if ( GetEnemy() == NULL )
			{
				DevWarning( 2, "***Combat state with no enemy!\n" );
				return NPC_STATE_ALERT;
			}
			break;
		}
	case NPC_STATE_SCRIPT:
		{
			NPC_STATE nState = SelectScriptIdealState();
			if ( nState != NPC_STATE_INVALID )
				return nState;
		}
		break;

	case NPC_STATE_DEAD:
		return NPC_STATE_DEAD;
	}

	// The best ideal state is the current ideal state.
	return m_IdealNPCState;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CAI_BaseNPC::GiveWeapon( string_t iszWeaponName )
{
	CBaseCombatWeapon *pWeapon = Weapon_Create( STRING(iszWeaponName) );
	if ( !pWeapon )
	{
		Warning( "Couldn't create weapon %s to give NPC %s.\n", STRING(iszWeaponName), STRING(GetEntityName()) );
		return;
	}

	// If I have a weapon already, drop it
	if ( GetActiveWeapon() )
	{
		Weapon_Drop( GetActiveWeapon() );
	}

	// If I have a name, make my weapon match it with "_weapon" appended
	if ( GetEntityName() != NULL_STRING )
	{
		pWeapon->SetName( AllocPooledString(UTIL_VarArgs("%s_weapon", STRING(GetEntityName()) )) );
	}

	Weapon_Equip( pWeapon );

	// Handle this case
	OnGivenWeapon( pWeapon );
}

//-----------------------------------------------------------------------------
// Rather specific function that tells us if an NPC is in the process of 
// moving to a weapon with the intent to pick it up.
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::IsMovingToPickupWeapon()
{
	return IsCurSchedule( SCHED_NEW_WEAPON );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::ShouldLookForBetterWeapon()
{
	if( m_flNextWeaponSearchTime > gpGlobals->curtime )
		return false;

	if( !(CapabilitiesGet() & bits_CAP_USE_WEAPONS) )
		return false;

	// Already armed and currently fighting. Don't try to upgrade.
	if( GetActiveWeapon() && m_NPCState == NPC_STATE_COMBAT )
		return false;

	if( IsMovingToPickupWeapon() )
		return false;

	if( !IsPlayerAlly() && GetActiveWeapon() )
		return false;

	if( IsInAScript() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:  Check if a better weapon is available.
//			 For now check if there is a weapon and I don't have one.  In
//			 the future
// UNDONE: actually rate the weapons based on there strength
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::Weapon_IsBetterAvailable()
{
	if( m_iszPendingWeapon != NULL_STRING )
	{
		// Some weapon is reserved for us.
		return true;
	}

	if( ShouldLookForBetterWeapon() )
	{
		if( GetActiveWeapon() )
		{
			m_flNextWeaponSearchTime = gpGlobals->curtime + 2;
		}
		else
		{
			if( IsInPlayerSquad() )
			{
				// Look for a weapon frequently.
				m_flNextWeaponSearchTime = gpGlobals->curtime + 1;
			}
			else
			{
				m_flNextWeaponSearchTime = gpGlobals->curtime + 2;
			}
		}

		if ( Weapon_FindUsable( WEAPON_SEARCH_DELTA ) )
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true is weapon has a line of sight.  If bSetConditions is
//			true, also sets LOC conditions
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::WeaponLOSCondition(const Vector &ownerPos, const Vector &targetPos, bool bSetConditions )
{
#if 0
	// @TODO (toml 03-07-04): this code might be better (not tested)
	Vector vecLocalRelativePosition;
	VectorITransform( npcOwner->Weapon_ShootPosition(), npcOwner->EntityToWorldTransform(), vecLocalRelativePosition );

	// Compute desired test transform

	// Compute desired x axis
	Vector xaxis;
	VectorSubtract( targetPos, ownerPos, xaxis );

	// FIXME: Insert angle test here?
	float flAngle = acos( xaxis.z / xaxis.Length() );

	xaxis.z = 0.0f;
	float flLength = VectorNormalize( xaxis );
	if ( flLength < 1e-3 )
	return false;

	Vector yaxis( -xaxis.y, xaxis.x, 0.0f );

	matrix3x4_t losTestToWorld;
	MatrixInitialize( losTestToWorld, ownerPos, xaxis, yaxis, zaxis );

	Vector barrelPos;
	VectorTransform( vecLocalRelativePosition, losTestToWorld, barrelPos );

#endif

	bool bHaveLOS = true;

	if (GetActiveWeapon())
	{
		bHaveLOS = GetActiveWeapon()->WeaponLOSCondition(ownerPos, targetPos, bSetConditions);
	}
	else if (CapabilitiesGet() & bits_CAP_INNATE_RANGE_ATTACK1)
	{
		bHaveLOS = InnateWeaponLOSCondition(ownerPos, targetPos, bSetConditions);
	}
	else
	{
		if (bSetConditions)
		{
			SetCondition( COND_NO_WEAPON );
		}
		bHaveLOS = false;
	}
	// -------------------------------------------
	//  Check for friendly fire with the player
	// -------------------------------------------
	if ( CapabilitiesGet() & ( bits_CAP_NO_HIT_PLAYER | bits_CAP_NO_HIT_SQUADMATES ) )
	{
		float spread = 0.92f;
		if ( GetActiveWeapon() )
		{
			Vector vSpread = GetAttackSpread( GetActiveWeapon() );
			if ( vSpread.x > VECTOR_CONE_15DEGREES.x )
				spread = FastCos( asin(vSpread.x) );
			else // too much error because using point not box
				spread = 0.99145f; // "15 degrees"
		}
		if ( CapabilitiesGet() & bits_CAP_NO_HIT_PLAYER)
		{
			// Check shoot direction relative to player
			if (PlayerInSpread( ownerPos, targetPos, spread, 8*12 ))
			{
				if (bSetConditions)
				{
					SetCondition( COND_WEAPON_PLAYER_IN_SPREAD );
				}
				bHaveLOS = false;
			}
			/* For grenades etc. check that player is clear?
			// Check player position also
			if (PlayerInRange( targetPos, 100 ))
			{
				SetCondition( COND_WEAPON_PLAYER_NEAR_TARGET );
			}
			*/
		}

		if ( bHaveLOS )
		{
			if ( ( CapabilitiesGet() & bits_CAP_NO_HIT_SQUADMATES) && m_pSquad && GetEnemy() )
			{
				if ( IsSquadmateInSpread( ownerPos, targetPos, spread, 8*12 ) )
				{
					SetCondition( COND_WEAPON_BLOCKED_BY_FRIEND );
					bHaveLOS = false;
				}
			}
		}
	}
	return bHaveLOS && !IsHiddenByFog( targetPos );
}

//-----------------------------------------------------------------------------
// Purpose: Check the innate weapon LOS for an owner at an arbitrary position
//			If bSetConditions is true, LOS related conditions will also be set
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::InnateWeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions )
{
	// --------------------
	// Check for occlusion
	// --------------------
	// Base class version assumes innate weapon position is at eye level
	Vector barrelPos		= ownerPos + GetViewOffset();
	trace_t tr;
	UTIL_TraceLine( barrelPos, targetPos, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

	if ( tr.fraction == 1.0 )
	{
		return true;
	}
	
	CBaseEntity	*pHitEntity = tr.m_pEnt;
	
	// Translate a hit vehicle into its passenger if found
	if ( GetEnemy() != NULL )
	{
		CBaseCombatCharacter *pCCEnemy = GetEnemy()->MyCombatCharacterPointer();
		if ( pCCEnemy != NULL && pCCEnemy->IsInAVehicle() )
		{
			// Ok, player in vehicle, check if vehicle is target we're looking at, fire if it is
			// Also, check to see if the owner of the entity is the vehicle, in which case it's valid too.
			// This catches vehicles that use bone followers.
			CBaseEntity *pVehicleEnt = pCCEnemy->GetVehicleEntity();
			if ( pHitEntity == pVehicleEnt || pHitEntity->GetOwnerEntity() == pVehicleEnt )
				return true;
		}
	}

	if ( pHitEntity == GetEnemy() )
	{
		return true;
	}
	else if ( pHitEntity && pHitEntity->MyCombatCharacterPointer() )
	{
		if (IRelationType( pHitEntity ) == D_HT)
		{
			return true;
		}
		else if (bSetConditions)
		{
			SetCondition(COND_WEAPON_BLOCKED_BY_FRIEND);
		}
	}
	else if (bSetConditions)
	{
		SetCondition(COND_WEAPON_SIGHT_OCCLUDED);
		SetEnemyOccluder(tr.m_pEnt);
	}

	return false;
}

//=========================================================
// CanCheckAttacks - prequalifies a npc to do more fine
// checking of potential attacks.
//=========================================================
bool CAI_BaseNPC::FCanCheckAttacks( void )
{
	// Not allowed to check attacks while climbing or jumping
	// Otherwise schedule is interrupted while on ladder/etc
	// which is NOT a legal place to attack from
	if ( GetNavType() == NAV_CLIMB || GetNavType() == NAV_JUMP )
		return false;

	if ( HasCondition(COND_SEE_ENEMY) && !HasCondition( COND_ENEMY_TOO_FAR))
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return dist. to enemy (closest of origin/head/feet)
// Input  :
// Output :
//-----------------------------------------------------------------------------
float CAI_BaseNPC::EnemyDistance( CBaseEntity *pEnemy )
{
	Vector enemyDelta = pEnemy->WorldSpaceCenter() - WorldSpaceCenter();
	
	// NOTE: We ignore rotation for computing height.  Assume it isn't an effect
	// we care about, so we simply use OBBSize().z for height.  
	// Otherwise you'd do this:
	// pEnemy->CollisionProp()->WorldSpaceSurroundingBounds( &enemyMins, &enemyMaxs );
	// float enemyHeight = enemyMaxs.z - enemyMins.z;

	float enemyHeight = pEnemy->CollisionProp()->OBBSize().z;
	float myHeight = CollisionProp()->OBBSize().z;
	
	// max distance our centers can be apart with the boxes still overlapping
	float flMaxZDist = ( enemyHeight + myHeight ) * 0.5f;

	// see if the enemy is closer to my head, feet or in between
	if ( enemyDelta.z > flMaxZDist )
	{
		// enemy feet above my head, compute distance from my head to his feet
		enemyDelta.z -= flMaxZDist;
	}
	else if ( enemyDelta.z < -flMaxZDist )
	{
		// enemy head below my feet, return distance between my feet and his head
		enemyDelta.z += flMaxZDist;
	}
	else
	{
		// boxes overlap in Z, no delta
		enemyDelta.z = 0;
	}

	return enemyDelta.Length();
}

//-----------------------------------------------------------------------------

float CAI_BaseNPC::GetReactionDelay( CBaseEntity *pEnemy )
{
	return ( m_NPCState == NPC_STATE_ALERT || m_NPCState == NPC_STATE_COMBAT ) ? 
				ai_reaction_delay_alert.GetFloat() : 
				ai_reaction_delay_idle.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Update information on my enemy
// Input  :
// Output : Returns true is new enemy, false is known enemy
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer )
{
	bool firstHand = ( pInformer == NULL || pInformer == this );
	
	AI_PROFILE_SCOPE(CAI_BaseNPC_UpdateEnemyMemory);
	
	if ( GetEnemies() )
	{
		// If the was eluding me and allow the NPC to play a sound
		if (GetEnemies()->HasEludedMe(pEnemy))
		{
			FoundEnemySound();
		}
		float reactionDelay = ( !pInformer || pInformer == this ) ? GetReactionDelay( pEnemy ) : 0.0;
		bool result = GetEnemies()->UpdateMemory(GetNavigator()->GetNetwork(), pEnemy, position, reactionDelay, firstHand);

		if ( !firstHand && pEnemy && result && GetState() == NPC_STATE_IDLE ) // if it's a new potential enemy
			ForceDecisionThink();

		if ( firstHand && pEnemy && m_pSquad )
		{
			m_pSquad->UpdateEnemyMemory( this, pEnemy, position );
		}
		return result;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Remembers the thing my enemy is hiding behind. Called when either
//			COND_ENEMY_OCCLUDED or COND_WEAPON_SIGHT_OCCLUDED is set.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::SetEnemyOccluder(CBaseEntity *pBlocker)
{
	m_hEnemyOccluder = pBlocker;
}


//-----------------------------------------------------------------------------
// Purpose: Gets the thing my enemy is hiding behind (assuming they are hiding).
//-----------------------------------------------------------------------------
CBaseEntity *CAI_BaseNPC::GetEnemyOccluder(void)
{
	return m_hEnemyOccluder.Get();
}


//-----------------------------------------------------------------------------
// Purpose: part of the Condition collection process
//			gets and stores data and conditions pertaining to a npc's
//			enemy.
// 			@TODO (toml 07-27-03): this should become subservient to the senses. right
// 			now, it yields different result
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CAI_BaseNPC::GatherEnemyConditions( CBaseEntity *pEnemy )
{
	AI_PROFILE_SCOPE(CAI_BaseNPC_GatherEnemyConditions);

	ClearCondition( COND_ENEMY_FACING_ME  );
	ClearCondition( COND_BEHIND_ENEMY   );

	// ---------------------------
	//  Set visibility conditions
	// ---------------------------
	if ( HasCondition( COND_NEW_ENEMY ) || GetSenses()->GetTimeLastUpdate( GetEnemy() ) == gpGlobals->curtime )
	{
		AI_PROFILE_SCOPE_BEGIN(CAI_BaseNPC_GatherEnemyConditions_Visibility);

		ClearCondition( COND_HAVE_ENEMY_LOS );
		ClearCondition( COND_ENEMY_OCCLUDED  );

		CBaseEntity *pBlocker = NULL;
		SetEnemyOccluder(NULL);

		bool bSensesDidSee = GetSenses()->DidSeeEntity( pEnemy );

		if ( !bSensesDidSee && ( ( EnemyDistance( pEnemy ) >= GetSenses()->GetDistLook() ) || !FVisible( pEnemy, MASK_BLOCKLOS, &pBlocker ) ) )
		{
			// No LOS to enemy
			SetEnemyOccluder(pBlocker);
			SetCondition( COND_ENEMY_OCCLUDED );
			ClearCondition( COND_SEE_ENEMY );

			if ( HasMemory( bits_MEMORY_HAD_LOS ) )
			{
				AI_PROFILE_SCOPE(CAI_BaseNPC_GatherEnemyConditions_Outputs);
				// Send output event
				if ( GetEnemy()->IsPlayer() || GetEnemy()->IsBaseObject() && !InSameTeam(pEnemy) )
				{
					m_OnLostPlayerLOS.FireOutput( GetEnemy(), this );
				}
				m_OnLostEnemyLOS.FireOutput( GetEnemy(), this );
			}
			Forget( bits_MEMORY_HAD_LOS );
		}
		else
		{
			// Have LOS but may not be in view cone
			SetCondition( COND_HAVE_ENEMY_LOS );

			if ( bSensesDidSee )
			{
				// Have LOS and in view cone
				SetCondition( COND_SEE_ENEMY );
			}
			else
			{
				ClearCondition( COND_SEE_ENEMY );
			}

			if (!HasMemory( bits_MEMORY_HAD_LOS ))
			{
				AI_PROFILE_SCOPE(CAI_BaseNPC_GatherEnemyConditions_Outputs);
				// Send output event
				EHANDLE hEnemy;
				hEnemy.Set( GetEnemy() );

				if ( GetEnemy()->IsPlayer() || GetEnemy()->IsBaseObject() && !InSameTeam( hEnemy ) )
				{
					m_OnFoundPlayer.Set(hEnemy, this, this);
					m_OnFoundEnemy.Set(hEnemy, this, this);
				}
				else
				{
					m_OnFoundEnemy.Set(hEnemy, this, this);
				}
			}
			Remember( bits_MEMORY_HAD_LOS );
		}

		AI_PROFILE_SCOPE_END();
	}

  	// -------------------
  	// If enemy is dead
  	// -------------------
  	if ( !pEnemy->IsAlive() )
  	{
  		SetCondition( COND_ENEMY_DEAD );
  		ClearCondition( COND_SEE_ENEMY );
  		ClearCondition( COND_ENEMY_OCCLUDED );
  		return;
  	}	
	
	float flDistToEnemy = EnemyDistance(pEnemy);

	AI_PROFILE_SCOPE_BEGIN(CAI_BaseNPC_GatherEnemyConditions_SeeEnemy);
	
	if ( HasCondition( COND_SEE_ENEMY ) )
	{
		// Trail the enemy a bit if he's moving
		if (pEnemy->GetSmoothedVelocity() != vec3_origin)
		{
			Vector vTrailPos = pEnemy->GetAbsOrigin() - pEnemy->GetSmoothedVelocity() * random->RandomFloat( -0.05, 0 );
			UpdateEnemyMemory(pEnemy,vTrailPos);
		}
		else
		{
			UpdateEnemyMemory(pEnemy,pEnemy->GetAbsOrigin());
		}

		// If it's not an NPC, assume it can't see me
		if ( pEnemy->MyCombatCharacterPointer() && pEnemy->MyCombatCharacterPointer()->FInViewCone ( this ) )
		{
			SetCondition ( COND_ENEMY_FACING_ME );
			ClearCondition ( COND_BEHIND_ENEMY );
		}
		else
		{
			ClearCondition( COND_ENEMY_FACING_ME );
			SetCondition ( COND_BEHIND_ENEMY );
		}
	}
	else if ( (!HasCondition(COND_ENEMY_OCCLUDED) && !HasCondition(COND_SEE_ENEMY)) && ( flDistToEnemy <= 256 ) )
	{
		// if the enemy is not occluded, and unseen, that means it is behind or beside the npc.
		// if the enemy is near enough the npc, we go ahead and let the npc know where the
		// enemy is. Send the enemy in as the informer so this knowledge will be regarded as 
		// secondhand so that the NPC doesn't 
		UpdateEnemyMemory( pEnemy, pEnemy->GetAbsOrigin(), pEnemy );
	}

	AI_PROFILE_SCOPE_END();

	float tooFar = m_flDistTooFar;
	if ( GetActiveWeapon() && HasCondition(COND_SEE_ENEMY) )
	{
		tooFar = MAX( m_flDistTooFar, GetActiveWeapon()->m_fMaxRange1 );
	}

	if ( flDistToEnemy >= tooFar )
	{
		// enemy is very far away from npc
		SetCondition( COND_ENEMY_TOO_FAR );
	}
	else
	{
		ClearCondition( COND_ENEMY_TOO_FAR );
	}

	if ( FCanCheckAttacks() )
	{
		// This may also call SetEnemyOccluder!
		GatherAttackConditions( GetEnemy(), flDistToEnemy );
	}
	else
	{
		ClearAttackConditions();
	}

	// If my enemy has moved significantly, or if the enemy has changed update my path
	UpdateEnemyPos();

	// If my target entity has moved significantly, update my path
	// This is an odd place to put this, but where else should it go?
	UpdateTargetPos();

	// ----------------------------------------------------------------------------
	// Check if enemy is reachable via the node graph unless I'm not on a network
	// ----------------------------------------------------------------------------
	if (GetNavigator()->IsOnNetwork())
	{
		// Note that unreachablity times out
		if (IsUnreachable(GetEnemy()))
		{
			SetCondition(COND_ENEMY_UNREACHABLE);
		}
	}

	//-----------------------------------------------------------------------
	// If I haven't seen the enemy in a while he may have eluded me
	//-----------------------------------------------------------------------
	if (gpGlobals->curtime - GetEnemyLastTimeSeen() > 8)
	{
		//-----------------------------------------------------------------------
		// I'm at last known position at enemy isn't in sight then has eluded me
		// ----------------------------------------------------------------------
		Vector flEnemyLKP = GetEnemyLKP();
		if (((flEnemyLKP - GetAbsOrigin()).Length2D() < 48) &&
			!HasCondition(COND_SEE_ENEMY))
		{
			MarkEnemyAsEluded();
		}
		//-------------------------------------------------------------------
		// If enemy isn't reachable, I can see last known position and enemy
		// isn't there, then he has eluded me
		// ------------------------------------------------------------------
		if (!HasCondition(COND_SEE_ENEMY) && HasCondition(COND_ENEMY_UNREACHABLE))
		{
			if ( !FVisible( flEnemyLKP ) )
			{
				MarkEnemyAsEluded();
			}
		}
	}
}


//-----------------------------------------------------------------------------
// In the case of goaltype enemy, update the goal position
//-----------------------------------------------------------------------------
float CAI_BaseNPC::GetGoalRepathTolerance( CBaseEntity *pGoalEnt, GoalType_t type, const Vector &curGoal, const Vector &curTargetPos )
{
	float distToGoal = ( GetAbsOrigin() - curTargetPos ).Length() - GetNavigator()->GetArrivalDistance();
	float distMoved1Sec = GetSmoothedVelocity().Length();
	float result = 120;  // FIXME: why 120?
	
	if (distMoved1Sec > 0.0)
	{
		float t = distToGoal / distMoved1Sec;

		result = clamp( 120.f * t, 0.f, 120.f );
		// Msg("t %.2f : d %.0f  (%.0f)\n", t, result, distMoved1Sec );
	}
		
	if ( !pGoalEnt->IsPlayer() )
		result *= 1.20;

	return result;
}

//-----------------------------------------------------------------------------
// In the case of goaltype enemy, update the goal position
//-----------------------------------------------------------------------------
void CAI_BaseNPC::UpdateEnemyPos()
{
	// Don't perform path recomputations during a climb or a jump
	if ( !GetNavigator()->IsInterruptable() )
		return;

	if ( m_AnyUpdateEnemyPosTimer.Expired() && m_UpdateEnemyPosTimer.Expired() )
	{
		// FIXME: does GetGoalRepathTolerance() limit re-routing enough to remove this?
		// m_UpdateEnemyPosTimer.Set( 0.5, 1.0 );
		
		// If my enemy has moved significantly, or if the enemy has changed update my path
		if ( GetNavigator()->GetGoalType() == GOALTYPE_ENEMY )
		{
			if (m_hEnemy != GetNavigator()->GetGoalTarget())
			{
				GetNavigator()->SetGoalTarget( m_hEnemy, vec3_origin );
			}
			else
			{
				Vector vEnemyLKP = GetEnemyLKP();
				TranslateNavGoal( GetEnemy(), vEnemyLKP );
				float tolerance = GetGoalRepathTolerance( GetEnemy(), GOALTYPE_ENEMY, GetNavigator()->GetGoalPos(), vEnemyLKP);
				if ( (GetNavigator()->GetGoalPos() - vEnemyLKP).Length() > tolerance )
				{
					// FIXME: when fleeing crowds, won't this severely limit the effectiveness of each individual?  Shouldn't this be a mutex that's held for some period so that at least one attacker is effective?
					m_AnyUpdateEnemyPosTimer.Set( 0.1 ); // FIXME: what's a reasonable interval?
					if ( !GetNavigator()->RefindPathToGoal( false ) )
					{	
						TaskFail( FAIL_NO_ROUTE );
					}
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// In the case of goaltype targetent, update the goal position
//-----------------------------------------------------------------------------
void CAI_BaseNPC::UpdateTargetPos()
{
	// BRJ 10/7/02
	// FIXME: make this check time based instead of distance based!

	// Don't perform path recomputations during a climb or a jump
	if ( !GetNavigator()->IsInterruptable() )
		return;

	// If my target entity has moved significantly, or has changed, update my path
	// This is an odd place to put this, but where else should it go?
	if ( GetNavigator()->GetGoalType() == GOALTYPE_TARGETENT )
	{
		if (m_hTargetEnt != GetNavigator()->GetGoalTarget())
		{
			GetNavigator()->SetGoalTarget( m_hTargetEnt, vec3_origin );
		}
		else if ( GetNavigator()->GetGoalFlags() & AIN_UPDATE_TARGET_POS )
		{
			if ( GetTarget() == NULL || (GetNavigator()->GetGoalPos() - GetTarget()->GetAbsOrigin()).Length() > GetGoalRepathTolerance( GetTarget(), GOALTYPE_TARGETENT, GetNavigator()->GetGoalPos(), GetTarget()->GetAbsOrigin()) )
			{
				if ( !GetNavigator()->RefindPathToGoal( false ) )
				{
					TaskFail( FAIL_NO_ROUTE );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: part of the Condition collection process
//			gets and stores data and conditions pertaining to a npc's
//			enemy.
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CAI_BaseNPC::CheckTarget( CBaseEntity *pTarget )
{
	AI_PROFILE_SCOPE(CAI_Enemies_CheckTarget);

	ClearCondition ( COND_HAVE_TARGET_LOS );
	ClearCondition ( COND_TARGET_OCCLUDED  );

	// ---------------------------
	//  Set visibility conditions
	// ---------------------------
	if ( ( EnemyDistance( pTarget ) >= GetSenses()->GetDistLook() ) || !FVisible( pTarget ) )
	{
		// No LOS to target
		SetCondition( COND_TARGET_OCCLUDED );
	}
	else
	{
		// Have LOS (may not be in view cone)
		SetCondition( COND_HAVE_TARGET_LOS );
	}

	UpdateTargetPos();
}

//-----------------------------------------------------------------------------
// Purpose: Creates a bullseye of limited lifespan at the provided position
// Input  : vecOrigin - Where to create the bullseye
//			duration - The lifespan of the bullseye
// Output : A BaseNPC pointer to the bullseye
//
// NOTES  :	It is the caller's responsibility to set up relationships with
//			this bullseye!
//-----------------------------------------------------------------------------
CAI_BaseNPC *CAI_BaseNPC::CreateCustomTarget( const Vector &vecOrigin, float duration )
{
#if defined (HL2_DLL) || defined (TF_CLASSIC)
	CNPC_Bullseye *pTarget = (CNPC_Bullseye*)CreateEntityByName( "npc_bullseye" );

	ASSERT( pTarget != NULL );

	// Build a nonsolid bullseye and place it in the desired location
	// The bullseye must take damage or the SetHealth 0 call will not be able
	pTarget->AddSpawnFlags( SF_BULLSEYE_NONSOLID );
	pTarget->SetAbsOrigin( vecOrigin );
	pTarget->Spawn();

	// Set it up to remove itself, unless told to be infinite (-1)
	if( duration > -1 )
	{
		variant_t value;
		value.SetFloat(0);
		g_EventQueue.AddEvent( pTarget, "SetHealth", value, duration, this, this );
	}

	return pTarget;
#else
	return NULL;
#endif// HL2_DLL
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eNewActivity - 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CAI_BaseNPC::NPC_TranslateActivity( Activity eNewActivity )
{
	Assert( eNewActivity != ACT_INVALID );

	if (InCond(TF_COND_STUNNED))
	{
		if (eNewActivity == ACT_MP_STUN_BEGIN || eNewActivity == ACT_MP_STUN_END)
		{
			if (eNewActivity == ACT_MP_STUN_END)
			{
				RemoveCond(TF_COND_STUNNED);
			}
			return eNewActivity;
		}
		return ACT_MP_STUN_MIDDLE;
	}

	if (eNewActivity == ACT_RANGE_ATTACK1)
	{
		if ( IsCrouching() )
		{
			eNewActivity = ACT_RANGE_ATTACK1_LOW;
		}
	}
	else if (eNewActivity == ACT_RELOAD)
	{
		if (IsCrouching())
		{
			eNewActivity = ACT_RELOAD_LOW;
		}
	}
	else if ( eNewActivity == ACT_IDLE )
	{
		if ( IsCrouching() )
		{
			eNewActivity = ACT_CROUCHIDLE;
		}
	}
	// ====
	// HACK : LEIPZIG 06 -	The underlying problem is that the AR2 and SMG1 cannot map IDLE_ANGRY to a crouched equivalent automatically
	//						which causes the character to pop up and down in their idle state of firing while crouched. -- jdw
	else if ( eNewActivity == ACT_IDLE_ANGRY_SMG1 )
	{
		if ( IsCrouching() )
		{
			eNewActivity = ACT_RANGE_AIM_LOW;
		}
	}
	// ====

	if (CapabilitiesGet() & bits_CAP_DUCK)
	{
		if (eNewActivity == ACT_RELOAD)
		{
			return GetReloadActivity(GetHintNode());
		}
		else if ((eNewActivity == ACT_COVER	)								 ||
				 (eNewActivity == ACT_IDLE && HasMemory(bits_MEMORY_INCOVER)))
		{
			Activity nCoverActivity = GetCoverActivity(GetHintNode());
			// ---------------------------------------------------------------
			// Some NPCs don't have a cover activity defined so just use idle
			// ---------------------------------------------------------------
			if (SelectWeightedSequence( nCoverActivity ) == ACTIVITY_NOT_AVAILABLE)
			{
				nCoverActivity = ACT_IDLE;
			}

			return nCoverActivity;
		}
	}
	return eNewActivity;
}


//-----------------------------------------------------------------------------

Activity CAI_BaseNPC::TranslateActivity( Activity idealActivity, Activity *pIdealWeaponActivity )
{
	const int MAX_TRIES = 5;
	int count = 0;

	bool bIdealWeaponRequired = false;
	Activity idealWeaponActivity;
	Activity baseTranslation;
	bool bWeaponRequired = false;
	Activity weaponTranslation;
	Activity last;
	Activity current;

	idealWeaponActivity = Weapon_TranslateActivity( idealActivity, &bIdealWeaponRequired );
	if ( pIdealWeaponActivity )
		*pIdealWeaponActivity = idealWeaponActivity;

	baseTranslation	  = idealActivity;
	weaponTranslation = idealActivity;
	last			  = idealActivity;
	while ( count++ < MAX_TRIES )
	{
		current = NPC_TranslateActivity( last );
		if ( current != last )
			baseTranslation = current;

		weaponTranslation = Weapon_TranslateActivity( current, &bWeaponRequired );

		if ( weaponTranslation == last )
			break;

		last = weaponTranslation;
	}
	AssertMsg( count < MAX_TRIES, "Circular activity translation!" );

	if ( last == ACT_SCRIPT_CUSTOM_MOVE )
		return ACT_SCRIPT_CUSTOM_MOVE;
	
	if ( HaveSequenceForActivity( weaponTranslation ) )
		return weaponTranslation;
	
	if ( bWeaponRequired )
	{
		// only complain about an activity once
		static CUtlVector< Activity > sUniqueActivities;

		if (!sUniqueActivities.Find( weaponTranslation))
		{
			// FIXME: warning
			DevWarning( "%s missing activity \"%s\" needed by weapon\"%s\"\n", 
				GetClassname(), GetActivityName( weaponTranslation ), GetActiveWeapon()->GetClassname() );

			sUniqueActivities.AddToTail( weaponTranslation );
		}
	}

	if ( baseTranslation != weaponTranslation && HaveSequenceForActivity( baseTranslation ) )
		return baseTranslation;

	if ( idealWeaponActivity != baseTranslation && HaveSequenceForActivity( idealWeaponActivity ) )
		return idealActivity;

	if ( idealActivity != idealWeaponActivity && HaveSequenceForActivity( idealActivity ) )
		return idealActivity;

	Assert( !HaveSequenceForActivity( idealActivity ) );
	if ( idealActivity == ACT_RUN )
	{
		idealActivity = ACT_WALK;
	}
	else if ( idealActivity == ACT_WALK )
	{
		idealActivity = ACT_RUN;
	}

	return idealActivity;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : NewActivity - 
//			iSequence - 
//			translatedActivity - 
//			weaponActivity - 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::ResolveActivityToSequence(Activity NewActivity, int &iSequence, Activity &translatedActivity, Activity &weaponActivity)
{
	AI_PROFILE_SCOPE( CAI_BaseNPC_ResolveActivityToSequence );

	iSequence = ACTIVITY_NOT_AVAILABLE;

	translatedActivity = TranslateActivity( NewActivity, &weaponActivity );

	if ( NewActivity == ACT_SCRIPT_CUSTOM_MOVE )
	{
		iSequence = GetScriptCustomMoveSequence();
	}
	else
	{
		iSequence = SelectWeightedSequence( translatedActivity );

		if ( iSequence == ACTIVITY_NOT_AVAILABLE )
		{
			static CAI_BaseNPC *pLastWarn;
			static Activity lastWarnActivity;
			static float timeLastWarn;

			if ( ( pLastWarn != this && lastWarnActivity != translatedActivity ) || gpGlobals->curtime - timeLastWarn > 5.0 )
			{
				DevWarning( "%s:%s:%s has no sequence for act:%s\n", GetClassname(), GetDebugName(), STRING( GetModelName() ), ActivityList_NameForIndex(translatedActivity) );
				pLastWarn = this;
				lastWarnActivity = translatedActivity;
				timeLastWarn = gpGlobals->curtime;
			}

			if ( translatedActivity == ACT_RUN )
			{
				translatedActivity = ACT_WALK;
				iSequence = SelectWeightedSequence( translatedActivity );
			}
		}
	}

	if ( iSequence == ACT_INVALID )
	{
		// Abject failure. Use sequence zero.
		iSequence = 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : NewActivity - 
//			iSequence - 
//			translatedActivity - 
//			weaponActivity - 
//-----------------------------------------------------------------------------
extern ConVar ai_sequence_debug;

void CAI_BaseNPC::SetActivityAndSequence(Activity NewActivity, int iSequence, Activity translatedActivity, Activity weaponActivity)
{
	m_translatedActivity = translatedActivity;

	if (ai_sequence_debug.GetBool() == true && (m_debugOverlays & OVERLAY_NPC_SELECTED_BIT))
	{
		DevMsg("SetActivityAndSequence : %s: %s:%s -> %s:%s / %s:%s\n", GetClassname(), 
			GetActivityName(GetActivity()), GetSequenceName(GetSequence()),
			GetActivityName(NewActivity), GetSequenceName(iSequence), 
			GetActivityName(translatedActivity), GetActivityName(weaponActivity) );
	
	}

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( GetSequence() != iSequence || !SequenceLoops() )
		{
			//
			// Don't reset frame between movement phased animations
			if (!IsActivityMovementPhased( m_Activity ) || 
				!IsActivityMovementPhased( NewActivity ))
			{
				SetCycle( 0 );
			}
		}

		ResetSequence( iSequence );
		Weapon_SetActivity( weaponActivity, SequenceDuration( iSequence ) );
	}
	else
	{
		// Not available try to get default anim
		ResetSequence( 0 );
	}

	// Set the view position based on the current activity
	SetViewOffset( EyeOffset(m_translatedActivity) );

	if (m_Activity != NewActivity)
	{
		OnChangeActivity(NewActivity);
	}

	// NOTE: We DO NOT write the translated activity here.
	// This is to abstract the activity translation from the AI code.
	// As far as the code is concerned, a translation is merely a new set of sequences
	// that should be regarded as the activity in question.

	// Go ahead and set this so it doesn't keep trying when the anim is not present
	m_Activity = NewActivity;

	// this cannot be called until m_Activity stores NewActivity!
	GetMotor()->RecalculateYawSpeed();
}


//-----------------------------------------------------------------------------
// Purpose: Sets the activity to the desired activity immediately, skipping any
//			transition sequences.
// Input  : NewActivity - 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::SetActivity( Activity NewActivity )
{
	// If I'm already doing the NewActivity I can bail.
	// FIXME: Should this be based on the current translated activity and ideal translated activity (calculated below)?
	//		  The old code only cared about the logical activity, not translated.

	if (m_Activity == NewActivity)
	{
		return;
	}

	// Don't do this if I'm playing a transition, unless it's ACT_RESET.
	if ( NewActivity != ACT_RESET && m_Activity == ACT_TRANSITION && m_IdealActivity != ACT_DO_NOT_DISTURB )
	{
		return;
	}

	if (ai_sequence_debug.GetBool() == true && (m_debugOverlays & OVERLAY_NPC_SELECTED_BIT))
	{
		DevMsg("SetActivity : %s: %s -> %s\n", GetClassname(), GetActivityName(GetActivity()), GetActivityName(NewActivity));
	}

	if ( !GetModelPtr() )
		return;

	// In case someone calls this with something other than the ideal activity.
	m_IdealActivity = NewActivity;

	// Resolve to ideals and apply directly, skipping transitions.
	ResolveActivityToSequence(m_IdealActivity, m_nIdealSequence, m_IdealTranslatedActivity, m_IdealWeaponActivity);

	//DevMsg("%s: SLAM %s -> %s\n", GetClassname(), GetSequenceName(GetSequence()), GetSequenceName(m_nIdealSequence));

	SetActivityAndSequence(m_IdealActivity, m_nIdealSequence, m_IdealTranslatedActivity, m_IdealWeaponActivity);
}


//-----------------------------------------------------------------------------
// Purpose: Sets the activity that we would like to transition toward.
// Input  : NewActivity - 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::SetIdealActivity( Activity NewActivity )
{
  	// ignore if it's an ACT_TRANSITION, it means somewhere we're setting IdealActivity with a bogus intermediate value
	if (NewActivity == ACT_TRANSITION)
	{
		Assert( 0 );
		return;
	}

	if (ai_sequence_debug.GetBool() == true && (m_debugOverlays & OVERLAY_NPC_SELECTED_BIT))
	{
		DevMsg("SetIdealActivity : %s: %s -> %s\n", GetClassname(), GetActivityName(GetActivity()), GetActivityName(NewActivity));
	}


	if (NewActivity == ACT_RESET)
	{
		// They probably meant to call SetActivity(ACT_RESET)... we'll fix it for them.
		SetActivity(ACT_RESET);
		return;
	}

	m_IdealActivity = NewActivity;

	if( NewActivity == ACT_DO_NOT_DISTURB )
	{
		// Don't resolve anything! Leave it the way the user has it right now.
		return;
	}

	if ( !GetModelPtr() )
		return;

	// Perform translation in case we need to change sequences within a single activity,
	// such as between a standing idle and a crouching idle.
	ResolveActivityToSequence(m_IdealActivity, m_nIdealSequence, m_IdealTranslatedActivity, m_IdealWeaponActivity);
}


//-----------------------------------------------------------------------------
// Purpose: Moves toward the ideal activity through any transition sequences.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::AdvanceToIdealActivity(void)
{
	// If there is a transition sequence between the current sequence and the ideal sequence...
	int nNextSequence = FindTransitionSequence(GetSequence(), m_nIdealSequence, NULL);
	if (nNextSequence != -1)
	{
		// We found a transition sequence or possibly went straight to
		// the ideal sequence.
		if (nNextSequence != m_nIdealSequence)
		{
//			DevMsg("%s: TRANSITION %s -> %s -> %s\n", GetClassname(), GetSequenceName(GetSequence()), GetSequenceName(nNextSequence), GetSequenceName(m_nIdealSequence));

			Activity eWeaponActivity = ACT_TRANSITION;
			Activity eTranslatedActivity = ACT_TRANSITION;

			// Figure out if the transition sequence has an associated activity that
			// we can use for our weapon. Do activity translation also.
			Activity eTransitionActivity = GetSequenceActivity(nNextSequence);
			if (eTransitionActivity != ACT_INVALID)
			{
				int nDiscard;
				ResolveActivityToSequence(eTransitionActivity, nDiscard, eTranslatedActivity, eWeaponActivity);
			}

			// Set activity and sequence to the transition stuff. Set the activity to ACT_TRANSITION
			// so we know we're in a transition.
			SetActivityAndSequence(ACT_TRANSITION, nNextSequence, eTranslatedActivity, eWeaponActivity);
		}
		else
		{
			//DevMsg("%s: IDEAL %s -> %s\n", GetClassname(), GetSequenceName(GetSequence()), GetSequenceName(m_nIdealSequence));

			// Set activity and sequence to the ideal stuff that was set up in MaintainActivity.
			SetActivityAndSequence(m_IdealActivity, m_nIdealSequence, m_IdealTranslatedActivity, m_IdealWeaponActivity);
		}
	}
	// Else go straight there to the ideal activity.
	else
	{
		//DevMsg("%s: Unable to get from sequence %s to %s!\n", GetClassname(), GetSequenceName(GetSequence()), GetSequenceName(m_nIdealSequence));
		SetActivity(m_IdealActivity);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Tries to achieve our ideal animation state, playing any transition
//			sequences that we need to play to get there.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::MaintainActivity(void)
{
	AI_PROFILE_SCOPE( CAI_BaseNPC_MaintainActivity );

	if ( m_lifeState == LIFE_DEAD )
	{
		// Don't maintain activities if we're daid.
		// Blame Speyrer
		return;
	}

	if ((GetState() == NPC_STATE_SCRIPT))
	{
		// HACK: finish any transitions we might be playing before we yield control to the script
		if (GetActivity() != ACT_TRANSITION)
		{
			// Our animation state is being controlled by a script.
			return;
		}
	}

	if( m_IdealActivity == ACT_DO_NOT_DISTURB || !GetModelPtr() )
	{
		return;
	}

	// We may have work to do if we aren't playing our ideal activity OR if we
	// aren't playing our ideal sequence.
	if ((GetActivity() != m_IdealActivity) || (GetSequence() != m_nIdealSequence))
	{
		if (ai_sequence_debug.GetBool() == true && (m_debugOverlays & OVERLAY_NPC_SELECTED_BIT))
		{
			DevMsg("MaintainActivity %s : %s:%s -> %s:%s\n", GetClassname(), 
				GetActivityName(GetActivity()), GetSequenceName(GetSequence()), 
				GetActivityName(m_IdealActivity), GetSequenceName(m_nIdealSequence));
		}
		
		bool bAdvance = false;

		// If we're in a transition activity, see if we are done with the transition.
		if (GetActivity() == ACT_TRANSITION)
		{
			// If the current sequence is finished, try to go to the next one
			// closer to our ideal sequence.
			if (IsSequenceFinished())
			{
				bAdvance = true;
			}
			// Else a transition sequence is in progress, do nothing.
		}
		// Else get a specific sequence for the activity and try to transition to that.
		else
		{
			// Save off a target sequence and translated activities to apply when we finish
			// playing all the transitions and finally arrive at our ideal activity.
			ResolveActivityToSequence(m_IdealActivity, m_nIdealSequence, m_IdealTranslatedActivity, m_IdealWeaponActivity);
			bAdvance = true;
		}
		
		if (bAdvance)
		{
			// Try to go to the next sequence closer to our ideal sequence.
			AdvanceToIdealActivity();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if our ideal activity has finished playing.
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::IsActivityFinished( void )
{
	return (IsSequenceFinished() && (GetSequence() == m_nIdealSequence));
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if the activity is one of the standard phase-matched movement activities
// Input  : activity
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::IsActivityMovementPhased( Activity activity )
{
	switch( activity )
	{
	case ACT_WALK:
	case ACT_WALK_AIM:
	case ACT_WALK_CROUCH:
	case ACT_WALK_CROUCH_AIM:
	case ACT_RUN:
	case ACT_RUN_AIM:
	case ACT_RUN_CROUCH:
	case ACT_RUN_CROUCH_AIM:
	case ACT_RUN_PROTECTED:
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::OnChangeActivity( Activity eNewActivity )
{
	if ( eNewActivity == ACT_RUN || 
		 eNewActivity == ACT_RUN_AIM ||
		 eNewActivity == ACT_WALK )
	{
		Stand();
	}
}

//=========================================================
// SetSequenceByName
//=========================================================
void CAI_BaseNPC::SetSequenceByName( const char *szSequence )
{
	int iSequence = LookupSequence( szSequence );

	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
		SetSequenceById( iSequence );
	else
	{
		DevWarning( 2, "%s has no sequence %s to match request\n", GetClassname(), szSequence );
		SetSequence( 0 );	// Set to the reset anim (if it's there)
	}
}

//-----------------------------------------------------------------------------

void CAI_BaseNPC::SetSequenceById( int iSequence )
{
	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( GetSequence() != iSequence || !SequenceLoops() )
		{
			SetCycle( 0 );
		}

		ResetSequence( iSequence );	// Set to the reset anim (if it's there)
		GetMotor()->RecalculateYawSpeed();
	}
	else
	{
		// Not available try to get default anim
		DevWarning( 2, "%s invalid sequence requested\n", GetClassname() );
		SetSequence( 0 );	// Set to the reset anim (if it's there)
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the target entity
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseEntity *CAI_BaseNPC::GetNavTargetEntity(void)
{
	if ( GetNavigator()->GetGoalType() == GOALTYPE_ENEMY )
		return m_hEnemy;
	else if ( GetNavigator()->GetGoalType() == GOALTYPE_TARGETENT )
		return m_hTargetEnt;
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: returns zero if the caller can jump from
//			vecStart to vecEnd ignoring collisions with pTarget
//
//			if the throw fails, returns the distance
//			that can be travelled before an obstacle is hit
//-----------------------------------------------------------------------------
#include "ai_initutils.h"
//#define _THROWDEBUG
float CAI_BaseNPC::ThrowLimit(	const Vector &vecStart,
								const Vector &vecEnd,
								float		fGravity,
								float		fArcSize,
								const Vector &mins,
								const Vector &maxs,
								CBaseEntity *pTarget,
								Vector		*jumpVel,
								CBaseEntity **pBlocker)
{
	// Get my jump velocity
	Vector rawJumpVel	= CalcThrowVelocity(vecStart, vecEnd, fGravity, fArcSize);
	*jumpVel			= rawJumpVel;
	Vector vecFrom		= vecStart;

	// Calculate the total time of the jump minus a tiny fraction
	float jumpTime		= (vecStart - vecEnd).Length2D()/rawJumpVel.Length2D();
	float timeStep		= jumpTime / 10.0;

	Vector gravity = Vector(0,0,fGravity);

	// this loop takes single steps to the goal.
	for (float flTime = 0 ; flTime < jumpTime-0.1 ; flTime += timeStep )
	{
		// Calculate my position after the time step (average velocity over this time step)
		Vector nextPos = vecFrom + (rawJumpVel - 0.5 * gravity * timeStep) * timeStep;

		// If last time step make next position the target position
		if ((flTime + timeStep) > jumpTime)
		{
			nextPos = vecEnd;
		}

		trace_t tr;
		UTIL_TraceHull( vecFrom, nextPos, mins, maxs, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

		if (tr.startsolid || tr.fraction < 1.0)
		{
			CBaseEntity *pEntity = tr.m_pEnt;

			// If we hit the target we are good to go!
			if (pEntity == pTarget)
			{
				return 0;
			}

#ifdef _THROWDEBUG
			NDebugOverlay::Line( vecFrom, nextPos, 255, 0, 0, true, 1.0 );
#endif
			// ----------------------------------------------------------
			// If blocked by an npc remember
			// ----------------------------------------------------------
			*pBlocker = pEntity;

			// Return distance sucessfully traveled before block encountered
			return ((tr.endpos - vecStart).Length());
		}
#ifdef _THROWDEBUG
		else
		{
			NDebugOverlay::Line( vecFrom, nextPos, 255, 255, 255, true, 1.0 );
		}
#endif


		rawJumpVel  = rawJumpVel - gravity * timeStep;
		vecFrom		= nextPos;
	}
	return 0;
}



//-----------------------------------------------------------------------------
// Purpose: Called to initialize or re-initialize the vphysics hull when the size
//			of the NPC changes
//-----------------------------------------------------------------------------
void CAI_BaseNPC::SetupVPhysicsHull()
{
	if ( GetMoveType() == MOVETYPE_VPHYSICS || GetMoveType() == MOVETYPE_NONE )
		return;

	if ( VPhysicsGetObject() )
	{
		// Disable collisions to get 
		VPhysicsGetObject()->EnableCollisions(false);
		VPhysicsDestroyObject();
	}
	VPhysicsInitShadow( true, false );
	IPhysicsObject *pPhysObj = VPhysicsGetObject();
	if ( pPhysObj )
	{
		float mass = Studio_GetMass(GetModelPtr());
		if ( mass > 0 )
		{
			pPhysObj->SetMass( mass );
		}
#if _DEBUG
		else
		{
			DevMsg("Warning: %s has no physical mass\n", STRING(GetModelName()));
		}
#endif
		IPhysicsShadowController *pController = pPhysObj->GetShadowController();
		float avgsize = (WorldAlignSize().x + WorldAlignSize().y) * 0.5;
		pController->SetTeleportDistance( avgsize * 0.5 );
		m_bCheckContacts = true;
	}
}


// Check for problematic physics objects resting on this NPC.
// They can screw up his navigation, so attach a controller to 
// help separate the NPC & physics when you encounter these.
ConVar ai_auto_contact_solver( "ai_auto_contact_solver", "1" );
void CAI_BaseNPC::CheckPhysicsContacts()
{
	if ( gpGlobals->frametime <= 0.0f || !ai_auto_contact_solver.GetBool() )
		return;

	m_bCheckContacts = false;
	if ( GetMoveType() == MOVETYPE_STEP && VPhysicsGetObject())
	{
		IPhysicsObject *pPhysics = VPhysicsGetObject();
		IPhysicsFrictionSnapshot *pSnapshot = pPhysics->CreateFrictionSnapshot();
		CBaseEntity *pGroundEntity = GetGroundEntity();
		float heightCheck = GetAbsOrigin().z + GetHullMaxs().z;
		Vector npcVel;
		pPhysics->GetVelocity( &npcVel, NULL );
		CBaseEntity *pOtherEntity = NULL;
		bool createSolver = false;
		float solverTime = 0.0f;
		while ( pSnapshot->IsValid() )
		{
			IPhysicsObject *pOther = pSnapshot->GetObject(1);
			pOtherEntity = static_cast<CBaseEntity *>(pOther->GetGameData());

			if ( pOtherEntity && pGroundEntity != pOtherEntity )
			{
				float otherMass = PhysGetEntityMass(pOtherEntity);

				if ( pOtherEntity->GetMoveType() == MOVETYPE_VPHYSICS &&  pOther->IsMoveable() && 
					otherMass < VPHYSICS_LARGE_OBJECT_MASS  && !pOtherEntity->GetServerVehicle() )
				{
					m_bCheckContacts = true;
					Vector vel, point;
					pOther->GetVelocity( &vel, NULL );
					pSnapshot->GetContactPoint( point );
					
					// compare the relative velocity
					vel -= npcVel;

					// slow moving object probably won't clear itself.
					// Either set ignore, or disable collisions entirely
					if ( vel.LengthSqr() < 5.0f*5.0f )
					{
						float topdist = fabs(point.z-heightCheck);
						// 4 seconds to ignore this for nav
						solverTime = 4.0f;
						if ( topdist < 2.0f )
						{
							// Resting on my head so disable collisions for a bit
							solverTime = 0.5f; // UNDONE: Tune
							if ( pOther->GetGameFlags() & FVPHYSICS_PLAYER_HELD )
							{
								// player is being a monkey
								solverTime = 0.25f;
							}

							//Msg("Dropping %s from %s\n", pOtherEntity->GetClassname(), GetClassname() );
							Assert( !NPCPhysics_SolverExists(this, pOtherEntity) );
							createSolver = true;
							break;
						}
					}
				}
			}
			pSnapshot->NextFrictionData();
		}
		pPhysics->DestroyFrictionSnapshot( pSnapshot );
		if ( createSolver )
		{
			// turn collisions back on once we've been separated for enough time
			NPCPhysics_CreateSolver( this, pOtherEntity, true, solverTime );
			pPhysics->RecheckContactPoints();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_BaseNPC::StartTouch( CBaseEntity *pOther )
{
	BaseClass::StartTouch(pOther);

	if ( pOther->GetMoveType() == MOVETYPE_VPHYSICS )
	{
		m_bCheckContacts = true;
	}
	if ( !InSameTeam( pOther ) )
	{
		if ( pOther->IsPlayer() )
		{
			CTFPlayer *pPlayer = ToTFPlayer( pOther );
			if ( pPlayer )
			{
				if ( pPlayer->m_Shared.InCond( TF_COND_STEALTHED ) )
				{
					pPlayer->m_Shared.OnSpyTouchedByEnemy();
				}

				if ( InCond( TF_COND_RUNE_PLAGUE ) && !pPlayer->m_Shared.InCond( TF_COND_RUNE_RESIST ) )
				{
					pPlayer->m_Shared.AddCond( TF_COND_PLAGUE );
					pPlayer->m_Shared.MakeBleed( NULL, NULL, 999, 4 );
					pPlayer->EmitSound( "Powerup.PickUpPlagueInfected" );
				}

				/*if ( InCond( TF_COND_HALLOWEEN_BOMB_HEAD ) )
					MerasmusPlayerBombExplode( false );*/
			}
		}

		if ( pOther->IsNPC() )
		{
			CAI_BaseNPC *pNPC = dynamic_cast<CAI_BaseNPC *>( pOther );
			if ( pNPC )
			{
				if ( InCond( TF_COND_RUNE_PLAGUE ) && !pNPC->InCond( TF_COND_RUNE_RESIST ) )
				{
					pNPC->AddCond( TF_COND_PLAGUE );
					pNPC->MakeBleed( NULL, NULL, 999, 4 );
					pNPC->EmitSound( "Powerup.PickUpPlagueInfected" );
				}

				/*if ( InCond( TF_COND_HALLOWEEN_BOMB_HEAD ) )
					MerasmusPlayerBombExplode( false );*/
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: To be called instead of UTIL_SetSize, so pathfinding hull and actual hull agree
//-----------------------------------------------------------------------------
void CAI_BaseNPC::SetHullSizeNormal( bool force )
{
	if ( m_fIsUsingSmallHull || force )
	{
		// Find out what the height difference will be between the versions and adjust our bbox accordingly to keep us level
		const float flScale = GetModelScale();
		Vector vecMins = ( GetHullMins() * flScale );
		Vector vecMaxs = ( GetHullMaxs() * flScale );
		
		UTIL_SetSize( this, vecMins, vecMaxs );

		m_fIsUsingSmallHull = false;
		if ( VPhysicsGetObject() )
		{
			SetupVPhysicsHull();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: To be called instead of UTIL_SetSize, so pathfinding hull
//			and actual hull agree
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::SetHullSizeSmall( bool force )
{
	if ( !m_fIsUsingSmallHull || force )
	{
		UTIL_SetSize(this, NAI_Hull::SmallMins(GetHullType()),NAI_Hull::SmallMaxs(GetHullType()));
		m_fIsUsingSmallHull = true;
		if ( VPhysicsGetObject() )
		{
			SetupVPhysicsHull();
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// Checks to see that the nav hull is valid for the NPC
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::IsNavHullValid() const
{
	Assert( GetSolid() != SOLID_BSP );

	Vector hullMin = GetHullMins();
	Vector hullMax = GetHullMaxs();
	Vector vecMins, vecMaxs;
	if ( GetSolid() == SOLID_BBOX )
	{
		vecMins = WorldAlignMins();
		vecMaxs = WorldAlignMaxs();
	}
	else if ( GetSolid() == SOLID_VPHYSICS )
	{
		Assert( VPhysicsGetObject() );
		const CPhysCollide *pPhysCollide = VPhysicsGetObject()->GetCollide();
		physcollision->CollideGetAABB( &vecMins, &vecMaxs, pPhysCollide, GetAbsOrigin(), GetAbsAngles() ); 
		vecMins -= GetAbsOrigin();
		vecMaxs -= GetAbsOrigin();
	}
	else
	{
		vecMins = hullMin;
		vecMaxs = hullMax;
	}

	if ( (hullMin.x > vecMins.x) || (hullMax.x < vecMaxs.x) ||
		(hullMin.y > vecMins.y) || (hullMax.y < vecMaxs.y) ||
		(hullMin.z > vecMins.z) || (hullMax.z < vecMaxs.z) )
	{
		return false;
	}

	return true;
}


//=========================================================
// NPCInit - after a npc is spawned, it needs to
// be dropped into the world, checked for mobility problems,
// and put on the proper path, if any. This function does
// all of those things after the npc spawns. Any
// initialization that should take place for all npcs
// goes here.
//=========================================================
void CAI_BaseNPC::NPCInit( void )
{
	if (!g_pGameRules->FAllowNPCs())
	{
		UTIL_Remove( this );
		return;
	}

	if( IsWaitingToRappel() )
	{
		// If this guy's supposed to rappel, keep him from
		// falling to the ground when he spawns.
		AddFlag( FL_FLY );
	}

	if ( TFGameRules()->IsSkillLevel( SKILL_HAUNTED ) )
	{
		SetClassname( "npc_zombie" );
		MaterialOverride( "models/effects/resist_shield/resist_shield_blue" );
	}

#ifdef _DEBUG
	// Make sure that the bounding box is appropriate for the hull size...
	// FIXME: We can't test vphysics objects because NPCInit occurs before VPhysics is set up
	if ( GetSolid() != SOLID_VPHYSICS && !IsSolidFlagSet(FSOLID_NOT_SOLID) )
	{
		if ( !IsNavHullValid() )
		{
			Warning("NPC Entity %s (%d) has a bounding box which extends outside its nav box!\n",
				STRING(m_iClassname), entindex() );
		}
	}
#endif

	// Set fields common to all npcs
	AddFlag( FL_AIMTARGET | FL_NPC );

	m_flOriginalYaw = GetAbsAngles().y;

	SetBlocksLOS( false );

	SetGravity(1.0);	// Don't change
	m_takedamage		= DAMAGE_YES;
	GetMotor()->SetIdealYaw( GetLocalAngles().y );
	m_iMaxHealth		= m_iHealth;
	m_lifeState			= LIFE_ALIVE;
	SetIdealState( NPC_STATE_IDLE );// Assume npc will be idle, until proven otherwise
	SetIdealActivity( ACT_IDLE );
	SetActivity( ACT_IDLE );

	if ( TFGameRules()->IsInHL1Map() )
		SetDeathPose( ACT_INVALID );

	// track where we are in the nav mesh
	UpdateLastKnownArea();

	ClearCommandGoal();

	ClearSchedule( "Initializing NPC" );
	GetNavigator()->ClearGoal();
	InitBoneControllers( ); // FIX: should be done in Spawn
	if ( GetModelPtr() )
	{
		ResetActivityIndexes();
		ResetEventIndexes();
	}

	SetHintNode( NULL );

	m_afMemory			= MEMORY_CLEAR;

	SetEnemy( NULL );

	m_flDistTooFar		= 1024.0;
	SetDistLook( 2048.0 );

	if ( HasSpawnFlags( SF_NPC_LONG_RANGE ) )
	{
		m_flDistTooFar	= 1e9f;
		SetDistLook( 6000.0 );
	}

	// Clear conditions
	m_Conditions.ClearAll();

	// set eye position
	SetDefaultEyeOffset();

	// Only give weapon of allowed to have one
	if (CapabilitiesGet() & bits_CAP_USE_WEAPONS)
	{	// Does this npc spawn with a weapon
		if ( m_spawnEquipment != NULL_STRING && strcmp(STRING(m_spawnEquipment), "0"))
		{
			CBaseCombatWeapon *pWeapon = Weapon_Create( STRING(m_spawnEquipment) );
			if ( pWeapon )
			{
				// If I have a name, make my weapon match it with "_weapon" appended
				if ( GetEntityName() != NULL_STRING )
				{
					pWeapon->SetName( AllocPooledString(UTIL_VarArgs("%s_weapon", STRING(GetEntityName()))) );
				}

				if ( GetEffects() & EF_NOSHADOW )
				{
					// BUGBUG: if this NPC drops this weapon it will forevermore have no shadow
					pWeapon->AddEffects( EF_NOSHADOW );
				}

				Weapon_Equip( pWeapon );
			}
		}
	}

	// Robin: Removed this, since it stomps the weapon's settings, and it's stomped
	//		  by OnUpdateShotRegulator() as soon as they finish firing the first time.
	//GetShotRegulator()->SetParameters( 2, 6, 0.3f, 0.8f );

	SetUse ( &CAI_BaseNPC::NPCUse );

	// NOTE: Can't call NPC Init Think directly... logic changed about
	// what time it is when worldspawn happens..

	// We must put off the rest of our initialization
	// until we're sure everything else has had a chance to spawn. Otherwise
	// we may try to reference entities that haven't spawned yet.(sjb)
	SetThink( &CAI_BaseNPC::NPCInitThink );
	SetNextThink( gpGlobals->curtime + 0.01f );

	ForceGatherConditions();

	// HACKHACK: set up a pre idle animation
	// NOTE: Must do this before CreateVPhysics() so bone followers have the correct initial positions.
	if ( HasSpawnFlags( SF_NPC_WAIT_FOR_SCRIPT ) )
	{
		const char *pStartSequence = CAI_ScriptedSequence::GetSpawnPreIdleSequenceForScript( this );
		if ( pStartSequence )
		{
			SetSequence( LookupSequence( pStartSequence ) );
		}
	}

	CreateVPhysics();

	if ( HasSpawnFlags( SF_NPC_START_EFFICIENT ) )
	{
		SetEfficiency( AIE_EFFICIENT );
	}

	m_bFadeCorpse = ShouldFadeOnDeath();

	m_GiveUpOnDeadEnemyTimer.Set( 0.75, 2.0 );

	m_flTimeLastMovement = FLT_MAX;

	m_flIgnoreDangerSoundsUntil = 0;
	
	SetDeathPose( ACT_INVALID );
	SetDeathPoseFrame( 0 );

	m_EnemiesSerialNumber = -1;

	if ( TFGameRules()->IsSkillLevel( SKILL_UNUSUAL ) )
		UnusualInit();

	if ( TFGameRules()->IsCoOp() && GetGlobalTFTeam(TF_TEAM_RED)->GetNumPlayers() < 9 )
	{
		m_iMaxHealth = m_iHealth = m_iHealth + m_iHealth * (0.25 * GetGlobalTFTeam(TF_TEAM_RED)->GetNumPlayers());
	}
	else if (TFGameRules()->IsCoOp() && GetGlobalTFTeam(TF_TEAM_RED)->GetNumPlayers() >= 9 )
	{
		m_iMaxHealth = m_iHealth = m_iHealth + m_iHealth * 2;
	}
	else if ( TFGameRules()->IsBluCoOp() && GetGlobalTFTeam(TF_TEAM_BLUE)->GetNumPlayers() < 9 )
	{
		m_iMaxHealth = m_iHealth = m_iHealth + m_iHealth * (0.25 * GetGlobalTFTeam(TF_TEAM_BLUE)->GetNumPlayers());
	}
	else if (TFGameRules()->IsBluCoOp() && GetGlobalTFTeam(TF_TEAM_BLUE)->GetNumPlayers() >= 9)
	{
		m_iMaxHealth = m_iHealth = m_iHealth + m_iHealth * 2;
	}
}

//-----------------------------------------------------------------------------

bool CAI_BaseNPC::CreateVPhysics()
{
	if ( IsAlive() && !VPhysicsGetObject() )
	{
		SetupVPhysicsHull();
	}
	return true;
}


//-----------------------------------------------------------------------------
// Set up the shot regulator based on the equipped weapon
//-----------------------------------------------------------------------------
void CAI_BaseNPC::OnUpdateShotRegulator( )
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if ( !pWeapon )
		return;

	// Default values
	m_ShotRegulator.SetBurstInterval( pWeapon->GetFireRate(), pWeapon->GetFireRate() );
	m_ShotRegulator.SetBurstShotCountRange( pWeapon->GetMinBurst(), pWeapon->GetMaxBurst() );
	m_ShotRegulator.SetRestInterval( pWeapon->GetMinRestTime(), pWeapon->GetMaxRestTime() );

	// Let the behavior have a whack at it.
	if ( GetRunningBehavior() )
	{
		GetRunningBehavior()->OnUpdateShotRegulator();
	}
}


//-----------------------------------------------------------------------------
// Set up the shot regulator based on the equipped weapon
//-----------------------------------------------------------------------------
void CAI_BaseNPC::OnChangeActiveWeapon( CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon )
{
	BaseClass::OnChangeActiveWeapon( pOldWeapon, pNewWeapon );

	// Shot regulator code
	if ( pNewWeapon )
	{
		OnUpdateShotRegulator();
		m_ShotRegulator.Reset( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Tests to see if NPC can holster their weapon (if animation exists to holster weapon)
// Output : true if holster weapon animation exists
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::CanHolsterWeapon( void )
{
	int seq = SelectWeightedSequence( ACT_DISARM );
	return (seq >= 0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CAI_BaseNPC::HolsterWeapon( void )
{
	if ( IsWeaponHolstered() )
		return -1;

	int iHolsterGesture = FindGestureLayer( ACT_DISARM );
	if ( iHolsterGesture != -1 )
		return iHolsterGesture;

	int iLayer = AddGesture( ACT_DISARM, true );
	//iLayer = AddGesture( ACT_GESTURE_DISARM, true );

	if (iLayer != -1)
	{
		// Prevent firing during the holster / unholster
		float flDuration = GetLayerDuration( iLayer );
		m_ShotRegulator.FireNoEarlierThan( gpGlobals->curtime + flDuration + 0.5 );

		if( m_iDesiredWeaponState == DESIREDWEAPONSTATE_HOLSTERED_DESTROYED )
		{
			m_iDesiredWeaponState = DESIREDWEAPONSTATE_CHANGING_DESTROY;
		}
		else
		{
			m_iDesiredWeaponState = DESIREDWEAPONSTATE_CHANGING;
		}

		// Make sure we don't try to reload while we're holstering
		ClearCondition(COND_LOW_PRIMARY_AMMO);
		ClearCondition(COND_NO_PRIMARY_AMMO);
		ClearCondition(COND_NO_SECONDARY_AMMO);
	}

	return iLayer;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_BaseNPC::UnholsterWeapon( void )
{
	if ( !IsWeaponHolstered() )
 		return -1;

	int iHolsterGesture = FindGestureLayer( ACT_ARM );
	if ( iHolsterGesture != -1 )
		return iHolsterGesture;

	// Deploy the first weapon you can find
	for (int i = 0; i < WeaponCount(); i++)
	{
		if ( GetWeapon( i ))
		{
			SetActiveWeapon( GetWeapon(i) );

			int iLayer = AddGesture( ACT_ARM, true );
			//iLayer = AddGesture( ACT_GESTURE_ARM, true );

			if (iLayer != -1)
			{
				// Prevent firing during the holster / unholster
				float flDuration = GetLayerDuration( iLayer );
				m_ShotRegulator.FireNoEarlierThan( gpGlobals->curtime + flDuration + 0.5 );

				m_iDesiredWeaponState = DESIREDWEAPONSTATE_CHANGING;
			}

			// Refill the clip
			if ( GetActiveWeapon()->UsesClipsForAmmo1() )
			{
				GetActiveWeapon()->m_iClip1 = GetActiveWeapon()->GetMaxClip1(); 
			}

			// Make sure we don't try to reload while we're unholstering
			ClearCondition(COND_LOW_PRIMARY_AMMO);
			ClearCondition(COND_NO_PRIMARY_AMMO);
			ClearCondition(COND_NO_SECONDARY_AMMO);

			return iLayer;
		}
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputHolsterWeapon( inputdata_t &inputdata )
{
	m_iDesiredWeaponState = DESIREDWEAPONSTATE_HOLSTERED;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputHolsterAndDestroyWeapon( inputdata_t &inputdata )
{
	m_iDesiredWeaponState = DESIREDWEAPONSTATE_HOLSTERED_DESTROYED;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputUnholsterWeapon( inputdata_t &inputdata )
{
	m_iDesiredWeaponState = DESIREDWEAPONSTATE_UNHOLSTERED;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::IsWeaponHolstered( void )
{
	if( !GetActiveWeapon() )
		return true;

	if( GetActiveWeapon()->IsEffectActive(EF_NODRAW) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::IsWeaponStateChanging( void )
{
	return ( m_iDesiredWeaponState == DESIREDWEAPONSTATE_CHANGING || m_iDesiredWeaponState == DESIREDWEAPONSTATE_CHANGING_DESTROY );
}

//-----------------------------------------------------------------------------
// Set up the shot regulator based on the equipped weapon
//-----------------------------------------------------------------------------
void CAI_BaseNPC::OnRangeAttack1()
{
	SetLastAttackTime( gpGlobals->curtime );

	// Houston, there is a problem!
	AssertOnce( GetShotRegulator()->ShouldShoot() );

	m_ShotRegulator.OnFiredWeapon();
	if ( m_ShotRegulator.IsInRestInterval() )
	{
		OnUpdateShotRegulator();
	}

	SetNextAttack( m_ShotRegulator.NextShotTime() );
}


//-----------------------------------------------------------------------------
// Purpose: Initialze the relationship table from the keyvalues
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InitRelationshipTable(void)
{
	AddRelationship( STRING( m_RelationshipString ), NULL );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_BaseNPC::AddRelationship( const char *pszRelationship, CBaseEntity *pActivator )
{
	// Parse the keyvalue data
	char parseString[1000];
	Q_strncpy(parseString, pszRelationship, sizeof(parseString));

	// Look for an entity string
	char *entityString = strtok(parseString," ");
	while (entityString)
	{
		// Get the disposition
		char *dispositionString = strtok(NULL," ");
		Disposition_t disposition = D_NU;
		if ( dispositionString )
		{
			if (!stricmp(dispositionString,"D_HT"))
			{
				disposition = D_HT;
			}
			else if (!stricmp(dispositionString,"D_FR"))
			{
				disposition = D_FR;
			}
			else if (!stricmp(dispositionString,"D_LI"))
			{
				disposition = D_LI;
			}
			else if (!stricmp(dispositionString,"D_NU"))
			{
				disposition = D_NU;
			}
			else
			{
				disposition = D_NU;
				Warning( "***ERROR***\nBad relationship type (%s) to unknown entity (%s)!\n", dispositionString,entityString );
				Assert( 0 );
				return;
			}
		}
		else
		{
			Warning("Can't parse relationship info (%s) - Expecting 'name [D_HT, D_FR, D_LI, D_NU] [1-99]'\n", pszRelationship );
			Assert(0);
			return;
		}

		// Get the priority
		char *priorityString	= strtok(NULL," ");
		int	priority = ( priorityString ) ? atoi(priorityString) : DEF_RELATIONSHIP_PRIORITY;

		bool bFoundEntity = false;

		// Try to get pointer to an entity of this name
		CBaseEntity *entity = gEntList.FindEntityByName( NULL, entityString );
		while( entity )
		{
			// make sure you catch all entities of this name.
			bFoundEntity = true;
			AddEntityRelationship(entity, disposition, priority );
			entity = gEntList.FindEntityByName( entity, entityString );
		}

		if( !bFoundEntity )
		{
			// Need special condition for player as we can only have one
			if (!stricmp("player", entityString) || !stricmp("!player", entityString))
			{
				AddClassRelationship( CLASS_PLAYER, disposition, priority );
			}
			// Otherwise try to create one too see if a valid classname and get class type
			else
			{
				// HACKHACK:
				CBaseEntity *pEntity = CanCreateEntityClass( entityString ) ? CreateEntityByName( entityString ) : NULL;
				if (pEntity)
				{
					AddClassRelationship( pEntity->Classify(), disposition, priority );
					UTIL_RemoveImmediate(pEntity);
				}
				else
				{
					DevWarning( "Couldn't set relationship to unknown entity or class (%s)!\n", entityString );
				}
			}
		}
		// Check for another entity in the list
		entityString		= strtok(NULL," ");
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_BaseNPC::AddEntityRelationship( CBaseEntity *pEntity, Disposition_t nDisposition, int nPriority )
{
#if 0
	ForceGatherConditions();
#endif
	BaseClass::AddEntityRelationship( pEntity, nDisposition, nPriority );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_BaseNPC::AddClassRelationship( Class_T nClass, Disposition_t nDisposition, int nPriority )
{
#if 0
	ForceGatherConditions();
#endif
	BaseClass::AddClassRelationship( nClass, nDisposition, nPriority );
}

//=========================================================
// NPCInitThink - Calls StartNPC. Startnpc is
// virtual, but this function cannot be
//=========================================================
void CAI_BaseNPC::NPCInitThink ( void )
{
	// Initialize the relationship table
	InitRelationshipTable();

	StartNPC();

	PostNPCInit();

	if( GetSleepState() == AISS_AUTO_PVS )
	{
		// This code is a bit wonky, but it makes it easier for level designers to
		// select this option in Hammer. So we set a sleep flag to indicate the choice,
		// and then set the sleep state to awake (normal)
		AddSleepFlags( AI_SLEEP_FLAG_AUTO_PVS );
		SetSleepState( AISS_AWAKE );
	}

	if( GetSleepState() == AISS_AUTO_PVS_AFTER_PVS )
	{
		AddSleepFlags( AI_SLEEP_FLAG_AUTO_PVS_AFTER_PVS );
		SetSleepState( AISS_AWAKE );
	}

	if ( GetSleepState() > AISS_AWAKE )
	{
		Sleep();
	}

	m_flLastRealThinkTime = gpGlobals->curtime;
}

//=========================================================
// StartNPC - final bit of initization before a npc
// is turned over to the AI.
//=========================================================
void CAI_BaseNPC::StartNPC( void )
{
	// Raise npc off the floor one unit, then drop to floor
	if ( (GetMoveType() != MOVETYPE_FLY) && (GetMoveType() != MOVETYPE_FLYGRAVITY) &&
		 !(CapabilitiesGet() & bits_CAP_MOVE_FLY) &&
		 !HasSpawnFlags( SF_NPC_FALL_TO_GROUND ) && !IsWaitingToRappel() && !GetMoveParent() )
	{
		Vector origin = GetLocalOrigin();

		if (!GetMoveProbe()->FloorPoint( origin + Vector(0, 0, 0.1), GetAITraceMask(), 0, -2048, &origin ))
		{
			Warning( "NPC %s stuck in wall--level design error at (%.2f %.2f %.2f)\n", GetClassname(), GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
			if ( g_pDeveloper->GetInt() > 1 )
			{
				m_debugOverlays |= OVERLAY_BBOX_BIT;
			}
		}

		SetLocalOrigin( origin );
	}
	else
	{
		SetGroundEntity( NULL );
	}

	if ( m_target != NULL_STRING )// this npc has a target
	{
		// Find the npc's initial target entity, stash it
		SetGoalEnt( gEntList.FindEntityByName( NULL, m_target ) );

		if ( !GetGoalEnt() )
		{
			Warning( "ReadyNPC()--%s couldn't find target %s\n", GetClassname(), STRING(m_target));
		}
		else
		{
			StartTargetHandling( GetGoalEnt() );
		}
	}

	//SetState ( m_IdealNPCState );
	//SetActivity ( m_IdealActivity );

	InitSquad();

	//---------------------------------
	//
	// Spread think times of simultaneously spawned NPCs so that they don't all happen at the same time
	//
	// Think distribution based on spawn order is:
	//
	// Tick offset	Think time	Spawn order
	// 	0			0			1
	// 	1			0.015		13
	// 	2			0.03		5
	// 	3			0.045		9
	// 	4			0.06		18
	// 	5			0.075		3
	// 	6			0.09		15
	// 	7			0.105		11
	// 	8			0.12		7
	// 	9			0.135		17
	// 	10			0.15		2
	// 	11			0.165		14
	// 	12			0.18		6
	// 	13			0.195		19
	// 	14			0.21		10
	// 	15			0.225		4
	// 	16			0.24		16
	// 	17			0.255		12
	// 	18			0.27		8
	// 	19			0.285		20


	// If this NPC is spawning late in the game, just push through the rest of the initialization
	// start thinking right now. Some spread is added to handle triggered spawns that bring
	// a bunch of NPCs into the level
	SetThink ( &CAI_BaseNPC::CallNPCThink );

	if ( gm_flTimeLastSpawn != gpGlobals->curtime )
	{
		gm_nSpawnedThisFrame = 0;
		gm_flTimeLastSpawn = gpGlobals->curtime;
	}

	static const float nextThinkTimes[20] = 
	{
		.0, .150, .075, .225, .030, .180, .120, .270, .045, .210, .105, .255, .015, .165, .090, .240, .135, .060, .195, .285
	};

	SetNextThink( gpGlobals->curtime + nextThinkTimes[gm_nSpawnedThisFrame % 20] );

	gm_nSpawnedThisFrame++;

	//---------------------------------

	m_ScriptArrivalActivity = AIN_DEF_ACTIVITY;
	m_strScriptArrivalSequence = NULL_STRING;

	if ( HasSpawnFlags(SF_NPC_WAIT_FOR_SCRIPT) )
	{
		SetState( NPC_STATE_IDLE );
		m_Activity = m_IdealActivity;
		m_nIdealSequence = GetSequence();
		SetSchedule( SCHED_WAIT_FOR_SCRIPT );
	}
}

//-----------------------------------------------------------------------------

void CAI_BaseNPC::StartTargetHandling( CBaseEntity *pTargetEnt )
{
	// set the npc up to walk a path corner path.
	// !!!BUGBUG - this is a minor bit of a hack.
	// JAYJAY

	// NPC will start turning towards his destination
	bool bIsFlying = (GetMoveType() == MOVETYPE_FLY) || (GetMoveType() == MOVETYPE_FLYGRAVITY);
	AI_NavGoal_t goal( GOALTYPE_PATHCORNER, pTargetEnt->GetAbsOrigin(),
					   bIsFlying ? ACT_FLY : ACT_WALK,
					   AIN_DEF_TOLERANCE, AIN_YAW_TO_DEST);

	SetState( NPC_STATE_IDLE );
	SetSchedule( SCHED_IDLE_WALK );

	if ( !GetNavigator()->SetGoal( goal ) )
	{
		DevWarning( 2, "Can't Create Route!\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Connect my memory to the squad's
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::InitSquad( void )
{
	// -------------------------------------------------------
	//  If I form squads add me to a squad
	// -------------------------------------------------------
	if (!m_pSquad && ( CapabilitiesGet() & bits_CAP_SQUAD ))
	{
		if ( !m_SquadName )
		{
			DevMsg(2, "Found %s that isn't in a squad\n",GetClassname());
		}
		else
		{
			m_pSquad = g_AI_SquadManager.FindCreateSquad(this, m_SquadName);
		}
	}

	return ( m_pSquad != NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Get the memory for this NPC
//-----------------------------------------------------------------------------
CAI_Enemies *CAI_BaseNPC::GetEnemies( void )
{
	return m_pEnemies;
}

//-----------------------------------------------------------------------------
// Purpose: Remove this NPC's memory
//-----------------------------------------------------------------------------
void CAI_BaseNPC::RemoveMemory( void )
{
	delete m_pEnemies;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_BaseNPC::TaskComplete(  bool fIgnoreSetFailedCondition )
{
	EndTaskOverlay();

	// Handy thing to use for debugging
	//if (IsCurSchedule(SCHED_PUT_HERE) &&
	//	GetTask()->iTask == TASK_PUT_HERE)
	//{
	//	int put_breakpoint_here = 5;
	//}

	if ( fIgnoreSetFailedCondition || !HasCondition(COND_TASK_FAILED) )
	{
		SetTaskStatus( TASKSTATUS_COMPLETE );
	}
}

void CAI_BaseNPC::TaskMovementComplete( void )
{
	switch( GetTaskStatus() )
	{
	case TASKSTATUS_NEW:
	case TASKSTATUS_RUN_MOVE_AND_TASK:
		SetTaskStatus( TASKSTATUS_RUN_TASK );
		break;

	case TASKSTATUS_RUN_MOVE:
		TaskComplete();
		break;

	case TASKSTATUS_RUN_TASK:
		// FIXME: find out how to safely restart movement
		//Warning( "Movement completed twice!\n" );
		//Assert( 0 );
		break;

	case TASKSTATUS_COMPLETE:
		break;
	}

	// JAY: Put this back in.
	// UNDONE: Figure out how much of the timestep was consumed by movement
	// this frame and restart the movement/schedule engine if necessary
	if ( m_scriptState != SCRIPT_CUSTOM_MOVE_TO_MARK )
	{
		SetIdealActivity( GetStoppedActivity() );
	}

	// Advance past the last node (in case there is some event at this node)
	if ( GetNavigator()->IsGoalActive() )
	{
		GetNavigator()->AdvancePath();
	}

	// Now clear the path, it's done.
	GetNavigator()->ClearGoal();

	OnMovementComplete();
}


int CAI_BaseNPC::TaskIsRunning( void )
{
	if ( GetTaskStatus() != TASKSTATUS_COMPLETE &&
		 GetTaskStatus() != TASKSTATUS_RUN_MOVE )
		 return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CAI_BaseNPC::TaskFail( AI_TaskFailureCode_t code )
{
	EndTaskOverlay();

	// Handy tool for debugging
	//if (IsCurSchedule(SCHED_PUT_NAME_HERE))
	//{
	//	int put_breakpoint_here = 5;
	//}

	// If in developer mode save the fail text for debug output
	if (g_pDeveloper->GetInt())
	{
		m_failText = TaskFailureToString( code );

		m_interuptSchedule	= NULL;
		m_failedSchedule    = GetCurSchedule();

		if (m_debugOverlays & OVERLAY_TASK_TEXT_BIT)
		{
			DevMsg(this, AIMF_IGNORE_SELECTED, "      TaskFail -> %s\n", m_failText );
		}

		ADD_DEBUG_HISTORY( HISTORY_AI_DECISIONS, UTIL_VarArgs("%s(%d):       TaskFail -> %s\n", GetDebugName(), entindex(), m_failText ) );

		//AddTimedOverlay( fail_text, 5);
	}

	m_ScheduleState.taskFailureCode = code;
	SetCondition(COND_TASK_FAILED);
	Forget( bits_MEMORY_TURNING );
}

//------------------------------------------------------------------------------
// Purpose : Remember that this entity wasn't reachable
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CAI_BaseNPC::RememberUnreachable(CBaseEntity *pEntity, float duration )
{
	if ( pEntity == GetEnemy() )
	{
		ForceChooseNewEnemy();
	}

	const float NPC_UNREACHABLE_TIMEOUT = ( duration > 0.0 ) ? duration : 3;
	// Only add to list if not already on it
	for (int i=m_UnreachableEnts.Size()-1;i>=0;i--)
	{
		// If record already exists just update mark time
		if (pEntity == m_UnreachableEnts[i].hUnreachableEnt)
		{
			m_UnreachableEnts[i].fExpireTime	 = gpGlobals->curtime + NPC_UNREACHABLE_TIMEOUT;
			m_UnreachableEnts[i].vLocationWhenUnreachable = pEntity->GetAbsOrigin();
			return;
		}
	}

	// Add new unreachabe entity to list
	int nNewIndex = m_UnreachableEnts.AddToTail();
	m_UnreachableEnts[nNewIndex].hUnreachableEnt = pEntity;
	m_UnreachableEnts[nNewIndex].fExpireTime	 = gpGlobals->curtime + NPC_UNREACHABLE_TIMEOUT;
	m_UnreachableEnts[nNewIndex].vLocationWhenUnreachable = pEntity->GetAbsOrigin();
}

//------------------------------------------------------------------------------
// Purpose : Returns true is entity was remembered as unreachable.
//			 After a time delay reachability is checked
// Input   :
// Output  :
//------------------------------------------------------------------------------
bool CAI_BaseNPC::IsUnreachable(CBaseEntity *pEntity)
{
	float UNREACHABLE_DIST_TOLERANCE_SQ = (120*120);

	// Note that it's ok to remove elements while I'm iterating
	// as long as I iterate backwards and remove them using FastRemove
	for (int i=m_UnreachableEnts.Size()-1;i>=0;i--)
	{
		// Remove any dead elements
		if (m_UnreachableEnts[i].hUnreachableEnt == NULL)
		{
			m_UnreachableEnts.FastRemove(i);
		}
		else if (pEntity == m_UnreachableEnts[i].hUnreachableEnt)
		{
			// Test for reachablility on any elements that have timed out
			if ( gpGlobals->curtime > m_UnreachableEnts[i].fExpireTime ||
				  pEntity->GetAbsOrigin().DistToSqr(m_UnreachableEnts[i].vLocationWhenUnreachable) > UNREACHABLE_DIST_TOLERANCE_SQ)
			{
				m_UnreachableEnts.FastRemove(i);
				return false;
			}
			return true;
		}
	}
	return false;
}

bool CAI_BaseNPC::IsValidEnemy( CBaseEntity *pEnemy )
{
	CAI_BaseNPC *pEnemyNPC = pEnemy->MyNPCPointer();
	if ( pEnemyNPC && pEnemyNPC->CanBeAnEnemyOf( this ) == false )
		return false;

	// Test our enemy filter
	if ( m_hEnemyFilter.Get()!= NULL && m_hEnemyFilter->PassesFilter( this, pEnemy ) == false )
		return false;

	CTFPlayer *pEnemyTFPlayer = ToTFPlayer( pEnemy );
	if ( pEnemyTFPlayer )
	{
		if ( pEnemyTFPlayer->m_Shared.InCond( TF_COND_STEALTHED_BLINK ) )
			return true;

		// Don't target players disguised as our team member but keep attacking if they disguise while being our target.
		if ( !InSameTeam( pEnemyTFPlayer ) && pEnemyTFPlayer->m_Shared.InCond( TF_COND_DISGUISED ) && pEnemyTFPlayer->m_Shared.GetDisguiseTeam() == GetTeamNumber() && pEnemyTFPlayer != GetEnemy() )
			return false;

		if ( pEnemyTFPlayer->m_Shared.GetPercentInvisible() > 0.5 )
			return false;
	}

	CBaseObject *pEnemyObject = dynamic_cast< CBaseObject * >( pEnemy );
	if ( pEnemyObject )
	{
		if ( InSameTeam( pEnemyObject ) )
			return false;

		if ( pEnemyObject->IsBeingCarried() )
			return false;
	}

	return true;
}


bool CAI_BaseNPC::CanBeAnEnemyOf( CBaseEntity *pEnemy )	
{ 
	if ( GetSleepState() > AISS_WAITING_FOR_THREAT )
		return false;

	return true; 
}


//-----------------------------------------------------------------------------
// Purpose: Picks best enemy from my list of enemies
//			Prefers reachable enemies over enemies that are unreachable,
//			regardless of priority.  For enemies that are both reachable or
//			unreachable picks by priority.  If priority is the same, picks
//			by distance.
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseEntity *CAI_BaseNPC::BestEnemy( void )
{
	AI_PROFILE_SCOPE( CAI_BaseNPC_BestEnemy );
	// TODO - may want to consider distance, attack types, back turned, etc.

	CBaseEntity*	pBestEnemy			= NULL;
	int				iBestDistSq			= MAX_COORD_RANGE * MAX_COORD_RANGE;// so first visible entity will become the closest.
	int				iBestPriority		= -1000;
	bool			bBestUnreachable	= true;			  // Forces initial check
	ThreeState_t	fBestSeen			= TRS_NONE;
	ThreeState_t	fBestVisible		= TRS_NONE;
	int				iDistSq;
	bool			bUnreachable		= false;

	AIEnemiesIter_t iter;

	DbgEnemyMsg( this, "BestEnemy() {\n" );

	for( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst(&iter); pEMemory != NULL; pEMemory = GetEnemies()->GetNext(&iter) )
	{
		CBaseEntity *pEnemy = pEMemory->hEnemy;

		if ( !pEnemy || !pEnemy->IsAlive() )
		{
			if ( pEnemy )
			{
				DbgEnemyMsg( this, "    %s rejected: dead\n", pEnemy->GetDebugName() );
			}
			continue;
		}
		
		if ( (pEnemy->GetFlags() & FL_NOTARGET) )
		{
			DbgEnemyMsg( this, "    %s rejected: no target\n", pEnemy->GetDebugName() );
			continue;
		}

		if ( m_bIgnoreUnseenEnemies )
		{
			const float TIME_CONSIDER_ENEMY_UNSEEN = .4;
			if ( pEMemory->timeLastSeen < gpGlobals->curtime - TIME_CONSIDER_ENEMY_UNSEEN )
			{
				DbgEnemyMsg( this, "    %s rejected: not seen and set to ignore unseen enemies\n", pEnemy->GetDebugName() );
				continue;
			}
		}

		// UNDONE: Move relationship checks into IsValidEnemy?
		Disposition_t relation = IRelationType( pEnemy );
		if ( (relation != D_HT && relation != D_FR)  )
		{
			DbgEnemyMsg( this, "    %s rejected: no hate/fear\n", pEnemy->GetDebugName() );
			continue;
		}

		if ( m_flAcceptableTimeSeenEnemy > 0.0 && pEMemory->timeLastSeen < m_flAcceptableTimeSeenEnemy )
		{
			DbgEnemyMsg( this, "    %s rejected: old\n", pEnemy->GetDebugName() );
			continue;
		}

		if ( pEMemory->timeValidEnemy > gpGlobals->curtime )
		{
			DbgEnemyMsg( this, "    %s rejected: not yet valid\n", pEnemy->GetDebugName() );
			continue;
		}

		// Skip enemies that have eluded me to prevent infinite loops
		if ( pEMemory->bEludedMe )
		{
			DbgEnemyMsg( this, "    %s rejected: eluded\n", pEnemy->GetDebugName() );
			continue;
		}

		// Skip enemies I fear that I've never seen. (usually seen through an enemy finder)
		if ( relation == D_FR && !pEMemory->bUnforgettable && pEMemory->timeFirstSeen == AI_INVALID_TIME )
		{
			DbgEnemyMsg( this, "    %s rejected: feared, but never seen\n", pEnemy->GetDebugName() );
			continue;
		}

		if ( !IsValidEnemy( pEnemy ) )
		{
			DbgEnemyMsg( this, "    %s rejected: not valid\n", pEnemy->GetDebugName() );
			continue;
		}

		// establish the reachability of this enemy
		bUnreachable = IsUnreachable(pEnemy);

		// If best is reachable and current is unreachable, skip the unreachable enemy regardless of priority
		if (!bBestUnreachable && bUnreachable)
		{
			DbgEnemyMsg( this, "    %s rejected: unreachable\n", pEnemy->GetDebugName() );
			continue;
		}

		//  If best is unreachable and current is reachable, always pick the current regardless of priority
		if (bBestUnreachable && !bUnreachable)
		{
			DbgEnemyMsg( this, "    %s accepted (1)\n", pEnemy->GetDebugName() );
			if ( pBestEnemy )
			{
				DbgEnemyMsg( this, "    (%s displaced)\n", pBestEnemy->GetDebugName() );
			}

			iBestPriority	 = IRelationPriority ( pEnemy );
			iBestDistSq		 = (pEnemy->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr();
			pBestEnemy		 = pEnemy;
			bBestUnreachable = bUnreachable;
			fBestSeen		 = TRS_NONE;
			fBestVisible	 = TRS_NONE;
		}
		// If both are unreachable or both are reachable, choose enemy based on priority and distance
		else if ( IRelationPriority( pEnemy ) > iBestPriority )
		{
			DbgEnemyMsg( this, "    %s accepted\n", pEnemy->GetDebugName() );
			if ( pBestEnemy )
			{
				DbgEnemyMsg( this, "    (%s displaced due to priority, %d > %d )\n", pBestEnemy->GetDebugName(), IRelationPriority( pEnemy ), iBestPriority );
			}
			// this entity is disliked MORE than the entity that we
			// currently think is the best visible enemy. No need to do
			// a distance check, just get mad at this one for now.
			iBestPriority	 = IRelationPriority ( pEnemy );
			iBestDistSq		 = ( pEnemy->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr();
			pBestEnemy		 = pEnemy;
			bBestUnreachable = bUnreachable;
			fBestSeen		 = TRS_NONE;
			fBestVisible	 = TRS_NONE;
		}
		else if ( IRelationPriority( pEnemy ) == iBestPriority )
		{
			// this entity is disliked just as much as the entity that
			// we currently think is the best visible enemy, so we only
			// get mad at it if it is closer.
			iDistSq = ( pEnemy->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr();

			bool bAcceptCurrent = false;
			bool bCloser = ( ( iBestDistSq - iDistSq ) > EnemyDistTolerance() );
			ThreeState_t fCurSeen	 = TRS_NONE;
			ThreeState_t fCurVisible = TRS_NONE;

			// The following code is constructed in such a verbose manner to 
			// ensure the expensive calls only occur if absolutely needed

			// If current is farther, and best has previously been confirmed as seen or visible, move on
			if ( !bCloser)
			{
				if ( fBestSeen == TRS_TRUE || fBestVisible == TRS_TRUE )
				{
					DbgEnemyMsg( this, "    %s rejected: current is closer and seen\n", pEnemy->GetDebugName() );
					continue;
				}
			}

			// If current is closer, and best has previously been confirmed as not seen and not visible, take it
			if ( bCloser)
			{
				if ( fBestSeen == TRS_FALSE && fBestVisible == TRS_FALSE )
				{
					bAcceptCurrent = true;
				}
			}

			if ( !bAcceptCurrent )
			{
				// If current is closer, and seen, take it
				if ( bCloser )
				{
					fCurSeen = ( GetSenses()->DidSeeEntity( pEnemy ) ) ? TRS_TRUE : TRS_FALSE;

					bAcceptCurrent = ( fCurSeen == TRS_TRUE );
				}
			}

			if ( !bAcceptCurrent )
			{
				// If current is farther, and best is seen, move on
				if ( !bCloser )
				{
					if ( fBestSeen == TRS_NONE )
					{
						fBestSeen = ( GetSenses()->DidSeeEntity( pBestEnemy ) ) ? TRS_TRUE : TRS_FALSE;
					}

					if ( fBestSeen == TRS_TRUE )
					{
						DbgEnemyMsg( this, "    %s rejected: current is closer and seen\n", pEnemy->GetDebugName() );
						continue;
					}
				}

				// At this point, need to start performing expensive tests
				if ( bCloser && fBestVisible == TRS_NONE )
				{
					// Perform shortest FVisible
					fCurVisible = ( ( EnemyDistance( pEnemy ) < GetSenses()->GetDistLook() ) && FVisible( pEnemy ) ) ? TRS_TRUE : TRS_FALSE;

					bAcceptCurrent = ( fCurVisible == TRS_TRUE );
				}

				// Alas, must do the most expensive comparison
				if ( !bAcceptCurrent )
				{
					if ( fBestSeen == TRS_NONE )
					{
						fBestSeen = ( GetSenses()->DidSeeEntity( pBestEnemy ) ) ? TRS_TRUE : TRS_FALSE;
					}

					if ( fBestVisible == TRS_NONE )
					{
						fBestVisible = ( ( EnemyDistance( pBestEnemy ) < GetSenses()->GetDistLook() ) && FVisible( pBestEnemy ) ) ? TRS_TRUE : TRS_FALSE;
					}

					if ( fCurSeen == TRS_NONE )
					{
						fCurSeen = ( GetSenses()->DidSeeEntity( pEnemy ) ) ? TRS_TRUE : TRS_FALSE;
					}

					if ( fCurVisible == TRS_NONE )
					{
						fCurVisible = ( ( EnemyDistance( pEnemy ) < GetSenses()->GetDistLook() ) && FVisible( pEnemy ) ) ? TRS_TRUE : TRS_FALSE;
					}

					bool bBestSeenOrVisible = ( fBestSeen == TRS_TRUE || fBestVisible == TRS_TRUE );
					bool bCurSeenOrVisible = ( fCurSeen == TRS_TRUE || fCurVisible == TRS_TRUE );

					if ( !bCloser)
					{
						if ( bBestSeenOrVisible )
						{
							DbgEnemyMsg( this, "    %s rejected: current is closer and seen\n", pEnemy->GetDebugName() );
							continue;
						}
						else if ( !bCurSeenOrVisible )
						{
							DbgEnemyMsg( this, "    %s rejected: current is closer and neither is seen\n", pEnemy->GetDebugName() );
							continue;
						}
					}
					else // Closer
					{
						if ( !bCurSeenOrVisible && bBestSeenOrVisible )
						{
							DbgEnemyMsg( this, "    %s rejected: current is father but seen\n", pEnemy->GetDebugName() );
							continue;
						}
					}
				}
			}

			DbgEnemyMsg( this, "    %s accepted\n", pEnemy->GetDebugName() );
			if ( pBestEnemy )
			{
				DbgEnemyMsg( this, "    (%s displaced due to distance/visibility)\n", pBestEnemy->GetDebugName() );
			}
			fBestSeen		 = fCurSeen;
			fBestVisible	 = fCurVisible;
			iBestDistSq		 = iDistSq;
			iBestPriority	 = IRelationPriority ( pEnemy );
			pBestEnemy		 = pEnemy;
			bBestUnreachable = bUnreachable;
		}
		else
		{
			DbgEnemyMsg( this, "    %s rejected: lower priority\n", pEnemy->GetDebugName() );
		}
	}

	DbgEnemyMsg( this, "} == %s\n", pBestEnemy->GetDebugName() );

	return pBestEnemy;
}

//-----------------------------------------------------------------------------
// Purpose: Given a node returns the appropriate reload activity
// Input  :
// Output :
//-----------------------------------------------------------------------------
Activity CAI_BaseNPC::GetReloadActivity( CAI_Hint* pHint )
{
	Activity nReloadActivity = ACT_RELOAD;

	if (pHint && GetEnemy()!=NULL)
	{
		switch (pHint->HintType())
		{
			case HINT_TACTICAL_COVER_LOW:
			case HINT_TACTICAL_COVER_MED:
			{
				if (SelectWeightedSequence( ACT_RELOAD_LOW ) != ACTIVITY_NOT_AVAILABLE)
				{
					Vector vEyePos = GetAbsOrigin() + EyeOffset(ACT_RELOAD_LOW);
					// Check if this location will block the threat's line of sight to me
					trace_t tr;
					AI_TraceLOS( vEyePos, GetEnemy()->EyePosition(), this, &tr );
					if (tr.fraction != 1.0)
					{
						nReloadActivity = ACT_RELOAD_LOW;
					}
				}
				break;
			}
		}
	}
	return nReloadActivity;
}

//-----------------------------------------------------------------------------
// Purpose: Given a node returns the appropriate cover activity
// Input  :
// Output :
//-----------------------------------------------------------------------------
Activity CAI_BaseNPC::GetCoverActivity( CAI_Hint *pHint )
{
	Activity nCoverActivity = ACT_INVALID;

	// ---------------------------------------------------------------
	//  Check if hint node specifies different cover type
	// ---------------------------------------------------------------
	if (pHint)
	{
		switch (pHint->HintType())
		{
			case HINT_TACTICAL_COVER_MED:
			{
				nCoverActivity = ACT_COVER_MED;
				break;
			}
			case HINT_TACTICAL_COVER_LOW:
			{
				nCoverActivity = ACT_COVER_LOW;
				break;
			}
		}
	}

	if ( nCoverActivity == ACT_INVALID )
		nCoverActivity = ACT_COVER;

	return nCoverActivity;
}

//=========================================================
// CalcIdealYaw - gets a yaw value for the caller that would
// face the supplied vector. Value is stuffed into the npc's
// ideal_yaw
//=========================================================
float CAI_BaseNPC::CalcIdealYaw( const Vector &vecTarget )
{
	Vector	vecProjection;

	// strafing npc needs to face 90 degrees away from its goal
	if ( GetNavigator()->GetMovementActivity() == ACT_STRAFE_LEFT )
	{
		vecProjection.x = -vecTarget.y;
		vecProjection.y = vecTarget.x;
		vecProjection.z = 0;

		return UTIL_VecToYaw( vecProjection - GetLocalOrigin() );
	}
	else if ( GetNavigator()->GetMovementActivity() == ACT_STRAFE_RIGHT )
	{
		vecProjection.x = vecTarget.y;
		vecProjection.y = vecTarget.x;
		vecProjection.z = 0;

		return UTIL_VecToYaw( vecProjection - GetLocalOrigin() );
	}
	else
	{
		return UTIL_VecToYaw ( vecTarget - GetLocalOrigin() );
	}
}

//=========================================================
// SetEyePosition
//
// queries the npc's model for $eyeposition and copies
// that vector to the npc's m_vDefaultEyeOffset and m_vecViewOffset
//
//=========================================================
void CAI_BaseNPC::SetDefaultEyeOffset ( void )
{
	if  ( GetModelPtr() )
	{
		GetEyePosition( GetModelPtr(), m_vDefaultEyeOffset );

		if ( m_vDefaultEyeOffset == vec3_origin )
		{
			if ( Classify() != CLASS_NONE )
			{
				DevMsg( "WARNING: %s(%s) has no eye offset in .qc!\n", GetClassname(), STRING(GetModelName()) );
			}
			VectorAdd( WorldAlignMins(), WorldAlignMaxs(), m_vDefaultEyeOffset );
			m_vDefaultEyeOffset *= 0.75;
		}
	}
	else
		m_vDefaultEyeOffset = vec3_origin;

	SetViewOffset( m_vDefaultEyeOffset );

}

//------------------------------------------------------------------------------
// Purpose : Returns eye offset for an NPC for the given activity
// Input   :
// Output  :
//------------------------------------------------------------------------------
Vector CAI_BaseNPC::EyeOffset( Activity nActivity )
{
	if ( CapabilitiesGet() & bits_CAP_DUCK )
	{
		if ( IsCrouchedActivity( nActivity ) )
			return GetCrouchEyeOffset();
	}

	// if the hint doesn't tell anything, assume current state
	if ( IsCrouching() )
		return GetCrouchEyeOffset();

	return m_vDefaultEyeOffset * GetModelScale();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Vector
//-----------------------------------------------------------------------------
Vector CAI_BaseNPC::EyePosition( void ) 
{
	if ( IsCrouching() )
		return GetAbsOrigin() + GetCrouchEyeOffset();

	return BaseClass::EyePosition();
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CAI_BaseNPC::HandleAnimEvent( animevent_t *pEvent )
{
	// UNDONE: Share this code into CBaseAnimating as appropriate?
	switch( pEvent->event )
	{
	case SCRIPT_EVENT_DEAD:
		if ( m_NPCState == NPC_STATE_SCRIPT )
		{
			m_lifeState = LIFE_DYING;
			// Kill me now! (and fade out when CineCleanup() is called)
#if _DEBUG
			DevMsg( 2, "Death event: %s\n", GetClassname() );
#endif
			m_iHealth = 0;
		}
#if _DEBUG
		else
			DevWarning( 2, "INVALID death event:%s\n", GetClassname() );
#endif
		break;
	case SCRIPT_EVENT_NOT_DEAD:
		if ( m_NPCState == NPC_STATE_SCRIPT )
		{
			m_lifeState = LIFE_ALIVE;
			// This is for life/death sequences where the player can determine whether a character is dead or alive after the script
			m_iHealth = m_iMaxHealth;
		}
		break;

	case SCRIPT_EVENT_SOUND:			// Play a named wave file
		{
			EmitSound( pEvent->options );
		}
		break;

	case SCRIPT_EVENT_SOUND_VOICE:
		{
			CPASAttenuationFilter filter(this, pEvent->options);

			EmitSound_t params;
			params.m_pSoundName = pEvent->options;
			params.m_bWarnOnDirectWaveReference = false;
			params.m_nChannel = CHAN_VOICE;

			EmitSound( filter, entindex(), params );
		}
		break;

	case SCRIPT_EVENT_SENTENCE_RND1:		// Play a named sentence group 33% of the time
		if (random->RandomInt(0,2) == 0)
			break;
		// fall through...
	case SCRIPT_EVENT_SENTENCE:			// Play a named sentence group
		PlaySentence( pEvent->options, 1.0f, 1.0f, SNDLVL_TALKING );
		break;

	case SCRIPT_EVENT_FIREEVENT:
	{
		//
		// Fire a script event. The number of the script event to fire is in the options string.
		//
		if ( m_hCine != NULL )
		{
			m_hCine->FireScriptEvent( atoi( pEvent->options ) );
		}
		else
		{
			// FIXME: look so see if it's playing a vcd and fire those instead
			// AssertOnce( 0 );
		}
		break;
	}
	case SCRIPT_EVENT_FIRE_INPUT:
		{
			variant_t emptyVariant;
			this->AcceptInput( pEvent->options, this, this, emptyVariant, 0 );
			break;
		}

	case SCRIPT_EVENT_NOINTERRUPT:		// Can't be interrupted from now on
		if ( m_hCine )
			m_hCine->AllowInterrupt( false );
		break;

	case SCRIPT_EVENT_CANINTERRUPT:		// OK to interrupt now
		if ( m_hCine )
			m_hCine->AllowInterrupt( true );
		break;

#if 0
	case SCRIPT_EVENT_INAIR:			// Don't engine->DropToFloor()
	case SCRIPT_EVENT_ENDANIMATION:		// Set ending animation sequence to
		break;
#endif
	case SCRIPT_EVENT_BODYGROUPON:
	case SCRIPT_EVENT_BODYGROUPOFF:
	case SCRIPT_EVENT_BODYGROUPTEMP:
			DevMsg( "Bodygroup!\n" );
		break;

	case AE_NPC_ATTACK_BROADCAST:
		break;

	case NPC_EVENT_BODYDROP_HEAVY:
		if ( GetFlags() & FL_ONGROUND )
		{
			EmitSound( "AI_BaseNPC.BodyDrop_Heavy" );
		}
		break;

	case NPC_EVENT_BODYDROP_LIGHT:
		if ( GetFlags() & FL_ONGROUND )
		{
			EmitSound( "AI_BaseNPC.BodyDrop_Light" );
		}
		break;

	case NPC_EVENT_SWISHSOUND:
		{
			// NO NPC may use this anim event unless that npc's precache precaches this sound!!!
			EmitSound( "AI_BaseNPC.SwishSound" );
			break;
		}


	case NPC_EVENT_180TURN:
		{
			//DevMsg( "Turned!\n" );
			SetIdealActivity( ACT_IDLE );
			Forget( bits_MEMORY_TURNING );
			SetBoneController( 0, GetLocalAngles().y );
			IncrementInterpolationFrame();
			break;
		}

	case NPC_EVENT_ITEM_PICKUP:
		{
			CBaseEntity *pPickup = NULL;

			//
			// Figure out what we're supposed to pick up.
			//
			if ( pEvent->options && strlen( pEvent->options ) > 0 )
			{
				// Pick up the weapon or item that was specified in the anim event.
				pPickup = gEntList.FindEntityGenericNearest( pEvent->options, GetAbsOrigin(), 256, this );
			}
			else
			{
				// Pick up the weapon or item that was found earlier and cached in our target pointer.
				pPickup = GetTarget();
			}

			// Make sure we found something to pick up.
			if ( !pPickup )
			{
				TaskFail("Item no longer available!\n");
				break;
			}

			// Make sure the item hasn't moved.
			float flDist = ( pPickup->WorldSpaceCenter() - GetAbsOrigin() ).Length2D();
			if ( flDist > ITEM_PICKUP_TOLERANCE )
			{
				TaskFail("Item has moved!\n");
				break;
			}

			CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon *>( pPickup );
			if ( pWeapon )
			{
				// Picking up a weapon.
				CBaseCombatCharacter *pOwner  = pWeapon->GetOwner();
				if ( pOwner )
				{
					TaskFail( "Weapon in use by someone else" );
				}
				else if ( !pWeapon )
				{
					TaskFail( "Weapon doesn't exist" );
				}
				else if (!Weapon_CanUse( pWeapon ))
				{
					TaskFail( "Can't use this weapon type" );
				}
				else
				{
					PickupWeapon( pWeapon );
					TaskComplete();
					break;
				}
			}
			else
			{
				// Picking up an item.
				PickupItem( pPickup );
				TaskComplete();
			}

			break;
		}

	case NPC_EVENT_WEAPON_SET_SEQUENCE_NUMBER:
	{
		CBaseCombatWeapon *pWeapon = GetActiveWeapon();
		if ((pWeapon) && (pEvent->options))
		{
			int nSequence = atoi(pEvent->options);
			if (nSequence != -1)
			{
				pWeapon->ResetSequence(nSequence);
			}
		}
		break;
	}

	case NPC_EVENT_WEAPON_SET_SEQUENCE_NAME:
	{
		CBaseCombatWeapon *pWeapon = GetActiveWeapon();
		if ((pWeapon) && (pEvent->options))
		{
			int nSequence = pWeapon->LookupSequence(pEvent->options);
			if (nSequence != -1)
			{
				pWeapon->ResetSequence(nSequence);
			}
		}
		break;
	}

	case NPC_EVENT_WEAPON_SET_ACTIVITY:
	{
		CBaseCombatWeapon *pWeapon = GetActiveWeapon();
		if ((pWeapon) && (pEvent->options))
		{
			Activity act = (Activity)pWeapon->LookupActivity(pEvent->options);
			if (act != ACT_INVALID)
			{
				// FIXME: where should the duration come from? normally it would come from the current sequence
				Weapon_SetActivity(act, 0);
			}
		}
		break;
	}

	case NPC_EVENT_WEAPON_DROP:
		{
			//
			// Drop our active weapon (or throw it at the specified target entity).
			//
			CBaseEntity *pTarget = NULL;
			if (pEvent->options)
			{
				pTarget = gEntList.FindEntityGeneric(NULL, pEvent->options, this);
			}

			if (pTarget)
			{
				Vector vecTargetPos = pTarget->WorldSpaceCenter();
				Weapon_Drop(GetActiveWeapon(), &vecTargetPos);
			}
			else
			{
				Weapon_Drop(GetActiveWeapon());
			}

			break;
		}

  	case EVENT_WEAPON_RELOAD:
		{
  			if ( GetActiveWeapon() )
  			{
  				GetActiveWeapon()->WeaponSound( RELOAD_NPC );
  				GetActiveWeapon()->m_iClip1 = GetActiveWeapon()->GetMaxClip1(); 
  				ClearCondition(COND_LOW_PRIMARY_AMMO);
  				ClearCondition(COND_NO_PRIMARY_AMMO);
  				ClearCondition(COND_NO_SECONDARY_AMMO);
  			}
  			break;
		}

  	case EVENT_WEAPON_RELOAD_SOUND:
		{
  			if ( GetActiveWeapon() )
  			{
  				GetActiveWeapon()->WeaponSound( RELOAD_NPC );
  			}
  			break;
		}

	case EVENT_WEAPON_RELOAD_FILL_CLIP:
		{
  			if ( GetActiveWeapon() )
  			{
  				GetActiveWeapon()->m_iClip1 = GetActiveWeapon()->GetMaxClip1(); 
  				ClearCondition(COND_LOW_PRIMARY_AMMO);
  				ClearCondition(COND_NO_PRIMARY_AMMO);
  				ClearCondition(COND_NO_SECONDARY_AMMO);
  			}
  			break;
		}

	case NPC_EVENT_LEFTFOOT:
	case NPC_EVENT_RIGHTFOOT:
		// For right now, do nothing. All functionality for this lives in individual npcs.
		break;

	case NPC_EVENT_OPEN_DOOR:
		{
			CBasePropDoor *pDoor = (CBasePropDoor *)(CBaseEntity *)GetNavigator()->GetPath()->GetCurWaypoint()->GetEHandleData();
			if (pDoor != NULL)
			{
				OpenPropDoorNow( pDoor );
			}
	
			break;
		}

	default:
		if ((pEvent->type & AE_TYPE_NEWEVENTSYSTEM) && (pEvent->type & AE_TYPE_SERVER))
		{
			if (pEvent->event == AE_NPC_HOLSTER)
			{
				// Cache off the weapon.
				CBaseCombatWeapon *pWeapon = GetActiveWeapon(); 

				Assert( pWeapon != NULL	); 

 				GetActiveWeapon()->Holster();
				SetActiveWeapon( NULL );

				//Force the NPC to recalculate it's arrival activity since it'll most likely be wrong now that we don't have a weapon out.
				GetNavigator()->SetArrivalSequence( ACT_INVALID );

				if ( m_iDesiredWeaponState == DESIREDWEAPONSTATE_CHANGING_DESTROY )
				{
					// Get rid of it!
					UTIL_Remove( pWeapon );
				}

				if ( m_iDesiredWeaponState != DESIREDWEAPONSTATE_IGNORE )
				{
					m_iDesiredWeaponState = DESIREDWEAPONSTATE_IGNORE;
					m_Activity = ACT_RESET;
				}

				return;
			}
			else if (pEvent->event == AE_NPC_DRAW)
			{
				if (GetActiveWeapon())
				{
					GetActiveWeapon()->Deploy();

					//Force the NPC to recalculate it's arrival activity since it'll most likely be wrong now.
					GetNavigator()->SetArrivalSequence( ACT_INVALID );

					if ( m_iDesiredWeaponState != DESIREDWEAPONSTATE_IGNORE )
					{
						m_iDesiredWeaponState = DESIREDWEAPONSTATE_IGNORE;
						m_Activity = ACT_RESET;
					}
				}
				return;
			}
			else if ( pEvent->event == AE_NPC_BODYDROP_HEAVY )
			{
				if ( GetFlags() & FL_ONGROUND )
				{
					EmitSound( "AI_BaseNPC.BodyDrop_Heavy" );
				}
				return;
			}
			else if (pEvent->event == AE_NPC_BODYDROP_LIGHT)
			{
				if (GetFlags() & FL_ONGROUND)
				{
					EmitSound("AI_BaseNPC.BodyDrop_Light");
				}
				return;
			}
			else if ( pEvent->event == AE_NPC_LEFTFOOT || pEvent->event == AE_NPC_RIGHTFOOT )
			{
				return;
			}
			else if ( pEvent->event == AE_NPC_RAGDOLL )
			{
				// Convert to ragdoll immediately
				BecomeRagdollOnClient( vec3_origin );
				//CreateRagdollEntity();
				return;
			}
			else if ( pEvent->event == AE_NPC_ADDGESTURE )
			{
				Activity act = ( Activity )LookupActivity( pEvent->options );
				if (act != ACT_INVALID)
				{
					act = TranslateActivity( act );
					if (act != ACT_INVALID)
					{
						AddGesture( act );
					}
				}
				return;
			}
			else if ( pEvent->event == AE_NPC_RESTARTGESTURE )
			{
				Activity act = ( Activity )LookupActivity( pEvent->options );
				if (act != ACT_INVALID)
				{
					act = TranslateActivity( act );
					if (act != ACT_INVALID)
					{
						RestartGesture( act );
					}
				}
				return;
			}
 			else if ( pEvent->event == AE_NPC_WEAPON_DROP )
			{
				// Drop our active weapon (or throw it at the specified target entity).
				CBaseEntity *pTarget = NULL;
				if (pEvent->options)
				{
					pTarget = gEntList.FindEntityGeneric(NULL, pEvent->options, this);
				}

				if (pTarget)
				{
					Vector vecTargetPos = pTarget->WorldSpaceCenter();
					Weapon_Drop(GetActiveWeapon(), &vecTargetPos);
				}
				else
				{
					Weapon_Drop(GetActiveWeapon());
				}
				return;
			}
			else if ( pEvent->event == AE_NPC_WEAPON_SET_ACTIVITY )
			{
				CBaseCombatWeapon *pWeapon = GetActiveWeapon();
				if ((pWeapon) && (pEvent->options))
				{
					Activity act = (Activity)pWeapon->LookupActivity(pEvent->options);
					if (act == ACT_INVALID)
					{
						// Try and translate it
						act = Weapon_TranslateActivity( (Activity)CAI_BaseNPC::GetActivityID(pEvent->options), NULL );
					}

					if (act != ACT_INVALID)
					{
						// FIXME: where should the duration come from? normally it would come from the current sequence
						Weapon_SetActivity(act, 0);
					}
				}
				return;
			}
			else if ( pEvent->event == AE_NPC_SET_INTERACTION_CANTDIE )
			{
				SetInteractionCantDie( (atoi(pEvent->options) != 0) );
				return;
			}
			else if ( pEvent->event == AE_NPC_HURT_INTERACTION_PARTNER )
			{
				// If we're currently interacting with an enemy, hurt them/me
				if ( m_hInteractionPartner )
				{
					CAI_BaseNPC *pTarget = NULL;
					CAI_BaseNPC *pAttacker = NULL;
					if ( pEvent->options )
					{
						char szEventOptions[128];
						Q_strncpy( szEventOptions, pEvent->options, sizeof(szEventOptions) );
						char *pszParam = strtok( szEventOptions, " " );
						if ( pszParam )
						{
							if ( !Q_strncmp( pszParam, "ME", 2 ) )
							{
								pTarget = this;
								pAttacker = m_hInteractionPartner;
							}
							else if ( !Q_strncmp( pszParam, "THEM", 4 ) ) 
							{
								pAttacker = this;
								pTarget = m_hInteractionPartner;
							}

							pszParam = strtok(NULL," ");
							if ( pAttacker && pTarget && pszParam )
							{
								int iDamage = atoi( pszParam );
 								if ( iDamage )
								{
									// We've got a target, and damage. Now hurt them.
									CTakeDamageInfo info;
									info.SetDamage( iDamage );
									info.SetAttacker( pAttacker );
									info.SetInflictor( pAttacker );
   									info.SetDamageType( DMG_GENERIC | DMG_PREVENT_PHYSICS_FORCE );
									pTarget->TakeDamage( info );
									return;
								}
							}
						}
					}
					
					// Bad data. Explain how to use this anim event.
					const char *pName = EventList_NameForIndex( pEvent->event );
					DevWarning( 1, "Bad %s format. Should be: { AE_NPC_HURT_INTERACTION_PARTNER <frame number> \"<ME/THEM> <Amount of damage done>\" }\n", pName );
					return;
				}

				DevWarning( "%s received AE_NPC_HURT_INTERACTION_PARTNER anim event, but it's not interacting with anything.\n", GetDebugName() );
				return;
			}
		}

		// FIXME: why doesn't this code pass unhandled events down to its parent?
		// Came from my weapon?
		//Adrian I'll clean this up once the old event system is phased out.
		if ( pEvent->pSource != this || ( pEvent->type & AE_TYPE_NEWEVENTSYSTEM && pEvent->type & AE_TYPE_WEAPON ) || (pEvent->event >= EVENT_WEAPON && pEvent->event <= EVENT_WEAPON_LAST) )
		{
			Weapon_HandleAnimEvent( pEvent );
		}
		else
		{
			BaseClass::HandleAnimEvent( pEvent );
		}
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Override base class to add display of routes
// Input  :
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
void CAI_BaseNPC::DrawDebugGeometryOverlays(void)
{
	// Handy for debug
	//NDebugOverlay::Cross3D(EyePosition(),Vector(-2,-2,-2),Vector(2,2,2),0,255,0,true);

	// ------------------------------
	// Remove me if requested
	// ------------------------------
	if (m_debugOverlays & OVERLAY_NPC_ZAP_BIT)
	{
		VacateStrategySlot();
		Weapon_Drop( GetActiveWeapon() );
		m_iHealth = 0;
		SetThink( &CAI_BaseNPC::SUB_Remove );
	}

	// ------------------------------
	// properly kill an NPC.
	// ------------------------------
	if (m_debugOverlays & OVERLAY_NPC_KILL_BIT) 
	{
		CTakeDamageInfo info;

		info.SetDamage( m_iHealth );
		info.SetAttacker( this );
		info.SetInflictor( ( AI_IsSinglePlayer() ) ? (CBaseEntity *)AI_GetSinglePlayer() : (CBaseEntity *)this );
		info.SetDamageType( DMG_GENERIC );

		m_debugOverlays &= ~OVERLAY_NPC_KILL_BIT;
		TakeDamage( info );
		return;
	}


	// ------------------------------
	// Draw route if requested
	// ------------------------------
	if ((m_debugOverlays & OVERLAY_NPC_ROUTE_BIT))
	{
		GetNavigator()->DrawDebugRouteOverlay();
		if ( IsMoving() )
		{
			float yaw = GetMotor()->GetIdealYaw();
			Vector vecYaw = UTIL_YawToVector(yaw);
			NDebugOverlay::Line(WorldSpaceCenter(),WorldSpaceCenter() + vecYaw * GetHullWidth() * .5,255,255,255,true,0.0);
		}
	}

	if (!(CAI_BaseNPC::m_nDebugBits & bits_debugDisableAI) && (IsCurSchedule(SCHED_FORCED_GO) || IsCurSchedule(SCHED_FORCED_GO_RUN)))
	{
		NDebugOverlay::Box(m_vecLastPosition, Vector(-5,-5,-5),Vector(5,5,5), 255, 0, 255, 0, 0);
		NDebugOverlay::HorzArrow( GetAbsOrigin(), m_vecLastPosition, 16, 255, 0, 255, 64, true, 0 );
	}

	// ------------------------------
	// Draw red box around if selected
	// ------------------------------
	if ((m_debugOverlays & OVERLAY_NPC_SELECTED_BIT) && !ai_no_select_box.GetBool())
	{
		NDebugOverlay::EntityBounds(this, 255, 0, 0, 20, 0);
	}

	// ------------------------------
	// Draw nearest node if selected
	// ------------------------------
	if ((m_debugOverlays & OVERLAY_NPC_NEAREST_BIT))
	{
		int iNodeID = GetPathfinder()->NearestNodeToNPC();
		if (iNodeID != NO_NODE)
		{
			NDebugOverlay::Box(GetNavigator()->GetNetwork()->AccessNodes()[iNodeID]->GetPosition(GetHullType()), Vector(-10,-10,-10),Vector(10,10,10), 255, 255, 255, 0, 0);
		}
	}

	// ------------------------------
	// Draw viewcone if selected
	// ------------------------------
	if ((m_debugOverlays & OVERLAY_NPC_VIEWCONE_BIT))
	{
		float flViewRange	= acos(m_flFieldOfView);
		Vector vEyeDir = EyeDirection2D( );
		Vector vLeftDir, vRightDir;
		float fSin, fCos;
		SinCos( flViewRange, &fSin, &fCos );

		vLeftDir.x			= vEyeDir.x * fCos - vEyeDir.y * fSin;
		vLeftDir.y			= vEyeDir.x * fSin + vEyeDir.y * fCos;
		vLeftDir.z			=  vEyeDir.z;
		fSin				= sin(-flViewRange);
		fCos				= cos(-flViewRange);
		vRightDir.x			= vEyeDir.x * fCos - vEyeDir.y * fSin;
		vRightDir.y			= vEyeDir.x * fSin + vEyeDir.y * fCos;
		vRightDir.z			=  vEyeDir.z;

		// Visualize it
		NDebugOverlay::VertArrow( EyePosition(), EyePosition() + ( vLeftDir * 200 ), 64, 255, 0, 0, 50, false, 0 );
		NDebugOverlay::VertArrow( EyePosition(), EyePosition() + ( vRightDir * 200 ), 64, 255, 0, 0, 50, false, 0 );
		NDebugOverlay::VertArrow( EyePosition(), EyePosition() + ( vEyeDir * 100 ), 8, 0, 255, 0, 50, false, 0 );
		NDebugOverlay::Box(EyePosition(), -Vector(2,2,2), Vector(2,2,2), 0, 255, 0, 128, 0 );
	}

	// ----------------------------------------------
	// Draw the relationships for this NPC to others
	// ----------------------------------------------
	if ( m_debugOverlays & OVERLAY_NPC_RELATION_BIT )
	{
		// Show the relationships to entities around us
		int r = 0;
		int g = 0;
		int b = 0;

		int	nRelationship;
		CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();

		// Rate all NPCs
		for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
		{
			if ( ppAIs[i] == NULL || ppAIs[i] == this )
				continue;
			
			// Get our relation to the target
			nRelationship = IRelationType( ppAIs[i] );

			// Get the color for the arrow
			UTIL_GetDebugColorForRelationship( nRelationship, r, g, b );

			// Draw an arrow
			NDebugOverlay::HorzArrow( GetAbsOrigin(), ppAIs[i]->GetAbsOrigin(), 16, r, g, b, 64, true, 0.0f );
		}

		// Also include all players
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer	*pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer == NULL )
				continue;

			// Get our relation to the target
			nRelationship = IRelationType( pPlayer );

			// Get the color for the arrow
			UTIL_GetDebugColorForRelationship( nRelationship, r, g, b );

			// Draw an arrow
			NDebugOverlay::HorzArrow( GetAbsOrigin(), pPlayer->GetAbsOrigin(), 16, r, g, b, 64, true, 0.0f );
		}
	}

	// ------------------------------
	// Draw enemies if selected
	// ------------------------------
	if ((m_debugOverlays & OVERLAY_NPC_ENEMIES_BIT))
	{
		AIEnemiesIter_t iter;
		for( AI_EnemyInfo_t *eMemory = GetEnemies()->GetFirst(&iter); eMemory != NULL; eMemory = GetEnemies()->GetNext(&iter) )
		{
			if (eMemory->hEnemy)
			{
				CBaseCombatCharacter *npcEnemy = (eMemory->hEnemy)->MyCombatCharacterPointer();
				if (npcEnemy)
				{
					float	r,g,b;
					char	debugText[255];
					debugText[0] = NULL;

					if (npcEnemy == GetEnemy())
					{
						Q_strncat(debugText,"Current Enemy", sizeof( debugText ), COPY_ALL_CHARACTERS );
					}
					else if (npcEnemy == GetTarget())
					{
						Q_strncat(debugText,"Current Target", sizeof( debugText ), COPY_ALL_CHARACTERS );
					}
					else
					{
						Q_strncat(debugText,"Other Memory", sizeof( debugText ), COPY_ALL_CHARACTERS );
					}
					if (IsUnreachable(npcEnemy))
					{
						Q_strncat(debugText," (Unreachable)", sizeof( debugText ), COPY_ALL_CHARACTERS );
					}
					if (eMemory->bEludedMe)
					{
						Q_strncat(debugText," (Eluded)", sizeof( debugText ), COPY_ALL_CHARACTERS );
					}
					// Unreachable enemy drawn in green
					if (IsUnreachable(npcEnemy))
					{
						r = 0;
						g = 255;
						b = 0;
					}
					// Eluded enemy drawn in blue
					else if (eMemory->bEludedMe)
					{
						r = 0;
						g = 0;
						b = 255;
					}
					// Current enemy drawn in red
					else if (npcEnemy == GetEnemy())
					{
						r = 255;
						g = 0;
						b = 0;
					}
					// Current traget drawn in magenta
					else if (npcEnemy == GetTarget())
					{
						r = 255;
						g = 0;
						b = 255;
					}
					// All other enemies drawn in pink
					else
					{
						r = 255;
						g = 100;
						b = 100;
					}


					Vector drawPos = eMemory->vLastKnownLocation;
					NDebugOverlay::Text( drawPos, debugText, false, 0.0 );

					// If has a line on the player draw cross slightly in front so player can see
					if ( npcEnemy->IsPlayer() &&
						(eMemory->vLastKnownLocation - npcEnemy->GetAbsOrigin()).Length()<10 )
					{
						Vector vEnemyFacing = npcEnemy->BodyDirection2D( );
						Vector eyePos = npcEnemy->EyePosition() + vEnemyFacing*10.0;
						Vector upVec	= Vector(0,0,2);
						Vector sideVec;
						CrossProduct( vEnemyFacing, upVec, sideVec);
						NDebugOverlay::Line(eyePos+sideVec+upVec, eyePos-sideVec-upVec, r,g,b, false,0);
						NDebugOverlay::Line(eyePos+sideVec-upVec, eyePos-sideVec+upVec, r,g,b, false,0);

						NDebugOverlay::Text( eyePos, debugText, false, 0.0 );
					}
					else
					{
						NDebugOverlay::Cross3D(drawPos,NAI_Hull::Mins(npcEnemy->GetHullType()),NAI_Hull::Maxs(npcEnemy->GetHullType()),r,g,b,false,0);
					}
				}
			}
		}
	}

	// ----------------------------------------------
	// Draw line to target and enemy entity if exist
	// ----------------------------------------------
	if ((m_debugOverlays & OVERLAY_NPC_FOCUS_BIT))
	{
		if (GetEnemy() != NULL)
		{
			NDebugOverlay::Line(EyePosition(),GetEnemy()->EyePosition(),255,0,0,true,0.0);
		}
		if (GetTarget() != NULL)
		{
			NDebugOverlay::Line(EyePosition(),GetTarget()->EyePosition(),0,0,255,true,0.0);
		}
	}


	GetPathfinder()->DrawDebugGeometryOverlays(m_debugOverlays);

	CBaseEntity::DrawDebugGeometryOverlays();
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Input  :
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CAI_BaseNPC::DrawDebugTextOverlays(void)
{
	int text_offset = 0;

	// ---------------------
	// Print Baseclass text
	// ---------------------
	text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_NPC_SQUAD_BIT)
	{
		// Print health
		char tempstr[512];
		Q_snprintf(tempstr,sizeof(tempstr),"Health: %i",m_iHealth.Get());
		EntityText(text_offset,tempstr,0);
		text_offset++;

		// Print squad name
		Q_strncpy(tempstr,"Squad: ",sizeof(tempstr));
		if (m_pSquad)
		{
			Q_strncat(tempstr,m_pSquad->GetName(),sizeof(tempstr), COPY_ALL_CHARACTERS);

			if( m_pSquad->GetLeader() == this )
			{
				Q_strncat(tempstr," (LEADER)",sizeof(tempstr), COPY_ALL_CHARACTERS);
			}

			Q_strncat(tempstr,"\n",sizeof(tempstr), COPY_ALL_CHARACTERS);
		}
		else
		{
			Q_strncat(tempstr," - \n",sizeof(tempstr), COPY_ALL_CHARACTERS);
		}
		EntityText(text_offset,tempstr,0);
		text_offset++;

		// Print enemy name
		Q_strncpy(tempstr,"Enemy: ",sizeof(tempstr));
		if (GetEnemy())
		{
			if (GetEnemy()->GetEntityName() != NULL_STRING)
			{
				Q_strncat(tempstr,STRING(GetEnemy()->GetEntityName()),sizeof(tempstr), COPY_ALL_CHARACTERS);
				Q_strncat(tempstr,"\n",sizeof(tempstr), COPY_ALL_CHARACTERS);
			}
			else
			{
				Q_strncat(tempstr,STRING(GetEnemy()->m_iClassname),sizeof(tempstr), COPY_ALL_CHARACTERS);
				Q_strncat(tempstr,"\n",sizeof(tempstr), COPY_ALL_CHARACTERS);
			}
		}
		else
		{
			Q_strncat(tempstr," - \n",sizeof(tempstr), COPY_ALL_CHARACTERS);
		}
		EntityText(text_offset,tempstr,0);
		text_offset++;

		// Print slot
		Q_snprintf(tempstr,sizeof(tempstr),"Slot:  %s (%d)\n",
			SquadSlotName(m_iMySquadSlot), m_iMySquadSlot);
		EntityText(text_offset,tempstr,0);
		text_offset++;

	}

	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		char tempstr[512];
		// --------------
		// Print Health
		// --------------
		Q_snprintf(tempstr,sizeof(tempstr),"Health: %i  (DACC:%1.2f)",m_iHealth.Get(), GetDamageAccumulator() );
		EntityText(text_offset,tempstr,0);
		text_offset++;

		// --------------
		// Print State
		// --------------
		static const char *pStateNames[] = { "None", "Idle", "Alert", "Combat", "Scripted", "PlayDead", "Dead" };
		if ( (int)m_NPCState < ARRAYSIZE(pStateNames) )
		{
			Q_snprintf(tempstr,sizeof(tempstr),"Stat: %s, ", pStateNames[m_NPCState] );
			EntityText(text_offset,tempstr,0);
			text_offset++;
		}

		// -----------------
		// Start Scripting?
		// -----------------
		if( IsInAScript() )
		{
			Q_snprintf(tempstr,sizeof(tempstr),"STARTSCRIPTING" );
			EntityText(text_offset,tempstr,0);
			text_offset++;
		}

		// -----------------
		// Hint Group?
		// -----------------
		if( GetHintGroup() != NULL_STRING )
		{
			Q_snprintf(tempstr,sizeof(tempstr),"Hint Group: %s", STRING(GetHintGroup()) );
			EntityText(text_offset,tempstr,0);
			text_offset++;
		}

		// -----------------
		// Print MotionType
		// -----------------
		int navTypeIndex = (int)GetNavType() + 1;
		static const char *pMoveNames[] = { "None", "Ground", "Jump", "Fly", "Climb" };
		Assert( navTypeIndex >= 0 && navTypeIndex < ARRAYSIZE(pMoveNames) );
		if ( navTypeIndex < ARRAYSIZE(pMoveNames) )
		{
			Q_snprintf(tempstr,sizeof(tempstr),"Move: %s, ", pMoveNames[navTypeIndex] );
			EntityText(text_offset,tempstr,0);
			text_offset++;
		}

		// --------------
		// Print Schedule
		// --------------
		if ( GetCurSchedule() )
		{
			CAI_BehaviorBase *pBehavior = GetRunningBehavior();
			if ( pBehavior )
			{
				Q_snprintf(tempstr,sizeof(tempstr),"Behv: %s, ", pBehavior->GetName() );
				EntityText(text_offset,tempstr,0);
				text_offset++;
			}

			const char *pName = NULL;
			pName = GetCurSchedule()->GetName();
			if ( !pName )
			{
				pName = "Unknown";
			}
			Q_snprintf(tempstr,sizeof(tempstr),"Schd: %s, ", pName );
			EntityText(text_offset,tempstr,0);
			text_offset++;

			if (m_debugOverlays & OVERLAY_NPC_TASK_BIT)
			{
				for (int i = 0 ; i < GetCurSchedule()->NumTasks(); i++)
				{
					Q_snprintf(tempstr,sizeof(tempstr),"%s%s%s%s",
						((i==0)					? "Task:":"       "),
						((i==GetScheduleCurTaskIndex())	? "->"   :"   "),
						TaskName(GetCurSchedule()->GetTaskList()[i].iTask),
						((i==GetScheduleCurTaskIndex())	? "<-"   :""));

					EntityText(text_offset,tempstr,0);
					text_offset++;
				}
			}
			else
			{
				const Task_t *pTask = GetTask();
				if ( pTask )
				{
					Q_snprintf(tempstr,sizeof(tempstr),"Task: %s (#%d), ", TaskName(pTask->iTask), GetScheduleCurTaskIndex() );
				}
				else
				{
					Q_strncpy(tempstr,"Task: None",sizeof(tempstr));
				}
				EntityText(text_offset,tempstr,0);
				text_offset++;
			}
		}

		// --------------
		// Print Acitivity
		// --------------
		if( m_Activity != ACT_INVALID && m_IdealActivity != ACT_INVALID && m_Activity != ACT_RESET)
		{
			Activity iActivity		= TranslateActivity( m_Activity );

			Activity iIdealActivity	= Weapon_TranslateActivity( m_IdealActivity );
			iIdealActivity			= NPC_TranslateActivity( iIdealActivity );

			const char *pszActivity = GetActivityName( iActivity );
			const char *pszIdealActivity = GetActivityName( iIdealActivity );
			const char *pszRootActivity = GetActivityName( m_Activity );

			Q_snprintf(tempstr,sizeof(tempstr),"Actv: %s (%s) [%s]\n", pszActivity, pszIdealActivity, pszRootActivity );
		}
		else if (m_Activity == ACT_RESET)
		{
			Q_strncpy(tempstr,"Actv: RESET",sizeof(tempstr) );
		}
		else
		{
			Q_strncpy(tempstr,"Actv: INVALID", sizeof(tempstr) );
		}
		EntityText(text_offset,tempstr,0);
		text_offset++;

		//
		// Print all the current conditions.
		//
		if (m_debugOverlays & OVERLAY_NPC_CONDITIONS_BIT)
		{
			bool bHasConditions = false;
			for (int i = 0; i < MAX_CONDITIONS; i++)
			{
				if (m_Conditions.IsBitSet(i))
				{
					Q_snprintf(tempstr, sizeof(tempstr), "Cond: %s\n", ConditionName(AI_RemapToGlobal(i)));
					EntityText(text_offset, tempstr, 0);
					text_offset++;
					bHasConditions = true;
				}
			}
			if (!bHasConditions)
			{
				Q_snprintf(tempstr,sizeof(tempstr),"(no conditions)");
				EntityText(text_offset,tempstr,0);
				text_offset++;
			}
		}

		if ( GetFlags() & FL_FLY )
		{
			EntityText(text_offset,"HAS FL_FLY",0);
			text_offset++;
		}

		// --------------
		// Print Interrupte
		// --------------
		if (m_interuptSchedule)
		{
			const char *pName = NULL;
			pName = m_interuptSchedule->GetName();
			if ( !pName )
			{
				pName = "Unknown";
			}

			Q_snprintf(tempstr,sizeof(tempstr),"Intr: %s (%s)\n", pName, m_interruptText );
			EntityText(text_offset,tempstr,0);
			text_offset++;
		}

		// --------------
		// Print Failure
		// --------------
		if (m_failedSchedule)
		{
			const char *pName = NULL;
			pName = m_failedSchedule->GetName();
			if ( !pName )
			{
				pName = "Unknown";
			}
			Q_snprintf(tempstr,sizeof(tempstr),"Fail: %s (%s)\n", pName,m_failText );
			EntityText(text_offset,tempstr,0);
			text_offset++;
		}


		// -------------------------------
		// Print any important condtions
		// -------------------------------
		if (HasCondition(COND_ENEMY_TOO_FAR))
		{
			EntityText(text_offset,"Enemy too far to attack",0);
			text_offset++;
		}
		if ( GetAbsVelocity() != vec3_origin || GetLocalAngularVelocity() != vec3_angle )
		{
			char tmp[512];
			Q_snprintf( tmp, sizeof(tmp), "Vel %.1f %.1f %.1f   Ang: %.1f %.1f %.1f\n", 
				GetAbsVelocity().x, GetAbsVelocity().y, GetAbsVelocity().z, 
				GetLocalAngularVelocity().x, GetLocalAngularVelocity().y, GetLocalAngularVelocity().z );
			EntityText(text_offset,tmp,0);
			text_offset++;
		}

		// -------------------------------
		// Print shot accuracy
		// -------------------------------
		if ( m_LastShootAccuracy != -1 && ai_shot_stats.GetBool() )
		{
			CFmtStr msg;
			EntityText(text_offset,msg.sprintf("Cur Accuracy: %.1f", m_LastShootAccuracy),0);
			text_offset++;
			if ( m_TotalShots )
			{
				EntityText(text_offset,msg.sprintf("Act Accuracy: %.1f", ((float)m_TotalHits/(float)m_TotalShots)*100.0),0);
				text_offset++;
			}

			if ( GetActiveWeapon() && GetEnemy() )
			{
				Vector curSpread = GetAttackSpread(GetActiveWeapon(), GetEnemy());
				float curCone = RAD2DEG(asin(curSpread.x)) * 2;
				float bias = GetSpreadBias( GetActiveWeapon(), GetEnemy());
				EntityText(text_offset,msg.sprintf("Cone %.1f, Bias %.2f", curCone, bias),0);
				text_offset++;
			}
		}

		if ( GetGoalEnt() && GetNavigator()->GetGoalType() == GOALTYPE_PATHCORNER )
		{
			Q_strncpy(tempstr,"Pathcorner/goal ent: ",sizeof(tempstr));
			if (GetGoalEnt()->GetEntityName() != NULL_STRING)
			{
				Q_strncat(tempstr,STRING(GetGoalEnt()->GetEntityName()),sizeof(tempstr), COPY_ALL_CHARACTERS);
			}
			else
			{
				Q_strncat(tempstr,STRING(GetGoalEnt()->m_iClassname),sizeof(tempstr), COPY_ALL_CHARACTERS);
			}
			EntityText(text_offset, tempstr, 0);
			text_offset++;
		}

		if ( VPhysicsGetObject() )
		{
			vphysics_objectstress_t stressOut;
			CalculateObjectStress( VPhysicsGetObject(), this, &stressOut );
			Q_snprintf(tempstr, sizeof(tempstr),"Stress: %.2f", stressOut.receivedStress );
			EntityText(text_offset, tempstr, 0);
			text_offset++;
		}
		if ( m_pSquad )
		{
			if( m_pSquad->IsLeader(this) )
			{
				Q_snprintf(tempstr, sizeof(tempstr),"**Squad Leader**" );
				EntityText(text_offset, tempstr, 0);
				text_offset++;
			}

			Q_snprintf(tempstr, sizeof(tempstr), "SquadSlot:%s", GetSquadSlotDebugName( GetMyStrategySlot() ) );
			EntityText(text_offset, tempstr, 0);
			text_offset++;
		}
	}
	return text_offset;
}


//-----------------------------------------------------------------------------
// Purpose: Displays information in the console about the state of this npc.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::ReportAIState( void )
{
	static const char *pStateNames[] = { "None", "Idle", "Alert", "Combat", "Scripted", "PlayDead", "Dead" };

	DevMsg( "%s: ", GetClassname() );
	if ( (int)m_NPCState < ARRAYSIZE(pStateNames) )
		DevMsg( "State: %s, ", pStateNames[m_NPCState] );

	if( m_Activity != ACT_INVALID && m_IdealActivity != ACT_INVALID )
	{
		const char *pszActivity = GetActivityName(m_Activity);
		const char *pszIdealActivity = GetActivityName(m_IdealActivity);

		DevMsg( "Activity: %s  -  Ideal Activity: %s\n", pszActivity, pszIdealActivity );
	}

	if ( GetCurSchedule() )
	{
		const char *pName = NULL;
		pName = GetCurSchedule()->GetName();
		if ( !pName )
			pName = "Unknown";
		DevMsg( "Schedule %s, ", pName );
		const Task_t *pTask = GetTask();
		if ( pTask )
			DevMsg( "Task %d (#%d), ", pTask->iTask, GetScheduleCurTaskIndex() );
	}
	else
		DevMsg( "No Schedule, " );

	if ( GetEnemy() != NULL )
	{
		g_pEffects->Sparks( GetEnemy()->GetAbsOrigin() + Vector( 0, 0, 64 ) );
		DevMsg( "\nEnemy is %s", GetEnemy()->GetClassname() );
	}
	else
		DevMsg( "No enemy " );

	if ( IsMoving() )
	{
		DevMsg( " Moving " );
		if ( m_flMoveWaitFinished > gpGlobals->curtime )
			DevMsg( ": Stopped for %.2f. ", m_flMoveWaitFinished - gpGlobals->curtime );
		else if ( m_IdealActivity == GetStoppedActivity() )
			DevMsg( ": In stopped anim. " );
	}

	DevMsg( "Leader." );

	DevMsg( "\n" );
	DevMsg( "Yaw speed:%3.1f,Health: %3d\n", GetMotor()->GetYawSpeed(), m_iHealth.Get() );

	if ( GetGroundEntity() )
	{
		DevMsg( "Groundent:%s\n\n", GetGroundEntity()->GetClassname() );
	}
	else
	{
		DevMsg( "Groundent: NULL\n\n" );
	}
}

//-----------------------------------------------------------------------------

ConVar ai_report_task_timings_on_limit( "ai_report_task_timings_on_limit", "0", FCVAR_ARCHIVE );
ConVar ai_think_limit_label( "ai_think_limit_label", "0", FCVAR_ARCHIVE );

void CAI_BaseNPC::ReportOverThinkLimit( float time )
{
	DevMsg( "%s thinking for %.02fms!!! (%s); r%.2f (c%.2f, pst%.2f, ms%.2f), p-r%.2f, m%.2f\n",
		 GetDebugName(), time, GetCurSchedule()->GetName(),
		 g_AIRunTimer.GetDuration().GetMillisecondsF(),
		 g_AIConditionsTimer.GetDuration().GetMillisecondsF(),
		 g_AIPrescheduleThinkTimer.GetDuration().GetMillisecondsF(),
		 g_AIMaintainScheduleTimer.GetDuration().GetMillisecondsF(),
		 g_AIPostRunTimer.GetDuration().GetMillisecondsF(),
		 g_AIMoveTimer.GetDuration().GetMillisecondsF() );

	if (ai_think_limit_label.GetBool()) 
	{
		Vector tmp;
		CollisionProp()->NormalizedToWorldSpace( Vector( 0.5f, 0.5f, 1.0f ), &tmp );
		tmp.z += 16;

		float max = -1;
		const char *pszMax = "unknown";

		if ( g_AIConditionsTimer.GetDuration().GetMillisecondsF() > max )
		{
			max = g_AIConditionsTimer.GetDuration().GetMillisecondsF();
			pszMax = "Conditions";
		}
		if ( g_AIPrescheduleThinkTimer.GetDuration().GetMillisecondsF() > max )
		{
			max = g_AIPrescheduleThinkTimer.GetDuration().GetMillisecondsF();
			pszMax = "Pre-think";
		}
		if ( g_AIMaintainScheduleTimer.GetDuration().GetMillisecondsF() > max )
		{
			max = g_AIMaintainScheduleTimer.GetDuration().GetMillisecondsF();
			pszMax = "Schedule";
		}
		if ( g_AIPostRunTimer.GetDuration().GetMillisecondsF() > max )
		{
			max = g_AIPostRunTimer.GetDuration().GetMillisecondsF();
			pszMax = "Post-run";
		}
		if ( g_AIMoveTimer.GetDuration().GetMillisecondsF() > max )
		{
			max = g_AIMoveTimer.GetDuration().GetMillisecondsF();
			pszMax = "Move";
		}
		NDebugOverlay::Text( tmp, (char*)(const char *)CFmtStr( "Slow %.1f, %s %.1f ", time, pszMax, max ), false, 1 );
	}

	if ( ai_report_task_timings_on_limit.GetBool() )
		DumpTaskTimings();
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not this npc can play the scripted sequence or AI
//			sequence that is trying to possess it. If DisregardState is set, the npc
//			will be sucked into the script no matter what state it is in. ONLY
//			Scripted AI ents should allow this.
// Input  : fDisregardNPCState -
//			interruptLevel -
//			eMode - If the function returns true, eMode will be one of the following values:
//				CAN_PLAY_NOW
//				CAN_PLAY_ENQUEUED
// Output :
//-----------------------------------------------------------------------------
CanPlaySequence_t CAI_BaseNPC::CanPlaySequence( bool fDisregardNPCState, int interruptLevel )
{
	CanPlaySequence_t eReturn = CAN_PLAY_NOW;

	if ( m_hCine )
	{
		if ( !m_hCine->CanEnqueueAfter() )
		{
			// npc is already running one scripted sequence and has an important script to play next
			return CANNOT_PLAY;
		}

		eReturn = CAN_PLAY_ENQUEUED;
	}

	if ( !IsAlive() )
	{
		// npc is dead!
		return CANNOT_PLAY;
	}

	// An NPC in a vehicle cannot play a scripted sequence
	if ( IsInAVehicle() )
		return CANNOT_PLAY;

	if ( fDisregardNPCState )
	{
		// ok to go, no matter what the npc state. (scripted AI)

		// No clue as to how to proced if they're climbing or jumping
		// Assert( GetNavType() != NAV_JUMP && GetNavType() != NAV_CLIMB );

		return eReturn;
	}

	if ( m_NPCState == NPC_STATE_NONE || m_NPCState == NPC_STATE_IDLE || m_IdealNPCState == NPC_STATE_IDLE )
	{
		// ok to go, but only in these states
		return eReturn;
	}

	if ( m_NPCState == NPC_STATE_ALERT && interruptLevel >= SS_INTERRUPT_BY_NAME )
	{
		return eReturn;
	}

	// unknown situation
	return CANNOT_PLAY;
}


//-----------------------------------------------------------------------------

void CAI_BaseNPC::SetHintGroup( string_t newGroup, bool bHintGroupNavLimiting )	
{ 
	string_t oldGroup = m_strHintGroup;
	m_strHintGroup = newGroup;
	m_bHintGroupNavLimiting = bHintGroupNavLimiting;

	if ( oldGroup != newGroup )
		OnChangeHintGroup( oldGroup, newGroup );

}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
Vector CAI_BaseNPC::GetShootEnemyDir( const Vector &shootOrigin, bool bNoisy )
{
	CBaseEntity *pEnemy = GetEnemy();

	if ( pEnemy )
	{
		Vector vecEnemyLKP = GetEnemyLKP();

		Vector vecEnemyOffset = pEnemy->BodyTarget( shootOrigin, bNoisy ) - pEnemy->GetAbsOrigin();

#ifdef PORTAL
		// Translate the enemy's position across the portals if it's only seen in the portal view cone
		if ( !FInViewCone( vecEnemyLKP ) || !FVisible( vecEnemyLKP ) )
		{
			CProp_Portal *pPortal = FInViewConeThroughPortal( vecEnemyLKP );
			if ( pPortal )
			{
				UTIL_Portal_VectorTransform( pPortal->m_hLinkedPortal->MatrixThisToLinked(), vecEnemyOffset, vecEnemyOffset );
				UTIL_Portal_PointTransform( pPortal->m_hLinkedPortal->MatrixThisToLinked(), vecEnemyLKP, vecEnemyLKP );
			}
		}
#endif

		Vector retval = vecEnemyOffset + vecEnemyLKP - shootOrigin;
		VectorNormalize( retval );
		return retval;
	}
	else
	{
		Vector forward;
		AngleVectors( GetLocalAngles(), &forward );
		return forward;
	}
}

//-----------------------------------------------------------------------------
// Simulates many times and reports statistical accuracy. 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::CollectShotStats( const Vector &vecShootOrigin, const Vector &vecShootDir )
{
	if (ai_shot_stats.GetBool() != 0 && GetEnemy()->IsPlayer() || GetEnemy()->IsBaseObject())
	{
		int iterations = ai_shot_stats_term.GetInt();
		int iHits = 0;
		Vector testDir = vecShootDir;

		CShotManipulator manipulator( testDir );

		for( int i = 0 ; i < iterations ; i++ )
		{
			// Apply appropriate accuracy.
			manipulator.ApplySpread( GetAttackSpread( GetActiveWeapon(), GetEnemy() ), GetSpreadBias( GetActiveWeapon(), GetEnemy() ) );
			Vector shotDir = manipulator.GetResult();

			Vector vecEnd = vecShootOrigin + shotDir * 8192;

			trace_t tr;
			UTIL_TraceLine( vecShootOrigin, vecEnd, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

			if( tr.m_pEnt && tr.m_pEnt == GetEnemy() )
			{
				iHits++;
			}
			Vector vecProjectedPosition = GetActualShootPosition( vecShootOrigin );
			Vector testDir = vecProjectedPosition - vecShootOrigin;
			VectorNormalize( testDir );
			manipulator.SetShootDir( testDir );
		}

		float flHitPercent = ((float)iHits / (float)iterations) * 100.0;
		m_LastShootAccuracy = flHitPercent;
		//DevMsg("Shots:%d   Hits:%d   Percentage:%.1f\n", iterations, iHits, flHitPercent);
	}
	else
	{
		m_LastShootAccuracy = -1;
	}
}

#if defined (HL2_DLL) || defined (TF_CLASSIC)
//-----------------------------------------------------------------------------
// Purpose: Return the actual position the NPC wants to fire at when it's trying
//			to hit it's current enemy.
//-----------------------------------------------------------------------------
Vector CAI_BaseNPC::GetActualShootPosition( const Vector &shootOrigin )
{
	// Project the target's location into the future.
	Vector vecEnemyLKP = GetEnemyLKP();
	Vector vecEnemyOffset = GetEnemy()->BodyTarget( shootOrigin ) - GetEnemy()->GetAbsOrigin();
	Vector vecTargetPosition = vecEnemyOffset + vecEnemyLKP;

#ifdef PORTAL
	// Check if it's also visible through portals
	CProp_Portal *pPortal = FInViewConeThroughPortal( vecEnemyLKP );
	if ( pPortal )
	{
		// Get the target's position through portals
		Vector vecEnemyOffsetTransformed;
		Vector vecEnemyLKPTransformed;
		UTIL_Portal_VectorTransform( pPortal->m_hLinkedPortal->MatrixThisToLinked(), vecEnemyOffset, vecEnemyOffsetTransformed );
		UTIL_Portal_PointTransform( pPortal->m_hLinkedPortal->MatrixThisToLinked(), vecEnemyLKP, vecEnemyLKPTransformed );
		Vector vecTargetPositionTransformed = vecEnemyOffsetTransformed + vecEnemyLKPTransformed;

		// Get the distance to the target with and without portals
		float fDistanceToEnemyThroughPortalSqr = GetAbsOrigin().DistToSqr( vecTargetPositionTransformed );
		float fDistanceToEnemySqr = GetAbsOrigin().DistToSqr( vecTargetPosition );

		if ( fDistanceToEnemyThroughPortalSqr < fDistanceToEnemySqr || !FInViewCone( vecEnemyLKP ) || !FVisible( vecEnemyLKP ) )
		{
			// We're better off shooting through the portals
			vecTargetPosition = vecTargetPositionTransformed;
		}
	}
#endif

	// lead for some fraction of a second.
	return (vecTargetPosition + ( GetEnemy()->GetSmoothedVelocity() * ai_lead_time.GetFloat() ));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CAI_BaseNPC::GetSpreadBias( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget )
{
	float bias = BaseClass::GetSpreadBias( pWeapon, pTarget );
	AI_EnemyInfo_t *pEnemyInfo = m_pEnemies->Find( pTarget );
	if ( ai_shot_bias.GetFloat() != 1.0 )
		bias = ai_shot_bias.GetFloat();
	if ( pEnemyInfo )
	{
		float timeToFocus = ai_spread_pattern_focus_time.GetFloat();
		if ( timeToFocus > 0.0 )
		{
			float timeSinceValidEnemy = gpGlobals->curtime - pEnemyInfo->timeValidEnemy;
			if ( timeSinceValidEnemy < 0.0f )
			{
				timeSinceValidEnemy = 0.0f;
			}
			float timeSinceReacquire = gpGlobals->curtime - pEnemyInfo->timeLastReacquired;
			if ( timeSinceValidEnemy < timeToFocus )
			{
				float scale = timeSinceValidEnemy / timeToFocus;
				Assert( scale >= 0.0 && scale <= 1.0 );
				bias *= scale;
			}
			else if ( timeSinceReacquire < timeToFocus ) // handled seperately as might be tuning seperately
			{
				float scale = timeSinceReacquire / timeToFocus;
				Assert( scale >= 0.0 && scale <= 1.0 );
				bias *= scale;
			}

		}
	}
	return bias;
}

//-----------------------------------------------------------------------------
Vector CAI_BaseNPC::GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget )
{
	Vector baseResult = BaseClass::GetAttackSpread( pWeapon, pTarget );

	AI_EnemyInfo_t *pEnemyInfo = m_pEnemies->Find( pTarget );
	if ( pEnemyInfo )
	{
		float timeToFocus = ai_spread_cone_focus_time.GetFloat();
		if ( timeToFocus > 0.0 )
		{
			float timeSinceValidEnemy = gpGlobals->curtime - pEnemyInfo->timeValidEnemy;
			if ( timeSinceValidEnemy < 0 )
				timeSinceValidEnemy = 0;
			if ( timeSinceValidEnemy < timeToFocus )
			{
				float coneMultiplier = ai_spread_defocused_cone_multiplier.GetFloat();
				if ( coneMultiplier > 1.0 )
				{
					float scale = 1.0 - timeSinceValidEnemy / timeToFocus;
					Assert( scale >= 0.0 && scale <= 1.0 );
					float multiplier = ( (coneMultiplier - 1.0) * scale ) + 1.0;
					baseResult *= multiplier;
				}
			}
		}
	}
	return baseResult;
}

//-----------------------------------------------------------------------------
// Similar to calling GetShootEnemyDir, but returns the exact trajectory to 
// fire the bullet along, after calculating for target speed, location, 
// concealment, etc.
//
// Ultimately, this code results in the shooter aiming his weapon somewhere ahead of
// a moving target. Since bullet traces are instant, aiming ahead of a target
// will result in more misses than hits. This is designed to provide targets with
// a bonus when moving perpendicular to the shooter's line of sight. 
//
// Do not confuse this with leading a target in an effort to more easily hit it.
// 
// This code PENALIZES a target for moving directly along the shooter's line of sight.
//
// This code REWARDS a target for moving perpendicular to the shooter's line of sight.
//-----------------------------------------------------------------------------
Vector CAI_BaseNPC::GetActualShootTrajectory( const Vector &shootOrigin )
{
	if( !GetEnemy() )
		return GetShootEnemyDir( shootOrigin );

	// If we're above water shooting at a player underwater, bias some of the shots forward of
	// the player so that they see the cool bubble trails in the water ahead of them.
	if ( GetEnemy()->IsPlayer() || GetEnemy()->IsBaseObject() && (GetWaterLevel() != 3) && (GetEnemy()->GetWaterLevel() == 3) )
	{
#if 1
		if (random->RandomInt(0, 4) < 3)
		{
			Vector vecEnemyForward;
			GetEnemy()->GetVectors( &vecEnemyForward, NULL, NULL );
			vecEnemyForward.z = 0;

			// Lead up to a second ahead of them unless they are moving backwards.
			Vector vecEnemyVelocity = GetEnemy()->GetSmoothedVelocity();
			VectorNormalize( vecEnemyVelocity );
			float flVelocityScale = vecEnemyForward.Dot( vecEnemyVelocity );
			if ( flVelocityScale < 0.0f )
			{
				flVelocityScale = 0.0f;
			}

			Vector vecAimPos = GetEnemy()->EyePosition() + ( 48.0f * vecEnemyForward ) + (flVelocityScale * GetEnemy()->GetSmoothedVelocity() );
			//NDebugOverlay::Cross3D(vecAimPos, Vector(-16,-16,-16), Vector(16,16,16), 255, 255, 0, true, 1.0f );
			
			//vecAimPos.z = UTIL_WaterLevel( vecAimPos, vecAimPos.z, vecAimPos.z + 400.0f );
			//NDebugOverlay::Cross3D(vecAimPos, Vector(-32,-32,-32), Vector(32,32,32), 255, 0, 0, true, 1.0f );

			Vector vecShotDir = vecAimPos - shootOrigin;
			VectorNormalize( vecShotDir );
			return vecShotDir;
		}
#else
		if (random->RandomInt(0, 4) < 3)
		{
			// Aim at a point a few feet in front of the player's eyes
			Vector vecEnemyForward;
			GetEnemy()->GetVectors( &vecEnemyForward, NULL, NULL );

			Vector vecAimPos = GetEnemy()->EyePosition() + (120.0f * vecEnemyForward );

			Vector vecShotDir = vecAimPos - shootOrigin;
			VectorNormalize( vecShotDir );

			CShotManipulator manipulator( vecShotDir );
			manipulator.ApplySpread( VECTOR_CONE_10DEGREES, 1 );
			vecShotDir = manipulator.GetResult();

			return vecShotDir;
		}
#endif
	}

	Vector vecProjectedPosition = GetActualShootPosition( shootOrigin );

	Vector shotDir = vecProjectedPosition - shootOrigin;
	VectorNormalize( shotDir );

	CollectShotStats( shootOrigin, shotDir );

	// NOW we have a shoot direction. Where a 100% accurate bullet should go.
	// Modify it by weapon proficiency.
	// construct a manipulator 
	CShotManipulator manipulator( shotDir );

	// Apply appropriate accuracy.
	bool bUsePerfectAccuracy = false;
	if ( GetEnemy() && GetEnemy()->Classify() == CLASS_BULLSEYE )
	{
		CNPC_Bullseye *pBullseye = dynamic_cast<CNPC_Bullseye*>(GetEnemy()); 
		if ( pBullseye && pBullseye->UsePerfectAccuracy() )
		{
			bUsePerfectAccuracy = true;
		}
	}

	if ( !bUsePerfectAccuracy )
	{
		manipulator.ApplySpread( GetAttackSpread( GetActiveWeapon(), GetEnemy() ), GetSpreadBias( GetActiveWeapon(), GetEnemy() ) );
		shotDir = manipulator.GetResult();
	}

	// Look for an opportunity to make misses hit interesting things.
	CBaseCombatCharacter *pEnemy;

	pEnemy = GetEnemy()->MyCombatCharacterPointer();

	if( pEnemy && pEnemy->ShouldShootMissTarget( this ) )
	{
		Vector vecEnd = shootOrigin + shotDir * 8192;
		trace_t tr;

		UTIL_TraceLine(shootOrigin, vecEnd, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

		if( tr.fraction != 1.0 && tr.m_pEnt && tr.m_pEnt->m_takedamage != DAMAGE_NO )
		{
			// Hit something we can harm. Just shoot it.
			return manipulator.GetResult();
		}

		// Find something interesting around the enemy to shoot instead of just missing.
		CBaseEntity *pMissTarget = pEnemy->FindMissTarget();
		
		if( pMissTarget )
		{
			shotDir = pMissTarget->WorldSpaceCenter() - shootOrigin;
			VectorNormalize( shotDir );
		}
	}

	return shotDir;
}
#endif // HL2_DLL

//-----------------------------------------------------------------------------

Vector CAI_BaseNPC::BodyTarget( const Vector &posSrc, bool bNoisy ) 
{ 
	Vector low = WorldSpaceCenter() - ( WorldSpaceCenter() - GetAbsOrigin() ) * .25;
	Vector high = EyePosition();
	Vector delta = high - low;
	Vector result;
	if ( bNoisy )
	{
		// bell curve
		float rand1 = random->RandomFloat( 0.0, 0.5 );
		float rand2 = random->RandomFloat( 0.0, 0.5 );
		result = low + delta * rand1 + delta * rand2;
	}
	else
		result = low + delta * 0.5; 

	return result;
}

//-----------------------------------------------------------------------------

bool CAI_BaseNPC::ShouldMoveAndShoot()
{ 
	return ( ( CapabilitiesGet() & bits_CAP_MOVE_SHOOT ) != 0 ); 
}


//=========================================================
// FacingIdeal - tells us if a npc is facing its ideal
// yaw. Created this function because many spots in the
// code were checking the yawdiff against this magic
// number. Nicer to have it in one place if we're gonna
// be stuck with it.
//=========================================================
bool CAI_BaseNPC::FacingIdeal( void )
{
	if ( fabs( GetMotor()->DeltaIdealYaw() ) <= 0.006 )//!!!BUGBUG - no magic numbers!!!
	{
		return true;
	}

	return false;
}

//---------------------------------

void CAI_BaseNPC::AddFacingTarget( CBaseEntity *pTarget, float flImportance, float flDuration, float flRamp )
{
	GetMotor()->AddFacingTarget( pTarget, flImportance, flDuration, flRamp );
}

void CAI_BaseNPC::AddFacingTarget( const Vector &vecPosition, float flImportance, float flDuration, float flRamp )
{
	GetMotor()->AddFacingTarget( vecPosition, flImportance, flDuration, flRamp );
}

void CAI_BaseNPC::AddFacingTarget( CBaseEntity *pTarget, const Vector &vecPosition, float flImportance, float flDuration, float flRamp )
{
	GetMotor()->AddFacingTarget( pTarget, vecPosition, flImportance, flDuration, flRamp );
}

float CAI_BaseNPC::GetFacingDirection( Vector &vecDir )
{
	return (GetMotor()->GetFacingDirection( vecDir ));
}


//---------------------------------


int CAI_BaseNPC::PlaySentence( const char *pszSentence, float delay, float volume, soundlevel_t soundlevel, CBaseEntity *pListener )
{
	int sentenceIndex = -1;

	if ( pszSentence && IsAlive() )
	{

		if ( pszSentence[0] == '!' )
		{
			sentenceIndex = SENTENCEG_Lookup( pszSentence );
			CPASAttenuationFilter filter( this, soundlevel );
			CBaseEntity::EmitSentenceByIndex( filter, entindex(), CHAN_VOICE, sentenceIndex, volume, soundlevel, 0, PITCH_NORM );
		}
		else
		{
			if (pszSentence[0] == '~')
				pszSentence++;

			sentenceIndex = SENTENCEG_PlayRndSz( edict(), pszSentence, volume, soundlevel, 0, PITCH_NORM );
		}
	}

	return sentenceIndex;
}


int CAI_BaseNPC::PlayScriptedSentence( const char *pszSentence, float delay, float volume, soundlevel_t soundlevel, bool bConcurrent, CBaseEntity *pListener )
{
	return PlaySentence( pszSentence, delay, volume, soundlevel, pListener );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseEntity *CAI_BaseNPC::FindNamedEntity( const char *name, IEntityFindFilter *pFilter )
{
	if ( !stricmp( name, "!player" ))
	{
#ifdef SecobMod__Enable_Fixed_Multiplayer_AI
		return UTIL_GetNearestPlayer( GetAbsOrigin() ); 
#else
		return ( CBaseEntity * )AI_GetSinglePlayer();
#endif //SecobMod__Enable_Fixed_Multiplayer_AI
	}
	else if ( !stricmp( name, "!enemy" ) )
	{
		if (GetEnemy() != NULL)
			return GetEnemy();
	}
	else if ( !stricmp( name, "!self" ) || !stricmp( name, "!target1" ) )
	{
		return this;
	}
	else if ( !stricmp( name, "!nearestfriend" ) || !stricmp( name, "!friend" ) )
	{
		// FIXME: look at CBaseEntity *CNPCSimpleTalker::FindNearestFriend(bool fPlayer)
		// punt for now
#ifdef SecobMod__Enable_Fixed_Multiplayer_AI
		return UTIL_GetNearestPlayer( GetAbsOrigin() ); 
#else
		return ( CBaseEntity * )AI_GetSinglePlayer();
#endif //SecobMod__Enable_Fixed_Multiplayer_AI
	}
	else if (!stricmp( name, "self" ))
	{
		static int selfwarningcount = 0;

		// fix the vcd, the reserved names have changed
		if ( ++selfwarningcount < 5 )
		{
			DevMsg( "ERROR: \"self\" is no longer used, use \"!self\" in vcd instead!\n" );
		}
		return this;
	}
	else if ( !stricmp( name, "Player" ))
	{
		static int playerwarningcount = 0;
		if ( ++playerwarningcount < 5 )
		{
			DevMsg( "ERROR: \"player\" is no longer used, use \"!player\" in vcd instead!\n" );
		}
#ifdef SecobMod__Enable_Fixed_Multiplayer_AI
		return UTIL_GetNearestPlayer( GetAbsOrigin() ); 
#else
		return ( CBaseEntity * )AI_GetSinglePlayer();
#endif //SecobMod__Enable_Fixed_Multiplayer_AI
	}
	else
	{
		// search for up to 32 entities with the same name and choose one randomly
		CBaseEntity *entityList[ FINDNAMEDENTITY_MAX_ENTITIES ];
		CBaseEntity *entity;
		int	iCount;

		entity = NULL;
		for( iCount = 0; iCount < FINDNAMEDENTITY_MAX_ENTITIES; iCount++ )
		{
			entity = gEntList.FindEntityByName( entity, name, NULL, NULL, NULL, pFilter );
			if ( !entity )
			{
				break;
			}
			entityList[ iCount ] = entity;
		}

		if ( iCount > 0 )
		{
			int index = RandomInt( 0, iCount - 1 );
			entity = entityList[ index ];
			return entity;
		}
	}

	return NULL;
}


void CAI_BaseNPC::CorpseFallThink( void )
{
	if ( GetFlags() & FL_ONGROUND )
	{
		SetThink ( NULL );

		SetSequenceBox( );
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

// Call after animation/pose is set up
void CAI_BaseNPC::NPCInitDead( void )
{
	InitBoneControllers();

	RemoveSolidFlags( FSOLID_NOT_SOLID );

	// so he'll fall to ground
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );

	SetCycle( 0 );
	ResetSequenceInfo( );
	m_flPlaybackRate = 0;

	// Copy health
	m_iMaxHealth		= m_iHealth;
	m_lifeState		= LIFE_DEAD;

	UTIL_SetSize(this, vec3_origin, vec3_origin );

	// Setup health counters, etc.
	SetThink( &CAI_BaseNPC::CorpseFallThink );

	SetNextThink( gpGlobals->curtime + 0.5f );
}

//=========================================================
// BBoxIsFlat - check to see if the npc's bounding box
// is lying flat on a surface (traces from all four corners
// are same length.)
//=========================================================
bool CAI_BaseNPC::BBoxFlat ( void )
{
	trace_t	tr;
	Vector		vecPoint;
	float		flXSize, flYSize;
	float		flLength;
	float		flLength2;

	flXSize = WorldAlignSize().x / 2;
	flYSize = WorldAlignSize().y / 2;

	vecPoint.x = GetAbsOrigin().x + flXSize;
	vecPoint.y = GetAbsOrigin().y + flYSize;
	vecPoint.z = GetAbsOrigin().z;

	UTIL_TraceLine ( vecPoint, vecPoint - Vector ( 0, 0, 100 ), GetAITraceMask_BrushOnly(), this, COLLISION_GROUP_NONE, &tr );
	flLength = (vecPoint - tr.endpos).Length();

	vecPoint.x = GetAbsOrigin().x - flXSize;
	vecPoint.y = GetAbsOrigin().y - flYSize;

	UTIL_TraceLine ( vecPoint, vecPoint - Vector ( 0, 0, 100 ), GetAITraceMask_BrushOnly(), this, COLLISION_GROUP_NONE, &tr );
	flLength2 = (vecPoint - tr.endpos).Length();
	if ( flLength2 > flLength )
	{
		return false;
	}
	flLength = flLength2;

	vecPoint.x = GetAbsOrigin().x - flXSize;
	vecPoint.y = GetAbsOrigin().y + flYSize;
	UTIL_TraceLine ( vecPoint, vecPoint - Vector ( 0, 0, 100 ), GetAITraceMask_BrushOnly(), this, COLLISION_GROUP_NONE, &tr );
	flLength2 = (vecPoint - tr.endpos).Length();
	if ( flLength2 > flLength )
	{
		return false;
	}
	flLength = flLength2;

	vecPoint.x = GetAbsOrigin().x + flXSize;
	vecPoint.y = GetAbsOrigin().y - flYSize;
	UTIL_TraceLine ( vecPoint, vecPoint - Vector ( 0, 0, 100 ), GetAITraceMask_BrushOnly(), this, COLLISION_GROUP_NONE, &tr );
	flLength2 = (vecPoint - tr.endpos).Length();
	if ( flLength2 > flLength )
	{
		return false;
	}
	flLength = flLength2;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEnemy - 
//			bSetCondNewEnemy - 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::SetEnemy( CBaseEntity *pEnemy, bool bSetCondNewEnemy )
{
	if (m_hEnemy != pEnemy)
	{
		ClearAttackConditions( );
		VacateStrategySlot();
		m_GiveUpOnDeadEnemyTimer.Stop();

		// If we've just found a new enemy, set the condition
		if ( pEnemy && bSetCondNewEnemy )
		{
			SetCondition( COND_NEW_ENEMY );
		}
	}

	// Assert( (pEnemy == NULL) || (m_NPCState == NPC_STATE_COMBAT) );

	m_hEnemy = pEnemy;
	m_flTimeEnemyAcquired = gpGlobals->curtime;

	m_LastShootAccuracy = -1;
	m_TotalShots		= 0;
	m_TotalHits			= 0;

	if ( !pEnemy )
		ClearCondition( COND_NEW_ENEMY );
}

const Vector &CAI_BaseNPC::GetEnemyLKP() const
{
	return (const_cast<CAI_BaseNPC *>(this))->GetEnemies()->LastKnownPosition( GetEnemy() );
}

float CAI_BaseNPC::GetEnemyLastTimeSeen() const
{
	return (const_cast<CAI_BaseNPC *>(this))->GetEnemies()->LastTimeSeen( GetEnemy() );
}

void CAI_BaseNPC::MarkEnemyAsEluded()
{
	GetEnemies()->MarkAsEluded( GetEnemy() );
}

void CAI_BaseNPC::ClearEnemyMemory()
{
	GetEnemies()->ClearMemory( GetEnemy() );
}

bool CAI_BaseNPC::EnemyHasEludedMe() const
{
	return (const_cast<CAI_BaseNPC *>(this))->GetEnemies()->HasEludedMe( GetEnemy() );
}

void CAI_BaseNPC::SetTarget( CBaseEntity *pTarget )
{
	m_hTargetEnt = pTarget;
}


//=========================================================
// Choose Enemy - tries to find the best suitable enemy for the npc.
//=========================================================

bool CAI_BaseNPC::ShouldChooseNewEnemy()
{
	CBaseEntity *pEnemy = GetEnemy();
	if ( pEnemy )
	{
		if ( GetEnemies()->GetSerialNumber() != m_EnemiesSerialNumber )
		{
			return true;
		}

		m_EnemiesSerialNumber = GetEnemies()->GetSerialNumber();

		if ( EnemyHasEludedMe() || (IRelationType( pEnemy ) != D_HT && IRelationType( pEnemy ) != D_FR) || !IsValidEnemy( pEnemy ) )
		{
			DbgEnemyMsg( this, "ShouldChooseNewEnemy() --> true (1)\n" );
			return true;
		}
		if ( HasCondition(COND_SEE_HATE) || HasCondition(COND_SEE_DISLIKE) || HasCondition(COND_SEE_NEMESIS) || HasCondition(COND_SEE_FEAR) )
		{
			DbgEnemyMsg( this, "ShouldChooseNewEnemy() --> true (2)\n" );
			return true;
		}
		if ( !pEnemy->IsAlive() )
		{
			if ( m_GiveUpOnDeadEnemyTimer.IsRunning() )
			{
				if ( m_GiveUpOnDeadEnemyTimer.Expired() )
				{
					DbgEnemyMsg( this, "ShouldChooseNewEnemy() --> true (3)\n" );
					return true;
				}
			}
			else
				m_GiveUpOnDeadEnemyTimer.Start();
		}

		AI_EnemyInfo_t *pInfo = GetEnemies()->Find( pEnemy );

		if ( m_FailChooseEnemyTimer.Expired() )
		{
			m_FailChooseEnemyTimer.Set( 1.5 );
			if ( HasCondition( COND_TASK_FAILED ) || 
				 ( pInfo && ( pInfo->timeAtFirstHand == AI_INVALID_TIME || gpGlobals->curtime - pInfo->timeLastSeen > 10 ) ) )
			{
				return true;
			}
		}

		if ( pInfo && pInfo->timeValidEnemy < gpGlobals->curtime )
		{
			DbgEnemyMsg( this, "ShouldChooseNewEnemy() --> false\n" );
			return false;
		}
	}

	DbgEnemyMsg( this, "ShouldChooseNewEnemy() --> true (4)\n" );
	m_EnemiesSerialNumber = GetEnemies()->GetSerialNumber();

	return true;
}
bool CAI_BaseNPC::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	if ( ( ( collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT ) && tf_avoidteammates.GetBool() ) ||
		collisionGroup == TFCOLLISION_GROUP_ROCKETS ||
		collisionGroup == HL2COLLISION_GROUP_COMBINE_BALL ||
		collisionGroup == HL2COLLISION_GROUP_COMBINE_BALL_NPC )
	{
		switch ( GetTeamNumber() )
		{
		case TF_TEAM_RED:
			if ( !( contentsMask & CONTENTS_REDTEAM ) )
				return false;
			break;

		case TF_TEAM_BLUE:
			if ( !( contentsMask & CONTENTS_BLUETEAM ) )
				return false;
			break;

		case TF_TEAM_GREEN:
			if ( !(contentsMask & CONTENTS_GREENTEAM ) )
				return false;
			break;

		case TF_TEAM_YELLOW:
			if ( !(contentsMask & CONTENTS_YELLOWTEAM ) )
				return false;
			break;
		}
	}

	return BaseClass::ShouldCollide( collisionGroup, contentsMask );
}

//-------------------------------------

bool CAI_BaseNPC::ChooseEnemy( void )
{
	AI_PROFILE_SCOPE(CAI_Enemies_ChooseEnemy);

	DbgEnemyMsg( this, "ChooseEnemy() {\n" );

	//---------------------------------
	//
	// Gather initial conditions
	//

	CBaseEntity *pInitialEnemy = GetEnemy();
	CBaseEntity *pChosenEnemy  = pInitialEnemy;

	// Use memory bits in case enemy pointer altered outside this function, (e.g., ehandle goes NULL)
	bool fHadEnemy  	 = ( HasMemory( bits_MEMORY_HAD_ENEMY | bits_MEMORY_HAD_PLAYER ) );
	bool fEnemyWasPlayer = HasMemory( bits_MEMORY_HAD_PLAYER );
	bool fEnemyWentNull  = ( fHadEnemy && !pInitialEnemy );
	bool fEnemyEluded	 = ( fEnemyWentNull || ( pInitialEnemy && GetEnemies()->HasEludedMe( pInitialEnemy ) ) );

	//---------------------------------
	//
	// Establish suitability of choosing a new enemy
	//

	bool fHaveCondNewEnemy;
	bool fHaveCondLostEnemy;

	if ( !m_ScheduleState.bScheduleWasInterrupted && GetCurSchedule() && !FScheduleDone() )
	{
		Assert( InterruptFromCondition( COND_NEW_ENEMY ) == COND_NEW_ENEMY && InterruptFromCondition( COND_LOST_ENEMY ) == COND_LOST_ENEMY );
		fHaveCondNewEnemy  = GetCurSchedule()->HasInterrupt( COND_NEW_ENEMY );
		fHaveCondLostEnemy = GetCurSchedule()->HasInterrupt( COND_LOST_ENEMY );

		// See if they've been added as a custom interrupt
		if ( !fHaveCondNewEnemy )
		{
			fHaveCondNewEnemy = IsCustomInterruptConditionSet( COND_NEW_ENEMY );
		}
		if ( !fHaveCondLostEnemy )
		{
			fHaveCondLostEnemy = IsCustomInterruptConditionSet( COND_LOST_ENEMY );
		}
	}
	else
	{
		fHaveCondNewEnemy  = true; // not having a schedule is the same as being interruptable by any condition
		fHaveCondLostEnemy = true;
	}

	if ( !fEnemyWentNull )
	{
		if ( !fHaveCondNewEnemy && !( fHaveCondLostEnemy && fEnemyEluded ) )
		{
			// DO NOT mess with the npc's enemy pointer unless the schedule the npc is currently
			// running will be interrupted by COND_NEW_ENEMY or COND_LOST_ENEMY. This will
			// eliminate the problem of npcs getting a new enemy while they are in a schedule
			// that doesn't care, and then not realizing it by the time they get to a schedule
			// that does. I don't feel this is a good permanent fix.
			m_bSkippedChooseEnemy = true;

			DbgEnemyMsg( this, "Skipped enemy selection due to schedule restriction\n" );
			DbgEnemyMsg( this, "}\n" );
			return ( pChosenEnemy != NULL );
		}
	}
	else if ( !fHaveCondNewEnemy && !fHaveCondLostEnemy && GetCurSchedule() )
	{
		DevMsg( 2, "WARNING: AI enemy went NULL but schedule (%s) is not interested\n", GetCurSchedule()->GetName() );
	}

	m_bSkippedChooseEnemy = false;

	//---------------------------------
	//
	// Select a target
	//

	if ( ShouldChooseNewEnemy()	)
	{
		pChosenEnemy = BestEnemy();
	}

	//---------------------------------
	//
	// React to result of selection
	//

	bool fChangingEnemy = ( pChosenEnemy != pInitialEnemy );

	if ( fChangingEnemy || fEnemyWentNull )
	{
		DbgEnemyMsg( this, "Enemy changed from %s to %s\n", pInitialEnemy->GetDebugName(), pChosenEnemy->GetDebugName() );
		Forget( bits_MEMORY_HAD_ENEMY | bits_MEMORY_HAD_PLAYER );

		// Did our old enemy snuff it?
		if ( pInitialEnemy && !pInitialEnemy->IsAlive() )
		{
			SetCondition( COND_ENEMY_DEAD );
		}

		SetEnemy( pChosenEnemy );

		if ( fHadEnemy )
		{
			// Vacate any strategy slot on old enemy
			VacateStrategySlot();

			// Force output event for establishing LOS
			Forget( bits_MEMORY_HAD_LOS );
			// m_flLastAttackTime	= 0;
		}

		if ( !pChosenEnemy )
		{
			// Don't break on enemies going null if they've been killed
			if ( !HasCondition(COND_ENEMY_DEAD) )
			{
				SetCondition( COND_ENEMY_WENT_NULL );
			}

			if ( fEnemyEluded )
			{
				SetCondition( COND_LOST_ENEMY );
				LostEnemySound();
			}

			if ( fEnemyWasPlayer )
			{
				m_OnLostPlayer.FireOutput( pInitialEnemy, this );
			}
			m_OnLostEnemy.FireOutput( pInitialEnemy, this);
		}
		else
		{
			Remember( ( pChosenEnemy->IsPlayer() || pChosenEnemy->IsBaseObject() ) ? bits_MEMORY_HAD_PLAYER : bits_MEMORY_HAD_ENEMY );
		}
	}

	//---------------------------------

	return ( pChosenEnemy != NULL );
}


//=========================================================
void CAI_BaseNPC::PickupWeapon( CBaseCombatWeapon *pWeapon )
{
	pWeapon->OnPickedUp( this );
	Weapon_Equip( pWeapon );
	m_iszPendingWeapon = NULL_STRING;
}

//=========================================================
// DropItem - dead npc drops named item
//=========================================================
CBaseEntity *CAI_BaseNPC::DropItem ( const char *pszItemName, Vector vecPos, QAngle vecAng )
{
	if ( !pszItemName )
	{
		DevMsg( "DropItem() - No item name!\n" );
		return NULL;
	}

	CBaseEntity *pItem = CBaseEntity::Create( pszItemName, vecPos, vecAng, this );

	if ( pItem )
	{
		if ( g_pGameRules->IsAllowedToSpawn( pItem ) == false )
		{
			UTIL_Remove( pItem );
			return NULL;
		}

		IPhysicsObject *pPhys = pItem->VPhysicsGetObject();

		if ( pPhys )
		{
			// Add an extra push in a random direction
			Vector			vel		= RandomVector( -64.0f, 64.0f );
			AngularImpulse	angImp	= RandomAngularImpulse( -300.0f, 300.0f );

			vel[2] = 0.0f;
			pPhys->AddVelocity( &vel, &angImp );
		}
		else
		{
			// do we want this behavior to be default?! (sjb)
			pItem->ApplyAbsVelocityImpulse( GetAbsVelocity() );
			pItem->ApplyLocalAngularVelocityImpulse( AngularImpulse( 0, random->RandomFloat( 0, 100 ), 0 ) );
		}

		return pItem;
	}
	else
	{
		DevMsg( "DropItem() - Didn't create!\n" );
		return NULL;
	}

}

bool CAI_BaseNPC::ShouldFadeOnDeath( void )
{
	if ( g_RagdollLVManager.IsLowViolence() )
	{
		return true;
	}
	else
	{
		// if flagged to fade out
		return HasSpawnFlags(SF_NPC_FADE_CORPSE);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Indicates whether or not this npc should play an idle sound now.
//
//
// Output : Returns true if yes, false if no.
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::ShouldPlayIdleSound( void )
{
	if ( ( m_NPCState == NPC_STATE_IDLE || m_NPCState == NPC_STATE_ALERT ) &&
		   random->RandomInt(0,99) == 0 && !HasSpawnFlags(SF_NPC_GAG) )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Make a sound that other AI's can hear, to broadcast our presence
// Input  : volume (radius) of the sound.
// Output :
//-----------------------------------------------------------------------------
void CAI_BaseNPC::MakeAIFootstepSound( float volume, float duration )
{
	CSoundEnt::InsertSound( SOUND_COMBAT, EyePosition(), volume, duration, this, SOUNDENT_CHANNEL_NPC_FOOTSTEP );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::FOkToMakeSound( int soundPriority )
{
	// ask the squad to filter sounds if I'm in one
	if ( m_pSquad )
	{
		if ( !m_pSquad->FOkToMakeSound( soundPriority ) )
			return false;
	}
	else
	{
		// otherwise, check my own sound timer
		// Am I making uninterruptable sound?
		if (gpGlobals->curtime <= m_flSoundWaitTime)
		{
			if ( soundPriority <= m_nSoundPriority )
				return false;
		}
	}

	// no talking outside of combat if gagged.
	if ( HasSpawnFlags(SF_NPC_GAG) && ( m_NPCState != NPC_STATE_COMBAT ) )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CAI_BaseNPC::JustMadeSound( int soundPriority, float flSoundLength )
{
	m_flSoundWaitTime = gpGlobals->curtime + flSoundLength + random->RandomFloat(1.5, 2.0);
	m_nSoundPriority = soundPriority;

	if (m_pSquad)
	{
		m_pSquad->JustMadeSound( soundPriority, gpGlobals->curtime + flSoundLength + random->RandomFloat(1.5, 2.0) );
	}
}

Activity CAI_BaseNPC::GetStoppedActivity( void )
{
	if (GetNavigator()->IsGoalActive())
	{
		Activity activity = GetNavigator()->GetArrivalActivity();

		if (activity > ACT_RESET)
		{
			return activity;
		}
	}

	return ACT_IDLE;
}


//=========================================================
//=========================================================
void CAI_BaseNPC::OnScheduleChange ( void )
{
	EndTaskOverlay();

	m_pNavigator->OnScheduleChange();

	m_flMoveWaitFinished = 0;

	VacateStrategySlot();

	// If I still have have a route, clear it
	// FIXME: Routes should only be cleared inside of tasks (kenb)
	GetNavigator()->ClearGoal();

	UnlockBestSound();

	// If I locked a hint node clear it
	if ( HasMemory(bits_MEMORY_LOCKED_HINT)	&& GetHintNode() != NULL)
	{
		float hintDelay = GetHintDelay(GetHintNode()->HintType());
		GetHintNode()->Unlock(hintDelay);
		SetHintNode( NULL );
	}
}



CBaseCombatCharacter* CAI_BaseNPC::GetEnemyCombatCharacterPointer()
{
	if ( GetEnemy() == NULL )
		return NULL;

	return GetEnemy()->MyCombatCharacterPointer();
}


// Global Savedata for npc
//
// This should be an exact copy of the var's in the header.  Fields
// that aren't save/restored are commented out

BEGIN_DATADESC( CAI_BaseNPC )
	//								m_pSchedule  (reacquired on restore)
	DEFINE_EMBEDDED( m_ScheduleState ),
	DEFINE_FIELD( m_IdealSchedule,				FIELD_INTEGER ), // handled specially but left in for "virtual" schedules
	DEFINE_FIELD( m_failSchedule,				FIELD_INTEGER ), // handled specially but left in for "virtual" schedules
	DEFINE_FIELD( m_bUsingStandardThinkTime,	FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flLastRealThinkTime,		FIELD_TIME ),
	//								m_iFrameBlocked (not saved)
	//								m_bInChoreo (not saved)
	//								m_bDoPostRestoreRefindPath (not saved)
	//								gm_flTimeLastSpawn (static)
	//								gm_nSpawnedThisFrame (static)
	//								m_Conditions (custom save)
	//								m_CustomInterruptConditions (custom save)
	//								m_ConditionsPreIgnore (custom save)
	//								m_InverseIgnoreConditions (custom save)
	//								m_poseAim_Pitch (not saved; recomputed on restore)
	//								m_poseAim_Yaw (not saved; recomputed on restore)
	//								m_poseMove_Yaw (not saved; recomputed on restore)
	DEFINE_FIELD( m_flTimePingEffect,			FIELD_TIME ),
	DEFINE_FIELD( m_bForceConditionsGather,		FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bConditionsGathered,		FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bSkippedChooseEnemy,		FIELD_BOOLEAN ),
	DEFINE_FIELD( m_NPCState,					FIELD_INTEGER ),
	DEFINE_FIELD( m_IdealNPCState,				FIELD_INTEGER ),
	DEFINE_FIELD( m_flLastStateChangeTime,		FIELD_TIME ),
	DEFINE_FIELD( m_Efficiency,					FIELD_INTEGER ),
	DEFINE_FIELD( m_MoveEfficiency,				FIELD_INTEGER ),
	DEFINE_FIELD( m_flNextDecisionTime,			FIELD_TIME ),
	DEFINE_KEYFIELD( m_SleepState,				FIELD_INTEGER, "sleepstate" ),
	DEFINE_FIELD( m_SleepFlags,					FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_flWakeRadius, FIELD_FLOAT, "wakeradius" ),
	DEFINE_KEYFIELD( m_bWakeSquad, FIELD_BOOLEAN, "wakesquad" ),
	DEFINE_FIELD( m_nWakeTick, FIELD_TICK ),
	
	DEFINE_CUSTOM_FIELD( m_Activity,				ActivityDataOps() ),
	DEFINE_CUSTOM_FIELD( m_translatedActivity,		ActivityDataOps() ),
	DEFINE_CUSTOM_FIELD( m_IdealActivity,			ActivityDataOps() ),
	DEFINE_CUSTOM_FIELD( m_IdealTranslatedActivity,	ActivityDataOps() ),
	DEFINE_CUSTOM_FIELD( m_IdealWeaponActivity,		ActivityDataOps() ),

	DEFINE_FIELD( m_nIdealSequence,				FIELD_INTEGER ),
	DEFINE_EMBEDDEDBYREF( m_pSenses ),
	DEFINE_EMBEDDEDBYREF( m_pLockedBestSound ),
  	DEFINE_FIELD( m_hEnemy,						FIELD_EHANDLE ),
	DEFINE_FIELD( m_flTimeEnemyAcquired,		FIELD_TIME ),
	DEFINE_FIELD( m_hTargetEnt,					FIELD_EHANDLE ),
	DEFINE_EMBEDDED( m_GiveUpOnDeadEnemyTimer ),
	DEFINE_EMBEDDED( m_FailChooseEnemyTimer ),
	DEFINE_FIELD( m_EnemiesSerialNumber,		FIELD_INTEGER ),
	DEFINE_FIELD( m_flAcceptableTimeSeenEnemy,	FIELD_TIME ),
	DEFINE_EMBEDDED( m_UpdateEnemyPosTimer ),
	//		m_flTimeAnyUpdateEnemyPos (static)
	DEFINE_FIELD( m_vecCommandGoal,				FIELD_VECTOR ),
	DEFINE_EMBEDDED( m_CommandMoveMonitor ),
	DEFINE_FIELD( m_flSoundWaitTime,			FIELD_TIME ),
	DEFINE_FIELD( m_nSoundPriority,				FIELD_INTEGER ),
	DEFINE_FIELD( m_flIgnoreDangerSoundsUntil,	FIELD_TIME ),
	DEFINE_FIELD( m_afCapability,				FIELD_INTEGER ),
	DEFINE_FIELD( m_flMoveWaitFinished,			FIELD_TIME ),
	DEFINE_FIELD( m_hOpeningDoor,				FIELD_EHANDLE ),
	DEFINE_EMBEDDEDBYREF( m_pNavigator ),
	DEFINE_EMBEDDEDBYREF( m_pLocalNavigator ),
	DEFINE_EMBEDDEDBYREF( m_pPathfinder ),
	DEFINE_EMBEDDEDBYREF( m_pMoveProbe ),
	DEFINE_EMBEDDEDBYREF( m_pMotor ),
	DEFINE_UTLVECTOR(m_UnreachableEnts,		FIELD_EMBEDDED),
	DEFINE_FIELD( m_hInteractionPartner,	FIELD_EHANDLE ),
	DEFINE_FIELD( m_hLastInteractionTestTarget,	FIELD_EHANDLE ),
	DEFINE_FIELD( m_hForcedInteractionPartner,	FIELD_EHANDLE ),
	DEFINE_FIELD( m_flForcedInteractionTimeout, FIELD_TIME ),
	DEFINE_FIELD( m_vecForcedWorldPosition,	FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_bCannotDieDuringInteraction, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iInteractionState,		FIELD_INTEGER ),
	DEFINE_FIELD( m_iInteractionPlaying,	FIELD_INTEGER ),
	DEFINE_UTLVECTOR(m_ScriptedInteractions,FIELD_EMBEDDED),
	DEFINE_FIELD( m_flInteractionYaw,		FIELD_FLOAT ),
	DEFINE_EMBEDDED( m_CheckOnGroundTimer ),
	DEFINE_FIELD( m_vDefaultEyeOffset,		FIELD_VECTOR ),
  	DEFINE_FIELD( m_flNextEyeLookTime,		FIELD_TIME ),
    DEFINE_FIELD( m_flEyeIntegRate,			FIELD_FLOAT ),
    DEFINE_FIELD( m_vEyeLookTarget,			FIELD_POSITION_VECTOR ),
    DEFINE_FIELD( m_vCurEyeTarget,			FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_hEyeLookTarget,			FIELD_EHANDLE ),
    DEFINE_FIELD( m_flHeadYaw,				FIELD_FLOAT ),
    DEFINE_FIELD( m_flHeadPitch,				FIELD_FLOAT ),
    DEFINE_FIELD( m_flOriginalYaw,			FIELD_FLOAT ),
	DEFINE_FIELD( m_bInAScript,				FIELD_BOOLEAN ),
    DEFINE_FIELD( m_scriptState,				FIELD_INTEGER ),
	DEFINE_FIELD( m_hCine,					FIELD_EHANDLE ),
	DEFINE_CUSTOM_FIELD( m_ScriptArrivalActivity,	ActivityDataOps() ),
	DEFINE_FIELD( m_strScriptArrivalSequence,	FIELD_STRING ),
	DEFINE_FIELD( m_flSceneTime,			FIELD_TIME ),
	DEFINE_FIELD( m_iszSceneCustomMoveSeq,	FIELD_STRING ),
	// 							m_pEnemies					Saved specially in ai_saverestore.cpp
	DEFINE_FIELD( m_afMemory,					FIELD_INTEGER ),
  	DEFINE_FIELD( m_hEnemyOccluder,			FIELD_EHANDLE ),
  	DEFINE_FIELD( m_flSumDamage,				FIELD_FLOAT ),
  	DEFINE_FIELD( m_flLastDamageTime,			FIELD_TIME ),
  	DEFINE_FIELD( m_flLastPlayerDamageTime,			FIELD_TIME ),
	DEFINE_FIELD( m_flLastSawPlayerTime,			FIELD_TIME ),
  	DEFINE_FIELD( m_flLastAttackTime,			FIELD_TIME ),
	DEFINE_FIELD( m_flLastEnemyTime,			FIELD_TIME ),
  	DEFINE_FIELD( m_flNextWeaponSearchTime,	FIELD_TIME ),
	DEFINE_FIELD( m_iszPendingWeapon,		FIELD_STRING ),
	DEFINE_KEYFIELD( m_bIgnoreUnseenEnemies, FIELD_BOOLEAN , "ignoreunseenenemies"),
	DEFINE_EMBEDDED( m_ShotRegulator ),
	DEFINE_FIELD( m_iDesiredWeaponState,	FIELD_INTEGER ),
	// 							m_pSquad					Saved specially in ai_saverestore.cpp
	DEFINE_KEYFIELD(m_SquadName,				FIELD_STRING, "squadname" ),
    DEFINE_FIELD( m_iMySquadSlot,				FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_strHintGroup,			FIELD_STRING, "hintgroup" ),
	DEFINE_KEYFIELD( m_bHintGroupNavLimiting,	FIELD_BOOLEAN, "hintlimiting" ),
 	DEFINE_EMBEDDEDBYREF( m_pTacticalServices ),
 	DEFINE_FIELD( m_flWaitFinished,			FIELD_TIME ),
	DEFINE_FIELD( m_flNextFlinchTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flNextDodgeTime,		FIELD_TIME ),
	DEFINE_EMBEDDED( m_MoveAndShootOverlay ),
	DEFINE_FIELD( m_vecLastPosition,			FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vSavePosition,			FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vInterruptSavePosition,		FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_pHintNode,				FIELD_EHANDLE),
	DEFINE_FIELD( m_cAmmoLoaded,				FIELD_INTEGER ),
    DEFINE_FIELD( m_flDistTooFar,				FIELD_FLOAT ),
	DEFINE_FIELD( m_hGoalEnt,					FIELD_EHANDLE ),
	DEFINE_FIELD( m_flTimeLastMovement,			FIELD_TIME ),
	DEFINE_KEYFIELD(m_spawnEquipment,			FIELD_STRING, "additionalequipment" ),
  	DEFINE_FIELD( m_fNoDamageDecal,			FIELD_BOOLEAN ),
  	DEFINE_FIELD( m_hStoredPathTarget,			FIELD_EHANDLE ),
	DEFINE_FIELD( m_vecStoredPathGoal,		FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_nStoredPathType,			FIELD_INTEGER ),
	DEFINE_FIELD( m_fStoredPathFlags,			FIELD_INTEGER ),
	DEFINE_FIELD( m_bDidDeathCleanup,			FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bCrouchDesired,				FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bForceCrouch,				FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bIsCrouching,				FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bPerformAvoidance,			FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bIsMoving,					FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bFadeCorpse,				FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iDeathPose,					FIELD_INTEGER ),
	DEFINE_FIELD( m_iDeathFrame,				FIELD_INTEGER ),
	DEFINE_FIELD( m_bCheckContacts,				FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bSpeedModActive,			FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iSpeedModRadius,			FIELD_INTEGER ),
	DEFINE_FIELD( m_iSpeedModSpeed,				FIELD_INTEGER ),
	DEFINE_FIELD( m_hEnemyFilter,				FIELD_EHANDLE ),
	DEFINE_KEYFIELD( m_iszEnemyFilterName,		FIELD_STRING, "enemyfilter" ),
	DEFINE_FIELD( m_bImportanRagdoll,			FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bPlayerAvoidState,			FIELD_BOOLEAN ),
	DEFINE_ARRAY( m_szClassname,				FIELD_STRING, 128 ),
	#ifdef TF_CLASSIC
	DEFINE_FIELD( m_flFallVelocity, 			FIELD_FLOAT ),
	DEFINE_KEYFIELD( m_pszLocalizeName,			FIELD_STRING, "targetidname" ),
	DEFINE_FIELD( m_nPlayerCond,				FIELD_INTEGER ),
	DEFINE_FIELD( m_flCondExpireTimeLeft, 			FIELD_FLOAT ),
	#endif

	// Satisfy classcheck
	// DEFINE_FIELD( m_ScheduleHistory, CUtlVector < AIScheduleChoice_t > ),

	//							m_fIsUsingSmallHull			TODO -- This needs more consideration than simple save/load
	// 							m_failText					DEBUG
	// 							m_interruptText				DEBUG
	// 							m_failedSchedule			DEBUG
	// 							m_interuptSchedule			DEBUG
	// 							m_nDebugCurIndex			DEBUG

	// 							m_LastShootAccuracy			DEBUG
	// 							m_RecentShotAccuracy		DEBUG
	// 							m_TotalShots				DEBUG
	// 							m_TotalHits					DEBUG
	//							m_bSelected					DEBUG
	// 							m_TimeLastShotMark			DEBUG
	//							m_bDeferredNavigation


	// Outputs
	DEFINE_OUTPUT( m_OnDamaged,				"OnDamaged" ),
	DEFINE_OUTPUT( m_OnDeath,					"OnDeath" ),
	DEFINE_OUTPUT( m_OnHalfHealth,				"OnHalfHealth" ),
	DEFINE_OUTPUT( m_OnFoundEnemy,				"OnFoundEnemy" ),
	DEFINE_OUTPUT( m_OnLostEnemyLOS,			"OnLostEnemyLOS" ),
	DEFINE_OUTPUT( m_OnLostEnemy,				"OnLostEnemy" ),
	DEFINE_OUTPUT( m_OnFoundPlayer,			"OnFoundPlayer" ),
	DEFINE_OUTPUT( m_OnLostPlayerLOS,			"OnLostPlayerLOS" ),
	DEFINE_OUTPUT( m_OnLostPlayer,				"OnLostPlayer" ),
	DEFINE_OUTPUT( m_OnHearWorld,				"OnHearWorld" ),
	DEFINE_OUTPUT( m_OnHearPlayer,				"OnHearPlayer" ),
	DEFINE_OUTPUT( m_OnHearCombat,				"OnHearCombat" ),
	DEFINE_OUTPUT( m_OnDamagedByPlayer,		"OnDamagedByPlayer" ),
	DEFINE_OUTPUT( m_OnDamagedByPlayerSquad,	"OnDamagedByPlayerSquad" ),
	DEFINE_OUTPUT( m_OnDenyCommanderUse,		"OnDenyCommanderUse" ),
	DEFINE_OUTPUT( m_OnRappelTouchdown,			"OnRappelTouchdown" ),
	DEFINE_OUTPUT( m_OnWake,					"OnWake" ),
	DEFINE_OUTPUT( m_OnSleep,					"OnSleep" ),
	DEFINE_OUTPUT( m_OnForcedInteractionStarted,	"OnForcedInteractionStarted" ),
	DEFINE_OUTPUT( m_OnForcedInteractionAborted,	"OnForcedInteractionAborted" ),
	DEFINE_OUTPUT( m_OnForcedInteractionFinished,	"OnForcedInteractionFinished" ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_STRING, "SetRelationship", InputSetRelationship ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetEnemyFilter", InputSetEnemyFilter ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetHealth", InputSetHealth ),
	DEFINE_INPUTFUNC( FIELD_VOID, "BeginRappel", InputBeginRappel ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetSquad", InputSetSquad ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Wake", InputWake ),
	DEFINE_INPUTFUNC( FIELD_STRING, "ForgetEntity", InputForgetEntity ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "IgnoreDangerSounds", InputIgnoreDangerSounds ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Break", InputBreak ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"StartScripting",	InputStartScripting ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"StopScripting",	InputStopScripting ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"GagEnable",	InputGagEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"GagDisable",	InputGagDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"InsideTransition",	InputInsideTransition ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"OutsideTransition",	InputOutsideTransition ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"ActivateSpeedModifier", InputActivateSpeedModifier ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"DisableSpeedModifier", InputDisableSpeedModifier ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetSpeedModRadius", InputSetSpeedModifierRadius ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetSpeedModSpeed", InputSetSpeedModifierSpeed ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"HolsterWeapon", InputHolsterWeapon ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"HolsterAndDestroyWeapon", InputHolsterAndDestroyWeapon ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"UnholsterWeapon", InputUnholsterWeapon ),
	DEFINE_INPUTFUNC( FIELD_STRING,	"ForceInteractionWithNPC", InputForceInteractionWithNPC ),
	DEFINE_INPUTFUNC( FIELD_STRING, "UpdateEnemyMemory", InputUpdateEnemyMemory ),

	#ifdef TF_CLASSIC
	DEFINE_INPUTFUNC( FIELD_FLOAT,		"SetPlaybackRate",	InputSetPlaybackRate ),
	DEFINE_INPUTFUNC( FIELD_FLOAT,		"SetGroundSpeed",	InputSetGroundSpeed ),
	DEFINE_INPUTFUNC( FIELD_INTEGER,	"AddCond",	InputAddCond ),
	DEFINE_INPUTFUNC( FIELD_INTEGER,	"RemoveCond",	InputRemoveCond ),
	#endif

	// Function pointers
	DEFINE_USEFUNC( NPCUse ),
	DEFINE_THINKFUNC( CallNPCThink ),
	DEFINE_THINKFUNC( CorpseFallThink ),
	DEFINE_THINKFUNC( NPCInitThink ),

END_DATADESC()

BEGIN_SIMPLE_DATADESC( AIScheduleState_t )
	DEFINE_FIELD( iCurTask,				FIELD_INTEGER ),
	DEFINE_FIELD( fTaskStatus,			FIELD_INTEGER ),
	DEFINE_FIELD( timeStarted,			FIELD_TIME ),
	DEFINE_FIELD( timeCurTaskStarted,	FIELD_TIME ),
	DEFINE_FIELD( taskFailureCode,		FIELD_INTEGER ),
	DEFINE_FIELD( iTaskInterrupt,		FIELD_INTEGER ),
	DEFINE_FIELD( bTaskRanAutomovement,	FIELD_BOOLEAN ),
	DEFINE_FIELD( bTaskUpdatedYaw,		FIELD_BOOLEAN ),
	DEFINE_FIELD( bScheduleWasInterrupted, FIELD_BOOLEAN ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CAI_BaseNPC, DT_AI_BaseNPC )
	SendPropDataTable( SENDINFO_DT( m_AttributeManager ), &REFERENCE_SEND_TABLE( DT_AttributeManager ) ),

	SendPropInt( SENDINFO( m_lifeState ), 3, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bPerformAvoidance ) ),
	SendPropBool( SENDINFO( m_bIsMoving ) ),
	SendPropBool( SENDINFO( m_bFadeCorpse ) ),
	SendPropInt( SENDINFO( m_iDeathPose ), ANIMATION_SEQUENCE_BITS ),
	SendPropInt( SENDINFO( m_iDeathFrame ), 5 ),
	SendPropInt(SENDINFO( m_iHealth ), 13 ),
	SendPropInt(SENDINFO( m_iMaxHealth ), 13 ),
	SendPropBool( SENDINFO( m_bSpeedModActive ) ),
	SendPropInt( SENDINFO( m_iSpeedModRadius ) ),
	SendPropInt( SENDINFO( m_iSpeedModSpeed ) ),
	SendPropBool( SENDINFO( m_bImportanRagdoll ) ),
	SendPropFloat( SENDINFO( m_flTimePingEffect ) ),
	SendPropString( SENDINFO( m_szClassname ) ),

#ifdef TF_CLASSIC
	SendPropInt( SENDINFO( m_nPlayerCond ), -1, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
	SendPropInt( SENDINFO( m_nPlayerCondEx ), -1, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
	SendPropInt( SENDINFO( m_nPlayerCondEx2 ), -1, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
	SendPropInt( SENDINFO( m_nPlayerCondEx3 ), -1, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
	SendPropInt( SENDINFO( m_nPlayerCondEx4 ), -1, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
	SendPropInt( SENDINFO( m_nNumHealers ), 5, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
	SendPropTime( SENDINFO( m_flStunExpireTime ) ),
	SendPropInt( SENDINFO( m_nStunFlags ), -1, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flStunMovementSpeed ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flStunResistance ), 0, SPROP_NOSCALE ),
	SendPropBool( SENDINFO( m_bBurningDeath ) ),
	SendPropBool( SENDINFO( m_bIceDeath ) ),
	SendPropInt( SENDINFO( m_iDamageCustomDeath ) ),
	SendPropBool( SENDINFO( m_bKingRuneBuffActive ) ),
	SendPropString( SENDINFO( m_pszLocalizeName ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_flCondExpireTimeLeft ), SendPropFloat( SENDINFO_ARRAY( m_flCondExpireTimeLeft ) ) ),
	SendPropEHandle( SENDINFO( m_hItem ) ),
	SendPropFloat( SENDINFO( m_flHeadScale ) ),
	SendPropFloat( SENDINFO( m_flTorsoScale ) ),
	SendPropFloat( SENDINFO( m_flHandScale ) ),
#endif
END_SEND_TABLE()

//-------------------------------------

BEGIN_SIMPLE_DATADESC( UnreachableEnt_t )

	DEFINE_FIELD( hUnreachableEnt,			FIELD_EHANDLE	),
	DEFINE_FIELD( fExpireTime,				FIELD_TIME		),
	DEFINE_FIELD( vLocationWhenUnreachable,	FIELD_POSITION_VECTOR	),

END_DATADESC()

//-------------------------------------

BEGIN_SIMPLE_DATADESC( ScriptedNPCInteraction_Phases_t )
DEFINE_FIELD( iszSequence,					FIELD_STRING	),
DEFINE_FIELD( iActivity,					FIELD_INTEGER	),
END_DATADESC()

//-------------------------------------

BEGIN_SIMPLE_DATADESC( ScriptedNPCInteraction_t )
	DEFINE_FIELD( iszInteractionName,			FIELD_STRING	),
	DEFINE_FIELD( iFlags,						FIELD_INTEGER	),
	DEFINE_FIELD( iTriggerMethod,				FIELD_INTEGER	),
	DEFINE_FIELD( iLoopBreakTriggerMethod,		FIELD_INTEGER	),
	DEFINE_FIELD( vecRelativeOrigin,			FIELD_VECTOR	),
	DEFINE_FIELD( angRelativeAngles,			FIELD_VECTOR	),
	DEFINE_FIELD( vecRelativeVelocity,			FIELD_VECTOR	),
	DEFINE_FIELD( flDelay,						FIELD_FLOAT		),
	DEFINE_FIELD( flDistSqr,					FIELD_FLOAT		),
	DEFINE_FIELD( iszMyWeapon,					FIELD_STRING	),
	DEFINE_FIELD( iszTheirWeapon,				FIELD_STRING	),
	DEFINE_EMBEDDED_ARRAY( sPhases, SNPCINT_NUM_PHASES ),
	DEFINE_FIELD( matDesiredLocalToWorld,		FIELD_VMATRIX	),
	DEFINE_FIELD( bValidOnCurrentEnemy,			FIELD_BOOLEAN	),
	DEFINE_FIELD( flNextAttemptTime,			FIELD_TIME		),
END_DATADESC()

//-------------------------------------

void CAI_BaseNPC::PostConstructor( const char *szClassname )
{
	BaseClass::PostConstructor( szClassname );

	// Ugly hack to get NPC classname on client side. Unfortunately, I don't see any other way,
	// other than adding a stub client class for every HL2 NPC. (Nicknine)
	Q_strncpy( m_szClassname.GetForModify(), szClassname, 128 );
	//Q_strncpy( m_pszLocalizeName.GetForModify(), szClassname, 128 );
	CreateComponents();

#ifdef TF_CLASSIC
	// Not the best place to put this but it's the only way to make sure NPC gets assigned to a team.
	for ( int i = 0; g_aNPCData[i].pszName != NULL; i++ )
	{
		if ( ClassMatches( g_aNPCData[i].pszName ) )
		{
			ChangeTeam( g_aNPCData[i].iTeam );
			m_nTFFlags = g_aNPCData[i].nFlags;
			break;
		}
	}

	m_AttributeManager.InitializeAttributes( this );

	m_AttributeList.SetManager( &m_AttributeManager );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::Activate( void )
{
	BaseClass::Activate();

	if ( GetModelPtr() )
	{
		ParseScriptedNPCInteractions();
	}

	// Get a handle to my enemy filter entity if there is one.
	if ( m_iszEnemyFilterName != NULL_STRING )
	{
		CBaseEntity *pFilter = gEntList.FindEntityByName( NULL, m_iszEnemyFilterName );
		if ( pFilter != NULL )
		{
			m_hEnemyFilter = dynamic_cast<CBaseFilter*>(pFilter);
		}
	}

#ifdef AI_MONITOR_FOR_OSCILLATION
	m_ScheduleHistory.RemoveAll();
#endif//AI_MONITOR_FOR_OSCILLATION

}

void CAI_BaseNPC::Precache( void )
{
	gm_iszPlayerSquad = AllocPooledString( PLAYER_SQUADNAME ); // cache for fast IsPlayerSquad calls

	if ( m_spawnEquipment != NULL_STRING && strcmp(STRING(m_spawnEquipment), "0") )
	{
		UTIL_PrecacheOther( STRING(m_spawnEquipment) );
	}

	// Make sure schedules are loaded for this NPC type
	if (!LoadedSchedules())
	{
		DevMsg("ERROR: Rejecting spawn of %s as error in NPC's schedules.\n",GetDebugName());
		UTIL_Remove(this);
		return;
	}

	PrecacheScriptSound( "AI_BaseNPC.SwishSound" );
	PrecacheScriptSound( "AI_BaseNPC.BodyDrop_Heavy" );
	PrecacheScriptSound( "AI_BaseNPC.BodyDrop_Light" );
	PrecacheScriptSound( "AI_BaseNPC.SentenceStop" );

	const char *pModelName = STRING( GetModelName() );
	if( pModelName )
	{
		PrecacheModel( pModelName );
		SetModelName( MAKE_STRING( pModelName ) );
	}

	BaseClass::Precache();
}


//-----------------------------------------------------------------------------

const short AI_EXTENDED_SAVE_HEADER_VERSION = 5;
const short AI_EXTENDED_SAVE_HEADER_RESET_VERSION = 3;

const short AI_EXTENDED_SAVE_HEADER_FIRST_VERSION_WITH_CONDITIONS = 2;
const short AI_EXTENDED_SAVE_HEADER_FIRST_VERSION_WITH_SCHEDULE_ID_FIXUP = 3;
const short AI_EXTENDED_SAVE_HEADER_FIRST_VERSION_WITH_SEQUENCE = 4;
const short AI_EXTENDED_SAVE_HEADER_FIRST_VERSION_WITH_NAVIGATOR_SAVE = 5;

struct AIExtendedSaveHeader_t
{
	AIExtendedSaveHeader_t()
	 :	version(AI_EXTENDED_SAVE_HEADER_VERSION), 
		flags(0),
		scheduleCrc(0)
	{
		szSchedule[0] = 0;
		szIdealSchedule[0] = 0;
		szFailSchedule[0] = 0;
		szSequence[0] = 0;
	}

	short version;
	unsigned flags;
	char szSchedule[128];
	CRC32_t scheduleCrc;
	char szIdealSchedule[128];
	char szFailSchedule[128];
	char szSequence[128];
	
	DECLARE_SIMPLE_DATADESC();
};

enum AIExtendedSaveHeaderFlags_t
{
	AIESH_HAD_ENEMY		= 0x01,
	AIESH_HAD_TARGET	= 0x02,
	AIESH_HAD_NAVGOAL	= 0x04,
};

//-------------------------------------

BEGIN_SIMPLE_DATADESC( AIExtendedSaveHeader_t )
	DEFINE_FIELD( 		version,		FIELD_SHORT ),
	DEFINE_FIELD( 		flags,			FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY(	szSchedule,		FIELD_CHARACTER ),
	DEFINE_FIELD( 		scheduleCrc,	FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY(	szIdealSchedule,	FIELD_CHARACTER ),
	DEFINE_AUTO_ARRAY(	szFailSchedule,		FIELD_CHARACTER ),
	DEFINE_AUTO_ARRAY(	szSequence,		FIELD_CHARACTER ),
END_DATADESC()

//-------------------------------------

int CAI_BaseNPC::Save( ISave &save )
{
	AIExtendedSaveHeader_t saveHeader;
	
	if ( GetEnemy() )
		saveHeader.flags |= AIESH_HAD_ENEMY;
	if ( GetTarget() )
		saveHeader.flags |= AIESH_HAD_TARGET;
	if ( GetNavigator()->IsGoalActive() )
		saveHeader.flags |= AIESH_HAD_NAVGOAL;
	
	if ( m_pSchedule )
	{
		const char *pszSchedule = m_pSchedule->GetName();

		Assert( Q_strlen( pszSchedule ) < sizeof( saveHeader.szSchedule ) - 1 );
		Q_strncpy( saveHeader.szSchedule, pszSchedule, sizeof( saveHeader.szSchedule ) );

		CRC32_Init( &saveHeader.scheduleCrc );
		CRC32_ProcessBuffer( &saveHeader.scheduleCrc, (void *)m_pSchedule->GetTaskList(), m_pSchedule->NumTasks() * sizeof(Task_t) );
		CRC32_Final( &saveHeader.scheduleCrc );
	}
	else
	{
		saveHeader.szSchedule[0] = 0;
		saveHeader.scheduleCrc = 0;
	}

	int idealSchedule = GetGlobalScheduleId( m_IdealSchedule );

	if ( idealSchedule != -1 && idealSchedule != AI_RemapToGlobal( SCHED_NONE ) && idealSchedule != AI_RemapToGlobal( SCHED_AISCRIPT ) )
	{
		CAI_Schedule *pIdealSchedule = GetSchedule( m_IdealSchedule );
		if ( pIdealSchedule )
		{
			const char *pszIdealSchedule = pIdealSchedule->GetName();
			Assert( Q_strlen( pszIdealSchedule ) < sizeof( saveHeader.szIdealSchedule ) - 1 );
			Q_strncpy( saveHeader.szIdealSchedule, pszIdealSchedule, sizeof( saveHeader.szIdealSchedule ) );
		}
	}

	int failSchedule = GetGlobalScheduleId( m_failSchedule );
	if ( failSchedule != -1 && failSchedule != AI_RemapToGlobal( SCHED_NONE ) && failSchedule != AI_RemapToGlobal( SCHED_AISCRIPT ) )
	{
		CAI_Schedule *pFailSchedule = GetSchedule( m_failSchedule );
		if ( pFailSchedule )
		{
			const char *pszFailSchedule = pFailSchedule->GetName();
			Assert( Q_strlen( pszFailSchedule ) < sizeof( saveHeader.szFailSchedule ) - 1 );
			Q_strncpy( saveHeader.szFailSchedule, pszFailSchedule, sizeof( saveHeader.szFailSchedule ) );
		}
	}

	if ( GetSequence() != ACT_INVALID && GetModelPtr() )
	{
		const char *pszSequenceName = GetSequenceName( GetSequence() );
		if ( pszSequenceName && *pszSequenceName )
		{
			Assert( Q_strlen( pszSequenceName ) < sizeof( saveHeader.szSequence ) - 1 );
			Q_strncpy( saveHeader.szSequence, pszSequenceName, sizeof(saveHeader.szSequence) );
		}
	}

	save.WriteAll( &saveHeader );

	save.StartBlock();
	SaveConditions( save, m_Conditions );
	SaveConditions( save, m_CustomInterruptConditions );
	SaveConditions( save, m_ConditionsPreIgnore );
	CAI_ScheduleBits ignoreConditions;
	m_InverseIgnoreConditions.Not( &ignoreConditions );
	SaveConditions( save, ignoreConditions );
	save.EndBlock();

	save.StartBlock();
	GetNavigator()->Save( save );
	save.EndBlock();

	return BaseClass::Save(save);
}

//-------------------------------------

void CAI_BaseNPC::DiscardScheduleState()
{
	// We don't save/restore routes yet
	GetNavigator()->ClearGoal();

	// We don't save/restore schedules yet
	ClearSchedule( "Restoring NPC" );

	// Reset animation
	m_Activity = ACT_RESET;

	// If we don't have an enemy, clear conditions like see enemy, etc.
	if ( GetEnemy() == NULL )
	{
		m_Conditions.ClearAll();
	}

	// went across a transition and lost my m_hCine
	bool bLostScript = ( m_NPCState == NPC_STATE_SCRIPT && m_hCine == NULL );
	if ( bLostScript )
	{
		// UNDONE: Do something better here?
		// for now, just go back to idle and let the AI figure out what to do.
		SetState( NPC_STATE_IDLE );
		SetIdealState( NPC_STATE_IDLE );
		DevMsg(1, "Scripted Sequence stripped on level transition for %s\n", GetDebugName() );
	}
}

//-------------------------------------

void CAI_BaseNPC::OnRestore()
{
	gm_iszPlayerSquad = AllocPooledString( PLAYER_SQUADNAME ); // cache for fast IsPlayerSquad calls

	if ( m_bDoPostRestoreRefindPath  && CAI_NetworkManager::NetworksLoaded() )
	{
		CAI_DynamicLink::InitDynamicLinks();
		if ( !GetNavigator()->RefindPathToGoal( false ) )
			DiscardScheduleState();
	}
	else
	{
		GetNavigator()->ClearGoal();
	}
	BaseClass::OnRestore();
	m_bCheckContacts = true;
}


//-------------------------------------

int CAI_BaseNPC::Restore( IRestore &restore )
{
	AIExtendedSaveHeader_t saveHeader;
	restore.ReadAll( &saveHeader );

	if ( saveHeader.version >= AI_EXTENDED_SAVE_HEADER_FIRST_VERSION_WITH_CONDITIONS )
	{
		restore.StartBlock();
		RestoreConditions( restore, &m_Conditions );
		RestoreConditions( restore, &m_CustomInterruptConditions );
		RestoreConditions( restore, &m_ConditionsPreIgnore );
		CAI_ScheduleBits ignoreConditions;
		RestoreConditions( restore, &ignoreConditions );
		ignoreConditions.Not( &m_InverseIgnoreConditions );
		restore.EndBlock();
	}

	if ( saveHeader.version >= AI_EXTENDED_SAVE_HEADER_FIRST_VERSION_WITH_NAVIGATOR_SAVE )
	{
		restore.StartBlock();
		GetNavigator()->Restore( restore );
		restore.EndBlock();
	}
	
	// do a normal restore
	int status = BaseClass::Restore(restore);
	if ( !status )
		return 0;

	// Do schedule fix-up
	if ( saveHeader.version >= AI_EXTENDED_SAVE_HEADER_FIRST_VERSION_WITH_SCHEDULE_ID_FIXUP )
	{
		if ( saveHeader.szIdealSchedule[0] )
		{
			CAI_Schedule *pIdealSchedule = g_AI_SchedulesManager.GetScheduleByName( saveHeader.szIdealSchedule );
			m_IdealSchedule = ( pIdealSchedule ) ? pIdealSchedule->GetId() : SCHED_NONE;
		}

		if ( saveHeader.szFailSchedule[0] )
		{
			CAI_Schedule *pFailSchedule = g_AI_SchedulesManager.GetScheduleByName( saveHeader.szFailSchedule );
			m_failSchedule = ( pFailSchedule ) ? pFailSchedule->GetId() : SCHED_NONE;
		}
	}

	bool bLostSequence = false;
	if ( saveHeader.version >= AI_EXTENDED_SAVE_HEADER_FIRST_VERSION_WITH_SEQUENCE && saveHeader.szSequence[0] && GetModelPtr() )
	{
		SetSequence( LookupSequence( saveHeader.szSequence ) );
		if ( GetSequence() == ACT_INVALID )
		{
			DevMsg( this, AIMF_IGNORE_SELECTED, "Discarding missing sequence %s on load.\n", saveHeader.szSequence );
			SetSequence( 0 );
			bLostSequence = true;
		}

		Assert( IsValidSequence( GetSequence() ) );
	}

	bool bLostScript = ( m_NPCState == NPC_STATE_SCRIPT && m_hCine == NULL );
	bool bDiscardScheduleState = ( bLostScript || 
								   bLostSequence ||
								   saveHeader.szSchedule[0] == 0 ||
								   saveHeader.version < AI_EXTENDED_SAVE_HEADER_RESET_VERSION ||
								   ( (saveHeader.flags & AIESH_HAD_ENEMY) && !GetEnemy() ) ||
								   ( (saveHeader.flags & AIESH_HAD_TARGET) && !GetTarget() ) );

	if ( m_ScheduleState.taskFailureCode >= NUM_FAIL_CODES )
		m_ScheduleState.taskFailureCode = FAIL_NO_TARGET; // must have been a string, gotta punt

	if ( !bDiscardScheduleState )
	{
		m_pSchedule = g_AI_SchedulesManager.GetScheduleByName( saveHeader.szSchedule );
		if ( m_pSchedule )
		{
			CRC32_t scheduleCrc;
			CRC32_Init( &scheduleCrc );
			CRC32_ProcessBuffer( &scheduleCrc, (void *)m_pSchedule->GetTaskList(), m_pSchedule->NumTasks() * sizeof(Task_t) );
			CRC32_Final( &scheduleCrc );

			if ( scheduleCrc != saveHeader.scheduleCrc )
			{
				m_pSchedule = NULL;
			}
		}
	}

	if ( !m_pSchedule )
		bDiscardScheduleState = true;

	if ( !bDiscardScheduleState )
		m_bDoPostRestoreRefindPath = ( ( saveHeader.flags & AIESH_HAD_NAVGOAL) != 0 );
	else 
	{
		m_bDoPostRestoreRefindPath = false;
		DiscardScheduleState();
	}

	return status;
}

//-------------------------------------

void CAI_BaseNPC::SaveConditions( ISave &save, const CAI_ScheduleBits &conditions )
{
	for (int i = 0; i < MAX_CONDITIONS; i++)
	{
		if (conditions.IsBitSet(i))
		{
			const char *pszConditionName = ConditionName(AI_RemapToGlobal(i));
			if ( !pszConditionName )
				break;
			save.WriteString( pszConditionName );
		}
	}
	save.WriteString( "" );
}

//-------------------------------------

void CAI_BaseNPC::RestoreConditions( IRestore &restore, CAI_ScheduleBits *pConditions )
{
	pConditions->ClearAll();
	char szCondition[256];
	for (;;)
	{
		restore.ReadString( szCondition, sizeof(szCondition), 0 );
		if ( !szCondition[0] )
			break;
		int iCondition = GetSchedulingSymbols()->ConditionSymbolToId( szCondition );
		if ( iCondition != -1 )
			pConditions->Set( AI_RemapFromGlobal( iCondition ) );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::KeyValue( const char *szKeyName, const char *szValue )
{
	bool bResult = BaseClass::KeyValue( szKeyName, szValue );

	if( !bResult )
	{
		// Defer unhandled Keys to behaviors
		CAI_BehaviorBase **ppBehaviors = AccessBehaviors();
		
		for ( int i = 0; i < NumBehaviors(); i++ )
		{
			if( ppBehaviors[ i ]->KeyValue( szKeyName, szValue ) )
			{
				return true;
			}
		}
	}

	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose: Debug function to make this NPC freeze in place (or unfreeze).
//-----------------------------------------------------------------------------
void CAI_BaseNPC::ToggleFreeze(void) 
{
	if (!IsCurSchedule(SCHED_NPC_FREEZE))
	{
		// Freeze them.
		SetCondition(COND_NPC_FREEZE);
		SetMoveType(MOVETYPE_NONE);
		SetGravity(0);
		SetLocalAngularVelocity(vec3_angle);
		SetAbsVelocity( vec3_origin );
	}
	else
	{
		// Unfreeze them.
		SetCondition(COND_NPC_UNFREEZE);
		m_Activity = ACT_RESET;

		// BUGBUG: this might not be the correct movetype!
		SetMoveType( MOVETYPE_STEP );

		// Doesn't restore gravity to the original value, but who cares?
		SetGravity(1);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Written by subclasses macro to load schedules
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::LoadSchedules(void)
{
	return true;
}

//-----------------------------------------------------------------------------

bool CAI_BaseNPC::LoadedSchedules(void)
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  :
// Output :
//-----------------------------------------------------------------------------
CAI_BaseNPC::CAI_BaseNPC( void )
 :	m_UnreachableEnts( 0, 4 ),
    m_bDeferredNavigation( false )
{	
	m_pAttributes = this;

	m_pMotor = NULL;
	m_pMoveProbe = NULL;
	m_pNavigator = NULL;
	m_pSenses = NULL;
	m_pPathfinder = NULL;
	m_pLocalNavigator = NULL;

	m_pSchedule = NULL;
	m_IdealSchedule = SCHED_NONE;

#ifdef _DEBUG
	// necessary since in debug, we initialize vectors to NAN for debugging
	m_vecLastPosition.Init();
	m_vSavePosition.Init();
	m_vEyeLookTarget.Init();
	m_vCurEyeTarget.Init();
	m_vDefaultEyeOffset.Init();

#endif
	m_bDidDeathCleanup = false;

	m_afCapability				= 0;		// Make sure this is cleared in the base class

	SetHullType(HULL_HUMAN);  // Give human hull by default, subclasses should override
	m_nAITraceMask				= MASK_NPCSOLID;

	m_iMySquadSlot				= SQUAD_SLOT_NONE;
	m_flSumDamage				= 0;
	m_flLastDamageTime			= 0;
	m_flLastAttackTime			= 0;
	m_flSoundWaitTime			= 0;
	m_flNextEyeLookTime			= 0;
	m_flHeadYaw					= 0;
	m_flHeadPitch				= 0;
	m_spawnEquipment			= NULL_STRING;
	m_pEnemies					= new CAI_Enemies;
	m_bIgnoreUnseenEnemies		= false;
	m_flEyeIntegRate			= 0.95;
	SetTarget( NULL );

	m_pSquad					= NULL;

	m_flMoveWaitFinished		= 0;

	m_fIsUsingSmallHull			= true;

	m_bHintGroupNavLimiting		= false;

	m_fNoDamageDecal			= false;

	SetInAScript( false );

	m_pLockedBestSound = new CSound;
	m_pLockedBestSound->m_iType = SOUND_NONE;

	// ----------------------------
	//  Debugging fields
	// ----------------------------
	m_interruptText				= NULL;
	m_failText					= NULL;
	m_failedSchedule			= NULL;
	m_interuptSchedule			= NULL;
	m_nDebugPauseIndex			= 0;

	g_AI_Manager.AddAI( this );
	
	if ( g_AI_Manager.NumAIs() == 1 )
	{
		m_AnyUpdateEnemyPosTimer.Force();
		gm_flTimeLastSpawn = -1;
		gm_nSpawnedThisFrame = 0;
		gm_iNextThinkRebalanceTick = 0;
	}

	m_iFrameBlocked = -1;
	m_bInChoreo = true; // assume so until call to UpdateEfficiency()
	
	SetCollisionGroup( COLLISION_GROUP_NPC );

#ifdef TF_CLASSIC
	m_nPlayerCond = 0;

	m_bBurningDeath = false;
	m_bIceDeath = false;
	m_bAshDeath = false;

	m_iDamageCustomDeath = TF_DMG_CUSTOM_NONE;

	m_bKingRuneBuffActive = false;
	m_nTFFlags = 0;
	memset( m_flChargeOffTime, 0, sizeof( m_flChargeOffTime ) );
	memset( m_bChargeSounds, 0, sizeof( m_bChargeSounds ) );

	m_bAirblasted = false;

	m_flCritTime = 0;
	m_flLastCritCheckTime = 0;
	m_iLastCritCheckFrame = 0;
	m_bCurrentAttackIsCrit = false;
	m_iCurrentSeed = -1;

	m_iStunPhase = 0;

	m_hItem = NULL;
	m_flNextHealthRegen = 0;

	m_flHeadScale = 1;
	m_flTorsoScale = 1;
	m_flHandScale = 1;

	ListenForGameEvent( "teamplay_point_startcapture" );
	ListenForGameEvent( "teamplay_point_captured" );
	ListenForGameEvent( "teamplay_flag_event" );
	ListenForGameEvent( "teamplay_round_win" );

	SetContextThink( &CAI_BaseNPC::TFNPCThink, gpGlobals->curtime + 0.1, "TFNPCThink" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
// Input  :
// Output :
//-----------------------------------------------------------------------------
CAI_BaseNPC::~CAI_BaseNPC(void)
{
	DestroyRagdoll();

	g_AI_Manager.RemoveAI( this );

	delete m_pLockedBestSound;

	RemoveMemory();

	delete m_pPathfinder;
	delete m_pNavigator;
	delete m_pMotor;
	delete m_pLocalNavigator;
	delete m_pMoveProbe;
	delete m_pSenses;
	delete m_pTacticalServices;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_BaseNPC::UpdateOnRemove(void)
{
	if ( !m_bDidDeathCleanup )
	{
		if ( m_NPCState == NPC_STATE_DEAD )
			DevMsg( "May not have cleaned up on NPC death\n");

		CleanupOnDeath( NULL, false );
	}

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_BaseNPC::FireGameEvent( IGameEvent *event )
{
	if ( FStrEq( event->GetName(), "teamplay_point_startcapture" ) )
	{
		CTeamControlPoint *pTeamControlPoint = (CTeamControlPoint *)gEntList.FindEntityByClassname( NULL, "team_control_point" );
		if ( pTeamControlPoint )
		{
			if ( pTeamControlPoint->GetPointIndex() == event->GetInt( "cp" ) )
			{
				if ( ( event->GetInt( "team" ) == GetTeamNumber() ) && ( event->GetInt( "capteam" ) != GetTeamNumber() ) )
				{
					ScheduledMoveToGoalEntity( SCHED_FORCED_GO_RUN, pTeamControlPoint, ACT_RUN );
					SetSchedule( SCHED_FORCED_GO_RUN );
				}
			}
		}
	}
	else if ( FStrEq( event->GetName(), "teamplay_point_captured" ) )
	{
	}
	else if ( FStrEq( event->GetName(), "teamplay_flag_event" ) )
	{
		CBaseEntity *pFlag = gEntList.FindEntityByClassname( NULL, "item_teamflag" );
		if ( pFlag )
		{
			if ( event->GetInt( "eventtype" ) == TF_FLAGEVENT_PICKUP )
			{
				if ( event->GetInt( "team" ) == GetTeamNumber() )
				{
					if ( HasTheFlag() )
					{
						CCaptureZone *pCapZone = GetFlagCaptureZone();
						if ( pCapZone )
						{
							ScheduledMoveToGoalEntity( SCHED_FORCED_GO_RUN, pCapZone, ACT_RUN );
							SetSchedule( SCHED_FORCED_GO_RUN );
						}
					}
					else
					{
						ScheduledMoveToGoalEntity( SCHED_FORCED_GO_RUN, pFlag, ACT_RUN );
						SetSchedule( SCHED_FORCED_GO_RUN );
					}
				}
			}
			else if ( event->GetInt( "eventtype" ) == TF_FLAGEVENT_CAPTURE )
			{
				if ( event->GetInt( "team" ) != GetTeamNumber() )
				{
					CTakeDamageInfo dummyinfo;
					PainSound( dummyinfo );
				}
			}
			else if ( event->GetInt( "eventtype" ) == TF_FLAGEVENT_DEFEND )
			{
			}
			else if ( event->GetInt( "eventtype" ) == TF_FLAGEVENT_DROPPED )
			{
			}
			else if ( event->GetInt( "eventtype" ) == TF_FLAGEVENT_RETURNED )
			{
			}
		}
	}
	else if ( FStrEq( event->GetName(), "teamplay_round_win" ) )
	{
		if ( event->GetInt( "team" ) == GetTeamNumber() )
		{
			if ( TFGameRules()->IsHolidayActive( kHoliday_AprilFools ) )
			{
				SetSchedule( GetSchedule(SCHED_VICTORY_DANCE) );
				AddCond( TF_COND_INVULNERABLE );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Thinking FAST.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::TFNPCThink( void )
{
	if ( TFGameRules()->IsHordeMode() )
	{
		CTFNavArea *area = static_cast<CTFNavArea *>( GetLastKnownArea() );
		if ( area != nullptr )
		{
			if ( GetTeamNumber() == TF_TEAM_BLUE || GetTeamNumber() == TF_TEAM_GREEN || GetTeamNumber() == TF_TEAM_YELLOW )
			{
				if ( area->HasTFAttributes( BLUE_SPAWN_ROOM ) )
				{
					AddCond( TF_COND_INVULNERABLE, 0.5f );
					//AddCond( TF_COND_INVULNERABLE_HIDE_UNLESS_DAMAGED, 0.5f );
					AddCond( TF_COND_INVULNERABLE_WEARINGOFF, 0.5f );
				}
			}
		}
	}

	/*CCaptureFlag *pCapFlag = ( CCaptureFlag *)gEntList.FindEntityByClassname( NULL, "item_teamflag" );
	if ( pCapFlag )
	{
		if ( pCapFlag && !HasTheFlag() )
		{
			if ( ( pCapFlag->GetGameType() == TF_FLAGTYPE_CTF ) && !InSameTeam( pCapFlag ) )
			{
				ScheduledMoveToGoalEntity( SCHED_TARGET_CHASE, pCapFlag, ACT_RUN );
			}
			else if ( ( pCapFlag->GetGameType() == TF_FLAGTYPE_ATTACK_DEFEND ) && InSameTeam( pCapFlag ) )
			{
				ScheduledMoveToGoalEntity( SCHED_TARGET_CHASE, pCapFlag, ACT_RUN );
			}
		}
	}*/

	if( gpGlobals->curtime > m_flNextHealthRegen )
	{
		int iHealthDrain = 0;
		CALL_ATTRIB_HOOK_INT( iHealthDrain, add_health_regen );

		if( iHealthDrain )
		{
			RegenThink();
			m_flNextHealthRegen = gpGlobals->curtime + TF_MEDIC_REGEN_TIME;
		}

		if( InCond( TF_COND_RUNE_REGEN ) )
		{
			RuneRegenThink();
			m_flNextHealthRegen = gpGlobals->curtime + TF_RUNE_REGEN_TIME;
		}
	}

    m_flHeadScale  = Approach( GetDesiredHeadScale(),  m_flHeadScale,  GetHeadScaleSpeed() );
    m_flTorsoScale = Approach( GetDesiredTorsoScale(), m_flTorsoScale, GetTorsoScaleSpeed() );
    m_flHandScale  = Approach( GetDesiredHandScale(),  m_flHandScale,  GetHandScaleSpeed() );

	//RunAnimation();

	SetContextThink( &CAI_BaseNPC::TFNPCThink, gpGlobals->curtime, "TFNPCThink" );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CAI_BaseNPC::UpdateTransmitState()
{
	if( gpGlobals->curtime < m_flTimePingEffect )
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	return BaseClass::UpdateTransmitState();
}

//-----------------------------------------------------------------------------

bool CAI_BaseNPC::CreateComponents()
{
	m_pSenses = CreateSenses();
	if ( !m_pSenses )
		return false;

	m_pMotor = CreateMotor();
	if ( !m_pMotor )
		return false;

	m_pLocalNavigator = CreateLocalNavigator();
	if ( !m_pLocalNavigator )
		return false;

	m_pMoveProbe = CreateMoveProbe();
	if ( !m_pMoveProbe )
		return false;

	m_pNavigator = CreateNavigator();
	if ( !m_pNavigator )
		return false;

	m_pPathfinder = CreatePathfinder();
	if ( !m_pPathfinder )
		return false;
		
	m_pTacticalServices = CreateTacticalServices();
	if ( !m_pTacticalServices )
		return false;
		
	m_MoveAndShootOverlay.SetOuter( this );

	m_pMotor->Init( m_pLocalNavigator );
	m_pLocalNavigator->Init( m_pNavigator );
	m_pNavigator->Init( g_pBigAINet );
	m_pPathfinder->Init( g_pBigAINet );
	m_pTacticalServices->Init( g_pBigAINet );
	
	return true;
}

//-----------------------------------------------------------------------------

CAI_Senses *CAI_BaseNPC::CreateSenses()
{
	CAI_Senses *pSenses = new CAI_Senses;
	pSenses->SetOuter( this );
	return pSenses;
}

//-----------------------------------------------------------------------------

CAI_Motor *CAI_BaseNPC::CreateMotor()
{
	return new CAI_Motor( this );
}

//-----------------------------------------------------------------------------

CAI_MoveProbe *CAI_BaseNPC::CreateMoveProbe()
{
	return new CAI_MoveProbe( this );
}

//-----------------------------------------------------------------------------

CAI_LocalNavigator *CAI_BaseNPC::CreateLocalNavigator()
{
	return new CAI_LocalNavigator( this );
}

//-----------------------------------------------------------------------------

CAI_TacticalServices *CAI_BaseNPC::CreateTacticalServices()
{
	return new CAI_TacticalServices( this );
}

//-----------------------------------------------------------------------------

CAI_Navigator *CAI_BaseNPC::CreateNavigator()
{
	return new CAI_Navigator( this );
}

//-----------------------------------------------------------------------------

CAI_Pathfinder *CAI_BaseNPC::CreatePathfinder()
{
	return new CAI_Pathfinder( this );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputSetRelationship( inputdata_t &inputdata )
{
	AddRelationship( inputdata.value.String(), inputdata.pActivator );
}


//-----------------------------------------------------------------------------
// Won't affect the current enemy, only future enemy acquisitions.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputSetEnemyFilter( inputdata_t &inputdata )
{
	// Get a handle to my enemy filter entity if there is one.
	CBaseEntity *pFilter = gEntList.FindEntityByName( NULL, inputdata.value.StringID() );
	m_hEnemyFilter = dynamic_cast<CBaseFilter*>(pFilter);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputSetHealth( inputdata_t &inputdata )
{
	int iNewHealth = inputdata.value.Int();

	if ( iNewHealth != m_iHealth )
	{
		m_iHealth = iNewHealth;

		if ( m_iMaxHealth == 0 )
		{
			Assert( false );
			m_iMaxHealth = 1;
		}

		// Output the new health as a percentage of max health [0..1]
		//float flRatio = clamp( (float)m_iHealth / (float)m_iMaxHealth, 0.f, 1.f );
		//m_OnHealthChanged.Set( flRatio, pActivator, this );

		if ( m_iHealth <= 0 )
		{
			//CTakeDamageInfo info;
			//info.SetAttacker( this );
			TakeDamage( CTakeDamageInfo( this, this, iNewHealth, DMG_GENERIC ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputBeginRappel( inputdata_t &inputdata )
{
	BeginRappel();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputSetSquad( inputdata_t &inputdata )
{
	if ( !( CapabilitiesGet() & bits_CAP_SQUAD ) )
	{
		Warning("SetSquad Input received for NPC %s, but that NPC can't use squads.\n", GetDebugName() );
		return;
	}

	m_SquadName = inputdata.value.StringID();

	// Removing from squad?
	if ( m_SquadName == NULL_STRING )
	{
		if ( m_pSquad )
		{
			m_pSquad->RemoveFromSquad(this, true);
			m_pSquad = NULL;
		}
	}
	else
	{
		m_pSquad = g_AI_SquadManager.FindCreateSquad(this, m_SquadName);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputWake( inputdata_t &inputdata )
{
	Wake();

	// Check if we have a path to follow.  This is normally done in StartNPC,
	// but putting the NPC to sleep will cancel it, so we have to do it again.
	if ( m_target != NULL_STRING )// this npc has a target
	{
		// Find the npc's initial target entity, stash it
		SetGoalEnt( gEntList.FindEntityByName( NULL, m_target ) );

		if ( !GetGoalEnt() )
		{
			Warning( "ReadyNPC()--%s couldn't find target %s\n", GetClassname(), STRING(m_target));
		}
		else
		{
			StartTargetHandling( GetGoalEnt() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputForgetEntity( inputdata_t &inputdata )
{
	const char *pszEntityToForget = inputdata.value.String();

	if ( g_pDeveloper->GetInt() && pszEntityToForget[strlen( pszEntityToForget ) - 1] == '*' )
		DevMsg( "InputForgetEntity does not support wildcards\n" );

	CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, pszEntityToForget );
	if ( pEntity )
	{
		if ( GetEnemy() == pEntity )
		{
			SetEnemy( NULL );
			SetIdealState( NPC_STATE_ALERT );
		}
		GetEnemies()->ClearMemory( pEntity );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputIgnoreDangerSounds( inputdata_t &inputdata )
{
	// Default is 10 seconds.
	float flDelay = 10.0f;

	if( inputdata.value.Float() > 0.0f )
	{
		flDelay = inputdata.value.Float();
	}

	m_flIgnoreDangerSoundsUntil = gpGlobals->curtime + flDelay;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputUpdateEnemyMemory( inputdata_t &inputdata )
{
	const char *pszEnemy = inputdata.value.String();
	CBaseEntity *pEnemy = gEntList.FindEntityByName( NULL, pszEnemy );

	if( pEnemy )
	{
		UpdateEnemyMemory( pEnemy, pEnemy->GetAbsOrigin(), this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputOutsideTransition( inputdata_t &inputdata )
{
}

//-----------------------------------------------------------------------------
// Purpose: Called when this NPC transitions to another level with the player
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputInsideTransition( inputdata_t &inputdata )
{
	CleanupScriptsOnTeleport( true );

	// If we're inside a vcd, tell it to stop
	if ( IsCurSchedule( SCHED_SCENE_GENERIC, false ) )
	{
		RemoveActorFromScriptedScenes( this, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::CleanupScriptsOnTeleport( bool bEnrouteAsWell )
{
	// If I'm running a scripted sequence, I need to clean up
	if ( m_NPCState == NPC_STATE_SCRIPT && m_hCine )
	{
		if ( !bEnrouteAsWell )
		{
			//
			// Don't cancel scripts when they're teleporting an NPC
			// to the script for the purposes of movement.
			//
			if ( ( m_scriptState == CAI_BaseNPC::SCRIPT_WALK_TO_MARK ) ||
				 ( m_scriptState == CAI_BaseNPC::SCRIPT_RUN_TO_MARK ) ||
				 ( m_scriptState == CAI_BaseNPC::SCRIPT_CUSTOM_MOVE_TO_MARK ) ||
				 m_hCine->IsTeleportingDueToMoveTo() )
			{
				return;
			}
		}

		m_hCine->ScriptEntityCancel( m_hCine, true );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt)
{
#if defined (HL2_DLL) || defined (TF_CLASSIC)
	if ( interactionType == g_interactionBarnacleVictimGrab )
	{
		// Make the victim stop thinking so they're as good as dead without 
		// shocking the system by destroying the entity.
		StopLoopingSounds();
		BarnacleDeathSound();
 		SetThink( NULL );

		// Gag the NPC so they won't talk anymore
		AddSpawnFlags( SF_NPC_GAG );

		// Drop any weapon they're holding
		if ( GetActiveWeapon() )
		{
			Weapon_Drop( GetActiveWeapon() );
		}

		return true;
	}
#endif // HL2_DLL

	return BaseClass::HandleInteraction( interactionType, data, sourceEnt );
}

CAI_BaseNPC *CAI_BaseNPC::GetInteractionPartner( void )
{
	if ( m_hInteractionPartner == NULL )
		return NULL;

	return m_hInteractionPartner->MyNPCPointer();
}

//-----------------------------------------------------------------------------
// Purpose: Called when exiting a scripted sequence.
// Output : Returns true if alive, false if dead.
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::ExitScriptedSequence( )
{
	if ( m_lifeState == LIFE_DYING )
	{
		// is this legal?
		// BUGBUG -- This doesn't call Killed()
		SetIdealState( NPC_STATE_DEAD );
		return false;
	}

	if (m_hCine)
	{
		m_hCine->CancelScript( );
	}

	return true;
}


ConVar sv_test_scripted_sequences( "sv_test_scripted_sequences", "0", FCVAR_NONE, "Tests for scripted sequences that are embedded in the world. Run through your map with this set to check for NPCs falling through the world." );

bool CAI_BaseNPC::CineCleanup()
{
	CAI_ScriptedSequence *pOldCine = m_hCine;
	int nSavedFlags = ( m_hCine ? m_hCine->m_savedFlags : GetFlags() );

 	bool bDestroyCine = false;
	if ( IsRunningDynamicInteraction() )
	{
		bDestroyCine = true;

		// Re-enable physics collisions between me & the other NPC
		if ( m_hInteractionPartner )
		{
			PhysEnableEntityCollisions( this, m_hInteractionPartner );
			//Msg("%s(%s) enabled collisions with %s(%s) at %0.2f\n", GetClassname(), GetDebugName(), m_hInteractionPartner->GetClassName(), m_hInteractionPartner->GetDebugName(), gpGlobals->curtime );
		}

		if ( m_hForcedInteractionPartner )
		{
			// We've finished a forced interaction. Let the mapmaker know.
			m_OnForcedInteractionFinished.FireOutput( this, this );
		}

		// Clear interaction partner, because we're not running a scripted sequence anymore
		m_hInteractionPartner = NULL;
		CleanupForcedInteraction();
	}

	// am I linked to a cinematic?
	if (m_hCine)
	{
		// okay, reset me to what it thought I was before
		m_hCine->SetTarget( NULL );
		// NOTE that this will have had EF_NODRAW removed in script.dll when it's cached off
		SetEffects( m_hCine->m_saved_effects );
		
		SetCollisionGroup( m_hCine->m_savedCollisionGroup );
	}
	else
	{
		// arg, punt
		AddSolidFlags( FSOLID_NOT_STANDABLE );
	}
	
	m_hCine = NULL;
	SetTarget( NULL );
	SetGoalEnt( NULL );
	if (m_lifeState == LIFE_DYING)
	{
		// last frame of death animation?
		if ( m_iHealth > 0 )
		{
			m_iHealth = 0;
		}
		
		AddSolidFlags( FSOLID_NOT_SOLID );
		SetState( NPC_STATE_DEAD );
		m_lifeState = LIFE_DEAD;
		UTIL_SetSize( this, WorldAlignMins(), Vector(WorldAlignMaxs().x, WorldAlignMaxs().y, WorldAlignMins().z + 2) );

		if ( pOldCine && pOldCine->HasSpawnFlags( SF_SCRIPT_LEAVECORPSE ) )
		{
			SetUse( NULL );		// BUGBUG -- This doesn't call Killed()
			SetThink( NULL );	// This will probably break some stuff
			SetTouch( NULL );
		}
		else
			SUB_StartFadeOut(); // SetThink( SUB_DoNothing );


		//Not becoming a ragdoll, so set the NOINTERP flag on.
		if ( CanBecomeRagdoll() == false )
		{
			StopAnimation();
			IncrementInterpolationFrame(); // Don't interpolate either, assume the corpse is positioned in its final resting place
		}

		SetMoveType( MOVETYPE_NONE );
		return false;
	}

	// If we actually played a sequence
	if ( pOldCine && pOldCine->m_iszPlay != NULL_STRING && pOldCine->PlayedSequence() )
	{
		if ( !pOldCine->HasSpawnFlags(SF_SCRIPT_DONT_TELEPORT_AT_END) )
		{
			// reset position
			Vector new_origin;
			QAngle new_angle;
			GetBonePosition( 0, new_origin, new_angle );

			// Figure out how far they have moved
			// We can't really solve this problem because we can't query the movement of the origin relative
			// to the sequence.  We can get the root bone's position as we do here, but there are
			// cases where the root bone is in a different relative position to the entity's origin
			// before/after the sequence plays.  So we are stuck doing this:

			// !!!HACKHACK: Float the origin up and drop to floor because some sequences have
			// irregular motion that can't be properly accounted for.

			// UNDONE: THIS SHOULD ONLY HAPPEN IF WE ACTUALLY PLAYED THE SEQUENCE.
			Vector oldOrigin = GetLocalOrigin();

			// UNDONE: ugly hack.  Don't move NPC if they don't "seem" to move
			// this really needs to be done with the AX,AY,etc. flags, but that aren't consistantly
			// being set, so animations that really do move won't be caught.
			if ((oldOrigin - new_origin).Length2D() < 8.0)
				new_origin = oldOrigin;

			Vector origin = GetLocalOrigin();

			origin.x = new_origin.x;
			origin.y = new_origin.y;
			origin.z += 1;

			if ( nSavedFlags & FL_FLY )
			{
				origin.z = new_origin.z;
				SetLocalOrigin( origin );
			}
			else
			{
				SetLocalOrigin( origin );

				int drop = UTIL_DropToFloor( this, GetAITraceMask(), UTIL_GetNearestVisiblePlayer( this ) ); 

				// Origin in solid?  Set to org at the end of the sequence
				if ( ( drop < 0 ) || sv_test_scripted_sequences.GetBool() )
				{
					SetLocalOrigin( oldOrigin );
				}
				else if ( drop == 0 ) // Hanging in air?
				{
					Vector origin = GetLocalOrigin();
					origin.z = new_origin.z;
					SetLocalOrigin( origin );
					SetGroundEntity( NULL );
				}
			}

			origin = GetLocalOrigin();

			// teleport if it's a non-trivial distance
			if ((oldOrigin - origin).Length() > 8.0)
			{
				// Call teleport to notify
				Teleport( &origin, NULL, NULL );
				SetLocalOrigin( origin );
				IncrementInterpolationFrame();
			}

			if ( m_iHealth <= 0 )
			{
				// Dropping out because he got killed
				SetIdealState( NPC_STATE_DEAD );
				SetCondition( COND_LIGHT_DAMAGE );
				m_lifeState = LIFE_DYING;
			}
		}

		// We should have some animation to put these guys in, but for now it's idle.
		// Due to NOINTERP above, there won't be any blending between this anim & the sequence
		m_Activity = ACT_RESET;
	}

	// set them back into a normal state
	if ( m_iHealth > 0 )
	{
		SetIdealState( NPC_STATE_IDLE );
	}
	else
	{
		// Dropping out because he got killed
		SetIdealState( NPC_STATE_DEAD );
		SetCondition( COND_LIGHT_DAMAGE );
	}

	//	SetAnimation( m_NPCState );
	CLEARBITS(m_spawnflags, SF_NPC_WAIT_FOR_SCRIPT );

	if ( bDestroyCine )
	{
		UTIL_Remove( pOldCine );
	}

	return true;
}

//-----------------------------------------------------------------------------

void CAI_BaseNPC::Teleport( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity )
{
	CleanupScriptsOnTeleport( false );

	BaseClass::Teleport( newPosition, newAngles, newVelocity );
}

//-----------------------------------------------------------------------------

bool CAI_BaseNPC::FindSpotForNPCInRadius( Vector *pResult, const Vector &vStartPos, CAI_BaseNPC *pNPC, float radius, bool bOutOfPlayerViewcone )
{
	CBasePlayer *pPlayer = UTIL_GetNearestPlayer( pNPC->GetAbsOrigin() ); 
	QAngle fan;

	fan.x = 0;
	fan.z = 0;

	for( fan.y = 0 ; fan.y < 360 ; fan.y += 18.0 )
	{
		Vector vecTest;
		Vector vecDir;

		AngleVectors( fan, &vecDir );

		vecTest = vStartPos + vecDir * radius;

		if ( bOutOfPlayerViewcone && pPlayer && !pPlayer->FInViewCone( vecTest ) )
			continue;

		trace_t tr;

		UTIL_TraceLine( vecTest, vecTest - Vector( 0, 0, 8192 ), MASK_SHOT, pNPC, COLLISION_GROUP_NONE, &tr );
		if( tr.fraction == 1.0 )
		{
			continue;
		}

		UTIL_TraceHull( tr.endpos,
						tr.endpos + Vector( 0, 0, 10 ),
						pNPC->GetHullMins(),
						pNPC->GetHullMaxs(),
						pNPC->GetAITraceMask(),
						pNPC,
						COLLISION_GROUP_NONE,
						&tr );

		if( tr.fraction == 1.0 && pNPC->GetMoveProbe()->CheckStandPosition( tr.endpos, pNPC->GetAITraceMask() ) )
		{
			*pResult = tr.endpos;
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------

bool CAI_BaseNPC::IsNavigationUrgent()
{
	// return true if the navigation is for something that can't react well to failure
	if ( IsCurSchedule( SCHED_SCRIPTED_WALK, false ) || 
		 IsCurSchedule( SCHED_SCRIPTED_RUN, false ) ||
		 IsCurSchedule( SCHED_SCRIPTED_CUSTOM_MOVE, false ) ||
		 ( IsCurSchedule( SCHED_SCENE_GENERIC, false ) && IsInLockedScene() ) )
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------

bool CAI_BaseNPC::ShouldFailNav( bool bMovementFailed )
{
#ifdef HL2_EPISODIC

	if ( ai_vehicle_avoidance.GetBool() )
	{
		// Never be blocked this way by a vehicle (creates too many headaches around the levels)
		CBaseEntity *pEntity = GetNavigator()->GetBlockingEntity();
		if ( pEntity && pEntity->GetServerVehicle() )
		{
			// Vital allies never get stuck, and urgent moves cannot be blocked by a vehicle
			if ( Classify() == CLASS_PLAYER_ALLY_VITAL || IsNavigationUrgent() )
				return false;
		}
	}

#endif // HL2_EPISODIC

	// It's up to the schedule that requested movement to deal with failed movement.  Currently, only a handfull of 
	// schedules are considered Urgent, and they need to deal with what to do when there's no route, which by inspection
	// they'd don't.

	if ( IsNavigationUrgent())
	{
		return false;
	}

	return true;
}

Navigation_t CAI_BaseNPC::GetNavType() const
{
	return m_pNavigator->GetNavType();
}

void CAI_BaseNPC::SetNavType( Navigation_t navType )
{
	m_pNavigator->SetNavType( navType );
}

//-----------------------------------------------------------------------------
// NPCs can override this to tweak with how costly particular movements are
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost )
{
	// We have nothing to say on the matter, but derived classes might
	return false;
}

bool CAI_BaseNPC::OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval )
{
	return false;
}

bool CAI_BaseNPC::OverrideMove( float flInterval )
{
	return false;
}


//=========================================================
// VecToYaw - turns a directional vector into a yaw value
// that points down that vector.
//=========================================================
float CAI_BaseNPC::VecToYaw( const Vector &vecDir )
{
	if (vecDir.x == 0 && vecDir.y == 0 && vecDir.z == 0)
		return GetLocalAngles().y;

	return UTIL_VecToYaw( vecDir );
}

//-----------------------------------------------------------------------------
// Inherited from IAI_MotorMovementServices
//-----------------------------------------------------------------------------
float CAI_BaseNPC::CalcYawSpeed( void )
{
	// Negative values are invalud
	return -1.0f;
}

bool CAI_BaseNPC::OnCalcBaseMove( AILocalMoveGoal_t *pMoveGoal,
										float distClear,
										AIMoveResult_t *pResult )
{
	if ( pMoveGoal->directTrace.pObstruction )
	{
		CBasePropDoor *pPropDoor = dynamic_cast<CBasePropDoor *>( pMoveGoal->directTrace.pObstruction );
		if ( pPropDoor && OnUpcomingPropDoor( pMoveGoal, pPropDoor, distClear, pResult ) )
		{
			return true;
		}
	}

	return false;
}


bool CAI_BaseNPC::OnObstructionPreSteer( AILocalMoveGoal_t *pMoveGoal,
										float distClear,
										AIMoveResult_t *pResult )
{
	if ( pMoveGoal->directTrace.pObstruction )
	{
		CBaseDoor *pDoor = dynamic_cast<CBaseDoor *>( pMoveGoal->directTrace.pObstruction );
		if ( pDoor && OnObstructingDoor( pMoveGoal, pDoor, distClear, pResult ) )
		{
			return true;
		}
	}

	return false;
}


bool CAI_BaseNPC::OnObstructingDoor( AILocalMoveGoal_t *pMoveGoal,
 									 CBaseDoor *pDoor,
									 float distClear,
									 AIMoveResult_t *pResult )
{
	if ( pMoveGoal->maxDist < distClear )
		return false;

	// By default, NPCs don't know how to open doors
	if ( pDoor->m_toggle_state ==  TS_AT_BOTTOM || pDoor->m_toggle_state == TS_GOING_DOWN )
	{
		if ( distClear < 0.1 )
		{
			*pResult = AIMR_BLOCKED_ENTITY;
		}
		else
		{
			pMoveGoal->maxDist = distClear;
			*pResult = AIMR_OK;
		}

		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pMoveGoal - 
//			pDoor - 
//			distClear - 
//			default - 
//			spawn - 
//			oldorg - 
//			pfPosition - 
//			neworg - 
// Output : Returns true if movement is solved, false otherwise.
//-----------------------------------------------------------------------------

bool CAI_BaseNPC::OnUpcomingPropDoor( AILocalMoveGoal_t *pMoveGoal,
 										CBasePropDoor *pDoor,
										float distClear,
										AIMoveResult_t *pResult )
{
	if ( (pMoveGoal->flags & AILMG_TARGET_IS_GOAL) && pMoveGoal->maxDist < distClear )
		return false;

	if ( pMoveGoal->maxDist + GetHullWidth() * .25 < distClear )
		return false;

	if (pDoor == m_hOpeningDoor)
	{
		if ( pDoor->IsNPCOpening( this ) )
		{
			// We're in the process of opening the door, don't be blocked by it.
			pMoveGoal->maxDist = distClear;
			*pResult = AIMR_OK;
			return true;
		}
		m_hOpeningDoor = NULL;
	}

	if ((CapabilitiesGet() & bits_CAP_DOORS_GROUP) && !pDoor->IsDoorLocked() && (pDoor->IsDoorClosed() || pDoor->IsDoorClosing()))
	{
		AI_Waypoint_t *pOpenDoorRoute = NULL;

		opendata_t opendata;
		pDoor->GetNPCOpenData(this, opendata);
		
		// dvs: FIXME: local route might not be sufficient
		pOpenDoorRoute = GetPathfinder()->BuildLocalRoute(
			GetLocalOrigin(), 
			opendata.vecStandPos,
			NULL, 
			bits_WP_TO_DOOR | bits_WP_DONT_SIMPLIFY, 
			NO_NODE,
			bits_BUILD_GROUND | bits_BUILD_IGNORE_NPCS,
			0.0);
		
		if ( pOpenDoorRoute )
		{
			if ( AIIsDebuggingDoors(this) )
			{
				NDebugOverlay::Cross3D(opendata.vecStandPos + Vector(0,0,1), 32, 255, 255, 255, false, 1.0 );
				Msg( "Opening door!\n" );
			}

			// Attach the door to the waypoint so we open it when we get there.
			// dvs: FIXME: this is kind of bullshit, I need to find the exact waypoint to open the door
			//		should I just walk the path until I find it?
			pOpenDoorRoute->m_hData = pDoor;

			GetNavigator()->GetPath()->PrependWaypoints( pOpenDoorRoute );

			m_hOpeningDoor = pDoor;
			pMoveGoal->maxDist = distClear;
			*pResult = AIMR_CHANGE_TYPE;
				
			return true;
		}
		else
			AIDoorDebugMsg( this, "Failed create door route!\n" );
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called by the navigator to initiate the opening of a prop_door
//			that is in our way.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::OpenPropDoorBegin( CBasePropDoor *pDoor )
{
	opendata_t opendata;
	pDoor->GetNPCOpenData(this, opendata);

	if (HaveSequenceForActivity(opendata.eActivity))
	{
		SetIdealActivity(opendata.eActivity);
	}
	else
	{
		// We don't have an appropriate sequence, just open the door magically.
		OpenPropDoorNow( pDoor );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when we are trying to open a prop_door and it's time to start
//			the door moving. This is called either in response to an anim event
//			or as a fallback when we don't have an appropriate open activity.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::OpenPropDoorNow( CBasePropDoor *pDoor )
{
	// Start the door moving.
	pDoor->NPCOpenDoor(this);

	// Wait for the door to finish opening before trying to move through the doorway.
	m_flMoveWaitFinished = gpGlobals->curtime + pDoor->GetOpenInterval();
}


//-----------------------------------------------------------------------------
// Purpose: Called when the door we were trying to open becomes fully open.
// Input  : pDoor - 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::OnDoorFullyOpen(CBasePropDoor *pDoor)
{
	// We're done with the door.
	m_hOpeningDoor = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Called when the door we were trying to open becomes blocked before opening.
// Input  : pDoor - 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::OnDoorBlocked(CBasePropDoor *pDoor)
{
	// dvs: FIXME: do something so that we don't loop forever trying to open this door
	//		not clearing out the door handle will cause the NPC to invalidate the connection
	// We're done with the door.
	//m_hOpeningDoor = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Template NPCs are marked as templates by the level designer. They
//			do not spawn, but their keyvalues are saved for use by a template
//			spawner.
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::IsTemplate( void )
{
	return HasSpawnFlags( SF_NPC_TEMPLATE );
}



//-----------------------------------------------------------------------------
//
// Movement code for walking + flying
//
//-----------------------------------------------------------------------------
int CAI_BaseNPC::FlyMove( const Vector& pfPosition, unsigned int mask )
{
	Vector		oldorg, neworg;
	trace_t		trace;

	// try the move	
	VectorCopy( GetAbsOrigin(), oldorg );
	VectorAdd( oldorg, pfPosition, neworg );
	UTIL_TraceEntity( this, oldorg, neworg, mask, &trace );				
	if (trace.fraction == 1)
	{
		if ( (GetFlags() & FL_SWIM) && enginetrace->GetPointContents(trace.endpos) == CONTENTS_EMPTY )
			return false;	// swim monster left water

		SetAbsOrigin( trace.endpos );
		PhysicsTouchTriggers();
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ent - 
//			Dir - Normalized direction vector for movement.
//			dist - Distance along 'Dir' to move.
//			iMode - 
// Output : Returns nonzero on success, zero on failure.
//-----------------------------------------------------------------------------
int CAI_BaseNPC::WalkMove( const Vector& vecPosition, unsigned int mask )
{	
	if ( GetFlags() & (FL_FLY | FL_SWIM) )
	{
		return FlyMove( vecPosition, mask );
	}

	if ( (GetFlags() & FL_ONGROUND) == 0 )
	{
		return 0;
	}

	trace_t	trace;
	Vector oldorg, neworg, end;
	Vector move( vecPosition[0], vecPosition[1], 0.0f );
	VectorCopy( GetAbsOrigin(), oldorg );
	VectorAdd( oldorg, move, neworg );

	// push down from a step height above the wished position
	float flStepSize = sv_stepsize.GetFloat();
	neworg[2] += flStepSize;
	VectorCopy(neworg, end);
	end[2] -= flStepSize*2;

	UTIL_TraceEntity( this, neworg, end, mask, &trace );
	if ( trace.allsolid )
		return false;

	if (trace.startsolid)
	{
		neworg[2] -= flStepSize;
		UTIL_TraceEntity( this, neworg, end, mask, &trace );
		if ( trace.allsolid || trace.startsolid )
			return false;
	}

	if (trace.fraction == 1)
	{
		// if monster had the ground pulled out, go ahead and fall
		if ( GetFlags() & FL_PARTIALGROUND )
		{
			SetAbsOrigin( oldorg + move );
			PhysicsTouchTriggers();
			SetGroundEntity( NULL );
			return true;
		}
	
		return false;		// walked off an edge
	}

	// check point traces down for dangling corners
	SetAbsOrigin( trace.endpos );

	if (UTIL_CheckBottom( this, NULL, flStepSize ) == 0)
	{
		if ( GetFlags() & FL_PARTIALGROUND )
		{	
			// entity had floor mostly pulled out from underneath it
			// and is trying to correct
			PhysicsTouchTriggers();
			return true;
		}

		// Reset to original position
		SetAbsOrigin( oldorg );
		return false;
	}

	if ( GetFlags() & FL_PARTIALGROUND )
	{
		// Con_Printf ("back on ground\n"); 
		RemoveFlag( FL_PARTIALGROUND );
	}

	// the move is ok
	SetGroundEntity( trace.m_pEnt );
	PhysicsTouchTriggers();
	return true;
}

//-----------------------------------------------------------------------------

static void AIMsgGuts( CAI_BaseNPC *pAI, unsigned flags, const char *pszMsg )
{
	int			len		= strlen( pszMsg );
	const char *pszFmt2 = NULL;

	if ( len && pszMsg[len-1] == '\n' )
	{
		(const_cast<char *>(pszMsg))[len-1] = 0;
		pszFmt2 = "%s (%s: %d/%s) [%d]\n";
	}
	else
		pszFmt2 = "%s (%s: %d/%s) [%d]";
	
	DevMsg( pszFmt2, 
		 pszMsg, 
		 pAI->GetClassname(),
		 pAI->entindex(),
		 ( pAI->GetEntityName() == NULL_STRING ) ? "<unnamed>" : STRING(pAI->GetEntityName()),
		 gpGlobals->tickcount );
}

void DevMsg( CAI_BaseNPC *pAI, unsigned flags, const char *pszFormat, ... )
{
	if ( (flags & AIMF_IGNORE_SELECTED) || (pAI->m_debugOverlays & OVERLAY_NPC_SELECTED_BIT) )
	{
		va_list ap;
		va_start(ap, pszFormat);
		char szTempMsgBuf[512];
		V_vsprintf_safe( szTempMsgBuf, pszFormat, ap );

		AIMsgGuts( pAI, flags, szTempMsgBuf );
		va_end(ap);
	}
}

//-----------------------------------------------------------------------------

void DevMsg( CAI_BaseNPC *pAI, const char *pszFormat, ... )
{
	if ( (pAI->m_debugOverlays & OVERLAY_NPC_SELECTED_BIT) )
	{
		va_list ap;
		va_start(ap, pszFormat);
		char szTempMsgBuf[512];
		V_vsprintf_safe( szTempMsgBuf, pszFormat, ap );

		AIMsgGuts( pAI, 0, szTempMsgBuf );
		va_end(ap);
	}
}

//-----------------------------------------------------------------------------

bool CAI_BaseNPC::IsPlayerAlly( CBasePlayer *pPlayer )											
{ 
	if (pPlayer == NULL)
	{
#ifdef TF_CLASSIC
		for ( int i = 0; i <= gpGlobals->maxClients; i++ )
		{
			pPlayer = ToTFPlayer(UTIL_PlayerByIndex(i));

			if ( !InSameTeam(pPlayer) )
				return false;
		}
	}
#else
		// in multiplayer mode we need a valid pPlayer 
		// or override this virtual function
		if ( !AI_IsSinglePlayer() )
			return false;

		// NULL means single player mode
		pPlayer = UTIL_GetLocalPlayer();
#endif

	return ( !pPlayer || IRelationType( pPlayer ) == D_LI ); 
}

//-----------------------------------------------------------------------------

void CAI_BaseNPC::SetCommandGoal( const Vector &vecGoal )
{ 
	m_vecCommandGoal = vecGoal; 
	m_CommandMoveMonitor.ClearMark(); 
}

//-----------------------------------------------------------------------------

void CAI_BaseNPC::ClearCommandGoal()
{ 
	m_vecCommandGoal = vec3_invalid; 
	m_CommandMoveMonitor.ClearMark(); 
}

//-----------------------------------------------------------------------------

bool CAI_BaseNPC::IsInPlayerSquad() const
{ 
	return ( m_pSquad && m_pSquad->GetPlayerCommander() != nullptr ) && !CAI_Squad::IsSilentMember(this);
}


//-----------------------------------------------------------------------------

bool CAI_BaseNPC::CanBeUsedAsAFriend( void )
{
	if ( IsCurSchedule(SCHED_FORCED_GO) || IsCurSchedule(SCHED_FORCED_GO_RUN) )
		return false;
	return true;
}

//-----------------------------------------------------------------------------

Vector CAI_BaseNPC::GetSmoothedVelocity( void )
{
	if( GetNavType() == NAV_GROUND || GetNavType() == NAV_FLY )
	{
		return m_pMotor->GetCurVel();
	}

	return BaseClass::GetSmoothedVelocity();
}


//-----------------------------------------------------------------------------

bool CAI_BaseNPC::IsCoverPosition( const Vector &vecThreat, const Vector &vecPosition )
{
	trace_t	tr;

	// By default, we ignore the viewer (me) when determining cover positions
	CTraceFilterLOS filter( NULL, COLLISION_GROUP_NONE, this );

	// If I'm trying to find cover from the player, and the player is in a vehicle,
	// ignore the vehicle for the purpose of determining line of sight.
	CBaseEntity *pEnemy = GetEnemy();
	if ( pEnemy )
	{
		// Hack to see if our threat position is our enemy
		bool bThreatPosIsEnemy = ( (vecThreat - GetEnemy()->EyePosition()).LengthSqr() < 0.1f );
		if ( bThreatPosIsEnemy )
		{
			CBaseCombatCharacter *pCCEnemy = GetEnemy()->MyCombatCharacterPointer();
			if ( pCCEnemy != NULL && pCCEnemy->IsInAVehicle() )
			{
				// Ignore the vehicle
				filter.SetPassEntity( pCCEnemy->GetVehicleEntity() );
			}

			if ( !filter.GetPassEntity() )
			{
				filter.SetPassEntity( pEnemy );
			}
		}
	}

	AI_TraceLOS( vecThreat, vecPosition, this, &tr, &filter );

	if( tr.fraction != 1.0 && hl2_episodic.GetBool() )
	{
		if( tr.m_pEnt->m_iClassname == m_iClassname )
		{
			// Don't hide behind buddies!
			return false;
		}
	}

	return (tr.fraction != 1.0);
}

//-----------------------------------------------------------------------------

float CAI_BaseNPC::SetWait( float minWait, float maxWait )
{
	int minThinks = Ceil2Int( minWait * 10 );

	if ( maxWait == 0.0 )
	{
		m_flWaitFinished = gpGlobals->curtime + ( 0.1 * minThinks );
	}
	else
	{
		if ( minThinks == 0 ) // random 0..n is almost certain to not return 0
			minThinks = 1;
		int maxThinks = Ceil2Int( maxWait * 10 );

		m_flWaitFinished = gpGlobals->curtime + ( 0.1 * random->RandomInt( minThinks, maxThinks ) );
	}
	return m_flWaitFinished;
}

//-----------------------------------------------------------------------------

void CAI_BaseNPC::ClearWait()
{
	m_flWaitFinished = FLT_MAX;
}

//-----------------------------------------------------------------------------

bool CAI_BaseNPC::IsWaitFinished()
{
	return ( gpGlobals->curtime >= m_flWaitFinished );
}

//-----------------------------------------------------------------------------

bool CAI_BaseNPC::IsWaitSet()
{
	return ( m_flWaitFinished != FLT_MAX );
}

void CAI_BaseNPC::TestPlayerPushing( CBaseEntity *pEntity )
{
	if ( HasSpawnFlags( SF_NPC_NO_PLAYER_PUSHAWAY ) )
		return;

#ifdef TF_CLASSIC
	// Don't give way to spectators.
	if ( !pEntity->IsAlive() )
		return;

	// Don't give way to enemy players.
	if ( GetTeamNumber() != pEntity->GetTeamNumber() )
		return;
#endif

	// Heuristic for determining if the player is pushing me away
	CBasePlayer *pPlayer = ToBasePlayer( pEntity );

	if ( pPlayer && !( pPlayer->GetFlags() & FL_NOTARGET ) )
	{
		if ( (pPlayer->m_nButtons & (IN_FORWARD|IN_BACK|IN_MOVELEFT|IN_MOVERIGHT)) || 
			 pPlayer->GetAbsVelocity().AsVector2D().LengthSqr() > 50*50 )
		{
			SetCondition( COND_PLAYER_PUSHING );
			Vector vecPush = GetAbsOrigin() - pPlayer->GetAbsOrigin();
			VectorNormalize( vecPush );
			CascadePlayerPush( vecPush, pPlayer->WorldSpaceCenter() );
		}
	}
}

void CAI_BaseNPC::CascadePlayerPush( const Vector &push, const Vector &pushOrigin )
{
	//
	// Try to push any friends that are in the way.
	//
	float			hullWidth						= GetHullWidth();
	const Vector &	origin							= GetAbsOrigin();
	const Vector2D &origin2D						= origin.AsVector2D();

	const float		MIN_Z_TO_TRANSMIT				= GetHullHeight() * 0.5 + 0.1;
	const float		DIST_REQD_TO_TRANSMIT_PUSH_SQ	= Square( hullWidth * 5 + 0.1 );
	const float		DIST_FROM_PUSH_VECTOR_REQD_SQ	= Square( hullWidth + 0.1 );

	Vector2D		pushTestPoint = vec2_invalid;

	for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
	{
		CAI_BaseNPC *pOther = g_AI_Manager.AccessAIs()[i];
		if ( pOther != this && pOther->IRelationType(this) == D_LI && !pOther->HasCondition( COND_PLAYER_PUSHING ) )
		{
			const Vector &friendOrigin = pOther->GetAbsOrigin();
			if ( fabsf( friendOrigin.z - origin.z ) < MIN_Z_TO_TRANSMIT &&
				 ( friendOrigin.AsVector2D() - origin.AsVector2D() ).LengthSqr() < DIST_REQD_TO_TRANSMIT_PUSH_SQ )
			{
				if ( pushTestPoint == vec2_invalid )
				{
					pushTestPoint = origin2D - pushOrigin.AsVector2D();
					// No normalize, since it wants to just be a big number and we can't be less that a hull away
					pushTestPoint *= 2000;
					pushTestPoint += origin2D;

				}
				float t;
				float distSq = CalcDistanceSqrToLine2D(  friendOrigin.AsVector2D(), origin2D, pushTestPoint, &t );
				if ( t > 0 && distSq < DIST_FROM_PUSH_VECTOR_REQD_SQ )
				{
					pOther->SetCondition( COND_PLAYER_PUSHING );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Break into pieces!
//-----------------------------------------------------------------------------
void CAI_BaseNPC::Break( CBaseEntity *pBreaker )
{
	m_takedamage = DAMAGE_NO;

	Vector velocity;
	AngularImpulse angVelocity;
	IPhysicsObject *pPhysics = VPhysicsGetObject();
	Vector origin;
	QAngle angles;
	AddSolidFlags( FSOLID_NOT_SOLID );
	if ( pPhysics )
	{
		pPhysics->GetVelocity( &velocity, &angVelocity );
		pPhysics->GetPosition( &origin, &angles );
		pPhysics->RecheckCollisionFilter();
	}
	else
	{
		velocity = GetAbsVelocity();
		QAngleToAngularImpulse( GetLocalAngularVelocity(), angVelocity );
		origin = GetAbsOrigin();
		angles = GetAbsAngles();
	}

	breakablepropparams_t params( GetAbsOrigin(), GetAbsAngles(), velocity, angVelocity );
	params.impactEnergyScale = m_impactEnergyScale;
	params.defCollisionGroup = GetCollisionGroup();
	if ( params.defCollisionGroup == COLLISION_GROUP_NONE )
	{
		// don't automatically make anything COLLISION_GROUP_NONE or it will
		// collide with debris being ejected by breaking
		params.defCollisionGroup = COLLISION_GROUP_INTERACTIVE;
	}

	// no damage/damage force? set a burst of 100 for some movement
	params.defBurstScale = 100;//pDamageInfo ? 0 : 100;
	PropBreakableCreateAll( GetModelIndex(), pPhysics, params, this, -1, false );

	UTIL_Remove(this);
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for breaking the breakable immediately.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputBreak( inputdata_t &inputdata )
{
	Break( inputdata.pActivator );
}


//-----------------------------------------------------------------------------

bool CAI_BaseNPC::FindNearestValidGoalPos( const Vector &vTestPoint, Vector *pResult )
{
	AIMoveTrace_t moveTrace;
	Vector vCandidate = vec3_invalid;
	if ( GetNavigator()->CanFitAtPosition( vTestPoint, MASK_SOLID_BRUSHONLY ) )
	{
		if ( GetMoveProbe()->CheckStandPosition( vTestPoint, MASK_SOLID_BRUSHONLY ) )
		{
			vCandidate = vTestPoint;
		}
	}

	if ( vCandidate == vec3_invalid )
	{
		int iNearestNode = GetPathfinder()->NearestNodeToPoint( vTestPoint );
		if ( iNearestNode != NO_NODE )
		{
			GetMoveProbe()->MoveLimit( NAV_GROUND, 
									   g_pBigAINet->GetNodePosition(GetHullType(), iNearestNode ), 
									   vTestPoint, 
									   MASK_SOLID_BRUSHONLY, 
									   NULL, 
									   0, 
									   &moveTrace );
			if ( ( moveTrace.vEndPosition - vTestPoint ).Length2DSqr() < Square( GetHullWidth() * 3.0 ) &&
				 GetMoveProbe()->CheckStandPosition( moveTrace.vEndPosition, MASK_SOLID_BRUSHONLY ) )
			{
				vCandidate = moveTrace.vEndPosition;
			}
		}
	}

	if ( vCandidate != vec3_invalid )
	{
#ifdef SecobMod__Enable_Fixed_Multiplayer_AI
		AI_Waypoint_t *pPathToPoint = GetPathfinder()->BuildRoute( GetAbsOrigin(), vCandidate, UTIL_GetNearestPlayer( GetAbsOrigin() ), 5*12, NAV_NONE, true ); 
#else
		AI_Waypoint_t *pPathToPoint = GetPathfinder()->BuildRoute( GetAbsOrigin(), vCandidate, AI_GetSinglePlayer(), 5*12, NAV_NONE, true );
#endif //SecobMod__Enable_Fixed_Multiplayer_AI
		if ( pPathToPoint )
		{
			GetPathfinder()->UnlockRouteNodes( pPathToPoint );
			CAI_Path tempPath;
			tempPath.SetWaypoints( pPathToPoint ); // path object will delete waypoints
		}
		else
			vCandidate = vec3_invalid;
	}

	if ( vCandidate == vec3_invalid )
	{
		GetMoveProbe()->MoveLimit( NAV_GROUND, 
								   GetAbsOrigin(), 
								   vTestPoint, 
								   MASK_SOLID_BRUSHONLY, 
								   NULL, 
								   0, 
								   &moveTrace );
		vCandidate = moveTrace.vEndPosition;
	}

	if ( vCandidate == vec3_invalid )
		return false;

	if ( pResult != NULL )
	{
		*pResult = vCandidate;
	}

	return true;
}

//---------------------------------------------------------
// Pass a direction to get how far an NPC would see if facing
// that direction. Pass nothing to get the length of the NPC's
// current line of sight.
//---------------------------------------------------------
float CAI_BaseNPC::LineOfSightDist( const Vector &vecDir, float zEye )
{
	Vector testDir;
	if( vecDir == vec3_invalid )
	{
		testDir = EyeDirection3D();
	}
	else
	{
		testDir = vecDir;
	}

	if ( zEye == FLT_MAX ) 
		zEye = EyePosition().z;
	
	trace_t tr;
	// Need to center trace so don't get erratic results based on orientation
	Vector testPos( GetAbsOrigin().x, GetAbsOrigin().y, zEye ); 
	AI_TraceLOS( testPos, testPos + testDir * MAX_COORD_RANGE, this, &tr );
	return (tr.startpos - tr.endpos ).Length();
}

ConVar ai_LOS_mode( "ai_LOS_mode", "0", FCVAR_REPLICATED );

//-----------------------------------------------------------------------------
// Purpose: Use this to perform AI tracelines that are trying to determine LOS between points.
//			LOS checks between entities should use FVisible.
//-----------------------------------------------------------------------------
void AI_TraceLOS( const Vector& vecAbsStart, const Vector& vecAbsEnd, CBaseEntity *pLooker, trace_t *ptr, ITraceFilter *pFilter )
{
	AI_PROFILE_SCOPE( AI_TraceLOS );

	if ( ai_LOS_mode.GetBool() )
	{
		// Don't use LOS tracefilter
		UTIL_TraceLine( vecAbsStart, vecAbsEnd, MASK_BLOCKLOS, pLooker, COLLISION_GROUP_NONE, ptr );
		return;
	}

	// Use the custom LOS trace filter
	CTraceFilterLOS traceFilter( pLooker, COLLISION_GROUP_NONE );
	if ( !pFilter )
		pFilter = &traceFilter;
	UTIL_TraceLine( vecAbsStart, vecAbsEnd, MASK_BLOCKLOS_AND_NPCS, pFilter, ptr );
}

void CAI_BaseNPC::InputSetSpeedModifierRadius( inputdata_t &inputdata )
{
	m_iSpeedModRadius = inputdata.value.Int();
	m_iSpeedModRadius *= m_iSpeedModRadius;
}
void CAI_BaseNPC::InputSetSpeedModifierSpeed( inputdata_t &inputdata )
{
	m_iSpeedModSpeed = inputdata.value.Int();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::IsAllowedToDodge( void ) 
{ 
	// Can't do it if I'm not available
	if ( m_NPCState != NPC_STATE_IDLE && m_NPCState != NPC_STATE_ALERT && m_NPCState != NPC_STATE_COMBAT  )
		return false;

	return ( m_flNextDodgeTime <= gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::ParseScriptedNPCInteractions( void )
{
	// Already parsed them?
	if ( m_ScriptedInteractions.Count() ) 
		return;

	// Parse the model's key values and find any dynamic interactions
	KeyValues *modelKeyValues = new KeyValues("");
	CUtlBuffer buf( 1024, 0, CUtlBuffer::TEXT_BUFFER );

	if (! modelinfo->GetModelKeyValue( GetModel(), buf ))
		return;
	
	if ( modelKeyValues->LoadFromBuffer( modelinfo->GetModelName( GetModel() ), buf ) )
	{
		// Do we have a dynamic interactions section?
		KeyValues *pkvInteractions = modelKeyValues->FindKey("dynamic_interactions");
		if ( pkvInteractions )
		{
			KeyValues *pkvNode = pkvInteractions->GetFirstSubKey();
			while ( pkvNode )
			{
				ScriptedNPCInteraction_t sInteraction;
				sInteraction.iszInteractionName = AllocPooledString( pkvNode->GetName() );

				// Trigger method
				const char *pszTrigger = pkvNode->GetString( "trigger", NULL );
				if ( pszTrigger )
				{
					if ( !Q_strncmp( pszTrigger, "auto_in_combat", 14) )
					{
						sInteraction.iTriggerMethod = SNPCINT_AUTOMATIC_IN_COMBAT;
					}
				}

				// Loop Break trigger method
				pszTrigger = pkvNode->GetString( "loop_break_trigger", NULL );
				if ( pszTrigger )
				{
					char szTrigger[256];
					Q_strncpy( szTrigger, pszTrigger, sizeof(szTrigger) );
					char *pszParam = strtok( szTrigger, " " );
					while (pszParam)
					{
						if ( !Q_strncmp( pszParam, "on_damage", 9) )
						{
							sInteraction.iLoopBreakTriggerMethod |= SNPCINT_LOOPBREAK_ON_DAMAGE;
						}
						if ( !Q_strncmp( pszParam, "on_flashlight_illum", 19) )
						{
							sInteraction.iLoopBreakTriggerMethod |= SNPCINT_LOOPBREAK_ON_FLASHLIGHT_ILLUM;
						}

						pszParam = strtok(NULL," ");
					}
				}

				// Origin
				const char *pszOrigin = pkvNode->GetString( "origin_relative", "0 0 0" );
				UTIL_StringToVector( sInteraction.vecRelativeOrigin.Base(), pszOrigin );

				// Angles
				const char *pszAngles = pkvNode->GetString( "angles_relative", NULL );
				if ( pszAngles )
				{
					sInteraction.iFlags |= SCNPC_FLAG_TEST_OTHER_ANGLES;
					UTIL_StringToVector( sInteraction.angRelativeAngles.Base(), pszAngles );
				}

				// Velocity 
				const char *pszVelocity = pkvNode->GetString( "velocity_relative", NULL );
				if ( pszVelocity )
				{
					sInteraction.iFlags |= SCNPC_FLAG_TEST_OTHER_VELOCITY;
					UTIL_StringToVector( sInteraction.vecRelativeVelocity.Base(), pszVelocity );
				}

				// Entry Sequence
				const char *pszSequence = pkvNode->GetString( "entry_sequence", NULL );
				if ( pszSequence )
				{
					sInteraction.sPhases[SNPCINT_ENTRY].iszSequence = AllocPooledString( pszSequence );
				}
				// Entry Activity
				const char *pszActivity = pkvNode->GetString( "entry_activity", NULL );
				if ( pszActivity )
				{
					sInteraction.sPhases[SNPCINT_ENTRY].iActivity = GetActivityID( pszActivity );
				}

				// Sequence
				pszSequence = pkvNode->GetString( "sequence", NULL );
				if ( pszSequence )
				{
					sInteraction.sPhases[SNPCINT_SEQUENCE].iszSequence = AllocPooledString( pszSequence );
				}
				// Activity
				pszActivity = pkvNode->GetString( "activity", NULL );
				if ( pszActivity )
				{
					sInteraction.sPhases[SNPCINT_SEQUENCE].iActivity = GetActivityID( pszActivity );
				}

				// Exit Sequence
				pszSequence = pkvNode->GetString( "exit_sequence", NULL );
				if ( pszSequence )
				{
					sInteraction.sPhases[SNPCINT_EXIT].iszSequence = AllocPooledString( pszSequence );
				}
				// Exit Activity
				pszActivity = pkvNode->GetString( "exit_activity", NULL );
				if ( pszActivity )
				{
					sInteraction.sPhases[SNPCINT_EXIT].iActivity = GetActivityID( pszActivity );
				}

				// Delay
				sInteraction.flDelay = pkvNode->GetFloat( "delay", 10.0 );

				// Delta
				sInteraction.flDistSqr = pkvNode->GetFloat( "origin_max_delta", (DSS_MAX_DIST * DSS_MAX_DIST) );

				// Loop?
				if ( pkvNode->GetFloat( "loop_in_action", 0 ) )
				{
					sInteraction.iFlags |= SCNPC_FLAG_LOOP_IN_ACTION;
				}

				// Fixup position?
				const char *pszDontFixup = pkvNode->GetString( "dont_teleport_at_end", NULL );
				if ( pszDontFixup )
				{
					if ( !Q_stricmp( pszDontFixup, "me" ) || !Q_stricmp( pszDontFixup, "both" ) )
					{
						sInteraction.iFlags |= SCNPC_FLAG_DONT_TELEPORT_AT_END_ME;
					}
					else if ( !Q_stricmp( pszDontFixup, "them" ) || !Q_stricmp( pszDontFixup, "both" ) ) 
					{
						sInteraction.iFlags |= SCNPC_FLAG_DONT_TELEPORT_AT_END_THEM;
					}
				}

				// Needs a weapon?
				const char *pszNeedsWeapon = pkvNode->GetString( "needs_weapon", NULL );
				if ( pszNeedsWeapon )
				{
					if ( !Q_strncmp( pszNeedsWeapon, "ME", 2 ) )
					{
						sInteraction.iFlags |= SCNPC_FLAG_NEEDS_WEAPON_ME;
					}
					else if ( !Q_strncmp( pszNeedsWeapon, "THEM", 4 ) ) 
					{
						sInteraction.iFlags |= SCNPC_FLAG_NEEDS_WEAPON_THEM;
					}
					else if ( !Q_strncmp( pszNeedsWeapon, "BOTH", 4 ) ) 
					{
						sInteraction.iFlags |= SCNPC_FLAG_NEEDS_WEAPON_ME;
						sInteraction.iFlags |= SCNPC_FLAG_NEEDS_WEAPON_THEM;
					}
				}

				// Specific weapon types
				const char *pszWeaponName = pkvNode->GetString( "weapon_mine", NULL );
				if ( pszWeaponName )
				{
					sInteraction.iFlags |= SCNPC_FLAG_NEEDS_WEAPON_ME;
					sInteraction.iszMyWeapon = AllocPooledString( pszWeaponName );
				}
				pszWeaponName = pkvNode->GetString( "weapon_theirs", NULL );
				if ( pszWeaponName )
				{
					sInteraction.iFlags |= SCNPC_FLAG_NEEDS_WEAPON_THEM;
					sInteraction.iszTheirWeapon = AllocPooledString( pszWeaponName );
				}

				// Add it to the list
				AddScriptedNPCInteraction( &sInteraction );

				// Move to next interaction
				pkvNode = pkvNode->GetNextKey();
			}
		}
	}

	modelKeyValues->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::AddScriptedNPCInteraction( ScriptedNPCInteraction_t *pInteraction  )
{
	int nNewIndex = m_ScriptedInteractions.AddToTail();

	if ( ai_debug_dyninteractions.GetBool() )
	{
		Msg("%s(%s): Added dynamic interaction: %s\n", GetClassname(), GetDebugName(), STRING(pInteraction->iszInteractionName) );
	}

	// Copy the interaction over
	ScriptedNPCInteraction_t *pNewInt = &(m_ScriptedInteractions[nNewIndex]);
	memcpy( pNewInt, pInteraction, sizeof(ScriptedNPCInteraction_t) );

	// Calculate the local to world matrix
	m_ScriptedInteractions[nNewIndex].matDesiredLocalToWorld.SetupMatrixOrgAngles( pInteraction->vecRelativeOrigin, pInteraction->angRelativeAngles );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CAI_BaseNPC::GetScriptedNPCInteractionSequence( ScriptedNPCInteraction_t *pInteraction, int iPhase )
{
	if ( pInteraction->sPhases[iPhase].iActivity != ACT_INVALID )
	{
		int iSequence = SelectWeightedSequence( (Activity)pInteraction->sPhases[iPhase].iActivity );
		return GetSequenceName( iSequence );
	}

	if ( pInteraction->sPhases[iPhase].iszSequence != NULL_STRING )
		return STRING(pInteraction->sPhases[iPhase].iszSequence);

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::StartRunningInteraction( CAI_BaseNPC *pOtherNPC, bool bActive )
{
	m_hInteractionPartner = pOtherNPC;
	if ( bActive )
	{
		m_iInteractionState = NPCINT_RUNNING_ACTIVE;
	}
	else
	{
		m_iInteractionState = NPCINT_RUNNING_PARTNER;
	}
	m_bCannotDieDuringInteraction = true;

	// Force the NPC into an idle schedule so they don't move.
	// NOTE: We must set SCHED_IDLE_STAND directly, to prevent derived NPC
	// classes from translating the idle stand schedule away to do something bad.
	SetSchedule( GetSchedule(SCHED_IDLE_STAND) );

	// Prepare the NPC for the script. Setting this allows the scripted sequences
	// that we're about to create to immediately grab & use this NPC right away.
	// This prevents the NPC from being able to make any schedule decisions 
	// before the DSS gets underway.
	m_scriptState = SCRIPT_PLAYING;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::StartScriptedNPCInteraction( CAI_BaseNPC *pOtherNPC, ScriptedNPCInteraction_t *pInteraction, Vector vecOtherOrigin, QAngle angOtherAngles )
{
	variant_t emptyVariant;

 	StartRunningInteraction( pOtherNPC, true );
	if ( pOtherNPC )
	{
		pOtherNPC->StartRunningInteraction( this, false );

		//Msg("%s(%s) disabled collisions with %s(%s) at %0.2f\n", GetClassname(), GetDebugName(), pOtherNPC->GetClassName(), pOtherNPC->GetDebugName(), gpGlobals->curtime );
		PhysDisableEntityCollisions( this, pOtherNPC );
	}

	// Determine which sequences we're going to use
	const char *pszEntrySequence = GetScriptedNPCInteractionSequence( pInteraction, SNPCINT_ENTRY );
	const char *pszSequence = GetScriptedNPCInteractionSequence( pInteraction, SNPCINT_SEQUENCE );
	const char *pszExitSequence = GetScriptedNPCInteractionSequence( pInteraction, SNPCINT_EXIT );

	// Debug
	if ( ai_debug_dyninteractions.GetBool() )
	{
		if ( pOtherNPC )
		{
			Msg("%s(%s) starting dynamic interaction \"%s\" with %s(%s).\n", GetClassname(), GetDebugName(), STRING(pInteraction->iszInteractionName), pOtherNPC->GetClassname(), pOtherNPC->GetDebugName() );
			if ( pszEntrySequence )
			{
				Msg( " - Entry sequence: %s\n", pszEntrySequence );
			}
			Msg( " - Core sequence: %s\n", pszSequence );
			if ( pszExitSequence )
			{
				Msg( " - Exit sequence: %s\n", pszExitSequence );
			}
		}
	}

	// Create a scripted sequence name that's guaranteed to be unique
	char szSSName[256];
	if ( pOtherNPC )
	{
		Q_snprintf( szSSName, sizeof(szSSName), "dss_%s%d%s%d", GetDebugName(), entindex(), pOtherNPC->GetDebugName(), pOtherNPC->entindex() );
	}
	else
	{
		Q_snprintf( szSSName, sizeof(szSSName), "dss_%s%d", GetDebugName(), entindex() );
	}
  	string_t iszSSName = AllocPooledString(szSSName);

	// Setup next attempt
	pInteraction->flNextAttemptTime = gpGlobals->curtime + pInteraction->flDelay + RandomFloat(-2,2);

	// Spawn a scripted sequence for this NPC to play the interaction anim
   	CAI_ScriptedSequence *pMySequence = (CAI_ScriptedSequence*)CreateEntityByName( "scripted_sequence" );
	pMySequence->KeyValue( "m_iszEntry", pszEntrySequence );
	pMySequence->KeyValue( "m_iszPlay", pszSequence );
	pMySequence->KeyValue( "m_iszPostIdle", pszExitSequence );
	pMySequence->KeyValue( "m_fMoveTo", "5" );
	pMySequence->SetAbsOrigin( GetAbsOrigin() );

	QAngle angDesired = GetAbsAngles();
	angDesired[YAW] = m_flInteractionYaw;

	pMySequence->SetAbsAngles( angDesired );
	pMySequence->ForceSetTargetEntity( this, true );
	pMySequence->SetName( iszSSName );
 	pMySequence->AddSpawnFlags( SF_SCRIPT_NOINTERRUPT | SF_SCRIPT_HIGH_PRIORITY | SF_SCRIPT_OVERRIDESTATE );
	if ((pInteraction->iFlags & SCNPC_FLAG_DONT_TELEPORT_AT_END_ME) != 0)
	{
	 	pMySequence->AddSpawnFlags( SF_SCRIPT_DONT_TELEPORT_AT_END );
	}
	pMySequence->SetLoopActionSequence( (pInteraction->iFlags & SCNPC_FLAG_LOOP_IN_ACTION) != 0 );
	pMySequence->SetSynchPostIdles( true );
	if ( ai_debug_dyninteractions.GetBool() )
	{
		pMySequence->m_debugOverlays |= OVERLAY_TEXT_BIT | OVERLAY_PIVOT_BIT;
	}

	// Spawn the matching scripted sequence for the other NPC
	CAI_ScriptedSequence *pTheirSequence = NULL;
	if ( pOtherNPC )
	{
		pTheirSequence = (CAI_ScriptedSequence*)CreateEntityByName( "scripted_sequence" );
		pTheirSequence->KeyValue( "m_iszEntry", pszEntrySequence );
		pTheirSequence->KeyValue( "m_iszPlay", pszSequence );
		pTheirSequence->KeyValue( "m_iszPostIdle", pszExitSequence );
		pTheirSequence->KeyValue( "m_fMoveTo", "5" );
		pTheirSequence->SetAbsOrigin( vecOtherOrigin );
		pTheirSequence->SetAbsAngles( angOtherAngles );
		pTheirSequence->ForceSetTargetEntity( pOtherNPC, true );
		pTheirSequence->SetName( iszSSName );
		pTheirSequence->AddSpawnFlags( SF_SCRIPT_NOINTERRUPT | SF_SCRIPT_HIGH_PRIORITY | SF_SCRIPT_OVERRIDESTATE );
		if ((pInteraction->iFlags & SCNPC_FLAG_DONT_TELEPORT_AT_END_THEM) != 0) 
		{
			pTheirSequence->AddSpawnFlags( SF_SCRIPT_DONT_TELEPORT_AT_END );
		}
		pTheirSequence->SetLoopActionSequence( (pInteraction->iFlags & SCNPC_FLAG_LOOP_IN_ACTION) != 0 );
		pTheirSequence->SetSynchPostIdles( true );
 		if ( ai_debug_dyninteractions.GetBool() )
		{
			pTheirSequence->m_debugOverlays |= OVERLAY_TEXT_BIT | OVERLAY_PIVOT_BIT;
		}

		// Tell their sequence to keep their position relative to me
		pTheirSequence->SetupInteractionPosition( this, pInteraction->matDesiredLocalToWorld );
	}

	// Spawn both sequences at once
	pMySequence->Spawn();
	if ( pTheirSequence )
	{
		pTheirSequence->Spawn();
	}

	// Call activate on both sequences at once
	pMySequence->Activate();
	if ( pTheirSequence )
	{
		pTheirSequence->Activate();
	}

	// Setup the outputs for both sequences. The first kills them both when it finishes
	pMySequence->KeyValue( "OnCancelFailedSequence", UTIL_VarArgs("%s,Kill,,0,-1", szSSName ) );
	if ( pszExitSequence )
	{
		pMySequence->KeyValue( "OnPostIdleEndSequence", UTIL_VarArgs("%s,Kill,,0,-1", szSSName ) );
		if ( pTheirSequence )
		{
			pTheirSequence->KeyValue( "OnPostIdleEndSequence", UTIL_VarArgs("%s,Kill,,0,-1", szSSName ) );
		}
	}
	else
	{
		pMySequence->KeyValue( "OnEndSequence", UTIL_VarArgs("%s,Kill,,0,-1", szSSName ) );
		if ( pTheirSequence )
		{
			pTheirSequence->KeyValue( "OnEndSequence", UTIL_VarArgs("%s,Kill,,0,-1", szSSName ) );
		}
	}
	if ( pTheirSequence )
	{
		pTheirSequence->KeyValue( "OnCancelFailedSequence", UTIL_VarArgs("%s,Kill,,0,-1", szSSName ) );
	}

	// Tell both sequences to start
	pMySequence->AcceptInput( "BeginSequence", this, this, emptyVariant, 0 );
	if ( pTheirSequence )
	{
		pTheirSequence->AcceptInput( "BeginSequence", this, this, emptyVariant, 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::CanRunAScriptedNPCInteraction( bool bForced )
{
  	if ( m_NPCState != NPC_STATE_IDLE && m_NPCState != NPC_STATE_ALERT && m_NPCState != NPC_STATE_COMBAT  )
 		return false;

	if ( !IsAlive() )
		return false;

	if ( IsOnFire() )
		return false;

	if ( IsCrouching() )
		return false;

	// Not while running scripted sequences
	if ( m_hCine )
		return false;

	if ( bForced )
	{
		if ( !m_hForcedInteractionPartner )
			return false;
	}
	else
	{
		if ( m_hForcedInteractionPartner || m_hInteractionPartner )
			return false;
		if ( IsInAScript() || !HasCondition(COND_IN_PVS) )
			return false;
		if ( HasCondition(COND_HEAR_DANGER) || HasCondition(COND_HEAR_MOVE_AWAY) )
			return false;

		// Default AI prevents interactions while melee attacking, but not ranged attacking
		if ( IsCurSchedule( SCHED_MELEE_ATTACK1 ) || IsCurSchedule( SCHED_MELEE_ATTACK2 ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::CheckForScriptedNPCInteractions( void )
{
	// Are we being forced to interact with another NPC? If so, do that
	if ( m_hForcedInteractionPartner )
	{
		CheckForcedNPCInteractions();
		return;
	}

	// Otherwise, see if we can interaction with our enemy
	if ( !m_ScriptedInteractions.Count() || !GetEnemy() )
		return;

	CAI_BaseNPC *pNPC = GetEnemy()->MyNPCPointer();

	if( !pNPC )
		return;

	// Recalculate interaction capability whenever we switch enemies
  	if ( m_hLastInteractionTestTarget != GetEnemy() )
	{
		m_hLastInteractionTestTarget = GetEnemy();

		CalculateValidEnemyInteractions();
	}

	// First, make sure I'm in a state where I can do this
	if ( !CanRunAScriptedNPCInteraction() )
		return;
	if ( pNPC && !pNPC->CanRunAScriptedNPCInteraction() )
		return;

	for ( int i = 0; i < m_ScriptedInteractions.Count(); i++ )
	{
		ScriptedNPCInteraction_t *pInteraction = &m_ScriptedInteractions[i];

		if ( !pInteraction->bValidOnCurrentEnemy )
			continue;

		if ( pInteraction->flNextAttemptTime > gpGlobals->curtime )
			continue;

		Vector vecOrigin;
		QAngle angAngles;
		if ( InteractionCouldStart( pNPC, pInteraction, vecOrigin, angAngles ) )
		{
			m_iInteractionPlaying = i;
			StartScriptedNPCInteraction( pNPC, pInteraction, vecOrigin, angAngles );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Calculate all the valid dynamic interactions we can perform with our current enemy
//-----------------------------------------------------------------------------
void CAI_BaseNPC::CalculateValidEnemyInteractions( void )
{
	CAI_BaseNPC *pNPC = GetEnemy()->MyNPCPointer();
	if ( !pNPC )
		return;

	bool bDebug = (m_debugOverlays & OVERLAY_NPC_SELECTED_BIT && ai_debug_dyninteractions.GetBool());
	if ( bDebug )
	{
		Msg("%s(%s): Computing valid interactions with %s(%s)\n", GetClassname(), GetDebugName(), pNPC->GetClassname(), pNPC->GetDebugName() );
	}

	bool bFound = false;
	for ( int i = 0; i < m_ScriptedInteractions.Count(); i++ )
	{
		ScriptedNPCInteraction_t *pInteraction = &m_ScriptedInteractions[i];
		pInteraction->bValidOnCurrentEnemy = false;

		// If the trigger method of the interaction isn't the one we're after, we're done
		if ( pInteraction->iTriggerMethod != SNPCINT_AUTOMATIC_IN_COMBAT )
			continue;

		if ( !pNPC->GetModelPtr() )
			continue;

		// If we have a damage filter that prevents us hurting the enemy,
		// don't interact with him, since most interactions kill the enemy.
		// Create a fake damage info to test it with.
		CTakeDamageInfo tempinfo( this, this, vec3_origin, vec3_origin, 1.0, DMG_BULLET );
		if ( !pNPC->PassesDamageFilter( tempinfo ) )
			continue;

		// Check the weapon requirements for the interaction
		if ( pInteraction->iFlags & SCNPC_FLAG_NEEDS_WEAPON_ME )
		{
			if ( !GetActiveWeapon())
				continue;

			// Check the specific weapon type
			if ( pInteraction->iszMyWeapon != NULL_STRING && GetActiveWeapon()->m_iClassname != pInteraction->iszMyWeapon )
				continue;
		}
		if ( pInteraction->iFlags & SCNPC_FLAG_NEEDS_WEAPON_THEM )
		{
			if ( !pNPC->GetActiveWeapon() )
				continue;

			// Check the specific weapon type
			if ( pInteraction->iszTheirWeapon != NULL_STRING && pNPC->GetActiveWeapon()->m_iClassname != pInteraction->iszTheirWeapon )
				continue;
		}

		// Script needs the other NPC, so make sure they're not dead
		if ( !pNPC->IsAlive() )
			continue;

		// Use sequence? or activity?
		if ( pInteraction->sPhases[SNPCINT_SEQUENCE].iActivity != ACT_INVALID )
		{
			// Resolve the activity to a sequence, and make sure our enemy has it
			const char *pszSequence = GetScriptedNPCInteractionSequence( pInteraction, SNPCINT_SEQUENCE );
			if ( !pszSequence )
				continue;
			if ( pNPC->LookupSequence( pszSequence ) == -1 )
				continue;
		}
		else
		{
			if ( pNPC->LookupSequence( STRING(pInteraction->sPhases[SNPCINT_SEQUENCE].iszSequence) ) == -1 )
				continue;
		}

		pInteraction->bValidOnCurrentEnemy = true;
		bFound = true;

		if ( bDebug )
		{
			Msg("   Found: %s\n", STRING(pInteraction->iszInteractionName) );
		}
	}

	if ( bDebug && !bFound )
	{
		Msg("   No valid interactions found.\n");
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::CheckForcedNPCInteractions( void )
{
	// If we don't have an interaction, we're waiting for our partner to start it. Do nothing.
	if ( m_iInteractionPlaying == NPCINT_NONE )
		return;

	CAI_BaseNPC *pNPC = m_hForcedInteractionPartner->MyNPCPointer();
	bool bAbort = false;

	// First, make sure both NPCs are able to do this
 	if ( !CanRunAScriptedNPCInteraction( true ) || !pNPC->CanRunAScriptedNPCInteraction( true ) )
	{
		// If we were still moving to our target, abort.
		if ( m_iInteractionState == NPCINT_MOVING_TO_MARK )
		{
			bAbort = true;
		}
		else
		{
			return;
		}
	}

	// Check to see if we can start our interaction. If we can, dance.
	Vector vecOrigin;
	QAngle angAngles;

	ScriptedNPCInteraction_t *pInteraction = &m_ScriptedInteractions[m_iInteractionPlaying];

	if ( !bAbort )
	{
		if ( !InteractionCouldStart( pNPC, pInteraction, vecOrigin, angAngles ) )
		{
			if ( ( gpGlobals->curtime > m_flForcedInteractionTimeout ) && ( m_iInteractionState == NPCINT_MOVING_TO_MARK ) )
			{
				bAbort = true;
			}
			else
			{
				return;
			}
		}
	}
	
	if ( bAbort )
	{
		if ( m_hForcedInteractionPartner )
		{
			// We've aborted a forced interaction. Let the mapmaker know.
			m_OnForcedInteractionAborted.FireOutput( this, this );
		}

		CleanupForcedInteraction();
		pNPC->CleanupForcedInteraction();

		return;
	}

	StartScriptedNPCInteraction( pNPC, pInteraction, vecOrigin, angAngles );
	m_OnForcedInteractionStarted.FireOutput( this, this );
}


//-----------------------------------------------------------------------------
// Returns whether two NPCs can fit at each other's origin.
// Kinda like that movie with Eddie Murphy and Dan Akroyd.
//-----------------------------------------------------------------------------
bool CanNPCsTradePlaces( CAI_BaseNPC *pNPC1, CAI_BaseNPC *pNPC2, bool bDebug )
{
	bool bTest1At2 = true;
	bool bTest2At1 = true;

	if ( ( pNPC1->GetHullMins().x <= pNPC2->GetHullMins().x ) &&
		 ( pNPC1->GetHullMins().y <= pNPC2->GetHullMins().y ) &&
		 ( pNPC1->GetHullMins().z <= pNPC2->GetHullMins().z ) &&
		 ( pNPC1->GetHullMaxs().x >= pNPC2->GetHullMaxs().x ) &&
		 ( pNPC1->GetHullMaxs().y >= pNPC2->GetHullMaxs().y ) &&
		 ( pNPC1->GetHullMaxs().z >= pNPC2->GetHullMaxs().z ) )
	{
		// 1 bigger than 2 in all axes, skip 2 in 1 test
		bTest2At1 = false;
	}
	else if ( ( pNPC2->GetHullMins().x <= pNPC1->GetHullMins().x ) &&
			  ( pNPC2->GetHullMins().y <= pNPC1->GetHullMins().y ) &&
			  ( pNPC2->GetHullMins().z <= pNPC1->GetHullMins().z ) &&
			  ( pNPC2->GetHullMaxs().x >= pNPC1->GetHullMaxs().x ) &&
			  ( pNPC2->GetHullMaxs().y >= pNPC1->GetHullMaxs().y ) &&
			  ( pNPC2->GetHullMaxs().z >= pNPC1->GetHullMaxs().z ) )
	{
		// 2 bigger than 1 in all axes, skip 1 in 2 test
		bTest1At2 = false;
	}

	trace_t tr;
	CTraceFilterSkipTwoEntities traceFilter( pNPC1, pNPC2, COLLISION_GROUP_NONE );

	if ( bTest1At2 )
	{
		UTIL_TraceHull( pNPC2->GetAbsOrigin(), pNPC2->GetAbsOrigin(), pNPC1->GetHullMins(), pNPC1->GetHullMaxs(), MASK_SOLID, &traceFilter, &tr );
		if ( tr.startsolid )
		{
			if ( bDebug )
			{
				NDebugOverlay::Box(  pNPC2->GetAbsOrigin(), pNPC1->GetHullMins(), pNPC1->GetHullMaxs(), 255,0,0, true, 1.0 );
			}
			return false;
		}
	}

	if ( bTest2At1 )
	{
		UTIL_TraceHull( pNPC1->GetAbsOrigin(), pNPC1->GetAbsOrigin(), pNPC2->GetHullMins(), pNPC2->GetHullMaxs(), MASK_SOLID, &traceFilter, &tr );
		if ( tr.startsolid )
		{
			if ( bDebug )
			{
				NDebugOverlay::Box(  pNPC1->GetAbsOrigin(), pNPC2->GetHullMins(), pNPC2->GetHullMaxs(), 255,0,0, true, 1.0 );
			}
			return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::InteractionCouldStart( CAI_BaseNPC *pOtherNPC, ScriptedNPCInteraction_t *pInteraction, Vector &vecOrigin, QAngle &angAngles )
{
	// Get a matrix that'll convert from my local interaction space to world space
	VMatrix matMeToWorld, matLocalToWorld;
	QAngle angMyCurrent = GetAbsAngles();
	angMyCurrent[YAW] = m_flInteractionYaw;
	matMeToWorld.SetupMatrixOrgAngles( GetAbsOrigin(), angMyCurrent );
	MatrixMultiply( matMeToWorld, pInteraction->matDesiredLocalToWorld, matLocalToWorld );

	// Get the desired NPC position in worldspace
	vecOrigin = matLocalToWorld.GetTranslation();
	MatrixToAngles( matLocalToWorld, angAngles );

	bool bDebug = ai_debug_dyninteractions.GetBool();
	if ( bDebug )
	{
		NDebugOverlay::Axis( vecOrigin, angAngles, 20, true, 0.1 );
	}

	// Determine whether or not the enemy is on the target
	float flDistSqr = (vecOrigin - pOtherNPC->GetAbsOrigin()).LengthSqr();
	if ( flDistSqr > pInteraction->flDistSqr )
	{
		if ( bDebug )
		{
			if ( m_debugOverlays & OVERLAY_NPC_SELECTED_BIT || pOtherNPC->m_debugOverlays & OVERLAY_NPC_SELECTED_BIT )
			{
				if ( ai_debug_dyninteractions.GetFloat() == 2 )
				{
					Msg("   %s distsqr: %0.2f (%0.2f %0.2f %0.2f), desired: <%0.2f (%0.2f %0.2f %0.2f)\n", GetDebugName(), flDistSqr,
						pOtherNPC->GetAbsOrigin().x, pOtherNPC->GetAbsOrigin().y, pOtherNPC->GetAbsOrigin().z, pInteraction->flDistSqr, vecOrigin.x, vecOrigin.y, vecOrigin.z );
				}
			}
		}
		return false;
	}

 	if ( bDebug )
	{
		Msg("DYNINT: (%s) testing interaction \"%s\"\n", GetDebugName(), STRING(pInteraction->iszInteractionName) );
		Msg("   %s is at: %0.2f %0.2f %0.2f\n", GetDebugName(), GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
		Msg("   %s distsqr: %0.2f (%0.2f %0.2f %0.2f), desired: (%0.2f %0.2f %0.2f)\n", GetDebugName(), flDistSqr,
			pOtherNPC->GetAbsOrigin().x, pOtherNPC->GetAbsOrigin().y, pOtherNPC->GetAbsOrigin().z, vecOrigin.x, vecOrigin.y, vecOrigin.z );

		if ( pOtherNPC )
		{
			float flOtherSpeed = pOtherNPC->GetSequenceGroundSpeed( pOtherNPC->GetSequence() );
			Msg("   %s Speed: %.2f\n", pOtherNPC->GetSequenceName( pOtherNPC->GetSequence() ), flOtherSpeed);
		}
	}

	// Angle check, if we're supposed to
	if ( pInteraction->iFlags & SCNPC_FLAG_TEST_OTHER_ANGLES )
	{
		QAngle angEnemyAngles = pOtherNPC->GetAbsAngles();
		bool bMatches = true;
		for ( int ang = 0; ang < 3; ang++ )
		{
			float flAngDiff = AngleDiff( angEnemyAngles[ang], angAngles[ang] );
			if ( fabs(flAngDiff) > DSS_MAX_ANGLE_DIFF )
			{
				bMatches = false;
				break;
			}
		}
		if ( !bMatches )
			return false;

		if ( bDebug )
		{
			Msg("   %s angle matched: (%0.2f %0.2f %0.2f), desired (%0.2f, %0.2f, %0.2f)\n", GetDebugName(),
				anglemod(angEnemyAngles.x), anglemod(angEnemyAngles.y), anglemod(angEnemyAngles.z), anglemod(angAngles.x), anglemod(angAngles.y), anglemod(angAngles.z) );
		}
	}

	// TODO: Velocity check, if we're supposed to
	if ( pInteraction->iFlags & SCNPC_FLAG_TEST_OTHER_VELOCITY )
	{

	}

	// Valid so far. Now check to make sure there's nothing in the way.
	// This isn't a very good method of checking, but it's cheap and rules out the problems we're seeing so far.
	// If we start getting interactions that start a fair distance apart, we're going to need to do more work here.
 	trace_t tr;
	UTIL_TraceLine( EyePosition(), pOtherNPC->EyePosition(), GetAITraceMask(), this, COLLISION_GROUP_NONE, &tr);
	if ( tr.fraction != 1.0 && tr.m_pEnt != pOtherNPC )
	{
		if ( bDebug )
		{
			Msg( "   %s Interaction was blocked.\n", GetDebugName() );
			NDebugOverlay::Line( tr.startpos, tr.endpos, 0,255,0, true, 1.0 );
			NDebugOverlay::Line( pOtherNPC->EyePosition(), tr.endpos, 255,0,0, true, 1.0 );
		}
		return false;
	}

	if ( bDebug )
	{
		NDebugOverlay::Line( tr.startpos, tr.endpos, 0,255,0, true, 1.0 );
	}

	// Do a knee-level trace to find low physics objects
	Vector vecMyKnee, vecOtherKnee;
	CollisionProp()->NormalizedToWorldSpace( Vector(0,0,0.25f), &vecMyKnee );
	pOtherNPC->CollisionProp()->NormalizedToWorldSpace( Vector(0,0,0.25f), &vecOtherKnee );
	UTIL_TraceLine( vecMyKnee, vecOtherKnee, GetAITraceMask(), this, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction != 1.0 && tr.m_pEnt != pOtherNPC )
	{
		if ( bDebug )
		{
			Msg( "   %s Interaction was blocked.\n", GetDebugName() );
			NDebugOverlay::Line( tr.startpos, tr.endpos, 0,255,0, true, 1.0 );
			NDebugOverlay::Line( vecOtherKnee, tr.endpos, 255,0,0, true, 1.0 );
		}
		return false;
	}

	if ( bDebug )
	{
		NDebugOverlay::Line( tr.startpos, tr.endpos, 0,255,0, true, 1.0 );
	}

	// Finally, make sure the NPC can actually fit at the interaction position
	// This solves problems with NPCs who are a few units or so above the 
	// interaction point, and would sink into the ground when playing the anim.
	CTraceFilterSkipTwoEntities traceFilter( pOtherNPC, this, COLLISION_GROUP_NONE );
	UTIL_TraceHull( vecOrigin, vecOrigin, pOtherNPC->GetHullMins(), pOtherNPC->GetHullMaxs(), MASK_SOLID, &traceFilter, &tr );
	if ( tr.startsolid )
	{
		if ( bDebug )
		{
			NDebugOverlay::Box( vecOrigin, pOtherNPC->GetHullMins(), pOtherNPC->GetHullMaxs(), 255,0,0, true, 1.0 );
		}
		return false;
	}

	// If the NPCs are swapping places during this interaction, make sure they can fit at each
	// others' origins before allowing the interaction.
	if ( !CanNPCsTradePlaces( this, pOtherNPC, bDebug ) )
	{
		return false;
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this NPC cannot die because it's in an interaction
//			and the flag has been set by the animation.
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::HasInteractionCantDie( void )
{
	return ( m_bCannotDieDuringInteraction && IsRunningDynamicInteraction() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputForceInteractionWithNPC( inputdata_t &inputdata )
{
	// Get the interaction name & target
	char parseString[255];
	Q_strncpy(parseString, inputdata.value.String(), sizeof(parseString));

	// First, the target's name
	char *pszParam = strtok(parseString," ");
	if ( !pszParam || !pszParam[0] )
	{
		Warning("%s(%s) received ForceInteractionWithNPC input with bad parameters: %s\nFormat should be: ForceInteractionWithNPC <target NPC> <interaction name>\n", GetClassname(), GetDebugName(), inputdata.value.String() );
		return;
	}
	// Find the target
 	CBaseEntity *pTarget = FindNamedEntity( pszParam );
	if ( !pTarget )
	{
		Warning("%s(%s) received ForceInteractionWithNPC input, but couldn't find entity named: %s\n", GetClassname(), GetDebugName(), pszParam );
		return;
	}
	CAI_BaseNPC *pNPC = pTarget->MyNPCPointer();
	if ( !pNPC || !pNPC->GetModelPtr() )
	{
		Warning("%s(%s) received ForceInteractionWithNPC input, but entity named %s cannot run dynamic interactions.\n", GetClassname(), GetDebugName(), pszParam );
		return;
	}

	// Second, the interaction name
	pszParam = strtok(NULL," ");
	if ( !pszParam || !pszParam[0] )
	{
		Warning("%s(%s) received ForceInteractionWithNPC input with bad parameters: %s\nFormat should be: ForceInteractionWithNPC <target NPC> <interaction name>\n", GetClassname(), GetDebugName(), inputdata.value.String() );
		return;
	}

	// Find the interaction from the name, and ensure it's one that the target NPC can play
	int iInteraction = -1;
	for ( int i = 0; i < m_ScriptedInteractions.Count(); i++ )
	{
		if ( Q_strncmp( pszParam, STRING(m_ScriptedInteractions[i].iszInteractionName), strlen(pszParam) ) )
			continue;

		// Use sequence? or activity?
		if ( m_ScriptedInteractions[i].sPhases[SNPCINT_SEQUENCE].iActivity != ACT_INVALID )
		{
			if ( !pNPC->HaveSequenceForActivity( (Activity)m_ScriptedInteractions[i].sPhases[SNPCINT_SEQUENCE].iActivity ) )
			{
				// Other NPC may have all the matching sequences, but just without the activity specified.
				// Lets find a single sequence for us, and ensure they have a matching one.
				int iMySeq = SelectWeightedSequence( (Activity)m_ScriptedInteractions[i].sPhases[SNPCINT_SEQUENCE].iActivity );
				if ( pNPC->LookupSequence( GetSequenceName(iMySeq) ) == -1 )
					continue;
			}
		}
		else
		{
			if ( pNPC->LookupSequence( STRING(m_ScriptedInteractions[i].sPhases[SNPCINT_SEQUENCE].iszSequence) ) == -1 )
				continue;
		}

		iInteraction = i;
		break;
	}

 	if ( iInteraction == -1 )
	{
		Warning("%s(%s) received ForceInteractionWithNPC input, but couldn't find an interaction named %s that entity named %s could run.\n", GetClassname(), GetDebugName(), pszParam, pNPC->GetDebugName() );
		return;
	}

	// Found both pieces of data, lets dance.
	StartForcedInteraction( pNPC, iInteraction );
	pNPC->StartForcedInteraction( this, NPCINT_NONE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::StartForcedInteraction( CAI_BaseNPC *pNPC, int iInteraction )
{
	m_hForcedInteractionPartner = pNPC;
	ClearSchedule( "Starting a forced interaction" );

	m_flForcedInteractionTimeout = gpGlobals->curtime + 8.0f;
	m_iInteractionPlaying = iInteraction;
	m_iInteractionState = NPCINT_MOVING_TO_MARK;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::CleanupForcedInteraction( void )
{
	m_hForcedInteractionPartner = NULL;
	m_iInteractionPlaying = NPCINT_NONE;
	m_iInteractionState = NPCINT_NOT_RUNNING;
	m_flForcedInteractionTimeout = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Calculate a position to move to so that I can interact with my
//			target NPC. 
//
//			FIXME: THIS ONLY WORKS FOR INTERACTIONS THAT REQUIRE THE TARGET
//				  NPC TO BE SOME DISTANCE DIRECTLY IN FRONT OF ME.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::CalculateForcedInteractionPosition( void )
{
 	if ( m_iInteractionPlaying == NPCINT_NONE )
		return;

   	ScriptedNPCInteraction_t *pInteraction = GetRunningDynamicInteraction();

	// Pretend I was facing the target, and extrapolate from that the position I should be at
	Vector vecToTarget = m_hForcedInteractionPartner->GetAbsOrigin() - GetAbsOrigin();
	VectorNormalize( vecToTarget );
	QAngle angToTarget;
	VectorAngles( vecToTarget, angToTarget );

	// Get the desired position in worldspace, relative to the target
	VMatrix matMeToWorld, matLocalToWorld;
 	matMeToWorld.SetupMatrixOrgAngles( GetAbsOrigin(), angToTarget );
	MatrixMultiply( matMeToWorld, pInteraction->matDesiredLocalToWorld, matLocalToWorld );

 	Vector vecOrigin = GetAbsOrigin() - matLocalToWorld.GetTranslation();
 	m_vecForcedWorldPosition = m_hForcedInteractionPartner->GetAbsOrigin() + vecOrigin;

	//NDebugOverlay::Axis( m_vecForcedWorldPosition, angToTarget, 20, true, 3.0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::PlayerHasIlluminatedNPC( CBasePlayer *pPlayer, float flDot )
{
#ifdef HL2_EPISODIC
	if ( IsActiveDynamicInteraction() )
	{
		ScriptedNPCInteraction_t *pInteraction = GetRunningDynamicInteraction();
		if ( pInteraction->iLoopBreakTriggerMethod & SNPCINT_LOOPBREAK_ON_FLASHLIGHT_ILLUM )
		{
			// Only do this in alyx darkness mode
			if ( TFGameRules()->IsAlyxInDarknessMode() )
			{
				// Can only break when we're in the action anim
				if ( m_hCine->IsPlayingAction() )
				{
					m_hCine->StopActionLoop( true );
				}
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::ModifyOrAppendCriteria( AI_CriteriaSet& set )
{
	BaseClass::ModifyOrAppendCriteria( set );

	// Append time since seen player
	if ( m_flLastSawPlayerTime )
	{
		set.AppendCriteria( "timesinceseenplayer", UTIL_VarArgs( "%f", gpGlobals->curtime - m_flLastSawPlayerTime ) );
	}
	else
	{
		set.AppendCriteria( "timesinceseenplayer", "-1" );
	}

	// Append distance to my enemy
	if ( GetEnemy() )
	{
		set.AppendCriteria( "distancetoenemy", UTIL_VarArgs( "%f", EnemyDistance(GetEnemy()) ) );
	}
	else
	{
		set.AppendCriteria( "distancetoenemy", "-1" );
	}
}

//-----------------------------------------------------------------------------
// If I were crouching at my current location, could I shoot this target?
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::CouldShootIfCrouching( CBaseEntity *pTarget )
{
	bool bWasStanding = !IsCrouching();
	Crouch();

	Vector vecTarget;
	if (GetActiveWeapon())
	{
		vecTarget = pTarget->BodyTarget( GetActiveWeapon()->GetLocalOrigin() );
	}
	else 
	{
		vecTarget = pTarget->BodyTarget( GetLocalOrigin() );
	}

	bool bResult = WeaponLOSCondition( GetLocalOrigin(), vecTarget, false );

	if ( bWasStanding )
	{
		Stand();
	}

	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::IsCrouchedActivity( Activity activity )
{
	Activity realActivity = TranslateActivity(activity);

	switch ( realActivity )
	{
		case ACT_RELOAD_LOW:
		case ACT_COVER_LOW:
		case ACT_COVER_PISTOL_LOW:
		case ACT_COVER_SMG1_LOW:
		case ACT_RELOAD_SMG1_LOW:
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Get shoot position of BCC at an arbitrary position
//-----------------------------------------------------------------------------
Vector CAI_BaseNPC::Weapon_ShootPosition( void )
{
	Vector right;
	GetVectors( NULL, &right, NULL );

	bool bStanding = !IsCrouching();
	if ( bStanding && (CapabilitiesGet() & bits_CAP_DUCK) )
	{
		if ( IsCrouchedActivity( GetActivity() ) )
		{
			bStanding = false;
		}
	}

	if  ( !bStanding )
		return (GetAbsOrigin() + GetCrouchGunOffset() + right * 8);

	return BaseClass::Weapon_ShootPosition();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::ShouldProbeCollideAgainstEntity( CBaseEntity *pEntity )
{
	if ( pEntity->GetMoveType() == MOVETYPE_VPHYSICS )
	{
		if ( ai_test_moveprobe_ignoresmall.GetBool() && IsNavigationUrgent() )
		{
			IPhysicsObject *pPhysics = pEntity->VPhysicsGetObject();

			if ( pPhysics->IsMoveable() && pPhysics->GetMass() < 40.0 )
				return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::Crouch( void )
{ 
	m_bIsCrouching = true; 
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::IsCrouching( void )
{
	return ( (CapabilitiesGet() & bits_CAP_DUCK) && m_bIsCrouching );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::Stand( void )
{ 
	if ( m_bForceCrouch )
		return false;

	m_bIsCrouching = false; 
	DesireStand(); 
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::DesireCrouch( void )		
{ 
	m_bCrouchDesired = true; 
}

bool CAI_BaseNPC::IsInChoreo() const
{
	return m_bInChoreo;
}

#ifdef TF_CLASSIC
void CAI_BaseNPC::ChangeTeam( int iTeamNum )
{
	CTFTeam *pTeam = ( CTFTeam * )GetGlobalTeam( iTeamNum );
	CTFTeam *pExisting = ( CTFTeam * )GetTeam();

	if ( !pTeam )
	{
		//Warning( "CAI_BaseNPC::ChangeTeam( %d ) - invalid team index.\n", iTeamNum );
		// Just change team number.
		BaseClass::ChangeTeam( iTeamNum );
		return;
	}

	// Already on this team
	if ( GetTeamNumber() == iTeamNum )
		return;

	// Remove him from his current team
	if ( pExisting )
	{
		pExisting->RemoveNPC( this );
	}

	// Are we being added to a team?
	if ( pTeam )
	{
		pTeam->AddNPC( this );
	}

	BaseClass::ChangeTeam( iTeamNum );
}

//-----------------------------------------------------------------------------
// Purpose: Send out npc_death event.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::DeathNotice( const CTakeDamageInfo &info )
{
	// Bullseyes shouldn't send death notices.
	if ( Classify() == CLASS_BULLSEYE )
		return;
/* // moved to Event_Killed.
	int killer_index = 0;

	// Find the killer & the scorer
	CAI_BaseNPC *pVictim = this;
	CBaseEntity *pInflictor = info.GetInflictor();
	CBaseEntity *pKiller = info.GetAttacker();
	CBasePlayer *pScorer = TFGameRules()->GetDeathScorer( pKiller, pInflictor, pVictim );
	CBaseEntity *pAssister = TFGameRules()->GetAssister( pVictim, pKiller, pInflictor );

	// Work out what killed the player, and send a message to all clients about it
	const char *killer_weapon_name = TFGameRules()->GetKillingWeaponName( info, NULL );

	if ( pScorer )	// Is the killer a client?
	{
		killer_index = pScorer->entindex();
	}
	else if ( pKiller && pKiller->IsNPC() )
	{
		killer_index = pKiller->entindex();
	}

	IGameEvent * event = gameeventmanager->CreateEvent( "npc_death" );

	if ( event )
	{
		event->SetInt( "victim_index", pVictim->entindex() );
		event->SetString( "victim_name", pVictim->GetClassname() );
		event->SetInt( "victim_team", pVictim->GetTeamNumber() );
		event->SetInt( "attacker_index", killer_index );
		event->SetString( "attacker_name", pKiller ? pKiller->GetClassname() : NULL );
		event->SetInt( "attacker_team", pKiller ? pKiller->GetTeamNumber() : 0 );
		event->SetInt( "assister_index", pAssister ? pAssister->entindex() : -1 );
		event->SetString( "assister_name", pAssister ? pAssister->GetClassname() : NULL );
		event->SetInt( "assister_team", pAssister ? pAssister->GetTeamNumber() : 0 );
		event->SetString( "weapon", killer_weapon_name );
		event->SetInt( "damagebits", info.GetDamageType() );
		event->SetInt( "customkill", info.GetDamageCustom() );
		event->SetInt( "priority", 7 );	// HLTV event priority, not transmitted

		gameeventmanager->FireEvent( event );
	}
*/ // moved to Event_Killed.
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CAI_BaseNPC::TakeHealth( float flHealth, int bitsDamageType )
{
	int iResult = false;

	// If the bit's set, add over the max health
	if ( bitsDamageType & DMG_IGNORE_MAXHEALTH )
	{
		//int iTimeBasedDamage = g_pGameRules->Damage_GetTimeBased();
		//m_bitsDamageType &= ~(bitsDamageType & ~iTimeBasedDamage);
		m_iHealth += flHealth;
		iResult = (int)flHealth;
	}
	else
	{
		float flHealthToAdd = flHealth;
		float flMaxHealth = GetMaxHealth();
		
		// don't want to add more than we're allowed to have
		if ( flHealthToAdd > flMaxHealth - m_iHealth )
		{
			flHealthToAdd = flMaxHealth - m_iHealth;
		}

		if ( flHealthToAdd <= 0 )
		{
			iResult = 0;
		}
		else
		{
			iResult = BaseClass::TakeHealth( flHealthToAdd, bitsDamageType );
		}
	}

	return iResult;
}

//-----------------------------------------------------------------------------
// Purpose: Heal NPCs.
// pPlayer is person who healed us
//-----------------------------------------------------------------------------
void CAI_BaseNPC::Heal( CTFPlayer *pPlayer, float flAmount, bool bDispenserHeal /* = false */ )
{
	Assert( FindHealerIndex(pPlayer) == m_aHealers.InvalidIndex() );

	healers_t newHealer;
	newHealer.pPlayer = pPlayer;
	newHealer.flAmount = flAmount;
	newHealer.bDispenserHeal = bDispenserHeal;
	newHealer.iRecentAmount = 0;
	newHealer.flNextNofityTime = gpGlobals->curtime + 1.0f;
	m_aHealers.AddToTail( newHealer );

	AddCond( TF_COND_HEALTH_BUFF, PERMANENT_CONDITION );

	RecalculateChargeEffects();

	m_nNumHealers = m_aHealers.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Heal NPCs.
// pPlayer is person who healed us
//-----------------------------------------------------------------------------
void CAI_BaseNPC::StopHealing( CTFPlayer *pPlayer )
{
	int iIndex = FindHealerIndex( pPlayer );
	if ( iIndex == m_aHealers.InvalidIndex() )
		return;

	m_aHealers.Remove( iIndex );

	if ( !m_aHealers.Count() )
	{
		RemoveCond( TF_COND_HEALTH_BUFF );
	}

	RecalculateChargeEffects();

	m_nNumHealers = m_aHealers.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
medigun_charge_types CAI_BaseNPC::GetChargeEffectBeingProvided( CTFPlayer *pPlayer )
{
	if ( !pPlayer->IsPlayerClass( TF_CLASS_MEDIC ) )
		return TF_CHARGE_NONE;

	CTFWeaponBase *pWpn = pPlayer->GetActiveTFWeapon();
	if ( !pWpn )
		return TF_CHARGE_NONE;

	CWeaponMedigun *pMedigun = dynamic_cast <CWeaponMedigun*>( pWpn );
	if ( pMedigun && pMedigun->IsReleasingCharge() )
		return pMedigun->GetChargeType();

	return TF_CHARGE_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::RecalculateChargeEffects( bool bInstantRemove )
{
	bool bShouldCharge[TF_CHARGE_COUNT] = { };
	CTFPlayer *pProviders[TF_CHARGE_COUNT] = { };

	// Check players healing us.
	for ( int i = 0; i < m_aHealers.Count(); i++ )
	{
		CTFPlayer *pPlayer = ToTFPlayer( m_aHealers[i].pPlayer );
		if ( !pPlayer )
			continue;

		medigun_charge_types chargeType = GetChargeEffectBeingProvided( pPlayer );

		if ( chargeType != TF_CHARGE_NONE )
		{
			bShouldCharge[chargeType] = true;
			pProviders[chargeType] = pPlayer;
		}
	}
	
	for ( int i = 0; i < TF_CHARGE_COUNT; i++ )
	{
		float flRemoveTime = ( i == TF_CHARGE_INVULNERABLE ) ? tf_invuln_time.GetFloat() : 0.0f;
		SetChargeEffect( (medigun_charge_types)i, bShouldCharge[i], bInstantRemove, g_MedigunEffects[i], flRemoveTime, pProviders[i] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::SetChargeEffect( medigun_charge_types chargeType, bool bShouldCharge, bool bInstantRemove, const MedigunEffects_t &chargeEffect, float flRemoveTime, CTFPlayer *pProvider )
{
	if ( InCond( chargeEffect.condition_enable ) == bShouldCharge )
	{
		if ( bShouldCharge && m_flChargeOffTime[chargeType] != 0.0f )
		{
			m_flChargeOffTime[chargeType] = 0.0f;

			if ( chargeEffect.condition_disable != TF_COND_LAST )
				RemoveCond( chargeEffect.condition_disable );
		}
		return;
	}

	if ( bShouldCharge )
	{
		Assert( chargeType != TF_CHARGE_INVULNERABLE );

		if ( m_flChargeOffTime[chargeType] != 0.0f )
		{
			StopSound( chargeEffect.sound_disable );

			m_flChargeOffTime[chargeType] = 0.0f;

			if ( chargeEffect.condition_disable != TF_COND_LAST )
				RemoveCond( chargeEffect.condition_disable );
		}

		// Charge on.
		AddCond( chargeEffect.condition_enable );

		CSingleUserRecipientFilter filter( pProvider );
		EmitSound( filter, entindex(), chargeEffect.sound_enable );
		m_bChargeSounds[chargeType] = true;
	}
	else
	{
		if ( m_bChargeSounds[chargeType] )
		{
			StopSound( chargeEffect.sound_enable );
			m_bChargeSounds[chargeType] = false;
		}

		if ( m_flChargeOffTime[chargeType] == 0.0f )
		{
			CSingleUserRecipientFilter filter( pProvider );
			EmitSound( filter, entindex(), chargeEffect.sound_disable );
		}

		if ( bInstantRemove )
		{
			m_flChargeOffTime[chargeType] = 0.0f;
			RemoveCond( chargeEffect.condition_enable );

			if ( chargeEffect.condition_disable != TF_COND_LAST )
				RemoveCond( chargeEffect.condition_disable );
		}
		else
		{
			// Already turning it off?
			if ( m_flChargeOffTime[chargeType] != 0.0f )
				return;

			if ( chargeEffect.condition_disable != TF_COND_LAST )
				AddCond( chargeEffect.condition_disable );

			m_flChargeOffTime[chargeType] = gpGlobals->curtime + flRemoveTime;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
EHANDLE	CAI_BaseNPC::GetHealerByIndex( int index )
{
	if (m_aHealers.IsValidIndex( index ))
		return m_aHealers[index].pPlayer;

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CAI_BaseNPC::FindHealerIndex( CTFPlayer *pPlayer )
{
	for ( int i = 0; i < m_aHealers.Count(); i++ )
	{
		if ( m_aHealers[i].pPlayer == pPlayer )
			return i;
	}

	return m_aHealers.InvalidIndex();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the first healer in the healer array.  Note that this
//		is an arbitrary healer.
//-----------------------------------------------------------------------------
EHANDLE CAI_BaseNPC::GetFirstHealer()
{
	if ( m_aHealers.Count() > 0 )
		return m_aHealers.Head().pPlayer;

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::HealthKitPickupEffects( int iAmount )
{
	if ( InCond( TF_COND_BURNING ) )
		RemoveCond( TF_COND_BURNING );
	if ( InCond( TF_COND_BLEEDING ) )
		RemoveCond( TF_COND_BLEEDING );

	if ( IsStealthed() )
		return;
}

void CAI_BaseNPC::ConditionGameRulesThink( void )
{
	int i;
	for ( i = 0; i < TF_COND_LAST; i++ )
	{
		if ( InCond( i ) )
		{
			// Ignore permanent conditions
			if ( m_flCondExpireTimeLeft[i] != PERMANENT_CONDITION )
			{
				float flReduction = gpGlobals->frametime;

				if ( ConditionExpiresFast( i ) )
				{
					// If we're being healed, we reduce bad conditions faster
					if ( m_aHealers.Count() > 0 )
					{
						if ( i == TF_COND_URINE || i == TF_COND_MAD_MILK || i == TF_COND_GAS )
							flReduction *= m_aHealers.Count() + 1;
						else
							flReduction += ( m_aHealers.Count() * flReduction * 4 );
					}
				}

				m_flCondExpireTimeLeft.Set( i, max( m_flCondExpireTimeLeft[i] - flReduction, 0 ) );

				if ( m_flCondExpireTimeLeft[i] == 0 )
				{
					RemoveCond( i );
				}
			}
		}
	}

	// Our health will only decay ( from being medic buffed ) if we are not being healed by a medic
	// Dispensers can give us the TF_COND_HEALTH_BUFF, but will not maintain or give us health above 100%s
	bool bDecayHealth = true;

	// Get our overheal differences, and our base overheal.
	int iOverhealDifference = ( GetMaxBuffedHealth() - GetMaxHealth() );

	// If we're being healed, heal ourselves
	if ( InCond( TF_COND_HEALTH_BUFF ) )
	{
		float flMaxOverhealRatio = 0.0;
		float flOverhealAmount;

		// Heal faster if we haven't been in combat for a while
		float flTimeSinceDamage = gpGlobals->curtime - GetLastDamageTime();
		float flScale = RemapValClamped( flTimeSinceDamage, 10, 15, 1.0, 3.0 );

		bool bHasFullHealth = GetHealth() >= GetMaxHealth();

		float fTotalHealAmount = 0.0f;
		for ( int i = 0; i < m_aHealers.Count(); i++ )
		{
			Assert( m_aHealers[i].pPlayer );

			// Dispensers don't heal above 100%
			if ( bHasFullHealth && m_aHealers[i].bDispenserHeal )
			{
				continue;
			}

			// Being healed by a medigun, don't decay our health
			bDecayHealth = false;

			// Dispensers heal at a constant rate
			if ( m_aHealers[i].bDispenserHeal )
			{
				// Dispensers heal at a slower rate, but ignore flScale
				m_flHealFraction += gpGlobals->frametime * m_aHealers[i].flAmount;
			}
			else
			{
				// We're being healed by a medic
				flOverhealAmount = 1.0f;
				// Check our overheal level, and cap if necessary.
				if ( m_aHealers[i].pPlayer.IsValid() )
				{
					CTFPlayer *pHealer = static_cast< CTFPlayer  *>( static_cast< CBaseEntity  *>( m_aHealers[i].pPlayer ) );
					CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pHealer, flOverhealAmount, mult_medigun_overheal_amount );
				}
						
				// Iterate our overheal amount, if we're a higher value.
				if ( flOverhealAmount > flMaxOverhealRatio )
					flMaxOverhealRatio = flOverhealAmount;

				// Check our healer's overheal attribute.
				if ( bHasFullHealth )
				{			
					// Calculate out the max health we can heal up to for the person.
					int iMaxOverheal = floor( ( iOverhealDifference * flOverhealAmount ) + GetMaxHealth() );
					// Don't heal if our health is above the overheal ratio.
					if ( GetHealth() > iMaxOverheal )
						continue;
				}
					// Player heals are affected by the last damage time
					m_flHealFraction += gpGlobals->frametime * m_aHealers[i].flAmount * flScale;
			}

			fTotalHealAmount += m_aHealers[i].flAmount;
		}

		int nHealthToAdd = ( int )m_flHealFraction;
		if ( nHealthToAdd > 0 )
		{
			m_flHealFraction -= nHealthToAdd;

			// Overheal modifier attributes
			CALL_ATTRIB_HOOK_FLOAT( flMaxOverhealRatio, mult_patient_overheal_penalty );
			if ( GetActiveWeapon() )
			{
				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( GetActiveWeapon(), flMaxOverhealRatio, mult_patient_overheal_penalty_active );
			}

			// Modify our max overheal.
			int iBoostMax;
			if ( flMaxOverhealRatio != 1.0f )
				iBoostMax = ( ( iOverhealDifference ) * flMaxOverhealRatio ) + GetMaxHealth();
			else
				iBoostMax = GetMaxBuffedHealth();

			// Cap it to the max we'll boost a npc's health
			CALL_ATTRIB_HOOK_INT( nHealthToAdd, mult_health_fromhealers );
			nHealthToAdd = clamp( nHealthToAdd, 0, ( iBoostMax - GetHealth() ) );
			TakeHealth( nHealthToAdd, DMG_IGNORE_MAXHEALTH );

			// split up total healing based on the amount each healer contributes
			for ( int i = 0; i < m_aHealers.Count(); i++ )
			{
				if ( m_aHealers[i].pPlayer.IsValid() )
				{
					CTFPlayer *pPlayer = static_cast<CTFPlayer *>( static_cast<CBaseEntity *>( m_aHealers[i].pPlayer ) );
					float flAmount = 0.0f;

					if ( InSameTeam( pPlayer ) )
					{
						flAmount = (float)nHealthToAdd * ( m_aHealers[i].flAmount / fTotalHealAmount );
						CTF_GameStats.Event_PlayerHealedOther( pPlayer, flAmount );
					}

					// Store off how much this guy healed.
					m_aHealers[i].iRecentAmount += nHealthToAdd;

					// Show how much this player healed every second.
					if ( gpGlobals->curtime >= m_aHealers[i].flNextNofityTime )
					{
						if ( m_aHealers[i].iRecentAmount > 0 )
						{
							IGameEvent *event = gameeventmanager->CreateEvent( "npc_healed" );
							if ( event )
							{
								event->SetInt( "priority", 1 );
								event->SetInt( "patient", entindex() );
								event->SetInt( "healer", pPlayer->GetUserID() );
								event->SetInt( "amount", m_aHealers[i].iRecentAmount );

								gameeventmanager->FireEvent( event );
							}
						}

						m_aHealers[i].iRecentAmount = 0;
						m_aHealers[i].flNextNofityTime = gpGlobals->curtime + 1.0f;
					}

					CWeaponMedigun *pMedigun = pPlayer->GetMedigun();
					if ( pMedigun && ( GetHealth() != GetMaxBuffedHealth() ) )
					{
						int nRageOnHeal = 0;
						CALL_ATTRIB_HOOK_INT_ON_OTHER( pMedigun, nRageOnHeal, generate_rage_on_heal );
						if ( nRageOnHeal != 0 )
							pMedigun->SetEnergyMeter( flAmount / 2.0f );
					}
				}
			}
		}

		if ( InCond( TF_COND_BURNING ) )
		{
			// Reduce the duration of this burn 
			float flReduction = 2;	 // ( flReduction + 1 ) x faster reduction
			m_flFlameRemoveTime -= flReduction * gpGlobals->frametime;
		}

		if ( InCond( TF_COND_BLEEDING ) )
		{
			for ( int i=0; i<m_aBleeds.Count(); ++i )
			{
				bleed_struct_t *bleed = &m_aBleeds[i];
				bleed->m_flEndTime -= gpGlobals->frametime + gpGlobals->frametime;
			}
		}
	}

	if ( bDecayHealth )
	{
		// If we're not being buffed, our health drains back to our max
		if ( GetHealth() > GetMaxHealth() )
		{
			float flBoostMaxAmount = GetMaxBuffedHealth() - GetMaxHealth();
			m_flHealFraction += ( gpGlobals->frametime * ( flBoostMaxAmount / tf_boost_drain_time.GetFloat() ) );

			int nHealthToDrain = (int)m_flHealFraction;
			if ( nHealthToDrain > 0 )
			{
				m_flHealFraction -= nHealthToDrain;

				// Manually subtract the health so we don't generate pain sounds / etc
				m_iHealth -= nHealthToDrain;
			}
		}
	}

	if ( GetHealth() > GetMaxHealth() )
	{
		if ( !InCond( TF_COND_HEALTH_OVERHEALED ) )
		{
			AddCond( TF_COND_HEALTH_OVERHEALED );
		}
	}
	else
	{
		if ( InCond( TF_COND_HEALTH_OVERHEALED ) )
		{
			RemoveCond( TF_COND_HEALTH_OVERHEALED );
		}
	}

	if ( InCond( TF_COND_BURNING ) )
	{
		// If we're underwater, put the fire out
		if ( gpGlobals->curtime > m_flFlameRemoveTime || GetWaterLevel() >= WL_Waist )
		{
			RemoveCond( TF_COND_BURNING );
		}
		else if ( ( gpGlobals->curtime >= m_flFlameBurnTime ) )
		{
			float flBurnDamage = TF_BURNING_DMG;
			CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( m_hBurnWeapon, flBurnDamage, mult_wpn_burndmg );

			// Burn the npc
			CTakeDamageInfo info( m_hBurnAttacker, m_hBurnAttacker, m_hBurnWeapon, flBurnDamage, DMG_BURN | DMG_PREVENT_PHYSICS_FORCE, TF_DMG_CUSTOM_BURNING );
			TakeDamage( info );
			m_flFlameBurnTime = gpGlobals->curtime + TF_BURNING_FREQUENCY;
		}
	}

	if ( InCond( TF_COND_BLEEDING ) )
	{
		for (int i = m_aBleeds.Count() - 1; i >= 0; --i)
		{
			bleed_struct_t *bleed = &m_aBleeds[i];
			if ( gpGlobals->curtime >= bleed->m_flEndTime )
			{
				m_aBleeds.FastRemove( i );
				continue;
			}
			else if ( gpGlobals->curtime >= bleed->m_flBleedTime )
			{
				bleed->m_flBleedTime = gpGlobals->curtime + TF_BLEEDING_FREQUENCY;

				CTakeDamageInfo info( bleed->m_hAttacker, bleed->m_hAttacker, bleed->m_hWeapon, (float)bleed->m_iDamage, DMG_SLASH, TF_DMG_CUSTOM_BLEEDING );
				TakeDamage( info );
			}
		}

		if (m_aBleeds.IsEmpty())
			RemoveCond( TF_COND_BLEEDING );
	}

	if ( IsJared() && GetWaterLevel() >= WL_Waist )
 	{
		RemoveCond( TF_COND_URINE );
		RemoveCond( TF_COND_MAD_MILK );
		RemoveCond( TF_COND_GAS );
	}

	TestAndExpireChargeEffect( TF_CHARGE_INVULNERABLE );
	TestAndExpireChargeEffect( TF_CHARGE_CRITBOOSTED );
	TestAndExpireChargeEffect( TF_CHARGE_MEGAHEAL );

	if ( InCond( TF_COND_STUNNED ) )
	{
		if ( gpGlobals->curtime > m_flStunExpireTime )
		{
			// Only check stun phase if we're unable to move
			//if ( !( m_nStunFlags & TF_STUNFLAG_BONKSTUCK ) || m_iStunPhase == STUN_PHASE_END )
			NPC_TranslateActivity( ACT_MP_STUN_END );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: no mini-crits for reserve shooter
//-----------------------------------------------------------------------------
void CAI_BaseNPC::SetAirblastState( bool bAirblastState )
{
	bAirblastState = m_bAirblasted;
	bAirblastState = true;

	AddCond( TF_COND_KNOCKED_INTO_AIR );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_BaseNPC::ClearAirblastState( void )
{
	m_bAirblasted = false;

	RemoveCond( TF_COND_KNOCKED_INTO_AIR );
}

//-----------------------------------------------------------------------------
// Purpose: Adds this damager to the history list of characters who damaged NPC
//-----------------------------------------------------------------------------
void CAI_BaseNPC::AddDamagerToHistory( EHANDLE hDamager )
{
	// sanity check: ignore damager if it is on our team.  (Catch-all for 
	// damaging self in rocket jumps, etc.)
	if ( !hDamager || ( !hDamager->IsPlayer() && !hDamager->IsNPC() ) || hDamager->GetTeam() == GetTeam() )
		return;

	// If this damager is different from the most recent damager, shift the
	// damagers down and drop the oldest damager.  (If this damager is already
	// the most recent, we will just update the damage time but not remove
	// other damagers from history.)
	if ( m_DamagerHistory[0].hDamager != hDamager )
	{
		for ( int i = 1; i < ARRAYSIZE( m_DamagerHistory ); i++ )
		{
			m_DamagerHistory[i] = m_DamagerHistory[i-1];
		}		
	}	
	// set this damager as most recent and note the time
	m_DamagerHistory[0].hDamager = hDamager;
	m_DamagerHistory[0].flTimeDamage = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Clears damager history
//-----------------------------------------------------------------------------
void CAI_BaseNPC::ClearDamagerHistory()
{
	for ( int i = 0; i < ARRAYSIZE( m_DamagerHistory ); i++ )
	{
		m_DamagerHistory[i].Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::TestAndExpireChargeEffect( medigun_charge_types chargeType )
{
	if ( InCond( g_MedigunEffects[chargeType].condition_enable ) )
	{
		bool bRemoveCharge = false;

		if ( ( TFGameRules()->State_Get() == GR_STATE_TEAM_WIN ) && ( TFGameRules()->GetWinningTeam() != GetTeamNumber() ) )
		{
			bRemoveCharge = true;
		}

		if ( m_flChargeOffTime[chargeType] != 0.0f )
		{
			if ( gpGlobals->curtime > m_flChargeOffTime[chargeType] )
			{
				bRemoveCharge = true;
			}
		}

		if ( bRemoveCharge == true )
		{
			m_flChargeOffTime[chargeType] = 0.0f;

			if ( g_MedigunEffects[chargeType].condition_disable != TF_COND_LAST )
			{
				RemoveCond( g_MedigunEffects[chargeType].condition_disable );
			}

			RemoveCond( g_MedigunEffects[chargeType].condition_enable );
		}
	}
	else if ( m_bChargeSounds[chargeType] )
	{
		// If we're still playing charge sound but not actually charged, stop the sound.
		// This can happen if player respawns while crit boosted.
		StopSound( g_MedigunEffects[chargeType].sound_enable );
		m_bChargeSounds[chargeType] = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFTeam *CAI_BaseNPC::GetTFTeam( void )
{
	CTFTeam *pTeam = dynamic_cast<CTFTeam *>( GetTeam() );
	Assert( pTeam );
	return pTeam;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CAI_BaseNPC::GetCritMult( void )
{
	float flRemapCritMul = RemapValClamped( m_iCritMult, 0, 255, 1.0, TF_DAMAGE_CRITMOD_MAXMULT );

	return flRemapCritMul;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::UpdateCritMult( void )
{
	const float flMinMult = 1.0;
	const float flMaxMult = TF_DAMAGE_CRITMOD_MAXMULT;

	if ( m_DamageEvents.Count() == 0 )
	{
		m_iCritMult = RemapValClamped( flMinMult, flMinMult, flMaxMult, 0, 255 );
		return;
	}

	// Go through the damage multipliers and remove expired ones, while summing damage of the others
	float flTotalDamage = 0;
	for ( int i = m_DamageEvents.Count() - 1; i >= 0; i-- )
	{
		float flDelta = gpGlobals->curtime - m_DamageEvents[i].flTime;
		if ( flDelta > tf_damage_events_track_for.GetFloat() )
		{
			//Msg( "      Discarded (%d: time %.2f, now %.2f)\n", i, m_DamageEvents[i].flTime, gpGlobals->curtime );
			m_DamageEvents.Remove( i );
			continue;
		}

		// Ignore damage we've just done. We do this so that we have time to get those damage events
		// to the client in time for using them in prediction in this code.
		if ( flDelta < TF_DAMAGE_CRITMOD_MINTIME )
		{
			continue;
		}

		if ( flDelta > TF_DAMAGE_CRITMOD_MAXTIME )
			continue;

		flTotalDamage += m_DamageEvents[i].flDamage;
	}

	float flMult = RemapValClamped( flTotalDamage, 0, TF_DAMAGE_CRITMOD_DAMAGE, flMinMult, flMaxMult );

	m_iCritMult = (int)RemapValClamped( flMult, flMinMult, flMaxMult, 0, 255 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::RecordDamageEvent( const CTakeDamageInfo &info, bool bKill )
{
	if ( m_DamageEvents.Count() >= MAX_NPC_DAMAGE_EVENTS )
	{
		// Remove the oldest event
		m_DamageEvents.Remove( m_DamageEvents.Count() - 1 );
	}

	int iIndex = m_DamageEvents.AddToTail();
	m_DamageEvents[iIndex].flDamage = info.GetDamage();
	m_DamageEvents[iIndex].flTime = gpGlobals->curtime;
	m_DamageEvents[iIndex].bKill = bKill;

	// Don't count critical damage
	if ( info.GetDamageType() & DMG_CRITICAL )
	{
		m_DamageEvents[iIndex].flDamage /= TF_DAMAGE_CRIT_MULTIPLIER;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CAI_BaseNPC::GetNumKillsInTime( float flTime )
{
	if ( tf_damage_events_track_for.GetFloat() < flTime )
	{
		Warning( "NPC asking for damage events for time %.0f, but tf_damage_events_track_for is only tracking events for %.0f\n", flTime, tf_damage_events_track_for.GetFloat() );
	}

	int iKills = 0;
	for ( int i = m_DamageEvents.Count() - 1; i >= 0; i-- )
	{
		float flDelta = gpGlobals->curtime - m_DamageEvents[i].flTime;
		if ( flDelta < flTime )
		{
			if ( m_DamageEvents[i].bKill )
			{
				iKills++;
			}
		}
	}

	return iKills;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputSetPlaybackRate( inputdata_t &inputdata )
{
	SetPlaybackRate( inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputSetGroundSpeed( inputdata_t &inputdata )
{
	m_flGroundSpeed = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputAddCond( inputdata_t &inputdata )
{
	AddCond( inputdata.value.Int() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputRemoveCond( inputdata_t &inputdata )
{
	RemoveCond( inputdata.value.Int() );
}

void CAI_BaseNPC::ApplyAbsVelocityImpulse( const Vector &inVecImpulse )
{
	if ( inVecImpulse != vec3_origin )
	{
		Vector vecImpulse = inVecImpulse;

		// Safety check against receive a huge impulse, which can explode physics
		switch ( CheckEntityVelocity( vecImpulse ) )
		{
			case -1:
				Warning( "Discarding ApplyAbsVelocityImpulse(%f,%f,%f) on %s\n", vecImpulse.x, vecImpulse.y, vecImpulse.z, GetDebugName() );
				Assert( false );
				return;
			case 0:
				if ( CheckEmitReasonablePhysicsSpew() )
				{
					Warning( "Clamping ApplyAbsVelocityImpulse(%f,%f,%f) on %s\n", inVecImpulse.x, inVecImpulse.y, inVecImpulse.z, GetDebugName() );
				}
				break;
		}

		Vector vecResult;

		float flImpulseScale = 1.0f;
		if ( InCond( TF_COND_AIMING ) )
			CALL_ATTRIB_HOOK_FLOAT( flImpulseScale, mult_aiming_knockback_resistance );

		if ( InCond( TF_COND_HALLOWEEN_TINY ) )
			flImpulseScale *= 2.0f;

		VectorAdd( GetAbsVelocity(), vecImpulse * flImpulseScale, vecResult );
		SetAbsVelocity( vecResult );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_BaseNPC::ApplyGenericPushbackImpulse( const Vector &vecDir )
{
	if ( GetCarryingRuneType() == TF_RUNE_KNOCKOUT ) 
		return;

	float flAirblastVulMulti = 1.0f;
	CALL_ATTRIB_HOOK_FLOAT( flAirblastVulMulti, airblast_vulnerability_multiplier );

	float flAirblastVerVulMulti = 1.0f;
	CALL_ATTRIB_HOOK_FLOAT( flAirblastVerVulMulti, airblast_vertical_vulnerability_multiplier );

	Vector dp = vecDir;

	/*float hull_size_factor = ( ( WorldAlignSize().x / 24) * ( WorldAlignSize().y / 24) * ( WorldAlignSize().z / 64) );

	float flImpulse = 360 / hull_size_factor;
	flImpulse = MIN( 1000.0, flImpulse );

	dp *= flImpulse;*/

	// do the z-minning BEFORE applying the regular vulnerability multiplier
	if ( tf_airblast_cray_ground_reflect.GetBool() )
	{
		if (( GetFlags() & FL_ONGROUND) != 0) {
			dp.z = Min(dp.z, tf_airblast_cray_ground_minz.GetFloat());
		}
	}

	dp   *= flAirblastVulMulti;
	dp.z *= flAirblastVerVulMulti;

	SetGroundEntity( NULL );
	AddCond( TF_COND_KNOCKED_INTO_AIR );

	ApplyAbsVelocityImpulse(dp);
}

//-----------------------------------------------------------------------------
// Purpose: Most calls use the prediction seed
//-----------------------------------------------------------------------------
void CAI_BaseNPC::CalcIsAttackMiniCritical( void)
{
	if ( IsMiniCritBoosted() || InCond( TF_COND_NOHEALINGDAMAGEBUFF ) )
	{
		m_bCurrentAttackIsMiniCrit = true;
	}
	else
	{
		m_bCurrentAttackIsMiniCrit = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Most calls use the prediction seed
//-----------------------------------------------------------------------------
void CAI_BaseNPC::CalcIsAttackCritical( void )
{
	if ( gpGlobals->framecount == m_iLastCritCheckFrame )
		return;

	m_iLastCritCheckFrame = gpGlobals->framecount;

	// if base entity seed has changed since last calculation, reseed with new seed
	int iSeed = CBaseEntity::GetPredictionRandomSeed();
	if ( iSeed != m_iCurrentSeed )
	{
		m_iCurrentSeed = iSeed;
		RandomSeed( m_iCurrentSeed );
	}

	if ( IsCritBoosted() )
	{
		m_bCurrentAttackIsCrit = true;
	}
	else
	{
		// call the weapon-specific helper method
		m_bCurrentAttackIsCrit = CalcIsAttackCriticalHelper();
	}
}

extern ConVar lfe_hl2_weapon_criticals_melee;
extern ConVar tf_debug_criticals;
//-----------------------------------------------------------------------------
// Purpose: Melee Crit Calulation
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::CalcIsAttackCriticalHelper( void )
{
	int nCvarValue = lfe_hl2_weapon_criticals_melee.GetInt();

	if ( nCvarValue == 0 )
		return false;

	if ( nCvarValue == 1 && !lfe_hl2_weapon_criticals_melee.GetBool() )
		return false;

	float flNPCCritMult = GetCritMult();

	float flCritChance = TF_DAMAGE_CRIT_CHANCE_MELEE * flNPCCritMult;
	CALL_ATTRIB_HOOK_FLOAT( flCritChance, mult_crit_chance );

	// If the chance is 0, just bail.
	if ( flCritChance == 0.0f )
		return false;

	if ( tf_debug_criticals.GetBool() )
	{
		Msg( "%s : Rolling crit: %.02f%% chance... ", GetClassname(), flCritChance * 100.0f );
	}

	bool bSuccess = ( RandomInt( 0, WEAPON_RANDOM_RANGE - 1 ) <= flCritChance * WEAPON_RANDOM_RANGE );

	if ( tf_debug_criticals.GetBool() )
	{
		Msg( "%s\n", bSuccess ? "SUCCESS" : "FAILURE" );
	}

	return bSuccess;
}

extern ConVar nb_last_area_update_tolerance;

//-----------------------------------------------------------------------------
// Purpose: Invoke this to update our last known nav area 
// (since there is no think method chained to me)
//-----------------------------------------------------------------------------
void CAI_BaseNPC::UpdateLastKnownArea( void )
{
	if ( TheNavMesh->IsGenerating() )
	{
		ClearLastKnownArea();
		return;
	}

	if ( nb_last_area_update_tolerance.GetFloat() > 0.0f )
	{
		// skip this test if we're not standing on the world (ie: elevators that move us)
		if ( GetGroundEntity() == NULL || GetGroundEntity()->IsWorld() )
		{
			if ( m_lastNavArea && m_NavAreaUpdateMonitor.IsMarkSet() && !m_NavAreaUpdateMonitor.TargetMoved( this ) )
				return;

			m_NavAreaUpdateMonitor.SetMark( this, nb_last_area_update_tolerance.GetFloat() );
		}
	}

	// find the area we are directly standing in
	CNavArea *area = TheNavMesh->GetNearestNavArea( this, GETNAVAREA_CHECK_GROUND | GETNAVAREA_CHECK_LOS, 50.0f );
	if ( !area )
		return;

	// make sure we can actually use this area - if not, consider ourselves off the mesh
	if ( !IsAreaTraversable( area ) )
		return;

	if ( area != m_lastNavArea )
	{
		// player entered a new nav area
		if ( m_lastNavArea )
		{
			m_lastNavArea->DecrementPlayerCount( m_registeredNavTeam, entindex() );
			m_lastNavArea->OnExit( this, area );
		}

		m_registeredNavTeam = GetTeamNumber();
		area->IncrementPlayerCount( m_registeredNavTeam, entindex() );
		area->OnEnter( this, m_lastNavArea );

		OnNavAreaChanged( area, m_lastNavArea );

		m_lastNavArea = area;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Return true if we can use (walk through) the given area 
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::IsAreaTraversable( const CNavArea *area ) const
{
	if ( !BaseClass::IsAreaTraversable( area ) )
		return false;

	const CTFNavArea *tfArea = dynamic_cast<const CTFNavArea *>( area );
	if ( tfArea == nullptr )
		return false;

	// unless the round is over and we are the winning team, we can't enter the other teams spawn
	if ( TFGameRules()->State_Get() != GR_STATE_TEAM_WIN )
	{
		switch ( GetTeamNumber() )
		{
			case TF_TEAM_RED:
			{
				if ( tfArea->HasTFAttributes( BLUE_SPAWN_ROOM ) )
					return false;
				break;
			}
			case TF_TEAM_BLUE:
			{
				if ( tfArea->HasTFAttributes( RED_SPAWN_ROOM ) )
					return false;
				break;
			}
			case TF_TEAM_GREEN:
			{
				if ( tfArea->HasTFAttributes( RED_SPAWN_ROOM ) || tfArea->HasTFAttributes( BLUE_SPAWN_ROOM ) )
					return false;
				break;
			}
			case TF_TEAM_YELLOW:
			{
				if ( tfArea->HasTFAttributes( RED_SPAWN_ROOM ) || tfArea->HasTFAttributes( BLUE_SPAWN_ROOM ) )
					return false;
				break;
			}
			default:
				break;
		}
	}
	else
	{
		if ( TFGameRules()->GetWinningTeam() != GetTeamNumber() )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Leaving the nav mesh
//-----------------------------------------------------------------------------
void CAI_BaseNPC::ClearLastKnownArea( void )
{
	OnNavAreaChanged( NULL, m_lastNavArea );
	
	if ( m_lastNavArea )
	{
		m_lastNavArea->DecrementPlayerCount( m_registeredNavTeam, entindex() );
		m_lastNavArea->OnExit( this, NULL );
		m_lastNavArea = NULL;
		m_registeredNavTeam = TEAM_INVALID;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_BaseNPC::OnNavAreaChanged( CNavArea *enteredArea, CNavArea *leftArea )
{
	if ( enteredArea )
	{
		FOR_EACH_VEC( enteredArea->GetPrerequisiteVector(), i )
		{
			CFuncNavPrerequisite *prereq = enteredArea->GetPrerequisiteVector()[i];
			if ( prereq == NULL )
				return;

			if ( prereq->IsTask( CFuncNavPrerequisite::TASK_DESTROY_ENTITY ))
			{
				SetTarget( prereq );
				SetEnemy( prereq );
				SetState( NPC_STATE_COMBAT );
				UpdateEnemyMemory( prereq, prereq->GetAbsOrigin() );
			}

			if ( prereq->IsTask( CFuncNavPrerequisite::TASK_MOVE_TO_ENTITY ))
			{
				SetGoalEnt( prereq );
				StartTargetHandling( GetGoalEnt() );
				SetState( NPC_STATE_ALERT );
			}

			if ( prereq->IsTask( CFuncNavPrerequisite::TASK_WAIT ))
			{
				SetGoalEnt( prereq );
				StartTargetHandling( GetGoalEnt() );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handling editor removing the area we're standing upon
//-----------------------------------------------------------------------------
void CAI_BaseNPC::OnNavAreaRemoved( CNavArea *removedArea )
{
	BaseClass::OnNavAreaRemoved( removedArea );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::DropFlag( void )
{
	if ( HasItem() )
	{
		CCaptureFlag *pFlag = dynamic_cast<CCaptureFlag*>( GetItem() );
		if ( pFlag )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "teamplay_flag_event" );
			if ( event )
			{
				event->SetInt( "player", entindex() );
				event->SetString( "player_name", GetClassname() );
				event->SetInt( "carrier", entindex() );
				event->SetInt( "eventtype", TF_FLAGEVENT_DROPPED );
				event->SetInt( "home", 0 ); // whether or not the flag was home (only set for TF_FLAGEVENT_PICKUP) 
				event->SetInt( "team", GetItem()->GetTeamNumber() );
				event->SetInt( "priority", 8 );

				gameeventmanager->FireEvent( event );
			}
			pFlag->Drop( this, true, true );
			RemoveGlowEffect();
		}
	}
}

// ----------------------------------------------------------------------------
// Vintage Purpose: Play the stun sound to nearby players of the victim and the attacker
//-----------------------------------------------------------------------------
void CAI_BaseNPC::StunSound( CTFPlayer *pStunner, int nStunFlags/*, int nCurrentStunFlags*/ )
{
	// Don't play the stun sound if sounds are disabled or the stunner isn't a player
	if ( pStunner == nullptr || ( nStunFlags & TF_STUNFLAG_NOSOUNDOREFFECT ) != 0 )
		return;

	const char *pszStunSound = "\0";
	if ( nStunFlags & TF_STUNFLAG_CHEERSOUND )
	{
		// Moonshot/Grandslam
		pszStunSound = "TFPlayer.StunImpactRange";
	}
	else if( !( nStunFlags & TF_STUNFLAG_GHOSTEFFECT ) )
	{
		// Normal stun
		pszStunSound = "TFPlayer.StunImpact";
	}
	else
	{
		// Spookily spooked
		pszStunSound = "Halloween.PlayerScream";
	}

	CRecipientFilter filter;
	CSingleUserRecipientFilter filterAttacker( pStunner );
	filter.AddRecipientsByPAS( GetAbsOrigin() );
	filter.RemoveRecipient( pStunner );

	EmitSound( filter, entindex(), pszStunSound );
	EmitSound( filterAttacker, pStunner->entindex(), pszStunSound );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : did this wrench hit do any work on the npc?
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::InputWrenchHit( CTFPlayer *pPlayer, CTFWrench *pWrench, Vector vecHitPos )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::OnWrenchHit( CTFPlayer *pPlayer, CTFWrench *pWrench, Vector vecHitPos )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::IsNearPoint( CTeamControlPoint *point ) const
{
	CTFNavArea *myArea = static_cast<CTFNavArea *>( GetLastKnownArea() );
	if ( myArea )
	{
		int iPointIdx = point->GetPointIndex();
		if ( iPointIdx < MAX_CONTROL_POINTS )
		{
			// ???
			CTFNavArea *cpArea = TFNavMesh()->GetMainControlPointArea( iPointIdx );
			return ( -( myArea->GetIncursionDistance( GetTeamNumber() ) - cpArea->GetIncursionDistance( GetTeamNumber() ) ) < 750 );
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCaptureZone *CAI_BaseNPC::GetFlagCaptureZone( void )
{
	for ( int i=0; i<=ICaptureZoneAutoList::AutoList().Count(); ++i )
	{
		CCaptureZone *pZone = static_cast<CCaptureZone *>( ICaptureZoneAutoList::AutoList()[i] );
		if ( pZone->GetTeamNumber() == GetTeamNumber() )
			return pZone;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCaptureFlag *CAI_BaseNPC::GetFlagToFetch( void )
{
	CUtlVector<CCaptureFlag *> flags;
	for ( int i=0; i<ICaptureFlagAutoList::AutoList().Count(); ++i )
	{
		CCaptureFlag *pFlag = static_cast<CCaptureFlag *>( ICaptureFlagAutoList::AutoList()[i] );
		if ( pFlag->IsDisabled() )
			continue;

		if ( HasTheFlag() && pFlag->GetOwnerEntity() == this )
			return pFlag;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_BaseNPC::Regenerate( void )
{
	SetHealth( GetMaxHealth() );

	// Remove burning condition
	if ( InCond( TF_COND_BURNING ) )
	{
		RemoveCond( TF_COND_BURNING );
	}

	// Remove bleeding condition
	if ( InCond( TF_COND_BLEEDING ) )
	{
		RemoveCond( TF_COND_BLEEDING );
	}

	// Remove liquid condition
	if ( IsJared() )
	{
		RemoveCond( TF_COND_URINE );
		RemoveCond( TF_COND_MAD_MILK );
		RemoveCond( TF_COND_GAS );
	}

	// Remove bonk condition
	if ( InCond( TF_COND_PHASE ) )
	{
		RemoveCond( TF_COND_PHASE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::RegenThink( void )
{
	// Health drain attribute
	int iHealthDrain = 0;
	CALL_ATTRIB_HOOK_INT( iHealthDrain, add_health_regen );

	if( IsAlive() )
	{
		int iHealthRestored = TakeHealth( iHealthDrain, DMG_GENERIC );
		if (iHealthRestored)
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "npc_healonhit" );

			if ( event )
			{
				event->SetInt( "amount", iHealthRestored );
				event->SetInt( "entindex", entindex() );

				gameeventmanager->FireEvent( event );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::RuneRegenThink( void )
{
	if( IsAlive() )
	{
		TakeHealth( 25, DMG_GENERIC );
		SetContextThink( &CAI_BaseNPC::RuneRegenThink, gpGlobals->curtime + TF_RUNE_REGEN_TIME, "RuneRegenThink" );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CAI_BaseNPC::GetDesiredHeadScale()
{
	float flScale = 1.0f;
	if ( InCond( TF_COND_BALLOON_HEAD ) || InCond( TF_COND_HALLOWEEN_KART ) || InCond( TF_COND_MELEE_ONLY ) )
		flScale = 1.4f;

	CALL_ATTRIB_HOOK_FLOAT( flScale, head_scale );
	return flScale;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CAI_BaseNPC::GetDesiredTorsoScale()
{
	float flScale = 1.0f;
	CALL_ATTRIB_HOOK_FLOAT( flScale, torso_scale );
	return flScale;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CAI_BaseNPC::GetDesiredHandScale()
{
	float flScale = 1.0f;
	if ( InCond( TF_COND_MELEE_ONLY ) )
		flScale = 1.4f;

	CALL_ATTRIB_HOOK_FLOAT( flScale, hand_scale );
	return flScale;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CAI_BaseNPC::GetHeadScaleSpeed()
{
	// scale instantly if in any of these conditions 
	if ( InCond( TF_COND_HALLOWEEN_BOMB_HEAD ) )
		return GetDesiredHeadScale();

	if ( InCond( TF_COND_MELEE_ONLY ) )
		return GetDesiredHeadScale();

	if ( InCond( TF_COND_HALLOWEEN_KART ) )
		return GetDesiredHeadScale();

	if ( InCond( TF_COND_BALLOON_HEAD ) )
		return GetDesiredHeadScale();

	// otherwise, scale at approximately 1.0x per second
	return gpGlobals->frametime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CAI_BaseNPC::GetTorsoScaleSpeed()
{
	// scale at approximately 1.0x per second 
	return gpGlobals->frametime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CAI_BaseNPC::GetHandScaleSpeed()
{
	// scale instantly if in this condition 
	if ( InCond( TF_COND_MELEE_ONLY ) )
		return GetDesiredHandScale();

	// otherwise, scale at approximately 1.0x per second 
	return gpGlobals->frametime;
}

//-----------------------------------------------------------------------------
// Purpose: Add an attribute
//-----------------------------------------------------------------------------
void CAI_BaseNPC::AddCustomAttribute( char const *pAttribute, float flValue, float flDuration )
{
	CEconAttributeDefinition *pAttrib = GetItemSchema()->GetAttributeDefinitionByName( pAttribute );
	if ( pAttrib )
		GetAttributeList()->SetRuntimeAttributeValue( pAttrib, flValue );

	int iAttributeID = GetItemSchema()->GetAttributeIndex( pAttribute );
	CEconItemAttribute econAttribute( iAttributeID, flValue );

	for ( int i = 0; i < WeaponCount(); i++ )
	{
		CBaseCombatWeapon *pWeapon = GetWeapon( i );
		if ( pWeapon )
		{
			pWeapon->GetItem()->AddAttribute( &econAttribute );
		}
	}

	for ( int i = 0; i < GetNumWearables(); i++ )
	{
		CTFWearable *pWearable = static_cast<CTFWearable *>( GetWearable( i ) );
		if ( pWearable )
		{
			pWearable->GetItem()->AddAttribute( &econAttribute );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update attributes
//-----------------------------------------------------------------------------
void CAI_BaseNPC::UpdateCustomAttributes( void )
{
	/*for ( int i = 0; i < WeaponCount(); i++ )
	{
		CBaseCombatWeapon *pWeapon = ( CBaseCombatWeapon * )GetWeapon( i );
		if ( pWeapon )
		{
			pWeapon->WeaponReset();
		}
	}*/
}

//-----------------------------------------------------------------------------
// Purpose: Remove an attribute
//-----------------------------------------------------------------------------
void CAI_BaseNPC::RemoveCustomAttribute( char const *pAttribute )
{
	CEconAttributeDefinition *pAttrib = GetItemSchema()->GetAttributeDefinitionByName( pAttribute );
	if ( pAttrib )
		GetAttributeList()->RemoveAttribute( pAttrib );

	for ( int i = 0; i < WeaponCount(); i++ )
	{
		CBaseCombatWeapon *pWeapon = GetWeapon( i );
		if ( pWeapon && pWeapon->GetAttributeList() )
		{
			pWeapon->GetAttributeList()->RemoveAttribute( pAttrib );
		}
	}

	for ( int i = 0; i < GetNumWearables(); i++ )
	{
		CTFWearable *pWearable = static_cast<CTFWearable *>( GetWearable( i ) );
		if ( pWearable && pWearable->GetAttributeList() )
		{
			pWearable->GetAttributeList()->RemoveAttribute( pAttrib );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Unusual Mode
//-----------------------------------------------------------------------------
void CAI_BaseNPC::UnusualInit( void )
{
	if ( ( m_nTFFlags & TFFL_UNUSUALCOND ) == 0 || ( TFGameRules()->IsCoOp() || TFGameRules()->IsVersus() ) && GetTeamNumber() == TF_STORY_TEAM || TFGameRules()->IsBluCoOp() && GetTeamNumber() == TF_COMBINE_TEAM  )
		return;

	int iRandom = RandomInt( 0, 20 );
	switch ( iRandom )
	{
	case 7:
		AddCond( TF_COND_OFFENSEBUFF );
		break;

	case 6:
		AddCond( TF_COND_PREVENT_DEATH );
		break;

	case 4:
		AddCond( TF_COND_SPEED_BOOST );
		break;

	/*case 9:
		AddCond( TF_COND_HALLOWEEN_TINY );
		break;*/

	case 12:
		AddCond( TF_COND_DEFENSEBUFF );
		break;

	case 15:
		AddCond(TF_COND_REGENONDAMAGEBUFF);
		break;
	}

	// stronker
	AddCustomAttribute( "dmg taken from bullets reduced", 0.5 );
	AddCustomAttribute( "dmg taken from blast reduced", 0.5 );
	AddCustomAttribute( "dmg taken from fire reduced", 0.5 );
	AddCustomAttribute( "dmg taken from crit reduced", 0.5 );
	AddCustomAttribute( "dmg from melee reduced", 0.5 );
}


//-----------------------------------------------------------------------------
// Purpose: 
// NOTE: If we don't let players drop ammo boxes, we don't need this code..
//-----------------------------------------------------------------------------
void CAI_BaseNPC::AmmoPackCleanUp( void )
{
	// If we have more than 3 ammo packs out now, destroy the oldest one.
	int iNumPacks = 0;
	CTFAmmoPack *pOldestBox = NULL;

	// Cycle through all ammobox in the world and remove them
	CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, "tf_ammo_pack" );
	while ( pEnt )
	{
		CBaseEntity *pOwner = pEnt->GetOwnerEntity();
		if (pOwner == this)
		{
			CTFAmmoPack *pThisBox = dynamic_cast<CTFAmmoPack *>( pEnt );
			Assert( pThisBox );
			if ( pThisBox )
			{
				iNumPacks++;

				// Find the oldest one
				if ( pOldestBox == NULL || pOldestBox->GetCreationTime() > pThisBox->GetCreationTime() )
				{
					pOldestBox = pThisBox;
				}
			}
		}

		pEnt = gEntList.FindEntityByClassname( pEnt, "tf_ammo_pack" );
	}

	// If they have more than 3 packs active, remove the oldest one
	if ( iNumPacks > 3 && pOldestBox )
	{
		UTIL_Remove( pOldestBox );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::DropAmmoPack( void )
{
	Vector vecPackOrigin = GetAbsOrigin();
	QAngle vecPackAngles = GetAbsAngles();

	// Fill the ammo pack with unused player ammo, if out add a minimum amount.
	int iPrimary = max( 5, GetAmmoCount( TF_AMMO_PRIMARY ) );
	int iSecondary = max( 5, GetAmmoCount( TF_AMMO_SECONDARY ) );
	int iMetal = max( 5, GetAmmoCount( TF_AMMO_METAL ) );
	int iGrenade1 = max( 5, GetAmmoCount( LFE_AMMO_GRENADES1 ) );
	int iGrenade2 = max( 5, GetAmmoCount( LFE_AMMO_GRENADES2 ) );

	// Create the ammo pack.
	CTFAmmoPack *pAmmoPack = CTFAmmoPack::Create( vecPackOrigin, vecPackAngles, this, "models/items/ammopack_medium.mdl" );
	Assert( pAmmoPack );
	if ( pAmmoPack )
	{
		// Fill up the ammo pack.
		pAmmoPack->GiveAmmo( iPrimary, TF_AMMO_PRIMARY );
		pAmmoPack->GiveAmmo( iSecondary, TF_AMMO_SECONDARY );
		pAmmoPack->GiveAmmo( iMetal, TF_AMMO_METAL );
		pAmmoPack->GiveAmmo( iGrenade1, LFE_AMMO_GRENADES1 );
		pAmmoPack->GiveAmmo( iGrenade2, LFE_AMMO_GRENADES2 );

		Vector vecRight, vecUp;
		AngleVectors( EyeAngles(), NULL, &vecRight, &vecUp );

		// Calculate the initial impulse on the weapon.
		Vector vecImpulse( 0.0f, 0.0f, 0.0f );
		vecImpulse += vecUp * random->RandomFloat( -0.25, 0.25 );
		vecImpulse += vecRight * random->RandomFloat( -0.25, 0.25 );
		VectorNormalize( vecImpulse );
		vecImpulse *= random->RandomFloat( 100.0f, 150.0f );
		vecImpulse += GetAbsVelocity();

		// Cap the impulse.
		float flSpeed = vecImpulse.Length();
		if ( flSpeed > 150.0f )
		{
			VectorScale( vecImpulse, 150.0f / flSpeed, vecImpulse );
		}

		if ( pAmmoPack->VPhysicsGetObject() )
		{
			// We can probably remove this when the mass on the weapons is correct!
			pAmmoPack->VPhysicsGetObject()->SetMass( 25.0f );
			AngularImpulse angImpulse( 0, random->RandomFloat( 0, 100 ), 0 );
			pAmmoPack->VPhysicsGetObject()->SetVelocityInstantaneous( &vecImpulse, &angImpulse );
		}

		pAmmoPack->SetInitialVelocity( vecImpulse );

		// Give the ammo pack some health, so that trains can destroy it.
		pAmmoPack->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
		pAmmoPack->m_takedamage = DAMAGE_YES;		
		pAmmoPack->SetHealth( 900 );
		
		pAmmoPack->SetBodygroup( 1, 1 );
	
		// Clean up old ammo packs if they exist in the world
		AmmoPackCleanUp();	
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::DropHealthPack( CTakeDamageInfo const &info, bool bSomething )
{
	CTFPowerup *pPack = (CTFPowerup *)CBaseEntity::Create( "item_healthkit_small", WorldSpaceCenter(), vec3_angle );
	if ( !pPack )
		return;

	// Investigate for constant expression
	Vector vecRand;
	vecRand.x = ( rand() * 0.000061037019 ) + -1.0f;
	vecRand.y = ( rand() * 0.000061037019 ) + -1.0f;
	vecRand.z = 250;

	vecRand.AsVector2D().NormalizeInPlace();

	pPack->DropSingleInstance( vecRand, this, 0.0f, 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: grave maker
//-----------------------------------------------------------------------------
void CAI_BaseNPC::DropDeathCallingCard( CTFPlayer *pKiller, CBaseEntity *pVictim )
{
	CTFPowerup *pPack = (CTFPowerup *)CBaseEntity::Create( "item_healthkit_small", WorldSpaceCenter(), vec3_angle );
	if ( !pPack )
		return;

	pPack->DropSingleInstance( vec3_origin, this, 0.0f, 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BaseNPC::DropRune( void )
{
	int iTeam = GetTeamNumber();
	if ( GetTeamNumber() == TF_TEAM_RED )
		iTeam = TEAM_UNASSIGNED;
	if ( GetTeamNumber() == TF_TEAM_BLUE || GetTeamNumber() == TF_TEAM_GREEN || GetTeamNumber() == TF_TEAM_YELLOW )
		iTeam = TF_TEAM_RED;

	CTFRune *pRune = CTFRune::CreateRune( WorldSpaceCenter(), GetCarryingRuneType(), iTeam );
	Vector vecForward;
	QAngle angAngs;

	angAngs = GetAbsAngles();
	angAngs.x -= 30;
	AngleVectors( angAngs, &vecForward );
	pRune->SetAbsVelocity( vecForward * 250 );
	pRune->SetAbsAngles( vec3_angle );
	pRune->SetOwnerEntity( this );
	pRune->m_flNextCollideTime = gpGlobals->curtime + 0.5f;

	switch ( GetCarryingRuneType() )
	{
	case TF_RUNE_NONE:
		break;
	case TF_RUNE_STRENGTH:
		RemoveCond( TF_COND_RUNE_STRENGTH );
		break;
	case TF_RUNE_HASTE:
		RemoveCond( TF_COND_RUNE_HASTE );
		break;
	case TF_RUNE_REGEN:
		RemoveCond( TF_COND_RUNE_REGEN );
		break;
	case TF_RUNE_RESIST:
		RemoveCond( TF_COND_RUNE_RESIST );
		break;
	case TF_RUNE_VAMPIRE:
		RemoveCond( TF_COND_RUNE_VAMPIRE );
		break;
	case TF_RUNE_WARLOCK:
		RemoveCond( TF_COND_RUNE_WARLOCK );
		break;
	case TF_RUNE_PRECISION:
		RemoveCond( TF_COND_RUNE_PRECISION );
		break;
	case TF_RUNE_AGILITY:
		RemoveCond( TF_COND_RUNE_AGILITY );
		break;
	case TF_RUNE_KNOCKOUT:
		RemoveCond( TF_COND_RUNE_KNOCKOUT );
		break;
	case TF_RUNE_KING:
		RemoveCond( TF_COND_RUNE_KING );
		break;
	case TF_RUNE_PLAGUE:
		RemoveCond( TF_COND_RUNE_PLAGUE );
		break;
	case TF_RUNE_SUPERNOVA:
		RemoveCond( TF_COND_RUNE_SUPERNOVA );
		break;

	default:
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTriggerAreaCapture *CAI_BaseNPC::GetControlPointStandingOn( void )
{
	touchlink_t *root = (touchlink_t *)GetDataObject( TOUCHLINK );
	if (root)
	{
		touchlink_t *next = root->nextLink;
		while (next != root)
		{
			CBaseEntity *pEntity = next->entityTouched;
			if (!pEntity)
				return NULL;

			if ( pEntity->IsSolidFlagSet( FSOLID_TRIGGER ) && pEntity->IsBSPModel() )
			{
				CTriggerAreaCapture *pCapArea = dynamic_cast<CTriggerAreaCapture *>( pEntity );
				if ( pCapArea )
					return pCapArea;
			}

			next = next->nextLink;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::IsCapturingPoint( void )
{
	CTriggerAreaCapture *pCapArea = GetControlPointStandingOn();
	if ( pCapArea )
	{
		CTeamControlPoint *pPoint = pCapArea->GetControlPoint();
		if ( pPoint && TFGameRules()->TeamMayCapturePoint( GetTeamNumber(), pPoint->GetPointIndex() ) && AllowCapturePoint() )
		{
			return pPoint->GetOwner() != GetTeamNumber();
		}
	}

	return false;
}

#endif