/*
surgeon:	corrections for odd_display in many places
		serverbrowser added from quakeforge
		setup menu added from zquake
		detail menu created
*/

/* 
Copyright (C) 1996-1997 Id Software, Inc. 
 
This program is free software; you can redistribute it and/or 
modify it under the terms of the GNU General Public License 
as published by the Free Software Foundation; either version 2 
of the License, or (at your option) any later version. 
 
This program is distributed in the hope that it will be useful, 
but WITHOUT ANY WARRANTY; without even the implied warranty of 
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.   
 
See the GNU General Public License for more details. 
 
You should have received a copy of the GNU General Public License 
along with this program; if not, write to the Free Software 
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. 

*/ 

#include "quakedef.h"
#include "winquake.h"
#include "cl_slist.h"
#include "sound.h" 

#ifdef PROXY
// quakeforge ping support
#ifdef AMIGA
#define	ntohs(x) x
#endif
#endif

//surgeon start
extern int odd_display;
//surgeon end 

void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int key);

#ifdef PROXY
enum {m_none, m_main, m_singleplayer, m_load, m_save, m_multiplayer, m_setup, m_net, m_options, m_video, m_keys, m_help, m_quit, m_serialconfig, m_modemconfig, m_lanconfig, m_gameoptions, m_search, m_slist, m_proxy, m_sedit, m_details, } m_state;
#else
enum {m_none, m_main, m_singleplayer, m_load, m_save, m_multiplayer, m_setup, m_net, m_options, m_video, m_keys, m_help, m_quit, m_serialconfig, m_modemconfig, m_lanconfig, m_gameoptions, m_search, m_slist} m_state;
#endif

void M_Menu_Main_f (void);
	void M_Menu_SinglePlayer_f (void);
		void M_Menu_Load_f (void);
		void M_Menu_Save_f (void);
	void M_Menu_MultiPlayer_f (void);
#ifdef PROXY
		void M_Menu_SEdit_f (void); 
		void M_Menu_Setup_f (void);
#endif
//		void M_Menu_Net_f (void);
	void M_Menu_Options_f (void);
#ifdef PROXY
		void M_Menu_Details_f (void); //surgeon
		void M_Menu_Proxy_f (void); //surgeon
#endif
		void M_Menu_Keys_f (void);
		void M_Menu_Video_f (void);
	void M_Menu_Help_f (void);
	void M_Menu_Quit_f (void);
void M_Menu_SerialConfig_f (void);
	void M_Menu_ModemConfig_f (void);
void M_Menu_LanConfig_f (void);
void M_Menu_GameOptions_f (void);
void M_Menu_Search_f (void);
void M_Menu_ServerList_f (void);

void M_Main_Draw (void);
	void M_SinglePlayer_Draw (void);
		void M_Load_Draw (void);
		void M_Save_Draw (void);
	void M_MultiPlayer_Draw (void);
		void M_Setup_Draw (void);
		void M_Net_Draw (void);
		void M_ServerList_Draw (void);
	void M_Options_Draw (void);
#ifdef PROXY
		void M_Details_Draw (void); //surgeon
		void M_Proxy_Draw (void); //surgeon
#endif
		void M_Keys_Draw (void);
		void M_Video_Draw (void);
	void M_Help_Draw (void);
	void M_Quit_Draw (void);
void M_SerialConfig_Draw (void);
	void M_ModemConfig_Draw (void);
void M_LanConfig_Draw (void);
void M_GameOptions_Draw (void);
void M_Search_Draw (void);

void M_Main_Key (int key);
	void M_SinglePlayer_Key (int key);
		void M_Load_Key (int key);
		void M_Save_Key (int key);
	void M_MultiPlayer_Key (int key);
#ifdef PROXY
		void M_Setup_Key (int key);
#endif
		void M_Net_Key (int key);
	void M_Options_Key (int key);
		void M_Keys_Key (int key);
#ifdef PROXY
		void M_Details_Key (int key); //surgeon
		void M_Proxy_Key (int key); //surgeon
#endif
		void M_Video_Key (int key);
	void M_Help_Key (int key);
	void M_Quit_Key (int key);
void M_SerialConfig_Key (int key);
	void M_ModemConfig_Key (int key);
void M_LanConfig_Key (int key);
void M_GameOptions_Key (int key);
void M_Search_Key (int key);
void M_ServerList_Key (int key);

qboolean	m_entersound;		// play after drawing a frame, so caching
								// won't disrupt the sound
qboolean	m_recursiveDraw;

int			m_return_state;
qboolean	m_return_onerror;
char		m_return_reason [32];

#define StartingGame	(m_multiplayer_cursor == 1)
#define JoiningGame		(m_multiplayer_cursor == 0)
#define SerialConfig	(m_net_cursor == 0)
#define DirectConfig	(m_net_cursor == 1)
#define	IPXConfig		(m_net_cursor == 2)
#define	TCPIPConfig		(m_net_cursor == 3)

void M_ConfigureNetSubsystem(void);

//=============================================================================
/* Support Routines */

/*
================
M_DrawCharacter

Draws one solid graphics character
================
*/
void M_DrawCharacter (int cx, int line, int num)
{
	Draw_Character ( cx + ((vid.width - 320)>>1), line, num);
}

void M_Print (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, (*str)+128);
		str++;
		cx += 8;
	}
}

void M_PrintWhite (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, *str);
		str++;
		cx += 8;
	}
}

void M_DrawTransPic (int x, int y, qpic_t *pic)
{
	Draw_TransPic (x + ((vid.width - 320)>>1), y, pic);
}

void M_DrawPic (int x, int y, qpic_t *pic)
{
	Draw_Pic (x + ((vid.width - 320)>>1), y, pic);
}

byte identityTable[256];
byte translationTable[256];

void M_BuildTranslationTable(int top, int bottom)
{
	int		j;
	byte	*dest, *source;

	for (j = 0; j < 256; j++)
		identityTable[j] = j;
	dest = translationTable;
	source = identityTable;
	memcpy (dest, source, 256);

	if (top < 128)	// the artists made some backwards ranges.  sigh.
		memcpy (dest + TOP_RANGE, source + top, 16);
	else
		for (j=0 ; j<16 ; j++)
			dest[TOP_RANGE+j] = source[top+15-j];

	if (bottom < 128)
		memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
	else
		for (j=0 ; j<16 ; j++)
			dest[BOTTOM_RANGE+j] = source[bottom+15-j];		
}


void M_DrawTransPicTranslate (int x, int y, qpic_t *pic)
{
	Draw_TransPicTranslate (x + ((vid.width - 320)>>1), y, pic, translationTable);
}


void M_DrawTextBox (int x, int y, int width, int lines)
{
	qpic_t	*p;
	int		cx, cy;
	int		n;

//surgeon start
if(odd_display)
	return;
//surgeon end

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp");
	M_DrawTransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_bl.lmp");
	M_DrawTransPic (cx, cy+8, p);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp");
		M_DrawTransPic (cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++)
		{
			cy += 8;

			if (n == 1)
				p = Draw_CachePic ("gfx/box_mm2.lmp");
			M_DrawTransPic (cx, cy, p);
		}
		p = Draw_CachePic ("gfx/box_bm.lmp");
		M_DrawTransPic (cx, cy+8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_CachePic ("gfx/box_tr.lmp");
	M_DrawTransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_mr.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_br.lmp");
	M_DrawTransPic (cx, cy+8, p);
}

//=============================================================================

int m_save_demonum;
		
/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f (void)
{
	m_entersound = true;

	if (key_dest == key_menu)
	{
		if (m_state != m_main)
		{
			M_Menu_Main_f ();
			return;
		}
		key_dest = key_game;
		m_state = m_none;
		return;
	}
/*
	if (key_dest == key_console)
	{
		Con_ToggleConsole_f ();
	}
*/
	else
	{
		M_Menu_Main_f ();
	}
}

		
//=============================================================================
/* MAIN MENU */

int	m_main_cursor;
#define	MAIN_ITEMS	5


void M_Menu_Main_f (void)
{
	if (key_dest != key_menu)
	{
		m_save_demonum = cls.demonum;
		cls.demonum = -1;
	}
	key_dest = key_menu;
	m_state = m_main;
	m_entersound = true;
}
				

void M_Main_Draw (void)
{
	int		f;
	qpic_t	*p;

 if(!odd_display)
 {
	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_main.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_DrawTransPic (72, 32, Draw_CachePic ("gfx/mainmenu.lmp") );
 }
 else
 {
	M_DrawTransPic (16, 2, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_main.lmp");
	M_DrawPic ( (320-p->width)/2, 2, p);
	M_DrawTransPic (72, 16, Draw_CachePic ("gfx/mainmenu.lmp") );
 }
	f = (int)(realtime * 10)%6;
if(!odd_display)	
	M_DrawTransPic (54, 32 + m_main_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );
else
	M_DrawTransPic (54, 16 + m_main_cursor * 10,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );

}


void M_Main_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		key_dest = key_game;
		m_state = m_none;
		cls.demonum = m_save_demonum;
		if (cls.demonum != -1 && !cls.demoplayback && cls.state == ca_disconnected)
			CL_NextDemo ();
		break;
		
	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = true;

		switch (m_main_cursor)
		{
		case 0:
			M_Menu_SinglePlayer_f ();
			break;

		case 1:
			M_Menu_MultiPlayer_f ();
			break;

		case 2:
			M_Menu_Options_f ();
			break;

		case 3:
			M_Menu_Help_f ();
			break;

		case 4:
			M_Menu_Quit_f ();
			break;
		}
	}
}


//=============================================================================
/* OPTIONS MENU */

#ifdef PROXY
#define	OPTIONS_ITEMS	17
#else
#define	OPTIONS_ITEMS	16
#endif

#define	SLIDER_RANGE	10

int		options_cursor;

void M_Menu_Options_f (void)
{
	key_dest = key_menu;
	m_state = m_options;
	m_entersound = true;
}

