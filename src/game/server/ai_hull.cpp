//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================//

#include "cbase.h"
#include "ai_hull.h"
#include "tf_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct ai_hull_t
{
	ai_hull_t( int bit, const char *pName, const Vector &_mins, const Vector &_maxs, const Vector &_smallMins, const Vector &_smallMaxs )
		: hullBit( bit ), mins( _mins ), maxs( _maxs ), smallMins( _smallMins ), smallMaxs( _smallMaxs ), name( pName ) {}
	int			hullBit;
	const char*	name;

	Vector	mins;
	Vector	maxs;

	Vector	smallMins;
	Vector	smallMaxs;
};

//=================================================================================
// Create the hull types here.
//=================================================================================
ai_hull_t  HL1_Wide_Human_Hull	(bits_WIDE_HUMAN_HULL,		"WIDE_HUMAN_HULL",		Vector(-32,-32,   0),	Vector(32, 32, 72),		Vector(-10,-10, 0),		Vector(10, 10, 72) );

ai_hull_t  Human_Hull			(bits_HUMAN_HULL,			"HUMAN_HULL",			Vector(-13,-13,   0),	Vector(13, 13, 72),		Vector(-8,-8,   0),		Vector( 8,  8, 72) );
ai_hull_t  Small_Centered_Hull	(bits_SMALL_CENTERED_HULL,	"SMALL_CENTERED_HULL",	Vector(-20,-20, -20),	Vector(20, 20, 20),		Vector(-12,-12,-12),	Vector(12, 12, 12) );
ai_hull_t  Wide_Human_Hull		(bits_WIDE_HUMAN_HULL,		"WIDE_HUMAN_HULL",		Vector(-15,-15,   0),	Vector(15, 15, 72),		Vector(-10,-10, 0),		Vector(10, 10, 72) );
ai_hull_t  Tiny_Hull			(bits_TINY_HULL,			"TINY_HULL",			Vector(-12,-12,   0),	Vector(12, 12, 24),		Vector(-12,-12, 0),	    Vector(12, 12, 24) );
ai_hull_t  Wide_Short_Hull		(bits_WIDE_SHORT_HULL,		"WIDE_SHORT_HULL",		Vector(-35,-35,   0),	Vector(35, 35, 32),		Vector(-20,-20, 0),	    Vector(20, 20, 32) );
ai_hull_t  Medium_Hull			(bits_MEDIUM_HULL,			"MEDIUM_HULL",			Vector(-16,-16,   0),	Vector(16, 16, 64),		Vector(-8,-8, 0),	    Vector(8, 8, 64) );
ai_hull_t  Tiny_Centered_Hull	(bits_TINY_CENTERED_HULL,	"TINY_CENTERED_HULL",	Vector(-8,	-8,  -4),	Vector(8, 8,  4),		Vector(-8,-8, -4),		Vector( 8, 8, 4) );
ai_hull_t  Large_Hull			(bits_LARGE_HULL,			"LARGE_HULL",			Vector(-40,-40,   0),	Vector(40, 40, 100),	Vector(-40,-40, 0),		Vector(40, 40, 100) );
ai_hull_t  Large_Centered_Hull	(bits_LARGE_CENTERED_HULL,	"LARGE_CENTERED_HULL",	Vector(-38,-38, -38),	Vector(38, 38, 38),		Vector(-30,-30,-30),	Vector(30, 30, 30) );
ai_hull_t  Medium_Tall_Hull		(bits_MEDIUM_TALL_HULL,		"MEDIUM_TALL_HULL",		Vector(-18,-18,   0),	Vector(18, 18, 100),	Vector(-12,-12, 0),	    Vector(12, 12, 100) );

//
// Array of hulls. These hulls must correspond with the enumerations in AI_Hull.h!
//
ai_hull_t*	hull[NUM_HULLS] =	
{
	&Human_Hull,
	&Small_Centered_Hull,
	&Wide_Human_Hull,
	&Tiny_Hull,
	&Wide_Short_Hull,
	&Medium_Hull,
	&Tiny_Centered_Hull,
	&Large_Hull,
	&Large_Centered_Hull,
	&Medium_Tall_Hull,
};

ai_hull_t *GetHullPtr(int id)
{
	if(id == HULL_WIDE_HUMAN && TFGameRules()->IsInHL1Map())
		return &HL1_Wide_Human_Hull;

	return hull[id];
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
const Vector &NAI_Hull::Mins(int id)			
{ 
	return GetHullPtr(id)->mins;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
const Vector &NAI_Hull::Maxs(int id)			
{ 
	return GetHullPtr(id)->maxs;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
const Vector &NAI_Hull::SmallMins(int id)			
{ 
	return GetHullPtr(id)->smallMins;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
const Vector &NAI_Hull::SmallMaxs(int id)			
{ 
	return GetHullPtr(id)->smallMaxs;
}

//-----------------------------------------------------------------------------
// Purpose:

// Input  :
// Output :
//-----------------------------------------------------------------------------
float NAI_Hull::Length(int id)
{
	ai_hull_t *h = GetHullPtr(id);
	return (h->maxs.x - h->mins.x); 
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
float NAI_Hull::Width(int id)
{
	ai_hull_t *h = GetHullPtr(id);
	return (h->maxs.y - h->mins.y);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
float NAI_Hull::Height(int id)
{
	ai_hull_t *h = GetHullPtr(id);
	return (h->maxs.z - h->mins.z); 
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int NAI_Hull::Bits(int id)			
{ 
	return GetHullPtr(id)->hullBit;		
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
const char *NAI_Hull::Name(int id)	
{ 
	return GetHullPtr(id)->name;	
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
Hull_t NAI_Hull::LookupId(const char *szName)	
{ 
	int i;
	if (!szName)
	{
		return HULL_HUMAN;
	}
	for (i = 0; i < NUM_HULLS; i++)
	{
		if (stricmp( szName, NAI_Hull::Name( i )) == 0)
		{
			return (Hull_t)i;
		}
	}
	return HULL_HUMAN;
}


