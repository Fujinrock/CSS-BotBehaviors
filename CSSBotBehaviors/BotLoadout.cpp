#include "BotLoadout.h"
#include "util.h"
#include "convar.h"
#include "edict.h"
#include "random.h"

extern CGlobalVars *gpGlobals;

extern const ConVar *g_pCvBotLoadout;
extern const ConVar *g_pCvStartMoney;

ConVar g_cvBotMinimumRounds( "bot_minimum_rounds", "15", FCVAR_REPLICATED, "How many rounds one half of the match should be (for bots' buy decisions)", true, 1.f, false, 1.f );

// Bit flags for items to buy
#define FL_BUY_HE			(1<<0)
#define FL_BUY_SMOKE		(1<<1)
#define FL_BUY_1_FLASH		(1<<2)
#define FL_BUY_2_FLASHES	(1<<3)
#define FL_BUY_DEFUSE_KIT	(1<<4)
#define FL_BUY_VEST			(1<<5)
#define FL_BUY_VESTHELM		(1<<6)

// Hardcoded item prices because these are so unlikely to be edited
enum
{
	DEFUSE_KIT_PRICE = 200,
	KEVLAR_PRICE = 650,
	HELMET_PRICE = 350,
	ASSAULTSUIT_PRICE = KEVLAR_PRICE + HELMET_PRICE,
	NVG_PRICE = 1000
};

struct BotBuyInfo
{
	CSWeaponID buyPistol;
	CSWeaponID buyPrimary;
	char buyItemBitFlags;

	// Game state
	static bool			isPistolRound;
	static bool			isEcoRound[ NUM_TEAMS ];
	static bool			isLastRound[ NUM_TEAMS ];
	static CSWeaponID	roundSpecialWeapon[ NUM_TEAMS ];
	static bool			bRoundCheckDone;
};

static BotBuyInfo s_BotBuyInfos[ MAXPLAYERS ];

bool BotBuyInfo::isPistolRound = false;
bool BotBuyInfo::isEcoRound[NUM_TEAMS] = {false};
bool BotBuyInfo::isLastRound[NUM_TEAMS] = {false};
CSWeaponID BotBuyInfo::roundSpecialWeapon[NUM_TEAMS] = {WEAPON_NONE};
bool BotBuyInfo::bRoundCheckDone = false;

//=======================================================================================================================

int GetPlayerAccount( CCSPlayer *player )
{
	const int accountOffs = 3612;

	return *(int*)((char*)player + accountOffs);
}

//=======================================================================================================================

enum { NO_ARMOR, VEST, VESTHELM };

int GetPlayerArmorType( CCSPlayer *player )
{
	const int armorValueOffs = 3064;
	const int hasHelmetOffs = 3596;

	int armorValue = *(int*)((char*)player + armorValueOffs);
	bool hasHelmet = (*(char*)player + hasHelmetOffs) != 0;

	if( armorValue == 0 )
		return NO_ARMOR;

	return hasHelmet ? VESTHELM : VEST;
}

//=======================================================================================================================