#ifdef PROXY
void M_AdjustSliders (int dir)
{
  S_LocalSound ("misc/menu3.wav");

  switch (options_cursor)
  {
  case 4: // screen size
    scr_viewsize.value += dir * 10;
    if (scr_viewsize.value < 30)
      scr_viewsize.value = 30;
    if (scr_viewsize.value > 120)
      scr_viewsize.value = 120;
    Cvar_SetValue ("viewsize", scr_viewsize.value);
    break;
  case 5: // gamma
    v_gamma.value -= dir * 0.05;
    if (v_gamma.value < 0.5)
      v_gamma.value = 0.5;
    if (v_gamma.value > 1)
      v_gamma.value = 1;
    Cvar_SetValue ("gamma", v_gamma.value);
    break;
  case 6: // mouse speed
    sensitivity.value += dir * 0.5;
    if (sensitivity.value < 1)
      sensitivity.value = 1;
    if (sensitivity.value > 11)
      sensitivity.value = 11;
    Cvar_SetValue ("sensitivity", sensitivity.value);
    break;
  case 7: // music volume
#ifdef _WIN32
    bgmvolume.value += dir * 1.0;
#else
    bgmvolume.value += dir * 0.1;
#endif
    if (bgmvolume.value < 0)
      bgmvolume.value = 0;
    if (bgmvolume.value > 1)
      bgmvolume.value = 1;
    Cvar_SetValue ("bgmvolume", bgmvolume.value);
    break;
  case 8: // sfx volume
    volume.value += dir * 0.1;
    if (volume.value < 0)
      volume.value = 0;
    if (volume.value > 1)
      volume.value = 1;
    Cvar_SetValue ("volume", volume.value);
    break;
    
  case 9: // allways run
    if (cl_forwardspeed.value > 200)
    {
      Cvar_SetValue ("cl_forwardspeed", 200);
      Cvar_SetValue ("cl_backspeed", 200);
    }
    else
    {
      Cvar_SetValue ("cl_forwardspeed", 400);
      Cvar_SetValue ("cl_backspeed", 400);
    }
    break;
  
  case 10: // invert mouse
    Cvar_SetValue ("m_pitch", -m_pitch.value);
    break;
  
  case 11:  // lookspring
    Cvar_SetValue ("lookspring", !lookspring.value);
    break;
  
  case 12:  // lookstrafe
    Cvar_SetValue ("lookstrafe", !lookstrafe.value);
    break;

  case 13:
    Cvar_SetValue ("cl_sbar", !cl_sbar.value);
    break;

  case 14:
    Cvar_SetValue ("cl_hudswap", !cl_hudswap.value);

  //case 15:
  //  break;

  case 16:  // _windowed_mouse
#ifdef GLQUAKE
	break;
#else
    Cvar_SetValue ("_windowed_mouse", !_windowed_mouse.value);
    break;
#endif
  }
}

#else
void M_AdjustSliders (int dir)
{
  S_LocalSound ("misc/menu3.wav");

  switch (options_cursor)
  {
  case 3: // screen size
    scr_viewsize.value += dir * 10;
    if (scr_viewsize.value < 30)
      scr_viewsize.value = 30;
    if (scr_viewsize.value > 120)
      scr_viewsize.value = 120;
    Cvar_SetValue ("viewsize", scr_viewsize.value);
    break;
  case 4: // gamma
    v_gamma.value -= dir * 0.05;
    if (v_gamma.value < 0.5)
      v_gamma.value = 0.5;
    if (v_gamma.value > 1)
      v_gamma.value = 1;
    Cvar_SetValue ("gamma", v_gamma.value);
    break;
  case 5: // mouse speed
    sensitivity.value += dir * 0.5;
    if (sensitivity.value < 1)
      sensitivity.value = 1;
    if (sensitivity.value > 11)
      sensitivity.value = 11;
    Cvar_SetValue ("sensitivity", sensitivity.value);
    break;
  case 6: // music volume
#ifdef _WIN32
    bgmvolume.value += dir * 1.0;
#else
    bgmvolume.value += dir * 0.1;
#endif
    if (bgmvolume.value < 0)
      bgmvolume.value = 0;
    if (bgmvolume.value > 1)
      bgmvolume.value = 1;
    Cvar_SetValue ("bgmvolume", bgmvolume.value);
    break;
  case 7: // sfx volume
    volume.value += dir * 0.1;
    if (volume.value < 0)
      volume.value = 0;
    if (volume.value > 1)
      volume.value = 1;
    Cvar_SetValue ("volume", volume.value);
    break;
    
  case 8: // allways run
    if (cl_forwardspeed.value > 200)
    {
      Cvar_SetValue ("cl_forwardspeed", 200);
      Cvar_SetValue ("cl_backspeed", 200);
    }
    else
    {
      Cvar_SetValue ("cl_forwardspeed", 400);
      Cvar_SetValue ("cl_backspeed", 400);
    }
    break;
  
  case 9: // invert mouse
    Cvar_SetValue ("m_pitch", -m_pitch.value);
    break;
  
  case 10:  // lookspring
    Cvar_SetValue ("lookspring", !lookspring.value);
    break;
  
  case 11:  // lookstrafe
    Cvar_SetValue ("lookstrafe", !lookstrafe.value);
    break;

  case 12:
    Cvar_SetValue ("cl_sbar", !cl_sbar.value);
    break;

  case 13:
    Cvar_SetValue ("cl_hudswap", !cl_hudswap.value);

  case 15:  // _windowed_mouse
    Cvar_SetValue ("_windowed_mouse", !_windowed_mouse.value);
    break;
  }
}

#endif


void M_DrawSlider (int x, int y, float range)
{
	int	i;

	if (range < 0)
		range = 0;
	if (range > 1)
		range = 1;
	M_DrawCharacter (x-8, y, 128);
	for (i=0 ; i<SLIDER_RANGE ; i++)
		M_DrawCharacter (x + i*8, y, 129);
	M_DrawCharacter (x+i*8, y, 130);
	M_DrawCharacter (x + (SLIDER_RANGE-1)*8 * range, y, 131);
}

void M_DrawCheckbox (int x, int y, int on)
{
#if 0
	if (on)
		M_DrawCharacter (x, y, 131);
	else
		M_DrawCharacter (x, y, 129);
#endif
	if (on)
		M_Print (x, y, "on");
	else
		M_Print (x, y, "off");
}

void M_Options_Draw (void)
{
	float		r;
	qpic_t	*p;

if(!odd_display)
 {	
	
	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

#ifdef PROXY
	M_Print (16, 32, "     Customize details");
#endif

	M_Print (16, 40, "    Customize controls");
	M_Print (16, 48, "         Go to console");
	M_Print (16, 56, "     Reset to defaults");

	M_Print (16, 64, "           Screen size");
	r = (scr_viewsize.value - 30) / (120 - 30);
	M_DrawSlider (220, 64, r);

	M_Print (16, 72, "            Brightness");
	r = (1.0 - v_gamma.value) / 0.5;
	M_DrawSlider (220, 72, r);

	M_Print (16, 80, "           Mouse Speed");
	r = (sensitivity.value - 1)/10;
	M_DrawSlider (220, 80, r);

	M_Print (16, 88, "       CD Music Volume");
	r = bgmvolume.value;
	M_DrawSlider (220, 88, r);

	M_Print (16, 96, "          Sound Volume");
	r = volume.value;
	M_DrawSlider (220, 96, r);

	M_Print (16, 104,  "            Always Run");
	M_DrawCheckbox (220, 104, cl_forwardspeed.value > 200);

	M_Print (16, 112, "          Invert Mouse");
	M_DrawCheckbox (220, 112, m_pitch.value < 0);

	M_Print (16, 120, "            Lookspring");
	M_DrawCheckbox (220, 120, lookspring.value);

	M_Print (16, 128, "            Lookstrafe");
	M_DrawCheckbox (220, 128, lookstrafe.value);

	M_Print (16, 136, "    Use old status bar");
	M_DrawCheckbox (220, 136, cl_sbar.value);

	M_Print (16, 144, "      HUD on left side");
	M_DrawCheckbox (220, 144, cl_hudswap.value);

	if (vid_menudrawfn)
		M_Print (16, 152, "         Video Options");

#ifdef _WIN32
	if (modestate == MS_WINDOWED)
	{
#endif
//		M_Print (16, 158, "             Use Mouse");
//		M_DrawCheckbox (220, 158, _windowed_mouse.value);
#ifdef _WIN32
	}
#endif

 }
 else
 {	
	M_DrawTransPic (16, 2, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ( (320-p->width)/2, 2, p);

#ifdef PROXY
	M_Print (16, 2, "     Customize details");
#endif

	M_Print (16, 8, "    Customize controls");
	M_Print (16, 14, "         Go to console");
	M_Print (16, 20, "     Reset to defaults");

	M_Print (16, 26, "           Screen size");
	r = (scr_viewsize.value - 30) / (120 - 30);
	M_DrawSlider (220, 26, r);

	M_Print (16, 32, "            Brightness");
	r = (1.0 - v_gamma.value) / 0.5;
	M_DrawSlider (220, 32, r);

	M_Print (16, 38, "           Mouse Speed");
	r = (sensitivity.value - 1)/10;
	M_DrawSlider (220, 38, r);

	M_Print (16, 44, "       CD Music Volume");
	r = bgmvolume.value;
	M_DrawSlider (220, 44, r);

	M_Print (16, 50, "          Sound Volume");
	r = volume.value;
	M_DrawSlider (220, 50, r);

	M_Print (16, 56,  "            Always Run");
	M_DrawCheckbox (220, 56, cl_forwardspeed.value > 200);

	M_Print (16, 62, "          Invert Mouse");
	M_DrawCheckbox (220, 62, m_pitch.value < 0);

	M_Print (16, 68, "            Lookspring");
	M_DrawCheckbox (220, 68, lookspring.value);

	M_Print (16, 74, "            Lookstrafe");
	M_DrawCheckbox (220, 74, lookstrafe.value);

	M_Print (16, 80, "    Use old status bar");
	M_DrawCheckbox (220, 80, cl_sbar.value);

	M_Print (16, 86, "      HUD on left side");
	M_DrawCheckbox (220, 86, cl_hudswap.value);

	if (vid_menudrawfn)
		M_Print (16, 92, "         Video Options");



#ifdef _WIN32
	if (modestate == MS_WINDOWED)
	{
#endif
//		M_Print (16, 92, "             Use Mouse");
//		M_DrawCheckbox (220, 92, _windowed_mouse.value);
#ifdef _WIN32
	}
#endif

 }

// cursor

#ifdef PROXY
if(!odd_display)
	M_DrawCharacter (200, 32 + options_cursor*8, 12+((int)(realtime*4)&1));
else
	M_DrawCharacter (200, 2 + options_cursor*6, 12+((int)(realtime*4)&1));

#else
if(!odd_display)
	M_DrawCharacter (200, 40 + options_cursor*8, 12+((int)(realtime*4)&1));
else
	M_DrawCharacter (200, 8 + options_cursor*6, 12+((int)(realtime*4)&1));
#endif
}

#ifdef PROXY
void M_Options_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;
		
	case K_ENTER:
		m_entersound = true;
		switch (options_cursor)
		{
		case 0:
			M_Menu_Details_f ();
			break;

		case 1:
			M_Menu_Keys_f ();
			break;
		case 2:
			m_state = m_none; 
			Con_ToggleConsole_f ();
			break;
		case 3:
			Cbuf_AddText ("exec default.cfg\n");
			break;
		case 15:
			M_Menu_Video_f ();
			break;
		default:
			M_AdjustSliders (1);
			break;
		}
		return;
	
	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor--;
		if (options_cursor < 0)
			options_cursor = OPTIONS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor++;
		if (options_cursor >= OPTIONS_ITEMS)
			options_cursor = 0;
		break;	

	case K_LEFTARROW:
		M_AdjustSliders (-1);
		break;

	case K_RIGHTARROW:
		M_AdjustSliders (1);
		break;
	}

	if (options_cursor == 15 && vid_menudrawfn == NULL)
	{
		if (k == K_UPARROW)
			options_cursor = 14;
		else
			options_cursor = 16;
	}

	if ((options_cursor == 16) 
#ifdef _WIN32
	&& (modestate != MS_WINDOWED)
#endif
	)
	{
		if (k == K_UPARROW)
			options_cursor = 15;
		else
			options_cursor = 0;
	}
}

