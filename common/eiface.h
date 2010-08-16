//=======================================================================
//			Copyright XashXT Group 2008 ©
//		   eiface.h - interface between engine and dlls
//=======================================================================
#ifndef EIFACE_H
#define EIFACE_H

#include "cvardef.h"

#define INTERFACE_VERSION		140	// GetEntityAPI, GetEntityAPI2
#define NEW_DLL_FUNCTIONS_VERSION	2	// GetNewDLLFunctions	(Xash3D uses version 2)

typedef unsigned long	CRC32_t;

// filter console messages
typedef enum
{
	at_notice,
	at_console,		// format: [msg]
	at_warning,		// format: Warning: [msg]
	at_error,			// format: Error: [msg]
	at_aiconsole,		// same as at_console, but only shown if developer level is 5!
	at_logged			// server print to console ( only in multiplayer games ). (NOT IMPLEMENTED)
} ALERT_TYPE;

// filter client messages
typedef enum
{
	print_console,		// dev. console messages
	print_center,		// at center of the screen
	print_chat,		// level high
} PRINT_TYPE;

// for integrity checking of content on clients
typedef enum
{
	force_exactfile,		// file on client must exactly match server's file
	force_model_samebounds,	// for model files only, the geometry must fit in the same bbox
	force_model_specifybounds,	// for model files only, the geometry must fit in the specified bbox
} FORCE_TYPE;

typedef enum
{
	AREA_SOLID,		// find any solid edicts
	AREA_TRIGGERS,		// find all SOLID_TRIGGER edicts
	AREA_CUSTOM,		// find all edicts with custom contents - water, lava, fog, laders etc
} AREA_TYPE;

typedef struct
{
	int	fAllSolid;	// if true, plane is not valid
	int	fStartSolid;	// if true, the initial point was in a solid area
	int	fInOpen;		// if true trace is open
	int	fInWater;		// if true trace is in water
	float	flFraction;	// time completed, 1.0 = didn't hit anything
	vec3_t	vecEndPos;	// final position
	float	flPlaneDist;	// planes distance
	vec3_t	vecPlaneNormal;	// surface normal at impact
	edict_t	*pHit;		// entity the surface is on
	int	iHitgroup;	// 0 == generic, non zero is specific body part
} TraceResult;

