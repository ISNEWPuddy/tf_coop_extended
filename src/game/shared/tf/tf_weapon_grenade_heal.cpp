//====== Copyright � 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: TF Heal Grenade.
//
//=============================================================================//
#include "cbase.h"
#include "tf_weaponbase.h"
#include "tf_gamerules.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "tf_weapon_grenade_heal.h"

// Server specific.
#ifdef GAME_DLL
#include "tf_player.h"
#include "items.h"
#include "tf_weaponbase_grenadeproj.h"
#include "soundent.h"
#include "KeyValues.h"
#include "particle_parse.h"
#endif

#define GRENADE_HEAL_TIMER	3.0f //Seconds
#define	GRENADE_HEAL_LEADIN	2.0f 

//=============================================================================
//
// TF Heal Grenade Projectile functions (Server specific).
//

#define GRENADE_MODEL "models/tfc/ngrenade.mdl"

LINK_ENTITY_TO_CLASS( tf_weapon_grenade_heal, CTFGrenadeHealProjectile );
PRECACHE_WEAPON_REGISTER( tf_weapon_grenade_heal );

#ifdef GAME_DLL
ConVar tf_grenade_heal_amount( "tf_grenade_heal_amount", "100", FCVAR_CHEAT, "Amount healed by the medic heal grenade.\n" );

BEGIN_DATADESC( CTFGrenadeHealProjectile )
	DEFINE_THINKFUNC( DetonateThink ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFGrenadeHealProjectile* CTFGrenadeHealProjectile::Create( const Vector &position, const QAngle &angles, 
																const Vector &velocity, const AngularImpulse &angVelocity, 
																CBaseCombatCharacter *pOwner, CBaseEntity *pWeapon, float flTimer, int iFlags )
{
	CTFGrenadeHealProjectile *pGrenade = static_cast<CTFGrenadeHealProjectile*>( CTFWeaponBaseGrenadeProj::Create( "tf_weapon_grenade_heal", position, angles, velocity, angVelocity, pOwner, pWeapon ) );
	if ( pGrenade )
	{
		pGrenade->InitGrenadeTFC( velocity, angVelocity, pOwner );
		pGrenade->ApplyLocalAngularVelocityImpulse( angVelocity );
	}

	return pGrenade;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGrenadeHealProjectile::Spawn()
{
	SetModel( GRENADE_MODEL );
	SetDetonateTimerLength( GRENADE_HEAL_TIMER );

	BaseClass::Spawn();

	m_bPlayedLeadIn = false;

	SetThink( &CTFGrenadeHealProjectile::DetonateThink );

	// Since this code only runs on the server, make sure it shows the tempents it creates.
	CDisablePredictionFiltering disabler;

	if ( GetTeamNumber() == TF_TEAM_BLUE )
	{
		DispatchParticleEffect( "heal_ticker_blue", PATTACH_ABSORIGIN_FOLLOW, this );
	}
	else
	{
		DispatchParticleEffect( "heal_ticker_red", PATTACH_ABSORIGIN_FOLLOW, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGrenadeHealProjectile::Precache()
{
	PrecacheModel( GRENADE_MODEL );
	PrecacheScriptSound( "Weapon_Grenade_Heal.LeadIn" );
	PrecacheScriptSound( "Weapon_Grenade_Heal.Bounce" );
	PrecacheScriptSound( "Weapon_Grenade_Heal.Explode" );
	PrecacheParticleSystem( "heal_ticker_blue" );
	PrecacheParticleSystem( "heal_ticker_red" );
	PrecacheParticleSystem( "heal_grenade_blue" );
	PrecacheParticleSystem( "heal_grenade_red" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGrenadeHealProjectile::BounceSound( void )
{
	EmitSound( "Weapon_HandGrenade.GrenadeBounce" );
}

extern ConVar tf_grenade_show_radius;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGrenadeHealProjectile::Detonate()
{
	if ( ShouldNotDetonate() )
	{
		RemoveGrenade();
		return;
	}

	float flRadius = 180;
	float flHealAmount = tf_grenade_heal_amount.GetFloat();

	if ( tf_grenade_show_radius.GetBool() )
	{
		DrawRadius( flRadius );
	}

	// Heal every friendly player in the radius for 100 health

	CBaseEntity *pEntityList[100];
	int nEntityCount = UTIL_EntitiesInSphere( pEntityList, 100, GetAbsOrigin(), flRadius, FL_CLIENT );
	int iEntity;
	for ( iEntity = 0; iEntity < nEntityCount; ++iEntity )
	{
		CTFPlayer *pPlayer = ToTFPlayer( pEntityList[iEntity] );

		if ( pPlayer  )
		{
			pPlayer->TakeHealth( flHealAmount, DMG_GENERIC );
		}
	}

	EmitSound( "Weapon_Grenade_Heal.Explode" );

	// Since this code only runs on the server, make sure it shows the tempents it creates.
	CDisablePredictionFiltering disabler;

	if ( GetTeamNumber() == TF_TEAM_BLUE )
	{
		DispatchParticleEffect( "heal_grenade_blue", GetAbsOrigin(), vec3_angle );
	}
	else
	{
		DispatchParticleEffect( "heal_grenade_red", GetAbsOrigin(), vec3_angle );
	}

	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGrenadeHealProjectile::DetonateThink( void )
{
	if ( !m_bPlayedLeadIn && gpGlobals->curtime > GetDetonateTime() - GRENADE_HEAL_LEADIN )
	{
		Vector soundPosition = GetAbsOrigin() + Vector( 0, 0, 5 );
		CPASAttenuationFilter filter( soundPosition );

		EmitSound( filter, entindex(), "Weapon_Grenade_Heal.LeadIn" );
		m_bPlayedLeadIn = true;
	}

	BaseClass::DetonateThink();
}

#endif