#else
void M_Options_Key (int k)
{
  switch (k)
  {
  case K_ESCAPE:
    M_Menu_Main_f ();
    break;
    
  case K_ENTER:
    m_entersound = true;
    switch (options_cursor)
    {
    case 0:
      M_Menu_Keys_f ();
      break;
    case 1:
      m_state = m_none;
      Con_ToggleConsole_f ();
      break;
    case 2:
      Cbuf_AddText ("exec default.cfg\n");
      break;
    case 14:
      M_Menu_Video_f ();
      break;
    default:
      M_AdjustSliders (1);
      break;
    }
    return;
  
  case K_UPARROW:
    S_LocalSound ("misc/menu1.wav");
    options_cursor--;
    if (options_cursor < 0)
      options_cursor = OPTIONS_ITEMS-1;
    break;

  case K_DOWNARROW:
    S_LocalSound ("misc/menu1.wav");
    options_cursor++;
    if (options_cursor >= OPTIONS_ITEMS)
      options_cursor = 0;
    break;  

  case K_LEFTARROW:
    M_AdjustSliders (-1);
    break;

  case K_RIGHTARROW:
    M_AdjustSliders (1);
    break;
  }

  if (options_cursor == 14 && vid_menudrawfn == NULL)
  {
    if (k == K_UPARROW)
      options_cursor = 13;
    else
      options_cursor = 0;
  }

  if ((options_cursor == 15) 
#ifdef _WIN32
  && (modestate != MS_WINDOWED)
#endif
  )
  {
    if (k == K_UPARROW)
      options_cursor = 14;
    else
      options_cursor = 0;
  }
}
#endif

//=============================================================================
/* KEYS MENU */

char *bindnames[][2] =
{
{"+attack", 		"attack"},
{"+jump", 			"jump"},
{"+forward", 		"walk forward"},
{"+back", 			"backpedal"},
{"+moveleft", 		"move left"}, 
{"+moveright", 		"move right"}, 
{"+moveup",			"swim up"}, 
{"+movedown",		"swim down"}, 
{"impulse 10", 		"change weapon"}, 
{"+speed", 			"run"},
{"+strafe", 		"sidestep"},
{"+left", 			"turn left"}, 
{"+right", 			"turn right"}, 
{"+lookup", 		"look up"},
{"+lookdown", 		"look down"},
{"centerview", 		"center view"},
{"+mlook", 			"mouse look"},
};

#define	NUMCOMMANDS	(sizeof(bindnames)/sizeof(bindnames[0]))

int		keys_cursor;
int		bind_grab;



void M_Menu_Keys_f (void)
{
	key_dest = key_menu;
	m_state = m_keys;
	m_entersound = true;
}


void M_FindKeysForCommand (char *command, int *twokeys)
{
	int		count;
	int		j;
	int		l;
	char	*b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen(command);
	count = 0;

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

void M_UnbindCommand (char *command)
{
	int		j;
	int		l;
	char	*b;

	l = strlen(command);

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
			Key_SetBinding (j, "");
	}
}


void M_Keys_Draw (void)
{
	int		i, l;
	int		keys[2];
	char	*name;
	int		x, y;
	qpic_t	*p;

	if(!odd_display)
	{
	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	if (bind_grab)
		M_Print (12, 32, "Press a key or button for this action");
	else
		M_Print (18, 32, "Enter to change, backspace to clear");
	}
	else
	{
	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ( (320-p->width)/2, 2, p);

	if (bind_grab)
		M_Print (12, 4, "Press a key or button for this action");
	else
		M_Print (18, 4, "Enter to change, backspace to clear");
	}
	
// search for known bindings
	for (i=0 ; i<NUMCOMMANDS ; i++)
	{
		if(!odd_display)
		y = 48 + 8*i;
		else
		y = 12 + 5*i;

		M_Print (16, y, bindnames[i][1]);

		l = strlen (bindnames[i][0]);
		
		M_FindKeysForCommand (bindnames[i][0], keys);
		
		if (keys[0] == -1)
		{
			M_Print (140, y, "???");
		}
		else
		{
			name = Key_KeynumToString (keys[0]);
			M_Print (140, y, name);
			x = strlen(name) * 8;
			if (keys[1] != -1)
			{
				M_Print (140 + x + 8, y, "or");
				M_Print (140 + x + 32, y, Key_KeynumToString (keys[1]));
			}
		}
	}
	if(!odd_display)
	{	
	if (bind_grab)
		M_DrawCharacter (130, 48 + keys_cursor*8, '=');
	else
		M_DrawCharacter (130, 48 + keys_cursor*8, 12+((int)(realtime*4)&1));
	}
	else
	{	
	if (bind_grab)
		M_DrawCharacter (130, 12 + keys_cursor*5, '=');
	else
		M_DrawCharacter (130, 12 + keys_cursor*5, 12+((int)(realtime*4)&1));
	}

}


void M_Keys_Key (int k)
{
	char	cmd[80];
	int		keys[2];
	
	if (bind_grab)
	{	// defining a key
		S_LocalSound ("misc/menu1.wav");
		if (k == K_ESCAPE)
		{
			bind_grab = false;
		}
		else if (k != '`')
		{
			sprintf (cmd, "bind %s \"%s\"\n", Key_KeynumToString (k), bindnames[keys_cursor][0]);			
			Cbuf_InsertText (cmd);
		}
		
		bind_grab = false;
		return;
	}
	
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Options_f ();
		break;

	case K_LEFTARROW:
	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor--;
		if (keys_cursor < 0)
			keys_cursor = NUMCOMMANDS-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor++;
		if (keys_cursor >= NUMCOMMANDS)
			keys_cursor = 0;
		break;
 
	case K_HOME: 
		S_LocalSound ("misc/menu1.wav"); 
		keys_cursor = 0; 
		break; 
 
	case K_END: 
		S_LocalSound ("misc/menu1.wav"); 
		keys_cursor = NUMCOMMANDS - 1; 
		break; 
 
	case K_ENTER:		// go into bind mode
		M_FindKeysForCommand (bindnames[keys_cursor][0], keys);
		S_LocalSound ("misc/menu2.wav");
		if (keys[1] != -1)
			M_UnbindCommand (bindnames[keys_cursor][0]);
		bind_grab = true;
		break;

	case K_BACKSPACE:		// delete bindings
	case K_DEL:				// delete bindings
		S_LocalSound ("misc/menu2.wav");
		M_UnbindCommand (bindnames[keys_cursor][0]);
		break;
	}
}


//=============================================================================
/* VIDEO MENU */

extern void VID_MenuDraw (void);
extern void VID_MenuKey (int key);

void M_Menu_Video_f (void)
{
	key_dest = key_menu;
	m_state = m_video;
	m_entersound = true;
}


void M_Video_Draw (void)
{
	(*vid_menudrawfn) ();
}


void M_Video_Key (int key)
{
	(*vid_menukeyfn) (key);
}


//=============================================================================
/* HELP MENU */

int		help_page;
#define	NUM_HELP_PAGES	6


void M_Menu_Help_f (void)
{
	key_dest = key_menu;
	m_state = m_help;
	m_entersound = true;
	help_page = 0;
}



void M_Help_Draw (void)
{
	M_DrawPic (0, 0, Draw_CachePic ( va("gfx/help%i.lmp", help_page)) );
}


void M_Help_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;
		
	case K_UPARROW:
	case K_RIGHTARROW:
		m_entersound = true;
		if (++help_page >= NUM_HELP_PAGES)
			help_page = 0;
		break;

	case K_DOWNARROW:
	case K_LEFTARROW:
		m_entersound = true;
		if (--help_page < 0)
			help_page = NUM_HELP_PAGES-1;
		break;
	}

}


//=============================================================================
/* QUIT MENU */

int   msgNumber;
int   m_quit_prevstate;
qboolean  wasInMenus;

char *quitMessage [] = 
{
/* .........1.........2.... */
  "  Are you gonna quit    ",
  "  this game just like   ",
  "   everything else?     ",
  "                        ",
 
  " Milord, methinks that  ",
  "   thou art a lowly     ",
  " quitter. Is this true? ",
  "                        ",

  " Do I need to bust your ",
  "  face open for trying  ",
  "        to quit?        ",
  "                        ",

  " Man, I oughta smack you",
  "   for trying to quit!  ",
  "     Press Y to get     ",
  "      smacked out.      ",
 
  " Press Y to quit like a ",
  "   big loser in life.   ",
  "  Press N to stay proud ",
  "    and successful!     ",
 
  "   If you press Y to    ",
  "  quit, I will summon   ",
  "  Satan all over your   ",
  "      hard drive!       ",
 
  "  Um, Asmodeus dislikes ",
  " his children trying to ",
  " quit. Press Y to return",
  "   to your Tinkertoys.  ",
 
  "  If you quit now, I'll ",
  "  throw a blanket-party ",
  "   for you next time!   ",
  "                        "
};

void M_Menu_Quit_f (void)
{
  if (m_state == m_quit)
    return;
  wasInMenus = (key_dest == key_menu);
  key_dest = key_menu;
  m_quit_prevstate = m_state;
  m_state = m_quit;
  m_entersound = true;
  msgNumber = rand()&7;
}


void M_Quit_Key (int key)
{
  switch (key)
  {
  case K_ESCAPE:
  case 'n':
  case 'N':
    if (wasInMenus)
    {
      m_state = m_quit_prevstate;
      m_entersound = true;
    }
    else
    {
      key_dest = key_game;
      m_state = m_none;
    }
    break;

  case 'Y':
  case 'y':
    key_dest = key_console;
    CL_Disconnect ();
    Sys_Quit ();
    break;

  default:
    break;
  }

}


 
void M_Menu_SinglePlayer_f (void) 
{
	m_state = m_singleplayer;
}

