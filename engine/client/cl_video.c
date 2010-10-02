//=======================================================================
//			Copyright XashXT Group 2009 ©
//			cl_video.c - avi video player
//=======================================================================

#include "common.h"
#include "client.h"
#include <vfw.h> // video for windows

/*
=================================================================

AVI PLAYING

=================================================================
*/

cvar_t			*vid_gamma;
static long		xres, yres;
static float		video_duration;
static float		cin_time;
static int		cin_frame;
static wavdata_t		cin_audio;
static movie_state_t	*cin_state;

void SCR_RebuildGammaTable( void )
{
	float	g;
	int	i;

	g = bound( 0.5f, vid_gamma->value, 2.3f );

	// screenshots gamma	
	for( i = 0; i < 256; i++ )
	{
		if( g == 1 ) clgame.ds.gammaTable[i] = i;
		else clgame.ds.gammaTable[i] = bound( 0, pow( i * ( 1.0f / 255.0f ), g ) * 255.0f, 255 );
	}
}

/*
==================
SCR_NextMovie

Called when a demo or cinematic finishes
If the "nextmovie" cvar is set, that command will be issued
==================
*/
bool SCR_NextMovie( void )
{
	string	str;

	S_StopAllSounds();
	SCR_StopCinematic();

	if( cls.movienum == -1 )
		return false; // don't play movies

	if( !cls.movies[cls.movienum][0] || cls.movienum == MAX_MOVIES )
	{
		cls.movienum = -1;
		return false;
	}

	com.snprintf( str, MAX_STRING, "movie %s\n", cls.movies[cls.movienum] );

	Cbuf_InsertText( str );
	cls.movienum++;

	return true;
}

/*
==================
SCR_StartMovies_f
==================
*/
void SCR_StartMovies_f( void )
{
	int	i, c;

	if( host.developer >= 2 )
	{
		// don't run movies where we in developer-mode
		cls.movienum = -1;
		return;
	}

	c = Cmd_Argc() - 1;
	if( c > MAX_MOVIES )
	{
		MsgDev( D_WARN, "Host_StartMovies: max %i movies in StartupVids\n", MAX_DEMOS );
		c = MAX_MOVIES;
	}

	for( i = 1; i < c + 1; i++ )
		com.strncpy( cls.movies[i-1], Cmd_Argv( i ), sizeof( cls.movies[0] ));

	if( !SV_Active() && cls.movienum != -1 && cls.state != ca_cinematic )
	{
		cls.movienum = 0;
		SCR_NextMovie ();
	}
	else cls.movienum = -1;
}
		
/*
==================
SCR_RunCinematic
==================
*/
void SCR_RunCinematic( void )
{
	if( !AVI_IsActive( cin_state ))
		return;

	if( vid_gamma->modified )
	{
		SCR_RebuildGammaTable();
		vid_gamma->modified = false;
	}

	// advances cinematic time	
	cin_time += cls.frametime;

	// stop the video after it finishes
	if( cin_time > video_duration + 0.1f )
	{
		SCR_NextMovie( );
		return;
	}

	// read the next frame
	cin_frame = AVI_GetVideoFrameNumber( cin_state, cin_time );
}

/*
==================
SCR_DrawCinematic

Returns true if a cinematic is active, meaning the view rendering
should be skipped
==================
*/
bool SCR_DrawCinematic( void )
{
	static int	last_frame = -1;
	bool		redraw = false;
	byte		*frame = NULL;

	if( !re || cin_time <= 0.0f )
		return false;

	if( cin_frame != last_frame )
	{
		frame = AVI_GetVideoFrame( cin_state, cin_frame );
		last_frame = cin_frame;
		redraw = true;
	}

	re->DrawStretchRaw( 0, 0, scr_width->integer, scr_height->integer, xres, yres, frame, redraw );

	return true;
}
  
/*
==================
SCR_PlayCinematic
==================
*/
bool SCR_PlayCinematic( const char *arg )
{
	string		path;
	const char	*fullpath;

	com.snprintf( path, sizeof( path ), "media/%s.avi", arg );
	fullpath = FS_GetDiskPath( path );

	if( FS_FileExists( path ) && !fullpath )
	{
		MsgDev( D_ERROR, "couldn't load %s from packfile. Please extract it\n", path );
		return false;
	}

	AVI_OpenVideo( cin_state, fullpath, true, true, false );
	if( !AVI_IsActive( cin_state ))
	{
		AVI_CloseVideo( cin_state );
		return false;
	}

	if( !( AVI_GetVideoInfo( cin_state, &xres, &yres, &video_duration ))) // couldn't open this at all.
	{
		AVI_CloseVideo( cin_state );
		return false;
	}

	if( AVI_GetAudioInfo( cin_state, &cin_audio ))
	{
		// begin streaming
		S_StopAllSounds();
		S_StartStreaming();
	}

	UI_SetActiveMenu( false );
	SCR_RebuildGammaTable();

	cls.state = ca_cinematic;
	cin_time = 0.0f;
	
	return true;
}

long SCR_GetAudioChunk( char *rawdata, long length )
{
	int	r;

	r = AVI_GetAudioChunk( cin_state, rawdata, cin_audio.loopStart, length );
	cin_audio.loopStart += r;	// advance play position

	return r;
}

wavdata_t *SCR_GetMovieInfo( void )
{
	if( AVI_IsActive( cin_state ))
		return &cin_audio;
	return NULL;
}
	
/*
==================
SCR_StopCinematic
==================
*/
void SCR_StopCinematic( void )
{
	if( cls.state != ca_cinematic )
		return;

	AVI_CloseVideo( cin_state );
	S_StopStreaming();
	cin_time = 0.0f;

	cls.state = ca_disconnected;
	UI_SetActiveMenu( true );
}

/*
==================
SCR_InitCinematic
==================
*/
void SCR_InitCinematic( void )
{
	AVIFileInit();

	// used for movie gamma correction
	vid_gamma = Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE, "gamma amount" );
	cin_state = AVI_GetState( CIN_MAIN );
}

/*
==================
SCR_FreeCinematic
==================
*/
void SCR_FreeCinematic( void )
{
	AVIFileExit();
}