//=======================================================================
//			Copyright XashXT Group 2010 �
//		   weaponinfo.h - client weapondata for predicting
//=======================================================================
#ifndef WEAPONINFO_H
#define WEAPONINFO_H

// info about weapons player might have in his/her possession
typedef struct weapon_data_s
{
	int		m_iId;
	int		m_iClip;

	float		m_flNextPrimaryAttack;
	float		m_flNextSecondaryAttack;
	float		m_flTimeWeaponIdle;

	int		m_fInReload;
	int		m_fInSpecialReload;
	float		m_flNextReload;
	float		m_flPumpTime;
	float		m_fReloadTime;

	float		m_fAimedDamage;
	float		m_fNextAimBonus;
	int		m_fInZoom;
	int		m_iWeaponState;

	int		iuser1;
	int		iuser2;
	int		iuser3;
	int		iuser4;
	float		fuser1;
	float		fuser2;
	float		fuser3;
	float		fuser4;
} weapon_data_t;

#endif//WEAPONINFO_H