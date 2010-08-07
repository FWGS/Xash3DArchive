//=======================================================================
//			Copyright XashXT Group 2009 ©
//		     event_api.h - network event definition
//=======================================================================
#ifndef EVENT_API_H
#define EVENT_API_H

#define EVENT_API_VERSION	2

typedef struct event_args_s
{
	int	flags;
	int	entindex;		// transmitted always

	float	origin[3];
	float	angles[3];
	float	velocity[3];

	int	ducking;
	float	fparam1;
	float	fparam2;

	int	iparam1;
	int	iparam2;
	int	bparam1;
	int	bparam2;
} event_args_t;

typedef void (*pfnEventHook)( event_args_t *args );

typedef struct event_api_s
{
	int	version;
	void	(*EV_PlaySound)( struct cl_entity_s *ent, float *org, int chan, const char *samp, float vol, float attn, int flags, int pitch );
	void	(*EV_StopSound)( int ent, int channel, const char *sample );
	int	(*EV_FindModelIndex)( const char *model );
	int	(*EV_IsLocal)( int playernum );
	modtype_t	(*EV_GetModelType)( int modelIndex );
	void	(*EV_LocalPlayerViewheight)( float *view_ofs );
	void	(*EV_GetModBounds)( int modelIndex, float *mins, float *maxs );
	int	(*EV_GetModFrames)( int modelIndex );
	void	(*EV_WeaponAnimation)( int sequence, int body, float framerate );
	word	(*EV_PrecacheEvent)( int type, const char* psz );
	void	(*EV_PlaybackEvent)( int flags, const struct cl_entity_s *pInvoker, word eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
	const char *(*EV_TraceTexture)( struct cl_entity_s *pTextureEntity, const float *v1, const float *v2 );
	void	(*EV_StopAllSounds)( struct cl_entity_s *ent, int entchannel );
	void	(*EV_KillEvents)( int entnum, const char *eventname );
} event_api_t;

#endif//EVENT_API_H