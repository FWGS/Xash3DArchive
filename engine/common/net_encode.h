//=======================================================================
//			Copyright XashXT Group 2010 ©
//		       net_encode.h - delta encode routines
//=======================================================================
#ifndef NET_ENCODE_H
#define NET_ENCODE_H

#define DT_BYTE		BIT( 0 )	// A byte
#define DT_SHORT		BIT( 1 ) 	// 2 byte field
#define DT_FLOAT		BIT( 2 )	// A floating point field
#define DT_INTEGER		BIT( 3 )	// 4 byte integer
#define DT_ANGLE		BIT( 4 )	// A floating point angle ( will get masked correctly )
#define DT_TIMEWINDOW	BIT( 5 )	// A floating point timestamp, relative to sv.time
				// and re-encoded on the client relative to the client's clock
#define DT_STRING		BIT( 6 )	// A null terminated string, sent as 8 byte chars
#define DT_SIGNED		BIT( 7 )	// sign modificator

#define offsetof( s, m )	(size_t)&(((s *)0)->m)
#define NUM_FIELDS( x )	((sizeof( x ) / sizeof( x[0] )) - 1)

// helper macroses
#define ENTS_DEF( x )	#x, offsetof( entity_state_t, x ), sizeof( ((entity_state_t *)0)->x )
#define UCMD_DEF( x )	#x, offsetof( usercmd_t, x ), sizeof( ((usercmd_t *)0)->x )
#define EVNT_DEF( x )	#x, offsetof( event_args_t, x ), sizeof( ((event_args_t *)0)->x )
#define PHYS_DEF( x )	#x, offsetof( movevars_t, x ), sizeof( ((movevars_t *)0)->x )
#define CLDT_DEF( x )	#x, offsetof( clientdata_t, x ), sizeof( ((clientdata_t *)0)->x )
#define WPDT_DEF( x )	#x, offsetof( weapon_data_t, x ), sizeof( ((weapon_data_t *)0)->x )

enum
{
	CUSTOM_NONE = 0,
	CUSTOM_SEREVR_ENCODE,	// keyword "gamedll"
	CUSTOM_CLIENT_ENCODE,	// keyword "client"
};

// struct info (filled by engine)
typedef struct
{
	const char	*name;
	const int		offset;
	const int		size;
} delta_field_t;

typedef struct delta_s
{
	const char	*name;
	int		offset;		// in bytes
	int		flags;		// DT_INTEGER, DT_FLOAT etc
	float		multiplier;
	float		post_multiplier;	// for DEFINE_DELTA_POST
	int		bits;		// how many bits we send\receive
} delta_t;

typedef void (*pfnDeltaEncode)( delta_t *pFields, const byte *from, const byte *to );

typedef struct
{
	const char	*pName;
	const delta_field_t	*pInfo;
	const int		maxFields;	// maximum number of fields in struct
	int		numFields;	// may be merged during initialization
	delta_t		*pFields;

	// added these for custom entity encode
	int		customEncode;
	char		funcName[32];
	pfnDeltaEncode	userCallback;
	bool		bInitialized;
} delta_info_t;

//
// net_encode.c
//
void Delta_Init( void );
void Delta_Shutdown( void );
void Delta_InitFields( void );
void MSG_DeltaAddEncoder( char *name, pfnDeltaEncode encodeFunc );
int MSG_DeltaFindField( delta_t *pFields, const char *fieldname );
void MSG_DeltaSetField( delta_t *pFields, const char *fieldname );
void MSG_DeltaUnsetField( delta_t *pFields, const char *fieldname );
void MSG_DeltaSetFieldByIndex( struct delta_s *pFields, int fieldNumber );
void MSG_DeltaUnsetFieldByIndex( struct delta_s *pFields, int fieldNumber );

// encode routines
void MSG_WriteDeltaUsercmd( bitbuf_t *msg, usercmd_t *from, usercmd_t *to );
void MSG_ReadDeltaUsercmd( bitbuf_t *msg, usercmd_t *from, usercmd_t *to );
void MSG_WriteDeltaEvent( bitbuf_t *msg, event_args_t *from, event_args_t *to );
void MSG_ReadDeltaEvent( bitbuf_t *msg, event_args_t *from, event_args_t *to );
void MSG_WriteClientData( bitbuf_t *msg, clientdata_t *from, clientdata_t *to, int timebase );
void MSG_ReadClientData( bitbuf_t *msg, clientdata_t *from, clientdata_t *to, int timebase );
void MSG_WriteDeltaEntity( entity_state_t *from, entity_state_t *to, bitbuf_t *msg, bool force, int timebase );
void MSG_ReadDeltaEntity( bitbuf_t *msg, entity_state_t *from, entity_state_t *to, int number, int timebase );

#endif//NET_ENCODE_H