// Check game state - what kind of round is this for both of the teams
void CheckRoundBuyStatus( void )
{
	if( BotBuyInfo::bRoundCheckDone )
		return;

	BotBuyInfo::bRoundCheckDone = true;

	int iCTWins, iTWins;
	GetTeamWins( iCTWins, iTWins );
	int mr = g_cvBotMinimumRounds.GetInt();
	int startmoney = (g_pCvStartMoney? g_pCvStartMoney->GetInt() : 16000);
	int iRoundNumber = (iCTWins + iTWins + 1) % mr; // Actual round number on either half

	BotBuyInfo::isPistolRound = (iRoundNumber == 1 && startmoney < 1250);

	// Check if it's the last round
	BotBuyInfo::isLastRound[ TEAM_T % NUM_TEAMS ] = false;
	BotBuyInfo::isLastRound[ TEAM_CT % NUM_TEAMS ] = false;

	if( iRoundNumber == 0 ) // On 15th or 30th round both teams are on last round
	{
		BotBuyInfo::isLastRound[ TEAM_T % NUM_TEAMS ] = true;
		BotBuyInfo::isLastRound[ TEAM_CT % NUM_TEAMS ] = true;
	}
	else if( iCTWins == mr )
	{
		BotBuyInfo::isLastRound[ TEAM_T % NUM_TEAMS ] = true;
	}
	else if( iTWins == mr )
	{
		BotBuyInfo::isLastRound[ TEAM_CT % NUM_TEAMS ] = true;
	}

	// Check if teams should eco
	BotBuyInfo::isEcoRound[ TEAM_T % NUM_TEAMS ] = false;
	BotBuyInfo::isEcoRound[ TEAM_CT % NUM_TEAMS ] = false;

	// If this is the second round and the match started with low money, the loser team will definitely eco
	if( iRoundNumber == 2 && startmoney < 1250 )
	{
		if( iCTWins == 1 )
			BotBuyInfo::isEcoRound[ TEAM_T % NUM_TEAMS ] = true;
		else
			BotBuyInfo::isEcoRound[ TEAM_CT % NUM_TEAMS ] = true;
	}
	else if( iRoundNumber > 1  ) // Check how rich each team is
	{
		int numCTs = 0, numTs = 0;
		int poorCTs = 0, poorTs = 0;

		for( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CCSPlayer *player = (CCSPlayer*)GetIndexEntity( i );

			if( !player )
				continue;

			int team = GetTeamNumber( player );

			if( team < TEAM_T || !IsAlive( player ) )
				continue;

			if( team == TEAM_T )	++numTs;
			else					++numCTs;

			int money = GetPlayerAccount( player );
			int armor = GetPlayerArmorType( player );

			// TODO: should armor even be considered here?
			if( armor == VEST )
				money += KEVLAR_PRICE;
			else if( armor == VESTHELM )
				money += ASSAULTSUIT_PRICE;

			CBaseEntity *primary = GetPlayerWeaponInSlot( player, SLOT_PRIMARY );

			if( primary )
			{
				/*CCSWeaponInfo *pInfo = GetWeaponInfo( GetWeaponID( primary ) );

				if( pInfo )
					money += pInfo->m_iWeaponPrice;*/
				money += GetWeaponPrice( GetWeaponID( primary ) );
			}

			const int poorLimit = (team == TEAM_T ? 3500 : 3750); // T weapons are generally a bit cheaper

			if( money < poorLimit )
			{
				if( team == TEAM_T )	++poorTs;
				else					++poorCTs;
			}
		}

		if( numCTs && numTs )
		{
			float fPoorRatioT = ((float)poorTs / numTs) * 100.f;
			float fPoorRatioCT = ((float)poorCTs / numCTs) * 100.f;

			if( fPoorRatioT > 50.f )
			{
				BotBuyInfo::isEcoRound[ TEAM_T % NUM_TEAMS ] = (RandomFloat( 0.f, 100.f ) < fPoorRatioT);
			}
			if( fPoorRatioCT > 50.f )
			{
				BotBuyInfo::isEcoRound[ TEAM_CT % NUM_TEAMS ] = (RandomFloat( 0.f, 100.f ) < fPoorRatioCT);
			}
		}
	}

	// Decide if teams should have a special weapon choice for this round
	BotBuyInfo::roundSpecialWeapon[ TEAM_T % NUM_TEAMS ] = WEAPON_NONE;
	BotBuyInfo::roundSpecialWeapon[ TEAM_CT % NUM_TEAMS ] = WEAPON_NONE;

	for( int team = TEAM_T - 2; team <= TEAM_CT - 2; ++team )
	{
		if( BotBuyInfo::isPistolRound )
			continue;

		int losses = GetTeamConsecutiveLosses( team );

		// Bots may go for expensive weapons on last round of the half
		if( iRoundNumber == 0 )
		{
			if( losses >= 3 )
				continue;

			const float chance = 20.f - 5.f * losses;
			if( RandomFloat( 0.f, 100.f ) < chance )
			{
				CSWeaponID weapon;
				if( RandomInt( 0, 1 ) == 0 )
				{
					weapon = WEAPON_M249;
				}
				else
				{
					weapon = (team ? WEAPON_SG550 : WEAPON_G3SG1);
				}
				BotBuyInfo::roundSpecialWeapon[ team ] = weapon;
			}

			continue;
		}

		// Don't go for special weapons if we're winning (no reason to change tactics)
		if( losses == 0 )
			continue;

		if( BotBuyInfo::isEcoRound[ team ] )
		{
			const float chance = 10.f + 2.f * losses;
			if( RandomFloat( 0.f, 100.f ) < chance )
			{
				int which = RandomInt( 1, 100 );

				if( which <= 35 )
				{
					BotBuyInfo::roundSpecialWeapon[ team ] = WEAPON_DEAGLE;
				}
				else if( which <= 60 )
				{
					BotBuyInfo::roundSpecialWeapon[ team ] = WEAPON_MP5NAVY;
				}
				else if( which <= 75 )
				{
					BotBuyInfo::roundSpecialWeapon[ team ] = (team ? WEAPON_TMP : WEAPON_MAC10);
				}
				else if( which <= 90 )
				{
					BotBuyInfo::roundSpecialWeapon[ team ] = (team ? WEAPON_FIVESEVEN : WEAPON_ELITE);
				}
				else
				{
					BotBuyInfo::roundSpecialWeapon[ team ] = WEAPON_M3;
				}
			}
		}
		else // Not eco
		{
			const float chance = 5.f + 1.f * losses;
			if( RandomFloat( 0.f, 100.f ) < chance )
			{
				int which = RandomInt( 1, 100 );

				if( which <= 33 )
				{
					BotBuyInfo::roundSpecialWeapon[ team ] = WEAPON_P90;
				}
				else if( which <= 55 )
				{
					BotBuyInfo::roundSpecialWeapon[ team ] = WEAPON_SCOUT;
				}
				else if( which <= 75 )
				{
					BotBuyInfo::roundSpecialWeapon[ team ] = WEAPON_XM1014;
				}
				else if( which <= 90 )
				{
					BotBuyInfo::roundSpecialWeapon[ team ] = WEAPON_M3;
				}
				else
				{
					BotBuyInfo::roundSpecialWeapon[ team ] = WEAPON_M249;
				}
			}
		}
	}
}

