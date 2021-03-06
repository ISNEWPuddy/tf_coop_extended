//====== Copyright � 1996-2005, Valve Corporation, All rights reserved. =======//
//
//
//=============================================================================//
#include "cbase.h"
#include "tf_item.h"
#include "tf_shareddefs.h"

#ifdef CLIENT_DLL
#include "c_tf_player.h"
#include "c_ai_basenpc.h"
#else
#include "tf_player.h"
#include "ai_basenpc.h"
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( TFItem, DT_TFItem )

BEGIN_NETWORK_TABLE( CTFItem, DT_TFItem )
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose: Identifier.
//-----------------------------------------------------------------------------
unsigned int CTFItem::GetItemID( void )
{ 
	return TF_ITEM_UNDEFINED; 
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFItem::PickUp( CBaseEntity *pPlayer, bool bInvisible )
{
	// SetParent with attachment point - look it up later if need be!
	SetOwnerEntity( pPlayer );
	SetParent( pPlayer );
	SetLocalOrigin( vec3_origin );
	SetLocalAngles( vec3_angle );

	// Make invisible?
	if ( bInvisible )
	{
		AddEffects( EF_NODRAW );
	}

	// Add the item to the player's item inventory.
	CTFPlayer *pTFPlayer = ToTFPlayer( pPlayer );
	if ( pTFPlayer )
		pTFPlayer->SetItem( this );

	CAI_BaseNPC *pNPC = pPlayer->MyNPCPointer();
	if ( pNPC )
		pNPC->SetItem( this );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFItem::Drop( CBaseEntity *pPlayer, bool bVisible, bool bThrown /*= false*/, bool bMessage /*= true*/ )
{
	// Remove the item from the player's item inventory.
	CTFPlayer *pTFPlayer = ToTFPlayer( pPlayer );
	if ( pTFPlayer )
		pTFPlayer->SetItem( NULL );

	CAI_BaseNPC *pNPC = pPlayer->MyNPCPointer();
	if ( pNPC )
		pNPC->SetItem( NULL );

	// Make visible?
	if ( bVisible )
	{
		RemoveEffects( EF_NODRAW );
	}

	// Clear the parent.
	SetParent( NULL );
	SetOwnerEntity( NULL );
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFItem::ShouldDraw()
{
	// If I'm carrying the flag, don't draw it
	if ( GetMoveParent() == C_BasePlayer::GetLocalPlayer() && !C_BasePlayer::ShouldDrawLocalPlayer() )
		return false;

	return BaseClass::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Should this object cast shadows?
//-----------------------------------------------------------------------------
ShadowType_t CTFItem::ShadowCastType()
{
	if ( GetMoveParent() == C_BasePlayer::GetLocalPlayer() )
		return SHADOWS_NONE;

	return BaseClass::ShadowCastType();
}

#endif