void M_SinglePlayer_Draw (void) 
{
	qpic_t	*p;

if(!odd_display)
{
	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
//	M_DrawTransPic (72, 32, Draw_CachePic ("gfx/sp_menu.lmp") );
}
else
{
	M_DrawTransPic (16, 2, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic ( (320-p->width)/2, 2, p);
//	M_DrawTransPic (72, 16, Draw_CachePic ("gfx/sp_menu.lmp") );
}

	if(!odd_display)
	{
	M_DrawTextBox (60, 10*8, 23, 4);	
	M_PrintWhite (92, 12*8, "QuakeWorld is for");
	M_PrintWhite (88, 13*8, "Internet play only");
	}
	else
	{
	M_PrintWhite (92, 5*8, "QuakeWorld is for");
	M_PrintWhite (88, 6*8, "Internet play only");
	}	

}

void M_SinglePlayer_Key (key) 
{
	if (key == K_ESCAPE || key == K_ENTER)
		m_state = m_main;
}
 
 
//============================================================================= 
/* MULTIPLAYER MENU */ 

int	m_multiplayer_cursor; 
#define	MULTIPLAYER_ITEMS	3 

#ifdef PROXY 
//quakeforge begin 
static server_entry_t *pingupdate = 0; 
static server_entry_t *statusupdate = 0; 
//quakeforge end
#endif
 
void M_Menu_MultiPlayer_f (void) 
{ 
	key_dest = key_menu; 
	m_state = m_multiplayer; 
	m_entersound = true; 
} 
 
 
void M_MultiPlayer_Draw (void) 
{ 
	int		f; 
	qpic_t	*p; 
 
if(!odd_display)
{
	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") ); 
	p = Draw_CachePic ("gfx/p_multi.lmp"); 
	M_DrawPic ( (320-p->width)/2, 4, p); 
	M_DrawTransPic (72, 32, Draw_CachePic ("gfx/mp_menu.lmp") ); 
}
else
{
	M_DrawTransPic (16, 2, Draw_CachePic ("gfx/qplaque.lmp") ); 
	p = Draw_CachePic ("gfx/p_multi.lmp"); 
	M_DrawPic ( (320-p->width)/2, 2, p); 
	M_DrawTransPic (72, 16, Draw_CachePic ("gfx/mp_menu.lmp") ); 
} 
	f = (int)(realtime * 10)%6; 

if(!odd_display)
	M_DrawTransPic (54, 32 + m_multiplayer_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) ); 
else
	M_DrawTransPic (54, 16 + m_multiplayer_cursor * 10,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) ); 

 
//	if (serialAvailable || ipxAvailable || tcpipAvailable) 
//		return;
 
//	M_PrintWhite ((320/2) - ((27*8)/2), 148, "No Communications Available"); 
 
} 
 
 
void M_MultiPlayer_Key (int key) 
{ 
	switch (key) 
	{ 
	case K_ESCAPE: 
		M_Menu_Main_f (); 
		break; 
 
	case K_DOWNARROW: 
		S_LocalSound ("misc/menu1.wav"); 
		if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS) 
			m_multiplayer_cursor = 0; 
		break; 
 
	case K_UPARROW: 
		S_LocalSound ("misc/menu1.wav"); 
		if (--m_multiplayer_cursor < 0) 
			m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1; 
		break; 
 
	case K_ENTER: 
		m_entersound = true; 
		switch (m_multiplayer_cursor) 
		{ 
		case 0: 
//			if (serialAvailable || ipxAvailable || tcpipAvailable) 
//				M_Menu_Net_f (); 
#ifdef PROXY
			M_Menu_ServerList_f (); 
#endif
			break; 
 
		case 1: 
//			if (serialAvailable || ipxAvailable || tcpipAvailable) 
//				M_Menu_ServerList_f (); 
			break; 
 
		case 2: 
#ifdef PROXY
			M_Menu_Setup_f (); 
#endif
			break; 
		} 
	} 
} 
 

 
 
//============================================================================= 
/* SETUP MENU */ 

#ifdef PROXY
 
int		setup_cursor = 0; 
int		setup_cursor_table[] = {40, 56, 80, 104, 140}; int		oddsetup_cursor_table[] = {20, 28, 40, 57, 70}; 		
 
char	setup_name[16]; 
char	setup_team[16]; 
int		setup_oldtop; 
int		setup_oldbottom; 
int		setup_top; 
int		setup_bottom; 
 
extern cvar_t	name, team; 
extern cvar_t	topcolor, bottomcolor; 
 
#define	NUM_SETUP_CMDS	5 
 
void M_Menu_Setup_f (void) 
{ 
	key_dest = key_menu; 
	m_state = m_setup; 
	m_entersound = true; 
	strcpy(setup_name, name.string); 
	strcpy(setup_team, team.string); 
	setup_top = setup_oldtop = (int)topcolor.value; 
	setup_bottom = setup_oldbottom = (int)bottomcolor.value; 
} 
 
 
void M_Setup_Draw (void) 
{ 
	qpic_t	*p; 
if(!odd_display)
	{ 
	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") ); 
	p = Draw_CachePic ("gfx/p_multi.lmp"); 
	M_DrawPic ( (320-p->width)/2, 4, p); 
 
	M_Print (64, 40, "Your name"); 
	M_DrawTextBox (160, 32, 16, 1); 
	M_PrintWhite (168, 40, setup_name); 
 
	M_Print (64, 56, "Your team"); 
	M_DrawTextBox (160, 48, 16, 1); 
	M_PrintWhite (168, 56, setup_team); 
 
	M_Print (64, 80, "Shirt color"); 
	M_Print (64, 104, "Pants color"); 
 
	M_DrawTextBox (64, 140-8, 14, 1); 
	M_Print (72, 140, "Accept Changes"); 
 
	p = Draw_CachePic ("gfx/bigbox.lmp"); 
	M_DrawTransPic (160, 64, p); 
	p = Draw_CachePic ("gfx/menuplyr.lmp"); 
	M_BuildTranslationTable(setup_top*16, setup_bottom*16); 
	M_DrawTransPicTranslate (172, 72, p); 
 
	M_DrawCharacter (56, setup_cursor_table [setup_cursor], 12+((int)(realtime*4)&1)); 
 
	if (setup_cursor == 0) 
		M_DrawCharacter (168 + 8*strlen(setup_name), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1)); 
 
	if (setup_cursor == 1) 
		M_DrawCharacter (168 + 8*strlen(setup_team), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1)); 
	}
	else
	{
	M_DrawTransPic (16, 2, Draw_CachePic ("gfx/qplaque.lmp") ); 
	p = Draw_CachePic ("gfx/p_multi.lmp"); 
	M_DrawPic ( (320-p->width)/2, 2, p); 
 
	M_Print (64, 20, "Your name"); 
	M_DrawTextBox (160, 16, 16, 1); 
	M_PrintWhite (168, 20, setup_name); 
 
	M_Print (64, 28, "Your team"); 
	M_DrawTextBox (160, 24, 16, 1); 
	M_PrintWhite (168, 28, setup_team); 
 
	M_Print (64, 40, "Shirt color"); 
	M_Print (64, 57, "Pants color"); 
 
	M_DrawTextBox (64, 70-8, 14, 1); 
	M_Print (72, 70, "Accept Changes"); 
 
	p = Draw_CachePic ("gfx/bigbox.lmp"); 
	M_DrawTransPic (160, 32, p); 
	p = Draw_CachePic ("gfx/menuplyr.lmp"); 
	M_BuildTranslationTable(setup_top*16, setup_bottom*16); 
	M_DrawTransPicTranslate (172, 36, p); 
 
	M_DrawCharacter (56, oddsetup_cursor_table [setup_cursor], 12+((int)(realtime*4)&1)); 
 
	if (setup_cursor == 0) 
		M_DrawCharacter (168 + 8*strlen(setup_name), oddsetup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1)); 
 
	if (setup_cursor == 1) 
		M_DrawCharacter (168 + 8*strlen(setup_team), oddsetup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1)); 
	}
} 
 
 
void M_Setup_Key (int k) 
{ 
	int			l; 

//	setup_top = setup_oldtop = (int)topcolor.value; 
//	setup_bottom = setup_oldbottom = (int)bottomcolor.value; 
 

	switch (k) 
	{ 
	case K_ESCAPE: 
		M_Menu_MultiPlayer_f (); 
		break; 
 
	case K_UPARROW: 
		S_LocalSound ("misc/menu1.wav"); 
		setup_cursor--; 
		if (setup_cursor < 0) 
			setup_cursor = NUM_SETUP_CMDS-1; 
		break; 
 
	case K_DOWNARROW: 
		S_LocalSound ("misc/menu1.wav"); 
		setup_cursor++; 
		if (setup_cursor >= NUM_SETUP_CMDS) 
			setup_cursor = 0; 
		break; 
 
	case K_LEFTARROW: 
		if (setup_cursor < 2) 
			return; 
		S_LocalSound ("misc/menu3.wav"); 
		if (setup_cursor == 2) 
			setup_top = setup_top - 1; 
		if (setup_cursor == 3) 
			setup_bottom = setup_bottom - 1; 
		break; 
	case K_RIGHTARROW: 
		if (setup_cursor < 2) 
			return; 
//forward: 
		S_LocalSound ("misc/menu3.wav"); 
		if (setup_cursor == 2) 
			setup_top = setup_top + 1; 
		if (setup_cursor == 3) 
			setup_bottom = setup_bottom + 1; 
		break; 
 
	case K_ENTER: 
//		if (setup_cursor == 0 || setup_cursor == 1) 
//			return; 
 
//		if (setup_cursor == 2 || setup_cursor == 3) 
//			goto forward; 
 
		// setup_cursor == 4 (OK) 
		Cvar_Set ("name", setup_name); 
		Cvar_Set ("team", setup_team); 

		Cvar_SetValue ("topcolor", setup_top); 
		Cvar_SetValue ("bottomcolor", setup_bottom); 
		m_entersound = true; 
		M_Menu_MultiPlayer_f (); 
		break; 
 
	case K_BACKSPACE: 
		if (setup_cursor == 0) 
		{ 
			if (strlen(setup_name)) 
				setup_name[strlen(setup_name)-1] = 0; 
		} 
 
		if (setup_cursor == 1) 
		{ 
			if (strlen(setup_team)) 
				setup_team[strlen(setup_team)-1] = 0; 
		} 
		break; 
 
	default: 
		if (k < 32 || k > 127) 
			break; 
		if (setup_cursor == 0) 
		{ 
			l = strlen(setup_name); 
			if (l < 15) 
			{ 
				setup_name[l+1] = 0; 
				setup_name[l] = k; 
			} 
		} 
		if (setup_cursor == 1) 
		{ 
			l = strlen(setup_team); 
			if (l < 15) 
			{ 
				setup_team[l+1] = 0; 
				setup_team[l] = k; 
			} 
		} 
	} 
 
	if (setup_top > 13) 
		setup_top = 0; 
	if (setup_top < 0) 
		setup_top = 13; 
	if (setup_bottom > 13) 
		setup_bottom = 0; 
	if (setup_bottom < 0) 
		setup_bottom = 13; 
} 
 


/*-------------------------------
Details Menu by SuRgEoN
-------------------------------*/
#define	NUM_DETAILS_CMDS	15

int		details_cursor = 0; 
int		details_cursor_table[] = {40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 128, 136, 144, 152};
int		odddetails_cursor_table[] = {8, 14, 20, 26, 32, 38, 44, 50, 56, 62, 68, 74, 80, 86, 92}; 		

