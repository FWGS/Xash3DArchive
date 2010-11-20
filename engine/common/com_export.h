//=======================================================================
//			Copyright XashXT Group 2009 ©
//		com_export.h - safe calls exports from other libraries
//=======================================================================
#ifndef COM_EXPORT_H
#define COM_EXPORT_H

#ifdef __cplusplus
extern "C" {
#endif

// linked interfaces
extern stdlib_api_t	com;
extern render_exp_t	*re;

#ifdef __cplusplus
}
#endif

typedef int	sound_t;

typedef struct
{
	string	name;
	int	entnum;
	int	entchannel;
	vec3_t	origin;
	float	volume;
	float	attenuation;
	qboolean	looping;
	int	pitch;
} soundlist_t;

#endif//COM_EXPORT_H