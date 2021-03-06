//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

===== item_suit.cpp ========================================================

  handling for the player's suit.
*/

#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "items.h"

#ifdef TF_CLASSIC
#include "tf_gamerules.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SF_SUIT_SHORTLOGON		0x0001

class CItemSuit : public CItem
{
public:
	DECLARE_CLASS( CItemSuit, CItem );

	void Spawn( void )
	{ 
		Precache( );

		if ( TFGameRules()->IsInHL1Map() )
			SetModel( "models/w_suit.mdl" );	//If we're in HL1
		else if (CBaseEntity::GetModelName() == NULL_STRING)
			SetModel( "models/items/hevsuit.mdl" );
		else
			SetModel( "models/items/hevsuit.mdl" );

		BaseClass::Spawn( );

		if ( TFGameRules()->IsInHL1Map() )
			CollisionProp()->UseTriggerBounds( true, 12.0f );
		else
			CollisionProp()->UseTriggerBounds( false, 0 );
	}
	void Precache( void )
	{
		if ( TFGameRules()->IsInHL1Map() )
			SetModelName(AllocPooledString("models/w_suit.mdl"));	//If we're in HL1
		else if (CBaseEntity::GetModelName() == NULL_STRING)
			SetModelName( AllocPooledString("models/items/hevsuit.mdl") );
		else
			SetModelName( CBaseEntity::GetModelName() );

		PrecacheModel( STRING( GetModelName( )) );
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
#ifdef TF_CLASSIC
		if ( !pPlayer )
			return false;

		// Can be picked up but doesn't actually do anything.
		ToTFPlayer( pPlayer )->SpeakConceptIfAllowed( MP_CONCEPT_MVM_LOOT_RARE );
		return true;
#else
		if ( pPlayer->IsSuitEquipped() )
			return false;

		if ( m_spawnflags & SF_SUIT_SHORTLOGON )
			UTIL_EmitSoundSuit(pPlayer->edict(), "!HEV_A0");		// short version of suit logon,
		else
			UTIL_EmitSoundSuit(pPlayer->edict(), "!HEV_AAx");	// long version of suit logon

		pPlayer->EquipSuit();
				
		return true;
#endif
	}
};

LINK_ENTITY_TO_CLASS( item_suit, CItemSuit );