int	setup_bfil;
int	setup_gfil;
int	setup_flame;
int	setup_fskip;
int	setup_bskip;
int	setup_dyna;
int	setup_muzz;
int	setup_fastl;
int	setup_r2g;
int	setup_fcont;
int	setup_mcap;
int	setup_expl;
int	setup_tele;
int	setup_alert;
int	setup_power;

extern cvar_t bodyfilter, gibfilter;
extern cvar_t beamskip, r_flames, r_dynamic, r_explosions, r_teleports, frameskip, r2g, cl_muzzleflash;
#ifndef GLQUAKE
extern cvar_t v_alert, v_powerup, d_mipcap, d_mipscale, r_fastcontents; 
#else
extern cvar_t gl_polyblend, gl_flashblend, gl_picmip, gl_playermip, gl_subdivide_size;
#endif

void M_Menu_Details_f (void) 
{ 
	key_dest = key_menu; 
	m_state = m_details; 
	m_entersound = true;
 
//filtering
	setup_bfil = (int)bodyfilter.value;
	setup_gfil = (int)gibfilter.value;
 	setup_flame = (int)r_flames.value;

//skipping
	setup_fskip = (int)frameskip.value; 
	setup_bskip = (int)beamskip.value; 

//light
	setup_dyna = (int)r_dynamic.value; 

	setup_r2g = (int)r2g.value; 

 	setup_muzz = (int)cl_muzzleflash.value;
#ifndef GLQUAKE
	setup_fastl = 0;

//sky & textures
	setup_fcont = (int)r_fastcontents.value;
	setup_mcap = (int)d_mipcap.value;
#else
	setup_fcont = (int)gl_subdivide_size.value;
	setup_mcap = (int)gl_picmip.value;
#endif

//particles
 	setup_expl = (int)r_explosions.value;
 	setup_tele = (int)r_teleports.value;

//palette shift
#ifndef GLQUAKE
 	setup_alert = (int)v_alert.value;
 	setup_power = (int)v_powerup.value;
#else
 	setup_power = setup_alert = (int)gl_polyblend.value;
#endif
} 
 
 
void M_Details_Draw (void) 
{ 

if(!odd_display)
	{ 
	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") ); 
	M_Print (64, 16, "Use arrowkeys.   Enter accepts");
	M_Print (64, 24, "   Space resets to defaults   ");
	M_PrintWhite  (64, 40, "   bodyfilter"); 
		if(setup_bfil > 0) 
		M_Print	(200, 40, "on");
		else
		M_Print	(200, 40, "off");
	M_PrintWhite  (64, 48, "    gibfilter"); 
		if(setup_gfil > 0) 
		M_Print	(200, 48, "on");
		else
		M_Print	(200, 48, "off");
	M_PrintWhite  (64, 56, "       flames"); 
		if(setup_flame > 0) 
		M_Print	(200, 56, "on");
		else
		M_Print	(200, 56, "off");
	M_PrintWhite  (64, 64, "    frameskip"); 
		if(setup_fskip < 1)
		M_Print	(200, 64, "off");
		else if (setup_fskip <= 1)
		M_Print	(200, 64, "1/5");
		else if (setup_fskip <= 2)
		M_Print	(200, 64, "1/3");
		else if (setup_fskip > 2)
		M_Print (200, 64, "1/2");
	M_PrintWhite  (64, 72, "     beamskip"); 
		if(setup_bskip < 1)
		M_Print	(200, 72, "off");
		else if (setup_bskip <= 1)
		M_Print	(200, 72, "1/3");
		else if (setup_bskip > 1)
		M_Print	(200, 72, "1/2");
	M_PrintWhite  (64, 80, "dynamic light"); 
		if(setup_dyna > 0) 
		M_Print	(200, 80, "on");
		else
		M_Print	(200, 80, "off");
	M_PrintWhite  (64, 88, "muzzleflashes"); 
		if(setup_muzz < 1)
		M_Print	(200, 88, "off");
		else if (setup_muzz <= 1)
		M_Print	(200, 88, "normal");
		else if (setup_muzz > 1)
		M_Print	(200, 88, "own off");
	M_PrintWhite  (64, 96, "    fastlight"); 
		if(setup_fastl > 0) 
		M_Print	(200, 96, "on");
		else
		M_Print	(200, 96, "off");
 	M_PrintWhite (64, 104, "rock->grenade"); 
		if(setup_r2g > 0) 
		M_Print	(200, 104, "on");
		else
		M_Print	(200, 104, "off");
	M_PrintWhite (64, 112, "fast contents"); 
#ifndef GLQUAKE
		if(setup_fcont > 0) 
#else
		if(setup_fcont > 128)
#endif 
		M_Print	(200, 112, "on");
		else
		M_Print	(200, 112, "off");
	M_PrintWhite (64, 120, "   explosions"); 
		if(setup_expl < 1)
		M_Print	(200, 120, "off");
		else if (setup_expl <= 1)
		M_Print	(200, 120, "normal");
		else if (setup_expl > 1)
		M_Print	(200, 120, "restricted");
	M_PrintWhite (64, 128, "    teleports"); 
		if(setup_tele < 1)
		M_Print	(200, 128, "off");
		else if (setup_tele <= 1)
		M_Print	(200, 128, "normal");
		else if (setup_tele > 1)
		M_Print	(200, 128, "restricted");
	M_PrintWhite (64, 136, " dmg/bf flash"); 
		if(setup_alert > 0) 
		M_Print	(200, 136, "on");
		else
		M_Print	(200, 136, "off");
	M_PrintWhite (64, 144, " pup palshift"); 
		if(setup_power > 0) 
		M_Print	(200, 144, "on");
		else
		M_Print	(200, 144, "off");
	M_PrintWhite  (56, 152, "texture detail"); 
		if(setup_mcap < 1)
		M_Print	(200, 152, "max");
		else if (setup_mcap <= 1)
		M_Print	(200, 152, "high");
		else if (setup_mcap <= 2)
		M_Print	(200, 152, "med");
		else if (setup_mcap > 2)
		M_Print (200, 152, "low");
 
	M_DrawCharacter (176, details_cursor_table [details_cursor], 12+((int)(realtime*4)&1)); 
	}

	else
	{
	M_DrawTransPic (16, 2, Draw_CachePic ("gfx/qplaque.lmp") ); 
	M_Print (0, 0, "use arrowkeys,space=default,enter=accept");
 
	M_PrintWhite (64, 8, "   bodyfilter"); 
		if(setup_bfil > 0) 
		M_Print	(200, 8, "on");
		else
		M_Print	(200, 8, "off");
	M_PrintWhite (64, 14, "    gibfilter"); 
		if(setup_gfil > 0) 
		M_Print	(200, 14, "on");
		else
		M_Print	(200, 14, "off");
	M_PrintWhite (64, 20, "       flames"); 
		if(setup_flame > 0) 
		M_Print	(200, 20, "on");
		else
		M_Print	(200, 20, "off");
	M_PrintWhite (64, 26, "    frameskip"); 
		if(setup_fskip < 1)
		M_Print	(200, 26, "off");
		else if (setup_fskip <= 1)
		M_Print	(200, 26, "1/5");
		else if (setup_fskip <= 2)
		M_Print	(200, 26, "1/3");
		else if (setup_fskip > 2)
		M_Print	(200, 26, "1/2");
	M_PrintWhite (64, 32, "     beamskip"); 
		if(setup_bskip < 1)
		M_Print	(200, 32, "off");
		else if (setup_bskip <= 1)
		M_Print	(200, 32, "1/3");
		else if (setup_bskip > 1)
		M_Print	(200, 32, "1/2");
	M_PrintWhite (64, 38, "dynamic light"); 
		if(setup_dyna > 0) 
		M_Print	(200, 38, "on");
		else
		M_Print	(200, 38, "off");
	M_PrintWhite (64, 44, "muzzleflashes"); 
		if(setup_muzz < 1)
		M_Print	(200, 44, "off");
		else if (setup_muzz <= 1)
		M_Print	(200, 44, "normal");
		else if (setup_muzz > 1)
		M_Print	(200, 44, "own off");
	M_PrintWhite (64, 50, "    fastlight"); 
		if(setup_fastl > 0) 
		M_Print	(200, 50, "on");
		else
		M_Print	(200, 50, "off");
	M_PrintWhite (64, 56, "rock->grenade"); 
		if(setup_r2g > 0) 
		M_Print	(200, 56, "on");
		else
		M_Print	(200, 56, "off");
	M_PrintWhite (64, 62, "fast contents"); 
#ifndef GLQUAKE
		if(setup_fcont > 0) 
#else
		if(setup_fcont > 128)
#endif 
		M_Print	(200, 62, "on");
		else
		M_Print	(200, 62, "off");
	M_PrintWhite (64, 68, "   explosions"); 
		if(setup_expl < 1)
		M_Print	(200, 68, "off");
		else if (setup_expl <= 1)
		M_Print	(200, 68, "normal");
		else if (setup_expl > 1)
		M_Print	(200, 68, "restricted");
	M_PrintWhite (64, 74, "    teleports"); 
		if(setup_tele < 1)
		M_Print	(200, 74, "off");
		else if (setup_tele <= 1)
		M_Print	(200, 74, "normal");
		else if (setup_tele > 1)
		M_Print	(200, 74, "restricted");
	M_PrintWhite (64, 80, " dmg/bf flash"); 
		if(setup_alert > 0) 
		M_Print	(200, 80, "on");
		else
		M_Print	(200, 80, "off");
	M_PrintWhite (64, 86, " pup palshift"); 
		if(setup_power > 0) 
		M_Print	(200, 86, "on");
		else
		M_Print	(200, 86, "off");
	M_PrintWhite  (56, 92, "texture detail"); 
		if(setup_mcap < 1)
		M_Print	(200, 92, "max");
		else if (setup_mcap <= 1)
		M_Print	(200, 92, "high");
		else if (setup_mcap <= 2)
		M_Print	(200, 92, "med");
		else if (setup_mcap > 2)
		M_Print (200, 92, "low");
 
	M_DrawCharacter (176, odddetails_cursor_table [details_cursor], 12+((int)(realtime*4)&1)); 
	}
} 
 
 
void M_Details_Key (int k) 
{ 
	int			l; 

	switch (k) 
	{ 
	case K_ESCAPE: 
		M_Menu_Options_f (); 
		break; 
 
	case K_UPARROW: 
		S_LocalSound ("misc/menu1.wav"); 
		details_cursor--; 
		if (details_cursor < 0) 
			details_cursor = NUM_DETAILS_CMDS-1; 
		break; 
 
	case K_DOWNARROW: 
		S_LocalSound ("misc/menu1.wav"); 
		details_cursor++; 
		if (details_cursor >= NUM_DETAILS_CMDS) 
			details_cursor = 0; 
		break; 
 
	case K_LEFTARROW: 
		S_LocalSound ("misc/menu3.wav"); 
		if (details_cursor == 0) 
			if(setup_bfil > 0)
			setup_bfil = 0; 
		if (details_cursor == 1)
			if(setup_gfil > 0) 
			setup_gfil = 0; 
		if (details_cursor == 2)
			if(setup_flame > 0) 
			setup_flame = 0; 
		if (details_cursor == 3)
		{
		if (setup_fskip <= 1)
		setup_fskip = 0;
		else if (setup_fskip <= 2)
		setup_fskip = 1;
		else if (setup_fskip > 2)
		setup_fskip = 2;
		}
		if (details_cursor == 4)
		{
		if (setup_bskip <= 1)
		setup_bskip = 0;
		else if (setup_bskip > 1)
		setup_bskip = 1;
		}
		if (details_cursor == 5)
			if(setup_dyna > 0) 
			setup_dyna = 0; 
		if (details_cursor == 6)
		{
		if (setup_muzz <= 1)
		setup_muzz = 0;
		else if (setup_muzz > 1)
		setup_muzz = 1;
		}
		if (details_cursor == 7)
			if(setup_fastl > 0) 
			setup_fastl = 0; 
		if (details_cursor == 8)
			if(setup_r2g > 0) 
			setup_r2g = 0; 
		if (details_cursor == 9)
#ifndef GLQUAKE
			if(setup_fcont > 0) 
			setup_fcont = 0; 
#else
			if(setup_fcont > 128)
			setup_fcont = 128;
#endif 

		if (details_cursor == 10)
		{
		if (setup_expl <= 1)
		setup_expl = 0;
		else if (setup_expl > 1)
		setup_expl = 1;
		}
		if (details_cursor == 11)
		{
		if (setup_tele <= 1)
		setup_tele = 0;
		else if (setup_tele > 1)
		setup_tele = 1;
		}
		if (details_cursor == 12)
			if(setup_alert > 0) 
			setup_alert = 0; 
		if (details_cursor == 13)
			if(setup_power > 0) 
			setup_power = 0; 
		if (details_cursor == 14)
		{
		if(setup_mcap < 1)
		setup_mcap = 1;
		else if (setup_mcap < 2)
		setup_mcap = 2;
		else if (setup_mcap >= 2)
		setup_mcap = 3;
		}
		break;
 
	case K_RIGHTARROW: 
		S_LocalSound ("misc/menu3.wav"); 
		if (details_cursor == 0) 
			if(setup_bfil < 1) 
			setup_bfil = 1; 
		if (details_cursor == 1)
			if(setup_gfil < 1) 
			setup_gfil = 1; 
		if (details_cursor == 2)
			if(setup_flame < 1) 
			setup_flame = 1; 
		if (details_cursor == 3)
		{
		if(setup_fskip < 1)
		setup_fskip = 1;
		else if (setup_fskip < 2)
		setup_fskip = 2;
		else if (setup_fskip >= 2)
		setup_fskip = 3;
		}
		if (details_cursor == 4)
		{
		if(setup_bskip < 1)
		setup_bskip = 1;
		else if (setup_bskip >= 1)
		setup_bskip = 2;
		}
		if (details_cursor == 5)
			if(setup_dyna < 1) 
			setup_dyna = 1; 
		if (details_cursor == 6)
		{
		if(setup_muzz < 1)
		setup_muzz = 1;
		else if (setup_muzz >= 1)
		setup_muzz = 2;
		}
		if (details_cursor == 7)
			if(setup_fastl < 1) 
			setup_fastl = 1; 
		if (details_cursor == 8)
			if(setup_r2g < 1) 
			setup_r2g = 1; 
		if (details_cursor == 9)
#ifndef GLQUAKE
			if(setup_fcont < 1) 
			setup_fcont = 1; 
#else
			if(setup_fcont < 1024) 
			setup_fcont = 1024; 
#endif
		if (details_cursor == 10)
		{
		if(setup_expl < 1)
		setup_expl = 1;
		else if (setup_expl >= 1)
		setup_expl = 2;
		}
		if (details_cursor == 11)
		{
		if(setup_tele < 1)
		setup_tele = 1;
		else if (setup_tele >= 1)
		setup_tele = 2;
		}
		if (details_cursor == 12)
			if(setup_alert < 1) 
			setup_alert = 1; 
		if (details_cursor == 13)
			if(setup_power < 1) 
			setup_power = 1; 
		if (details_cursor == 14)
		{
		if (setup_mcap <= 1)
		setup_mcap = 0;
		else if (setup_mcap <= 2)
		setup_mcap = 1;
		else if (setup_mcap > 2)
		setup_mcap = 2;
		}
		break; 

	case K_SPACE: //set vars to default
	S_LocalSound ("misc/menu3.wav"); 
	setup_bfil = 0; 
	setup_gfil = 0; 
	setup_flame = 1; 
#ifndef GLQUAKE
	setup_fcont = 0; 
#else
	setup_fcont = 128;
#endif 
	setup_fskip = 0; 
	setup_bskip = 0; 
	setup_dyna = 1;
	setup_fastl = 0;
	setup_muzz = 1;
	setup_r2g = 0;
	setup_expl = 1;
	setup_tele = 1;
	setup_alert = 1;
	setup_power = 1;
	setup_mcap = 0;
	break; 

	case K_ENTER: //set vars to new values

	Cvar_SetValue ("bodyfilter", setup_bfil);
	Cvar_SetValue ("gibfilter", setup_gfil);
	Cvar_SetValue ("r_flames", setup_flame);
	Cvar_SetValue ("beamskip", setup_bskip);
 	Cvar_SetValue ("r_explosions", setup_expl);
	Cvar_SetValue ("r_teleports", setup_tele);
	Cvar_SetValue ("r2g", setup_r2g);
	Cvar_SetValue ("frameskip", setup_fskip);
	Cvar_SetValue ("cl_muzzleflash", setup_muzz);
	Cvar_SetValue ("r_dynamic", setup_dyna);
#ifndef GLQUAKE
	Cvar_SetValue ("r_fastcontents", setup_fcont);
	Cvar_SetValue ("v_alert", setup_alert);
	Cvar_SetValue ("v_powerup", setup_power);
	Cvar_SetValue ("d_mipcap", setup_mcap);
	if (setup_mcap == 2)
	Cvar_SetValue ("d_mipscale", 2);
	else if (setup_mcap == 3)
	Cvar_SetValue ("d_mipscale", 100);
	else
	Cvar_SetValue ("d_mipscale", 1);
#else
	Cvar_SetValue ("gl_subdivide_size", setup_fcont);
	Cvar_SetValue ("gl_polyblend", setup_power);
	Cvar_SetValue ("gl_playermip", setup_mcap);
	if (setup_mcap == 2)
	Cvar_SetValue ("gl_picmip", 1);
	else if (setup_mcap == 3)
	Cvar_SetValue ("gl_picmip", 2);
	else
	Cvar_SetValue ("gl_picmip", 0);
#endif

	m_entersound = true; 
	M_Menu_Options_f (); 
	break; 
 
 
	default: 
		if (k < 32 || k > 127) 
			break; 
	} 
} 