//=======================================================================================================================

// Decide what we will be buying this round here
bool OnEnterBuyState( CCSBot *me )
{
	// TODO: Check what we already have before deciding on what to buy...

	// Call original function if bot will be given weapons
	const char *cheatWeaponString = (g_pCvBotLoadout ? g_pCvBotLoadout->GetString() : NULL);
	if( cheatWeaponString && *cheatWeaponString )
		return false;

	CheckRoundBuyStatus();

	BotBuyInfo &myInfo = s_BotBuyInfos[ GetEntityIndex( me ) % MAXPLAYERS ];
	memset( &myInfo, 0, sizeof(myInfo) ); // Reset buy info

	const int iRoundsPlayed = GetNumRoundsPlayed();
	const float skill = GetBotSkill( me );
	int moneyLeft = GetPlayerAccount( me );

	if( BotBuyInfo::isPistolRound )
	{
		// Decide what to buy on pistol round
		// Don't care about weapon preferences here

		// High-skill bots are more likely to buy armor
		const float buyVestChance = MAX( 7.5f, (skill * 50.f) );
		if( RandomFloat( 0.f, 100.f ) < buyVestChance )
		{
			myInfo.buyItemBitFlags |= FL_BUY_VEST;
			return true;
		}

		// High-skill bots are less likely to buy a deagle on pistol round
		const float buyDeagleChance = 25.f * (1.2f - skill);
		if( RandomFloat( 0.f, 100.f ) < buyDeagleChance )
		{
			myInfo.buyPistol = WEAPON_DEAGLE;
			return true;
		}

		if( GetTeamNumber( me ) == TEAM_CT )
		{
			// CTs are less likely to buy a pistol, but they might buy a kit
			const float buyPistolChance = 13.f * (1.3f - skill);

			if( RandomFloat( 0.f, 100.f ) < buyPistolChance )
			{
				int pistol = RandomInt( 1, 10 );

				if( pistol < 3 && moneyLeft >= GetWeaponPrice( WEAPON_GLOCK ) )
				{
					myInfo.buyPistol = WEAPON_GLOCK;
					moneyLeft -= GetWeaponPrice( WEAPON_GLOCK );
				}
				else if( pistol < 7 && moneyLeft >= GetWeaponPrice( WEAPON_P228 ) )
				{
					myInfo.buyPistol = WEAPON_P228;
					moneyLeft -= GetWeaponPrice( WEAPON_P228 );
				}
				else if( moneyLeft >= GetWeaponPrice( WEAPON_FIVESEVEN ) )
				{
					myInfo.buyPistol = WEAPON_FIVESEVEN;
					moneyLeft -= GetWeaponPrice( WEAPON_FIVESEVEN );
				}
			}

			// Check if we have enough money left to buy equipment
			if( moneyLeft >= DEFUSE_KIT_PRICE && MapHasBombTarget() )
			{
				const float buyKitChance = MAX( 10.f, (30.f * skill) );

				if( RandomFloat( 0.f, 100.f ) < buyKitChance )
				{
					myInfo.buyItemBitFlags |= FL_BUY_DEFUSE_KIT;
					moneyLeft -= DEFUSE_KIT_PRICE;
				}
			}
		}
		else // TERRORIST
		{
			const float buyPistolChance = 25.f * (1.0f - skill * 0.5f);

			if( RandomFloat( 0.f, 100.f ) < buyPistolChance )
			{
				int pistol = RandomInt( 1, 10 );

				if( pistol < 6 && moneyLeft >= GetWeaponPrice( WEAPON_USP ) )
				{
					myInfo.buyPistol = WEAPON_USP;
					moneyLeft -= GetWeaponPrice( WEAPON_USP );
				}
				else if( pistol < 8 && moneyLeft >= GetWeaponPrice( WEAPON_P228 ) )
				{
					myInfo.buyPistol = WEAPON_P228;
					moneyLeft -= GetWeaponPrice( WEAPON_P228 );
				}
				else if( moneyLeft >= GetWeaponPrice( WEAPON_ELITE ) )
				{
					myInfo.buyPistol = WEAPON_ELITE;
					moneyLeft -= GetWeaponPrice( WEAPON_ELITE );
				}
			}
		}

		// Decide if we should buy a grenade
		int grenade = RandomInt( 1, 100 );

		if( grenade <= 15 )
		{
			if( moneyLeft >= GetWeaponPrice( WEAPON_HEGRENADE ) )
				myInfo.buyItemBitFlags |= FL_BUY_HE;
		}
		else if( grenade <= 25 )
		{
			if( moneyLeft >= GetWeaponPrice( WEAPON_FLASHBANG ) )
				myInfo.buyItemBitFlags |= FL_BUY_1_FLASH;
		}
		else if( grenade <= 33 )
		{
			if( moneyLeft >= GetWeaponPrice( WEAPON_SMOKEGRENADE ) )
				myInfo.buyItemBitFlags |= FL_BUY_SMOKE;
		}

		return true;
	}

	int team = GetTeamNumber( me );
	CSWeaponID specialWeapon = BotBuyInfo::roundSpecialWeapon[ team % NUM_TEAMS ];

	// Not pistol round, but is it an eco round?
	if( BotBuyInfo::isEcoRound[ team % NUM_TEAMS ] )
	{
		// Should we buy the round special weapon?
		if( specialWeapon != WEAPON_NONE && moneyLeft >= GetWeaponPrice( specialWeapon ) )
		{
			const float chance = 95.f;
			if( RandomFloat( 0.f, 100.f ) < chance )
			{
				CCSWeaponInfo *pInfo = GetWeaponInfo( specialWeapon );

				if( pInfo )
				{
					if( pInfo->m_WeaponType == WEAPONTYPE_PISTOL )
						myInfo.buyPistol = specialWeapon;
					else
						myInfo.buyPrimary = specialWeapon;

					moneyLeft -= pInfo->GetRealWeaponPrice();
				}
			}
		}

		// Think about buying something else if we're not buying a special weapon
		if( !myInfo.buyPistol && !myInfo.buyPrimary )
		{
			if( team == TEAM_T )
			{
				// Terrorists may even buy a rifle, since they're cheaper on their side
				if( moneyLeft >= 3000 )
				{
					const float rifleChance = 7.5f * (1.2f - skill) + ((moneyLeft-3000) / 100.f);
					if( RandomFloat( 0.f, 100.f ) < rifleChance )
					{
						int which = RandomInt( 0, 1 );
						if( which == 0 && moneyLeft >= GetWeaponPrice( WEAPON_AK47 ) )
						{
							myInfo.buyPrimary = WEAPON_AK47;
							moneyLeft -= GetWeaponPrice( WEAPON_AK47 );
						}
						else if( moneyLeft >= GetWeaponPrice( WEAPON_GALIL ) )
						{
							myInfo.buyPrimary = WEAPON_GALIL;
							moneyLeft -= GetWeaponPrice( WEAPON_GALIL );
						}
					}
				}
				if( !myInfo.buyPrimary )
				{
					// Not buying a rifle, try to buy a cheaper primary or just a pistol
					const float chanceToBuy = 13.f + (moneyLeft / 200.f);
					if( RandomFloat( 0.f, 100.f ) < chanceToBuy )
					{
						int which = RandomInt( 1, 100 );

						if( which <= 33 && moneyLeft >= GetWeaponPrice( WEAPON_DEAGLE ) )
						{
							myInfo.buyPistol = WEAPON_DEAGLE;
							moneyLeft -= GetWeaponPrice( WEAPON_DEAGLE );
						}
						else if( which <= 47 && moneyLeft >= GetWeaponPrice( WEAPON_USP ) )
						{
							myInfo.buyPistol = WEAPON_USP;
							moneyLeft -= GetWeaponPrice( WEAPON_USP );
						}
						else if( which <= 55 && moneyLeft >= GetWeaponPrice( WEAPON_P228 ) )
						{
							myInfo.buyPistol = WEAPON_P228;
							moneyLeft -= GetWeaponPrice( WEAPON_P228 );
						}
						else if( which <= 65 && moneyLeft >= GetWeaponPrice( WEAPON_ELITE ) )
						{
							myInfo.buyPistol = WEAPON_ELITE;
							moneyLeft -= GetWeaponPrice( WEAPON_ELITE );
						}
						else if( which <= 80 && moneyLeft >= (GetWeaponPrice( WEAPON_MP5NAVY ) + 400) )
						{
							myInfo.buyPrimary = WEAPON_MP5NAVY;
							moneyLeft -= GetWeaponPrice( WEAPON_MP5NAVY );
						}
						else if( which <= 88 && moneyLeft >= (GetWeaponPrice( WEAPON_MAC10 ) + 300) )
						{
							myInfo.buyPrimary = WEAPON_MAC10;
							moneyLeft -= GetWeaponPrice( WEAPON_MAC10 );
						}
						else if( which <= 92 && skill < 0.75f && moneyLeft >= (GetWeaponPrice( WEAPON_UMP45 ) + 250) )
						{
							myInfo.buyPrimary = WEAPON_UMP45;
							moneyLeft -= GetWeaponPrice( WEAPON_UMP45 );
						}
						else if( skill < 0.75f && moneyLeft >= (GetWeaponPrice( WEAPON_M3 ) + 300) )
						{
							myInfo.buyPrimary = WEAPON_M3;
							moneyLeft -= GetWeaponPrice( WEAPON_M3 );
						}
					}
				}
			}
			else // CT
			{
				const float chanceToBuy = 15.f + (moneyLeft / 200.f);
				if( RandomFloat( 0.f, 100.f ) < chanceToBuy )
				{
					int which = RandomInt( 1, 100 );

					if( which <= 40 && moneyLeft >= GetWeaponPrice( WEAPON_DEAGLE ) )
					{
						myInfo.buyPistol = WEAPON_DEAGLE;
						moneyLeft -= GetWeaponPrice( WEAPON_DEAGLE );
					}
					else if( which <= 48 && moneyLeft >= GetWeaponPrice( WEAPON_P228 ) )
					{
						myInfo.buyPistol = WEAPON_P228;
						moneyLeft -= GetWeaponPrice( WEAPON_P228 );
					}
					else if( which <= 57 && moneyLeft >= GetWeaponPrice( WEAPON_FIVESEVEN ) )
					{
						myInfo.buyPistol = WEAPON_FIVESEVEN;
						moneyLeft -= GetWeaponPrice( WEAPON_FIVESEVEN );
					}
					else if( which <= 77 && moneyLeft >= (GetWeaponPrice( WEAPON_MP5NAVY ) + 400) )
					{
						myInfo.buyPrimary = WEAPON_MP5NAVY;
						moneyLeft -= GetWeaponPrice( WEAPON_MP5NAVY );
					}
					else if( which <= 90 && moneyLeft >= (GetWeaponPrice( WEAPON_TMP ) + 300) )
					{
						myInfo.buyPrimary = WEAPON_TMP;
						moneyLeft -= GetWeaponPrice( WEAPON_TMP );
					}
					else if( which <= 94 && skill < 0.75f && moneyLeft >= (GetWeaponPrice( WEAPON_UMP45 ) + 250) )
					{
						myInfo.buyPrimary = WEAPON_UMP45;
						moneyLeft -= GetWeaponPrice( WEAPON_UMP45 );
					}
					else if( skill < 0.75f && moneyLeft >= (GetWeaponPrice( WEAPON_M3 ) + 300) )
					{
						myInfo.buyPrimary = WEAPON_M3;
						moneyLeft -= GetWeaponPrice( WEAPON_M3 );
					}
				}
			}
		}

		// Decide if we want to buy a grenade
		int grenade = RandomInt( 1, 100 );

		if( grenade <= 18 && moneyLeft >= GetWeaponPrice( WEAPON_HEGRENADE ) )
		{
			myInfo.buyItemBitFlags |= FL_BUY_HE;
			moneyLeft -= GetWeaponPrice( WEAPON_HEGRENADE );
		}
		else if( grenade <= 30 && moneyLeft >= GetWeaponPrice( WEAPON_FLASHBANG ) )
		{
			myInfo.buyItemBitFlags |= FL_BUY_1_FLASH;
			moneyLeft -= GetWeaponPrice( WEAPON_FLASHBANG );
		}
		else if( grenade <= 33 && moneyLeft >= GetWeaponPrice( WEAPON_SMOKEGRENADE ) )
		{
			myInfo.buyItemBitFlags |= FL_BUY_SMOKE;
			moneyLeft -= GetWeaponPrice( WEAPON_SMOKEGRENADE );
		}

		// Decide if we want to buy armor
		if( moneyLeft >= KEVLAR_PRICE
		&& (myInfo.buyPistol || myInfo.buyPrimary)
		&& GetPlayerArmorType(me) == NO_ARMOR )
		{
			const float chanceToBuyArmor = MAX((15.f * skill + ((moneyLeft-KEVLAR_PRICE) / 100.f)), 75.f);
			if( RandomFloat( 0.f, 100.f ) < chanceToBuyArmor )
			{
				int which = RandomInt( 0, 2 );

				if( which == 0 && moneyLeft >= ASSAULTSUIT_PRICE )
				{
					myInfo.buyItemBitFlags |= FL_BUY_VESTHELM;
				}
				else
				{
					myInfo.buyItemBitFlags |= FL_BUY_VEST;
				}
			}
		}

		return true;
	}

	// Not eco either
	// Check weapon preferences here
	// TODO...

	return true;
}

//=======================================================================================================================

bool OnUpdateBuyState( CCSBot *me )
{
	const char *cheatWeaponString = (g_pCvBotLoadout ? g_pCvBotLoadout->GetString() : NULL);
	if( cheatWeaponString && *cheatWeaponString )
		return false;

	// TODO implement buying logic...

	return true;
}

//=======================================================================================================================