// engine hands this to DLLs for functionality callbacks
typedef struct enginefuncs_s
{
	int	(*pfnPrecacheModel)( const char* s );
	int	(*pfnPrecacheSound)( const char* s );
	void	(*pfnSetModel)( edict_t *e, const char *m );
	int	(*pfnModelIndex)( const char *m );
	int	(*pfnModelFrames)( int modelIndex );
	void	(*pfnSetSize)( edict_t *e, const float *rgflMin, const float *rgflMax );
	void	(*pfnChangeLevel)( const char* s1, const char* s2 );
	edict_t*	(*pfnFindClientInPHS)( edict_t *pEdict );
	edict_t*	(*pfnEntitiesInPHS)( edict_t *pplayer );
	float	(*pfnVecToYaw)( const float *rgflVector );
	void	(*pfnVecToAngles)( const float *rgflVectorIn, float *rgflVectorOut );
	void	(*pfnMoveToOrigin)( edict_t *ent, const float *pflGoal, float dist, int iMoveType );
	void	(*pfnChangeYaw)( edict_t* ent );
	void	(*pfnChangePitch)( edict_t* ent );
	edict_t*	(*pfnFindEntityByString)( edict_t *pStartEdict, const char *pszField, const char *pszValue);
	int	(*pfnGetEntityIllum)( edict_t* pEnt );
	edict_t*	(*pfnFindEntityInSphere)( edict_t *pStartEdict, const float *org, float rad );
	edict_t*	(*pfnFindClientInPVS)( edict_t *pEdict );
	edict_t*	(*pfnEntitiesInPVS)( edict_t *pplayer );
	void	(*pfnMakeVectors)( const float *rgflVector );
	void	(*pfnAngleVectors)( const float *rgflVector, float *forward, float *right, float *up );
	edict_t*	(*pfnCreateEntity)( void );
	void	(*pfnRemoveEntity)( edict_t* e );
	edict_t*	(*pfnCreateNamedEntity)( string_t className );
	void	(*pfnMakeStatic)( edict_t *ent );
	void	(*pfnLinkEdict)( edict_t *e, int touch_triggers ); // a part of CustomPhysics implementation
	int	(*pfnDropToFloor)( edict_t* e );
	int	(*pfnWalkMove)( edict_t *ent, float yaw, float dist, int iMode );
	void	(*pfnSetOrigin)( edict_t *e, const float *rgflOrigin );
	void	(*pfnEmitSound)( edict_t *ent, int chan, const char *sample, float vol, float attn, int flags, int pitch );
	void	(*pfnEmitAmbientSound)( edict_t *ent, float *pos, const char *samp, float vol, float attn, int flags, int pitch );
	void	(*pfnTraceLine)( const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr );
	void	(*pfnTraceToss)( edict_t* pent, edict_t* pentToIgnore, TraceResult *ptr );
	int	(*pfnTraceMonsterHull)( edict_t *pEdict, const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr );
	void	(*pfnTraceHull)( const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr );
	void	(*pfnTraceModel)( const float *v1, const float *v2, int hullNumber, edict_t *pent, TraceResult *ptr );
	const char *(*pfnTraceTexture)( edict_t *pTextureEntity, const float *v1, const float *v2 );
	int	(*pfnAreaEdicts)( const float *mins, const float *maxs, edict_t **list, int maxcount, AREA_TYPE areatype ); // a part of CustomPhysics implementation
	void	(*pfnGetAimVector)( edict_t* ent, float speed, float *rgflReturn );
	void	(*pfnServerCommand)( const char* str );
	void	(*pfnServerExecute)( void );
	void	(*pfnClientCommand)( edict_t* pEdict, char* szFmt, ... );
	void	(*pfnParticleEffect)( const float *org, const float *dir, float color, float count );
	void	(*pfnLightStyle)( int style, const char* val );
	int	(*pfnDecalIndex)( const char *name );
	int	(*pfnPointContents)( const float *rgflVector );
	void	(*pfnMessageBegin)( int msg_dest, int msg_type, const float *pOrigin, edict_t *ed );
	void	(*pfnMessageEnd)( void );
	void	(*pfnWriteByte)( int iValue );
	void	(*pfnWriteChar)( int iValue );
	void	(*pfnWriteShort)( int iValue );
	void	(*pfnWriteLong)( int iValue );
	void	(*pfnWriteAngle)( float flValue );
	void	(*pfnWriteCoord)( float flValue );
	void	(*pfnWriteString)( const char *sz );
	void	(*pfnWriteEntity)( int iValue );
	cvar_t*	(*pfnCVarRegister)( const char *name, const char *value, int flags, const char *desc );
	float	(*pfnCVarGetFloat)( const char *szVarName );
	char*	(*pfnCVarGetString)( const char *szVarName );
	void	(*pfnCVarSetFloat)( const char *szVarName, float flValue );
	void	(*pfnCVarSetString)( const char *szVarName, const char *szValue );
	void	(*pfnAlertMessage)( ALERT_TYPE level, char *szFmt, ... );
	void	(*pfnDropClient)( int clientIndex );
	void*	(*pfnPvAllocEntPrivateData)( edict_t *pEdict, long cb );
	void*	(*pfnPvEntPrivateData)( edict_t *pEdict );
	void	(*pfnFreeEntPrivateData)( edict_t *pEdict );
	const char *(*pfnSzFromIndex)( string_t iString );
	string_t	(*pfnAllocString)( const char *szValue );
	entvars_t *(*pfnGetVarsOfEnt)( edict_t *pEdict );
	edict_t*	(*pfnPEntityOfEntOffset)( int iEntOffset );
	int	(*pfnEntOffsetOfPEntity)( const edict_t *pEdict );
	int	(*pfnIndexOfEdict)( const edict_t *pEdict );
	edict_t*	(*pfnPEntityOfEntIndex)( int iEntIndex );
	edict_t*	(*pfnFindEntityByVars)( entvars_t* pvars );
	void*	(*pfnGetModelPtr)( edict_t* pEdict );
	int	(*pfnRegUserMsg)( const char *pszName, int iSize );
	void	(*pfnAnimationAutomove)( const edict_t* pEdict, float flTime );
	void	(*pfnGetBonePosition)( const edict_t* pEdict, int iBone, float *rgflOrigin, float *rgflAngles );
	dword	(*pfnFunctionFromName)( const char *pName );
	const char *(*pfnNameForFunction)( dword function );
	void	(*pfnClientPrintf)( edict_t* pEdict, PRINT_TYPE ptype, const char *szMsg );
	void	(*pfnServerPrint)( const char *szMsg );
	const char *(*pfnCmd_Args)( void );
	const char *(*pfnCmd_Argv)( int argc );
	int	(*pfnCmd_Argc)( void );
	void	(*pfnGetAttachment)( const edict_t *pEdict, int iAttachment, float *rgflOrigin, float *rgflAngles );
	void	(*pfnCRC32_Init)( CRC32_t *pulCRC );
	void	(*pfnCRC32_ProcessBuffer)( CRC32_t *pulCRC, void *p, int len );
	void	(*pfnCRC32_ProcessByte)( CRC32_t *pulCRC, byte ch );
	CRC32_t	(*pfnCRC32_Final)( CRC32_t pulCRC );
	long	(*pfnRandomLong)( long lLow, long lHigh );
	float	(*pfnRandomFloat)( float flLow, float flHigh );
	void	(*pfnSetView)( const edict_t *pClient, const edict_t *pViewent );
	float	(*pfnTime)( void );					// host.realtime, not sv.time
	void	(*pfnCrosshairAngle)( const edict_t *pClient, float pitch, float yaw );
	byte*	(*pfnLoadFileForMe)( const char *filename, int *pLength );
	void	(*pfnFreeFile)( void *buffer );
	void	(*pfnEndSection)( const char *pszSectionName );
	int	(*pfnCompareFileTime)( const char *filename1, const char *filename2, int *iCompare );
	void	(*pfnGetGameDir)( char *szGetGameDir );
	void	(*pfnHostError)( const char *szFmt, ... );
	void	(*pfnFadeClientVolume)( const edict_t *pEdict, float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds );
	void	(*pfnSetClientMaxspeed)( const edict_t *pEdict, float fNewMaxspeed );
	edict_t	*(*pfnCreateFakeClient)( const char *netname ); // returns NULL if fake client can't be created
	void	(*pfnRunPlayerMove)( edict_t *fakeclient, const float *viewangles, float forwardmove, float sidemove, float upmove, word buttons, byte impulse, byte msec );
	int	(*pfnNumberOfEntities)( void );
	char*	(*pfnGetInfoKeyBuffer)( edict_t *e );			// passing in NULL gets the serverinfo
	char*	(*pfnInfoKeyValue)( char *infobuffer, char *key );
	void	(*pfnSetKeyValue)( char *infobuffer, char *key, char *value );
	void	(*pfnSetClientKeyValue)( int clientIndex, char *infobuffer, char *key, char *value );
	int	(*pfnIsMapValid)( char *filename );
	void	(*pfnStaticDecal)( const float *origin, int decalIndex, int entityIndex, int modelIndex );
	int	(*pfnPrecacheGeneric)( const char* s );
	int	(*pfnGetPlayerUserId)(edict_t *e ); // returns the server assigned userid for this player.  useful for logging frags, etc.
	void	(*pfnSoundTrack)( const char *trackname, int flags );	// playing looped soundtrack
	int	(*pfnIsDedicatedServer)( void );			// is this a dedicated server?
	cvar_t	*(*pfnCVarGetPointer)( const char *szVarName );
	uint	(*pfnGetPlayerWONId)( edict_t *e ); // returns the server assigned WONid for this player. useful for logging frags, etc. returns -1 if the edict couldn't be found in the list of clients
	void	(*pfnInfo_RemoveKey)( char *s, char *key );
	const char *(*pfnGetPhysicsKeyValue)( const edict_t *pClient, const char *key );
	void	(*pfnSetPhysicsKeyValue)( const edict_t *pClient, const char *key, const char *value );
	const char *(*pfnGetPhysicsInfoString)( const edict_t *pClient );
	word	(*pfnPrecacheEvent)( int type, const char *psz );
	void	(*pfnPlaybackEvent)( int flags, const edict_t *pInvoker, word eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
	byte	*(*pfnSetFatPVS)( const float *org );
	byte	*(*pfnSetFatPAS)( const float *org );
	int	(*pfnCheckVisibility)( const edict_t *entity, byte *pset );
	void	(*pfnDeltaSetField)	( struct delta_s *pFields, const char *fieldname );
	void	(*pfnDeltaUnsetField)( struct delta_s *pFields, const char *fieldname );
	void	(*pfnDeltaAddEncoder)( char *name, void (*conditionalencode)( struct delta_s *pFields, const byte *from, const byte *to ));
	int	(*pfnGetCurrentPlayer)( void );
	int	(*pfnCanSkipPlayer)( const edict_t *player );
	int	(*pfnDeltaFindField)( struct delta_s *pFields, const char *fieldname );
	void	(*pfnDeltaSetFieldByIndex)( struct delta_s *pFields, int fieldNumber );
	void	(*pfnDeltaUnsetFieldByIndex)( struct delta_s *pFields, int fieldNumber );
	void	(*pfnSetGroupMask)( int mask, int op );
	int	(*pfnCreateInstancedBaseline)( int classname, struct entity_state_s *baseline );
	void	(*pfnCvar_DirectSet)( cvar_t *var, char *value );
	void	(*pfnForceUnmodified)( FORCE_TYPE type, float *mins, float *maxs, const char *filename );
	void	(*pfnGetPlayerStats)( const edict_t *pClient, int *ping, int *packet_loss );
	void	(*pfnAddServerCommand)( const char *cmd_name, void (*function)(void), const char *cmd_desc );
	const char *(*pfnGetPlayerAuthId)( edict_t *e );
	// ONLY ADD NEW FUNCTIONS TO THE END OF THIS STRUCT.  INTERFACE VERSION IS FROZEN AT 138
} enginefuncs_t;

// passed to pfnKeyValue
typedef struct KeyValueData_s
{
	char	*szClassName;	// in: entity classname
	char	*szKeyName;	// in: name of key
	char	*szValue;		// in: value of key
	long	fHandled;		// out: DLL sets to true if key-value pair was understood
} KeyValueData;

typedef struct
{
	char	mapName[32];
	char	landmarkName[32];
	edict_t	*pentLandmark;
	vec3_t	vecLandmarkOrigin;
} LEVELLIST;

typedef struct 
{
	int	id;		// ordinal ID of this entity (used for entity <--> pointer conversions)
	edict_t	*pent;		// pointer to the in-game entity

	int	location;		// offset from the base data of this entity
	int	size;		// byte size of this entity's data
	int	flags;		// bit mask of transitions that this entity is in the PVS of
	string_t	classname;	// entity class name
} ENTITYTABLE;

#define FTYPEDESC_GLOBAL		0x0001	// This field is masked for global entity save/restore
#define MAX_LEVEL_CONNECTIONS		16	// These are encoded in the lower 16bits of ENTITYTABLE->flags

#define FENTTABLE_GLOBAL		0x10000000
#define FENTTABLE_MOVEABLE		0x20000000
#define FENTTABLE_REMOVED		0x40000000
#define FENTTABLE_PLAYER		0x80000000

typedef struct saverestore_s
{
	char		*pBaseData;	// start of all entity save data
	char		*pCurrentData;	// current buffer pointer for sequential access
	int		size;		// current data size
	int		bufferSize;	// total space for data (Valve used 512 kb's)
	int		tokenSize;	// always equal 0 (probably not used)
	int		tokenCount;	// number of elements in the pTokens table (Valve used 4096 tokens)
	char		**pTokens;	// hash table of entity strings (sparse)
	int		currentIndex;	// holds a global entity table ID
	int		tableCount;	// number of elements in the entity table (numEntities)
	int		connectionCount;	// number of elements in the levelList[]
	ENTITYTABLE	*pTable;		// array of ENTITYTABLE elements (1 for each entity)
	LEVELLIST		levelList[MAX_LEVEL_CONNECTIONS]; // list of connections from this level

	// smooth transition
	int		fUseLandmark;
	char		szLandmarkName[20];	// landmark we'll spawn near in next level
	vec3_t		vecLandmarkOffset;	// for landmark transitions
	float		time;		// current sv.time
	char		szCurrentMapName[32];// To check global entities
} SAVERESTOREDATA;

typedef enum _fieldtypes
{
	FIELD_FLOAT = 0,		// any floating point value
	FIELD_STRING,		// a string ID (return from ALLOC_STRING)
	FIELD_ENTITY,		// an entity offset (EOFFSET)
	FIELD_CLASSPTR,		// CBaseEntity *
	FIELD_EHANDLE,		// entity handle
	FIELD_EVARS,		// EVARS *
	FIELD_EDICT,		// edict_t *, or edict_t *  (same thing)
	FIELD_VECTOR,		// any vector
	FIELD_POSITION_VECTOR,	// a world coordinate (these are fixed up across level transitions automagically)
	FIELD_POINTER,		// arbitrary data pointer... to be removed, use an array of FIELD_CHARACTER
	FIELD_INTEGER,		// any integer or enum
	FIELD_FUNCTION,		// a class function pointer (Think, Use, etc)
	FIELD_BOOLEAN,		// boolean, implemented as an int, I may use this as a hint for compression
	FIELD_SHORT,		// 2 byte integer
	FIELD_CHARACTER,		// a byte
	FIELD_TIME,		// a floating point time (these are fixed up automatically too!)
	FIELD_MODELNAME,		// engine string that is a model name (needs precache)
	FIELD_SOUNDNAME,		// engine string that is a sound name (needs precache)
	FIELD_WEAPONTIME,		// custom field for predicted and normal weapons

	FIELD_TYPECOUNT,		// MUST BE LAST
} FIELDTYPE;

typedef struct 
{
	FIELDTYPE	fieldType;
	char	*fieldName;
	int	fieldOffset;
	short	fieldSize;
	short	flags;
} TYPEDESCRIPTION;

#define offsetof( s, m )				(size_t)&(((s *)0)->m)
#define ARRAYSIZE( p )				(sizeof( p ) / sizeof( p[0] ))
#define _FIELD( type, name, fieldtype, count, flags )	{ fieldtype, #name, offsetof( type, name ), count, flags }
#define DEFINE_FIELD( type, name, fieldtype )		_FIELD( type, name, fieldtype, 1, 0 )
#define DEFINE_ARRAY( type, name, fieldtype, count )	_FIELD( type, name, fieldtype, count, 0 )
#define DEFINE_ENTITY_FIELD( name, fieldtype )		_FIELD( entvars_t, name, fieldtype, 1, 0 )
#define DEFINE_ENTITY_FIELD_ARRAY( name, fieldtype, count )	_FIELD( entvars_t, name, fieldtype, count, 0 )
#define DEFINE_ENTITY_GLOBAL_FIELD( name, fieldtype )	_FIELD( entvars_t, name, fieldtype, 1, FTYPEDESC_GLOBAL )
#define DEFINE_GLOBAL_FIELD( type, name, fieldtype )	_FIELD( type, name, fieldtype, 1, FTYPEDESC_GLOBAL )

typedef struct 
{
	// initialize/shutdown the game (one-time call after loading of game .dll )
	void	(*pfnGameInit)( void );				
	int	(*pfnSpawn)( edict_t *pent );
	void	(*pfnThink)( edict_t *pent );
	void	(*pfnUse)( edict_t *pentUsed, edict_t *pentOther );
	void	(*pfnTouch)( edict_t *pentTouched, edict_t *pentOther );
	void	(*pfnBlocked)( edict_t *pentBlocked, edict_t *pentOther );
	void	(*pfnKeyValue)( edict_t *pentKeyvalue, KeyValueData *pkvd );
	void	(*pfnSave)( edict_t *pent, SAVERESTOREDATA *pSaveData );
	int 	(*pfnRestore)( edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity );
	void	(*pfnSetAbsBox)( edict_t *pent );

	void	(*pfnSaveWriteFields)( SAVERESTOREDATA*, const char*, void*, TYPEDESCRIPTION*, int );
	void	(*pfnSaveReadFields)( SAVERESTOREDATA*, const char*, void*, TYPEDESCRIPTION*, int );
	void	(*pfnSaveGlobalState)( SAVERESTOREDATA * );
	void	(*pfnRestoreGlobalState)( SAVERESTOREDATA * );
	void	(*pfnResetGlobalState)( void );

	int	(*pfnClientConnect)( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128] );
	
	void	(*pfnClientDisconnect)( edict_t *pEntity );
	void	(*pfnClientKill)( edict_t *pEntity );
	void	(*pfnClientPutInServer)( edict_t *pEntity );
	void	(*pfnClientCommand)( edict_t *pEntity );
	void	(*pfnClientUserInfoChanged)( edict_t *pEntity, char *infobuffer );
	void	(*pfnServerActivate)( edict_t *pEdictList, int edictCount, int clientMax );
	void	(*pfnServerDeactivate)( void );
	void	(*pfnPlayerPreThink)( edict_t *pEntity );
	void	(*pfnPlayerPostThink)( edict_t *pEntity );

	void	(*pfnStartFrame)( void );
	void	(*pfnParmsNewLevel)( void );
	void	(*pfnParmsChangeLevel)( void );

	 // returns string describing current .dll.  E.g., TeamFotrress 2, Half-Life
	const char *(*pfnGetGameDescription)( void );     

	// Notify dll about a player customization. (completely ignored in Xash3D)
	void	(*pfnPlayerCustomization)( edict_t *pEntity, void *pUnused );

	// Spectator funcs
	void	(*pfnSpectatorConnect)( edict_t *pEntity );
	void	(*pfnSpectatorDisconnect)( edict_t *pEntity );
	void	(*pfnSpectatorThink)( edict_t *pEntity );

	// Notify game .dll that engine is going to shut down. Allows mod authors to set a breakpoint.
	void	(*pfnSys_Error)( const char *error_string );

	void	(*pfnPM_Move)( struct playermove_s *ppmove, int server );
	void	(*pfnPM_Init)( struct playermove_s *ppmove );
	char	(*pfnPM_FindTextureType)( char *name );
	void	(*pfnSetupVisibility)( edict_t *pViewEntity, edict_t *pClient, byte **pvs, byte **pas );
	void	(*pfnUpdateClientData)( const edict_t *ent, int sendweapons, struct clientdata_s *cd );

	int	(*pfnAddToFullPack)( struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, byte *pSet );
	void	(*pfnCreateBaseline)( int player, int eindex, struct entity_state_s *baseline, edict_t *entity, int playermodelindex, vec3_t player_mins, vec3_t player_maxs );
	void	(*pfnRegisterEncoders)( void );
	int	(*pfnGetWeaponData)( edict_t *player, struct weapon_data_s *info );
	void	(*pfnCmdStart)( const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed );
	void	(*pfnCmdEnd)( const edict_t *player );

	int	(*pfnConnectionlessPacket)( const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size );
	int	(*pfnGetHullBounds)( int hullnumber, float *mins, float *maxs );
	void	(*pfnCreateInstancedBaselines)( void );
	int	(*pfnInconsistentFile)( const edict_t *player, const char *filename, char *disconnect_message );
	int	(*pfnAllowLagCompensation)( void );
} DLL_FUNCTIONS;

typedef struct
{
	void	(*pfnOnFreeEntPrivateData)( edict_t *pEnt );		// ñalled right before the object's memory is freed. 
	void	(*pfnGameShutdown)( void );
	int	(*pfnShouldCollide)( edict_t *pentTouched, edict_t *pentOther );
	int	(*pfnCreate)( edict_t *pent, const char *szName );	// passed through pfnCreate (0 is attempt to create, -1 is reject)
	int	(*pfnPhysicsEntity)( edict_t *pEntity );		// run custom physics for each entity (return 0 to use engine physic)
} NEW_DLL_FUNCTIONS;

typedef int (*APIFUNCTION)( DLL_FUNCTIONS *pFunctionTable, int interfaceVersion );
typedef int (*APIFUNCTION2)( DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion );

#endif//EIFACE_H