/*------------------------------------------
serverlist menu
------------------------------------------*/
 
// SLIST -->
#define NMENU_X 48 
#define NMENU_Y 30 
#define NSTAT_X 48 
#define NSTAT_Y 122
 
#define OMENU_X 48 
#define OMENU_Y 8 
#define OSTAT_X 48 
#define OSTAT_Y 40 
 
int m_multip_cursor=0;
int m_multip_mins;
int m_multip_maxs;
int m_multip_horiz;
int m_multip_state;

void M_Menu_ServerList_f (void) {
	key_dest = key_menu;
	m_entersound = true;
//	m_state = m_multiplayer; 
	m_state = m_slist;
//	m_multip_cursor = 0;
	m_multip_mins = 0;
	if(!odd_display)
	m_multip_maxs = 10;
	else
	{
	if (vid.height == 100)
	m_multip_maxs = 3;
	else if (vid.height == 120)
	m_multip_maxs = 5;
	else
	m_multip_maxs = 7;
	}
	m_multip_horiz = 0;
	m_multip_state = 0;
}


//Quakeforge begin
void 
M_ServerList_Draw (void) 
{ 
	int         serv; 
	int         line = 1; 
	server_entry_t *cp; 
	qpic_t     *p; 
 
	static double lastping = 0; 
	int f; 

	static	int MENU_X,MENU_Y,STAT_X,STAT_Y;

if(!odd_display)
{
MENU_X = NMENU_X;
MENU_Y = NMENU_Y;
STAT_X = NSTAT_X;
STAT_Y = NSTAT_Y;
} 
else
{

	if(vid.height == 100)
	{
	MENU_X = OMENU_X;
	MENU_Y = OMENU_Y;
	STAT_X = OSTAT_X;
	STAT_Y = OSTAT_Y;
	}
	else if(vid.height == 120)
	{
	MENU_X = OMENU_X;
	MENU_Y = OMENU_Y;
	STAT_X = OSTAT_X;
	STAT_Y = OSTAT_Y + 16;
	}
	else
	{
	MENU_X = OMENU_X;
	MENU_Y = OMENU_Y;
	STAT_X = OSTAT_X;
	STAT_Y = OSTAT_Y + 40;
	}

}

if(!odd_display)
	{
	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp")); 
	p = Draw_CachePic ("gfx/p_multi.lmp"); 
	M_DrawPic ((320 - p->width) / 2, 4, p); 
	}
	else
 	{
	M_DrawTransPic (16, 2, Draw_CachePic ("gfx/qplaque.lmp")); 
	p = Draw_CachePic ("gfx/p_multi.lmp"); 
	M_DrawPic ((320 - p->width) / 2, 2, p); 
	}

	if (!slist) { 
		if(!odd_display)
		{
		M_DrawTextBox (60, 80, 23, 4); 
		M_PrintWhite (110, 12 * 8, "No server list"); 
		M_PrintWhite (140, 13 * 8, "found."); 
		}
		else
		{
		M_DrawTextBox (60, 40, 23, 4); 
		M_PrintWhite (110, 6 * 8, "No server list"); 
		M_PrintWhite (140, 7 * 8, "found."); 
		}
		return; 
	} 
	M_DrawTextBox (STAT_X, STAT_Y, 30, 7); 
	//M_DrawTextBox (STAT_X, STAT_Y + 38, 23, 3); 
	M_DrawTextBox (MENU_X, MENU_Y, 30, (m_multip_maxs - m_multip_mins) + 1); 
	for (serv = m_multip_mins; serv <= m_multip_maxs && serv < SL_Len (slist); serv++) { 
		cp = SL_Get_By_Num (slist, serv); 
		M_Print (MENU_X + 18, line * 8 + MENU_Y, 
				 va ("%1.22s", 
					 strlen (cp->desc) <= 
					 m_multip_horiz ? "" : cp->desc + m_multip_horiz)); 
		line++; 
	} 
	cp = SL_Get_By_Num (slist, m_multip_cursor); 
	M_PrintWhite (STAT_X + 10, STAT_Y + 8, "IP:"); 
	M_Print (STAT_X + 42, STAT_Y + 8, cp->server); 

	if (pingupdate && realtime - lastping >= .25) 
//was .25  - should wait longer for ping repliess
	{ 
		netadr_t addy; 
		char data[6]; 
 
		data[0] = '\377'; 
		data[1] = '\377'; 
		data[2] = '\377'; 
		data[3] = '\377'; 
		data[4] = A2A_PING; 
		data[5] = 0; 
	 
		NET_StringToAdr (pingupdate->server, &addy); 
	 
		if (!addy.port) 
			addy.port = ntohs (27500); 
 
		pingupdate->pingsent = Sys_DoubleTime (); 
		pingupdate->pongback = 0.0; 
		NET_SendPacket (6, data, addy); 
		pingupdate = pingupdate->next; 
		lastping = realtime; 

	} 
	if (statusupdate && realtime - lastping >= .25) 
	{ 
		netadr_t addy; 
		char data[] = "\377\377\377\377status";	 
	 
		NET_StringToAdr (statusupdate->server, &addy); 
	 
		if (!addy.port) 
			addy.port = ntohs (27500); 
 
		NET_SendPacket (strlen(data) + 1, data, addy); 
		statusupdate->waitstatus = 1; 
		statusupdate = statusupdate->next; 
		lastping = realtime; 
	} 
 
	if (!pingupdate && !statusupdate) 
	{ 
		int playercount = 0, i; 

		M_PrintWhite (STAT_X + 10, STAT_Y + 24, "Ping:"); 
		M_PrintWhite (STAT_X + 10, STAT_Y + 32, "Game:"); 
		M_PrintWhite (STAT_X + 10, STAT_Y + 40, "Map:"); 
		M_PrintWhite (STAT_X + 10, STAT_Y + 48, "Players:"); 
		if (cp->pongback) 
			M_Print (STAT_X + 58, STAT_Y + 24, va("%i ms", (int)(cp->pongback * 1000))); 
		else 
			M_Print (STAT_X + 58, STAT_Y + 24, "N/A"); 
		if (cp->status) 
		{ 
			for (i = 0; i < strlen(cp->status); i++) 
				if (cp->status[i] == '\n') 
					playercount++; 
			M_Print (STAT_X + 10, STAT_Y + 16, Info_ValueForKey (cp->status, "hostname")); 
			M_Print (STAT_X + 58, STAT_Y + 32, Info_ValueForKey (cp->status, "*gamedir")); 
			M_Print (STAT_X + 50, STAT_Y + 40, Info_ValueForKey (cp->status, "map")); 
			M_Print (STAT_X + 82, STAT_Y + 48, va("%i/%s", playercount, Info_ValueForKey(cp->status, "maxclients"))); 
		} 
		else 
		{ 
			M_Print (STAT_X + 58, STAT_Y + 16, "N/A"); 
			M_Print (STAT_X + 58, STAT_Y + 32, "N/A"); 
			M_Print (STAT_X + 50, STAT_Y + 40, "N/A"); 
			M_Print (STAT_X + 82, STAT_Y + 48, "N/A"); 
		} 
	} 
	else 
	{ 
		M_PrintWhite (STAT_X + 10, STAT_Y + 16, "Updating..."); 
		f = (int)(realtime * 10) % 6; 
if(!odd_display)		M_PrintWhite(STAT_X+118,STAT_Y+48,"uakeforge!"); 
else
M_PrintWhite(STAT_X+118,STAT_Y+40,"uakeforge!"); 		M_DrawTransPic(STAT_X+105,STAT_Y+38,Draw_CachePic(va("gfx/menudot%i.lmp",f+1))); 
	} 
 
	M_DrawCharacter (MENU_X + 8, (m_multip_cursor - m_multip_mins + 1) * 8 + MENU_Y, 12 + ((int) (realtime * 4) & 1)); 
} 
 
void 
M_ServerList_Key (key) 
{ 
	server_entry_t *temp; 
 
	if (!slist && key != K_ESCAPE && key != K_INS) 
		return; 
	switch (key) { 
		case K_ESCAPE: 
			M_Menu_MultiPlayer_f();
			break; 
		case K_DOWNARROW: 
			S_LocalSound ("misc/menu1.wav"); 
			m_multip_cursor++; 
			break; 
		case K_UPARROW: 
			S_LocalSound ("misc/menu1.wav"); 
			m_multip_cursor--; 
			break; 
		case K_PGUP: 
			S_LocalSound ("misc/menu1.wav"); 
			m_multip_cursor -= (m_multip_maxs - m_multip_mins); 
			if (m_multip_cursor < 0) 
				m_multip_cursor = 0; 
			break; 
		case K_PGDN: 
			S_LocalSound ("misc/menu1.wav"); 
			m_multip_cursor += (m_multip_maxs - m_multip_mins); 
			if (SL_Len (slist) - 1 < m_multip_cursor) 
				m_multip_cursor = SL_Len (slist) - 1; 
			break; 
		case K_RIGHTARROW: 
			S_LocalSound ("misc/menu1.wav"); 
			if (m_multip_horiz < 256) 
				m_multip_horiz++; 
			break; 
		case K_LEFTARROW: 
			S_LocalSound ("misc/menu1.wav"); 
			if (m_multip_horiz > 0) 
				m_multip_horiz--; 
			break; 
		case K_ENTER: 
			m_state = m_main; 
			M_ToggleMenu_f (); 
			CL_Disconnect (); 
			strncpy (cls.servername, 
					 SL_Get_By_Num (slist, m_multip_cursor)->server, 
					 sizeof (cls.servername) - 1); 
			CL_BeginServerConnect (); 
			break; 
		case 'e': 
		case 'E': 
			M_Menu_SEdit_f (); 
			break; 
		case 'p': 
		case 'P': 
			if (pingupdate || statusupdate) 
				break; 
			pingupdate = slist; 
			for (temp = slist; temp; temp = temp->next) 
			{	temp->pingsent = temp->pongback = 0.0; 
			} 
			break; 
		case 's': 
		case 'S': 
			if (pingupdate || statusupdate) 
				break; 
			statusupdate = slist; 
			for (temp = slist; temp; temp = temp->next) 
				temp->waitstatus = 0; 
			break; 
		case 'u': 
		case 'U': 
			if (pingupdate || statusupdate) 
				break; 
			pingupdate = slist; 
			statusupdate = slist; 
			for (temp = slist; temp; temp = temp->next) 
			{ 
				temp->pingsent = temp->pongback = 0.0; 
				temp->waitstatus = 0; 
			} 
			break;	 
		case K_INS:
		case   'a':
		case   'A': 
			if (pingupdate || statusupdate) 
				break; 
			S_LocalSound ("misc/menu2.wav"); 
			if (!slist) { 
				m_multip_cursor = 0; 
				slist = SL_Add (slist, cls.state >= ca_connected ? cls.servername : "127.0.0.1", cls.state == ca_connected ? Info_ValueForKey (cl.serverinfo, "hostname") : "<BLANK>"); 
			} else { 
				temp = SL_Get_By_Num (slist, m_multip_cursor); 
				slist = SL_InsB (slist, temp, cls.state >= ca_connected ? cls.servername : "127.0.0.1", cls.state >= ca_connected ? Info_ValueForKey (cl.serverinfo, "hostname") : "<BLANK>"); 
			} 
			break; 
		case K_DEL: 
		case   'd':
		case   'D': 
			if (pingupdate || statusupdate) 
				break; 
			S_LocalSound ("misc/menu2.wav"); 
			if (SL_Len (slist) > 0) { 
				slist = SL_Del (slist, SL_Get_By_Num (slist, m_multip_cursor)); 
				if (SL_Len (slist) == m_multip_cursor && slist) 
					m_multip_cursor--; 
			} 
			break; 
		case ']': 
		case '}': 
			if (pingupdate || statusupdate) 
				break; 
			S_LocalSound ("misc/menu1.wav"); 
			if (m_multip_cursor != SL_Len (slist) - 1) { 
				SL_Swap (SL_Get_By_Num (slist, m_multip_cursor), 
						 SL_Get_By_Num (slist, m_multip_cursor + 1)); 
				m_multip_cursor++; 
			} 
			break; 
		case '[': 
		case '{': 
			if (pingupdate || statusupdate) 
				break; 
			S_LocalSound ("misc/menu1.wav"); 
			if (m_multip_cursor) { 
				SL_Swap (SL_Get_By_Num (slist, m_multip_cursor), 
						 SL_Get_By_Num (slist, m_multip_cursor - 1)); 
				m_multip_cursor--; 
			} 
			break; 
		default: 
			break; 
	} 
	if (m_multip_cursor < 0) 
		m_multip_cursor = SL_Len (slist) - 1; 
	if (m_multip_cursor >= SL_Len (slist)) 
		m_multip_cursor = 0; 
	if (m_multip_cursor < m_multip_mins) { 
		m_multip_maxs -= (m_multip_mins - m_multip_cursor); 
		m_multip_mins = m_multip_cursor; 
	} 
	if (m_multip_cursor > m_multip_maxs) { 
		m_multip_mins += (m_multip_cursor - m_multip_maxs); 
		m_multip_maxs = m_multip_cursor; 
	} 
} 

#define SERV_X 60 
#define SERV_Y 64 
#define DESC_X 60 
#define DESC_Y 40 
#define SERV_L 22 
#define DESC_L 22 
 
char        serv[256]; 
char        desc[256]; 
int         serv_max; 
int         serv_min; 
int         desc_max; 
int         desc_min; 
qboolean    sedit_state; 
 
void 
M_Menu_SEdit_f (void) 
{ 
	server_entry_t *c; 
 
	key_dest = key_menu; 
	m_entersound = true; 
	m_state = m_sedit; 
	sedit_state = false; 
	c = SL_Get_By_Num (slist, m_multip_cursor); 
	strncpy (serv, c->server, 255); 
	serv[strlen (c->server) + 1] = 0; 
	strncpy (desc, c->desc, 255); 
	desc[strlen (c->desc) + 1] = 0; 
	serv_max = strlen (serv) > SERV_L ? strlen (serv) : SERV_L; 
	serv_min = serv_max - (SERV_L); 
	desc_max = strlen (desc) > DESC_L ? strlen (desc) : DESC_L; 
	desc_min = desc_max - (DESC_L); 
} 
 
void 
M_SEdit_Draw (void) 
{ 
	qpic_t     *p; 

 if(!odd_display)
 { 
	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp")); 
	p = Draw_CachePic ("gfx/p_multi.lmp"); 
	M_DrawPic ((320 - p->width) / 2, 4, p); 
 
	M_DrawTextBox (SERV_X, SERV_Y, 23, 1); 
	M_DrawTextBox (DESC_X, DESC_Y, 23, 1); 
	M_PrintWhite (SERV_X, SERV_Y - 4, "Hostname/IP:"); 
	M_PrintWhite (DESC_X, DESC_Y - 4, "Description:"); 
	M_Print (SERV_X + 9, SERV_Y + 8, va ("%1.22s", serv + serv_min)); 
	M_Print (DESC_X + 9, DESC_Y + 8, va ("%1.22s", desc + desc_min)); 
	if (sedit_state == 0) 
		M_DrawCharacter (SERV_X + 9 + 8 * (strlen (serv) - serv_min), 
						 SERV_Y + 8, 10 + ((int) (realtime * 4) & 1)); 
	if (sedit_state == 1) 
		M_DrawCharacter (DESC_X + 9 + 8 * (strlen (desc) - desc_min), 
						 DESC_Y + 8, 10 + ((int) (realtime * 4) & 1)); 
 }
 else
{
	M_DrawTransPic (16, 2, Draw_CachePic ("gfx/qplaque.lmp") ); 
	p = Draw_CachePic ("gfx/p_multi.lmp"); 
	M_DrawPic ( (320-p->width)/2, 2, p); 

	M_PrintWhite (SERV_X, 64 - 4, "Hostname/IP:"); 
	M_PrintWhite (DESC_X, 40 - 4, "Description:"); 
	M_Print (SERV_X + 9, 64 + 8, va ("%1.22s", serv + serv_min)); 
	M_Print (DESC_X + 9, 40 + 8, va ("%1.22s", desc + desc_min)); 
	if (sedit_state == 0) 
		M_DrawCharacter (SERV_X + 9 + 8 * (strlen (serv) - serv_min), 
						 64 + 8, 10 + ((int) (realtime * 4) & 1)); 
	if (sedit_state == 1) 
		M_DrawCharacter (DESC_X + 9 + 8 * (strlen (desc) - desc_min), 
						 40 + 8, 10 + ((int) (realtime * 4) & 1)); 
 } 
} 
 
 
void 
M_SEdit_Key (int key) 
{ 
	int         l; 
	server_entry_t *c; 
 
	switch (key) { 
		case K_ESCAPE: 
			M_Menu_ServerList_f (); 
			break; 
		case K_ENTER: 
			c = SL_Get_By_Num (slist, m_multip_cursor); 
			free (c->server); 
			free (c->desc); 
			c->server = malloc (strlen (serv) + 1); 
			c->desc = malloc (strlen (desc) + 1); 
			strcpy (c->server, serv); 
			strcpy (c->desc, desc); 
			M_Menu_ServerList_f (); 
			break; 
		case K_UPARROW: 
			S_LocalSound ("misc/menu1.wav"); 
			sedit_state = !sedit_state; 
			break; 
		case K_DOWNARROW: 
			S_LocalSound ("misc/menu1.wav"); 
			sedit_state = !sedit_state; 
			break; 
		case K_BACKSPACE: 
			if (sedit_state) { 
				if ((l = strlen (desc))) 
					desc[--l] = 0; 
				if (strlen (desc) - 6 < desc_min && desc_min) { 
					desc_min--; 
					desc_max--; 
				} 
			} else { 
				if ((l = strlen (serv))) 
					serv[--l] = 0; 
				if (strlen (serv) - 6 < serv_min && serv_min) { 
					serv_min--; 
					serv_max--; 
				} 
			} 
			break; 
		default: 
			if (key < 32 || key > 127) 
				break; 
			if (sedit_state) { 
				l = strlen (desc); 
				if (l < 254) { 
					desc[l + 1] = 0; 
					desc[l] = key; 
				} 
				if (strlen (desc) > desc_max) { 
					desc_min++; 
					desc_max++; 
				} 
			} else { 
				l = strlen (serv); 
				if (l < 254) { 
					serv[l + 1] = 0; 
					serv[l] = key; 
				} 
				if (strlen (serv) > serv_max) { 
					serv_min++; 
					serv_max++; 
				} 
			} 
			break; 
	} 
} 

//quakeforge end
// <-- SLIST
#endif //PROXY

void M_Quit_Draw (void)
{
#define VSTR(x) #x
#define VSTR2(x) VSTR(x)
  char *cmsg[] = {
//    0123456789012345678901234567890123456789
  "0            QuakeWorld",
  "1    version " VSTR2(VERSION) " by id Software",
  "0Programming",
  "1 John Carmack    Michael Abrash",
  "1 John Cash       Christian Antkow",
  "0Additional Programming",
  "1 Dave 'Zoid' Kirsch",
  "1 Jack 'morbid' Mathews",
  "0Amiga Port",
  "1 Frank 'Phx' Wille",
  "1 Steffen 'MagicSN' Haeuser",
#ifdef GLQUAKE 
  "0Zquake features & GL port",
  "1Christian 'SuRgEoN' Michael",
#else 
  "0Zquake features & misc additions",
  "1 Christian 'SuRgEoN' Michael",
#endif
  "0Id Software is not responsible for",
    "0providing technical support for",
  "0QUAKEWORLD(tm). (c)1996 Id Software,",
  "0Inc. QUAKEWORLD(tm) is a trademark",
  "0of Id Software, Inc.",
  "1(c) & (tm) NOTICES FOR QUAKE(r) ARE",
  "1NOT MODIFIED BY THE USE OF QUAKEWORLD",
  "1(tm) AND REMAIN IN FULL FORCE",
  NULL };
  char **p;
  int y;

  if (wasInMenus)
  {
    m_state = m_quit_prevstate;
    m_recursiveDraw = true;
//    M_Draw();
    m_state = m_quit;
  }

#if 1

if(!odd_display)
{
  M_DrawTextBox (0, 0, 38, 23);
  y = 12;
  for (p = cmsg; *p; p++, y += 8) {
    if (**p == '0')
      M_PrintWhite (16, y, *p + 1);
    else
      M_Print (16, y, *p + 1);
  }
}
else
{
 Draw_TileClear (0, 0, vid.width, vid.height);
  y = 2;
  for (p = cmsg; *p; p++, y += 6) {
    if (**p == '0')
      M_PrintWhite (16, y, *p + 1);
    else
      M_Print (16, y, *p + 1);
  }
}

#else

if(!odd_display)
{
  M_DrawTextBox (56, 76, 24, 4);
  M_Print (64, 84,  quitMessage[msgNumber*4+0]);
  M_Print (64, 92,  quitMessage[msgNumber*4+1]);
  M_Print (64, 100, quitMessage[msgNumber*4+2]);
  M_Print (64, 108, quitMessage[msgNumber*4+3]);
}
else
{
  M_Print (64, 8,  quitMessage[msgNumber*4+0]);
  M_Print (64, 16,  quitMessage[msgNumber*4+1]);
  M_Print (64, 24, quitMessage[msgNumber*4+2]);
  M_Print (64, 32, quitMessage[msgNumber*4+3]);
}

#endif
}



//=============================================================================
/* Menu Subsystem */


void M_Init (void)
{
	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f); 
	Cmd_AddCommand ("menu_multiplayer", M_Menu_MultiPlayer_f); 
#ifdef PROXY
	Cmd_AddCommand ("menu_setup", M_Menu_Setup_f); 
#endif
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
#ifdef PROXY
	Cmd_AddCommand ("menu_details", M_Menu_Details_f);
#endif
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("help", M_Menu_Help_f); 
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
}


void M_Draw (void)
{
	if (m_state == m_none || key_dest != key_menu)
		return;

	if (!m_recursiveDraw)
	{
		scr_copyeverything = 1;

		if (scr_con_current)
		{
			Draw_ConsoleBackground (vid.height);
			VID_UnlockBuffer ();
			S_ExtraUpdate ();
			VID_LockBuffer ();
		}
		else
			Draw_FadeScreen ();

		scr_fullupdate = 0;
	}
	else
	{
		m_recursiveDraw = false;
	}

	switch (m_state)
	{
	case m_none:
		break;

	case m_main:
		M_Main_Draw ();
		break;

	case m_singleplayer:
		M_SinglePlayer_Draw ();
		break;

	case m_load:
//		M_Load_Draw ();
		break;

	case m_save:
//		M_Save_Draw ();
		break;

	case m_multiplayer:
		M_MultiPlayer_Draw ();
		break;

	case m_setup:
#ifdef PROXY
		M_Setup_Draw ();
#endif
		break;

	case m_net:
//		M_Net_Draw ();
		break;

	case m_options:
		M_Options_Draw ();
		break;

	case m_keys:
		M_Keys_Draw ();
		break;
#ifdef PROXY
	case m_details:
		M_Details_Draw ();
		break;
#endif

	case m_video:
		M_Video_Draw ();
		break;

	case m_help:
		M_Help_Draw ();
		break;

	case m_quit:
		M_Quit_Draw ();
		break;

	case m_serialconfig:
//		M_SerialConfig_Draw ();
		break;

	case m_modemconfig:
//		M_ModemConfig_Draw ();
		break;

	case m_lanconfig:
//		M_LanConfig_Draw ();
		break;


	case m_search:
//		M_Search_Draw ();
		break;

	case m_slist:
#ifdef PROXY
		M_ServerList_Draw ();
#endif
		break; 
#ifdef PROXY
	case m_sedit: 
		M_SEdit_Draw (); 
		break; 
#endif
	}

	if (m_entersound)
	{
		S_LocalSound ("misc/menu2.wav");
		m_entersound = false;
	}

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();
}


void M_Keydown (int key)
{
	switch (m_state)
	{
	case m_none:
		return;

	case m_main:
		M_Main_Key (key);
		return;

	case m_singleplayer:
		M_SinglePlayer_Key (key);
		return;

	case m_load:
//		M_Load_Key (key);
		return;

	case m_save:
//		M_Save_Key (key);
		return;

	case m_multiplayer:
		M_MultiPlayer_Key (key);
		return;

	case m_setup:
#ifdef PROXY
		M_Setup_Key (key);
#endif
		return;

	case m_net:
//		M_Net_Key (key);
		return;

	case m_options:
		M_Options_Key (key);
		return;

	case m_keys:
		M_Keys_Key (key);
		return;
#ifdef PROXY
	case m_details:
		M_Details_Key (key);
		return;
#endif

	case m_video:
		M_Video_Key (key);
		return;

	case m_help:
		M_Help_Key (key);
		return;

	case m_quit:
		M_Quit_Key (key);
		return;

	case m_serialconfig:
//		M_SerialConfig_Key (key);
		return;

	case m_modemconfig:
//		M_ModemConfig_Key (key);
		return;

	case m_lanconfig:
//		M_LanConfig_Key (key);
		return;


	case m_search:
//		M_Search_Key (key);
		return;

	case m_slist:
#ifdef PROXY
		M_ServerList_Key (key);
#endif
		return; 

#ifdef PROXY
	case m_sedit: 
		M_SEdit_Key (key); 
		return; 
#endif

	}
}


