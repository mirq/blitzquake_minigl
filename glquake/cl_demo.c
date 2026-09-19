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
#include "frame_profile.h"
#ifdef MINIGL_PERF_DIAGNOSTICS
#include <libraries/minigl_perf.h>
#endif

void CL_FinishTimeDemo (void);
#ifdef MINIGL_PERF_DIAGNOSTICS
static FILE *mgl_perf_log;

static void CL_PrintMiniGLPerfDerived (void)
{
  GLint frames, records, flushes, submits, dwords, vertices;
  double frameDivisor, vertexDivisor;

  glGetIntegerv (MGL_PERF_FRAMES, &frames);
  glGetIntegerv (MGL_PERF_DRAW_RECORDS, &records);
  glGetIntegerv (MGL_PERF_FLUSH_CALLS, &flushes);
  glGetIntegerv (MGL_PERF_SUBMITS, &submits);
  glGetIntegerv (MGL_PERF_RECORD_DWORDS, &dwords);
  glGetIntegerv (MGL_PERF_SUBMITTED_VERTICES, &vertices);
  frameDivisor = frames > 0 ? (double)frames : 1.0;
  vertexDivisor = vertices > 0 ? (double)vertices : 1.0;
  Con_Printf ("MGL_PERF_DERIVED draw_records_per_frame=%.3f "
              "flushes_per_frame=%.3f submits_per_frame=%.3f "
              "record_dwords_per_vertex=%.3f\n",
              records / frameDivisor, flushes / frameDivisor,
              submits / frameDivisor, dwords / vertexDivisor);
  if (mgl_perf_log)
    fprintf (mgl_perf_log,
             "MGL_PERF_DERIVED draw_records_per_frame=%.3f "
             "flushes_per_frame=%.3f submits_per_frame=%.3f "
             "record_dwords_per_vertex=%.3f\n",
             records / frameDivisor, flushes / frameDivisor,
             submits / frameDivisor, dwords / vertexDivisor);
}

/* The R200 backend exposes cumulative counters through glGetIntegerv().
 * Timedemo is the only automated renderer path, so reset them exactly when
 * its frame timer starts and report the same interval when it completes. */
static void CL_PrintMiniGLPerfCounter (char *name, GLenum counter, GLint rate)
{
  GLint value;

  glGetIntegerv (counter, &value);
  if (counter == MGL_PERF_FRONTEND_TICKS ||
      counter == MGL_PERF_SERIALIZE_TICKS ||
      counter == MGL_PERF_EXECUTE_TICKS ||
      counter == MGL_PERF_WAIT_TICKS ||
      counter == MGL_PERF_PRESENT_TICKS ||
      counter == MGL_PERF_TEXTURE_RESIDENCY_TICKS ||
      counter == MGL_PERF_TEXTURE_UPLOAD_TICKS)
    Con_Printf ("MGL_PERF %s=%ld ticks ms=%.3f\n", name, (long)value,
                rate ? ((double)value * 1000.0) / (double)rate : 0.0);
  else
    Con_Printf ("MGL_PERF %s=%ld\n", name, (long)value);
  if (mgl_perf_log)
  {
    if (counter == MGL_PERF_FRONTEND_TICKS ||
        counter == MGL_PERF_SERIALIZE_TICKS ||
        counter == MGL_PERF_EXECUTE_TICKS ||
        counter == MGL_PERF_WAIT_TICKS ||
        counter == MGL_PERF_PRESENT_TICKS ||
        counter == MGL_PERF_TEXTURE_RESIDENCY_TICKS ||
        counter == MGL_PERF_TEXTURE_UPLOAD_TICKS)
      fprintf (mgl_perf_log, "MGL_PERF %s=%ld ticks ms=%.3f\n", name,
               (long)value, rate ? ((double)value * 1000.0) / (double)rate : 0.0);
    else
      fprintf (mgl_perf_log, "MGL_PERF %s=%ld\n", name, (long)value);
  }
}

static void CL_PrintMiniGLWorstFrames (GLint rate)
{
  GLint count, index;
  GLint frame, total, serialize, execute, wait, present, residency;
  double scale = rate ? 1000.0 / (double)rate : 0.0;

  glGetIntegerv (MGL_PERF_WORST_FRAME_COUNT, &count);
  if (count > 10)
    count = 10;
  for (index = 0; index < count; ++index)
  {
    glGetIntegerv (MGL_PERF_WORST_FRAME_NUMBER(index), &frame);
    glGetIntegerv (MGL_PERF_WORST_FRAME_TICKS(index), &total);
    glGetIntegerv (MGL_PERF_WORST_SERIALIZE_TICKS(index), &serialize);
    glGetIntegerv (MGL_PERF_WORST_EXECUTE_TICKS(index), &execute);
    glGetIntegerv (MGL_PERF_WORST_WAIT_TICKS(index), &wait);
    glGetIntegerv (MGL_PERF_WORST_PRESENT_TICKS(index), &present);
    glGetIntegerv (MGL_PERF_WORST_RESIDENCY_TICKS(index), &residency);
    Con_Printf ("MGL_PERF_WORST rank=%ld frame=%ld ms=%.3f "
                "serialize_ms=%.3f execute_ms=%.3f wait_ms=%.3f "
                "present_ms=%.3f residency_ms=%.3f\n",
                (long)index, (long)frame, total * scale,
                serialize * scale, execute * scale, wait * scale,
                present * scale, residency * scale);
    if (mgl_perf_log)
      fprintf (mgl_perf_log,
               "MGL_PERF_WORST rank=%ld frame=%ld ms=%.3f "
               "serialize_ms=%.3f execute_ms=%.3f wait_ms=%.3f "
               "present_ms=%.3f residency_ms=%.3f\n",
               (long)index, (long)frame, total * scale,
               serialize * scale, execute * scale, wait * scale,
               present * scale, residency * scale);
  }
}

static void CL_PrintMiniGLPerf (int frames, float time)
{
  GLint rate;

  mgl_perf_log = fopen (va("%s/phase0_timedemo.log", com_gamedir), "a");
  if (mgl_perf_log)
    fprintf (mgl_perf_log, "TIMEDMO frames=%d seconds=%.3f fps=%.3f\n",
             frames, time, frames / time);
  glGetIntegerv (MGL_PERF_CLOCK_RATE, &rate);
  Con_Printf ("MGL_PERF_CLOCK_RATE=%ld\n", (long)rate);
  if (mgl_perf_log)
    fprintf (mgl_perf_log, "MGL_PERF_CLOCK_RATE=%ld\n", (long)rate);
  CL_PrintMiniGLPerfCounter ("FRAMES", MGL_PERF_FRAMES, rate);
  CL_PrintMiniGLPerfCounter ("PRIMITIVES", MGL_PERF_PRIMITIVES, rate);
  CL_PrintMiniGLPerfCounter ("INPUT_VERTICES", MGL_PERF_INPUT_VERTICES, rate);
  CL_PrintMiniGLPerfCounter ("SUBMITTED_VERTICES", MGL_PERF_SUBMITTED_VERTICES, rate);
  CL_PrintMiniGLPerfCounter ("DRAW_RECORDS", MGL_PERF_DRAW_RECORDS, rate);
  CL_PrintMiniGLPerfCounter ("FLUSH_CALLS", MGL_PERF_FLUSH_CALLS, rate);
  CL_PrintMiniGLPerfCounter ("SUBMITS", MGL_PERF_SUBMITS, rate);
  CL_PrintMiniGLPerfCounter ("RECORD_DWORDS", MGL_PERF_RECORD_DWORDS, rate);
  CL_PrintMiniGLPerfCounter ("GENERATED_DWORDS", MGL_PERF_GENERATED_DWORDS, rate);
  CL_PrintMiniGLPerfCounter ("FRONTEND_TICKS", MGL_PERF_FRONTEND_TICKS, rate);
  CL_PrintMiniGLPerfCounter ("SERIALIZE_TICKS", MGL_PERF_SERIALIZE_TICKS, rate);
  CL_PrintMiniGLPerfCounter ("EXECUTE_TICKS", MGL_PERF_EXECUTE_TICKS, rate);
  CL_PrintMiniGLPerfCounter ("WAIT_TICKS", MGL_PERF_WAIT_TICKS, rate);
  CL_PrintMiniGLPerfCounter ("PRESENT_TICKS", MGL_PERF_PRESENT_TICKS, rate);
  CL_PrintMiniGLPerfCounter ("TEXTURE_RESIDENCY_TICKS", MGL_PERF_TEXTURE_RESIDENCY_TICKS, rate);
  CL_PrintMiniGLPerfCounter ("TEXTURE_UPLOAD_TICKS", MGL_PERF_TEXTURE_UPLOAD_TICKS, rate);
  CL_PrintMiniGLPerfCounter ("ASYNC_SUBMITS", MGL_PERF_ASYNC_SUBMITS, rate);
  CL_PrintMiniGLPerfCounter ("FENCE_WAITS", MGL_PERF_FENCE_WAITS, rate);
  CL_PrintMiniGLPerfCounter ("FENCE_POLL_HITS", MGL_PERF_FENCE_POLL_HITS, rate);
  CL_PrintMiniGLPerfCounter ("BUFFER_WAITS", MGL_PERF_BUFFER_WAITS, rate);
  CL_PrintMiniGLPerfCounter ("TEXTURE_WAITS", MGL_PERF_TEXTURE_WAITS, rate);
  CL_PrintMiniGLPerfCounter ("DRAIN_WAITS", MGL_PERF_DRAIN_WAITS, rate);
  CL_PrintMiniGLPerfCounter ("STATE_CALLS", MGL_PERF_STATE_CALLS, rate);
  CL_PrintMiniGLPerfCounter ("STATE_NOOPS", MGL_PERF_STATE_NOOPS, rate);
  CL_PrintMiniGLPerfCounter ("DEFERRED_SPLITS", MGL_PERF_DEFERRED_SPLITS, rate);
  CL_PrintMiniGLPerfCounter ("DRAW_MERGES", MGL_PERF_DRAW_MERGES, rate);
  CL_PrintMiniGLPerfCounter ("SNAPSHOT_BUILDS", MGL_PERF_SNAPSHOT_BUILDS, rate);
  CL_PrintMiniGLPerfCounter ("SNAPSHOT_FAST_HITS", MGL_PERF_SNAPSHOT_FAST_HITS, rate);
  CL_PrintMiniGLPerfCounter ("NATIVE_ATTEMPTS", MGL_PERF_NATIVE_ATTEMPTS, rate);
  CL_PrintMiniGLPerfCounter ("NATIVE_USED", MGL_PERF_NATIVE_USED, rate);
  CL_PrintMiniGLPerfCounter ("NATIVE_FALLBACK_CAPS", MGL_PERF_NATIVE_FALLBACK_CAPS, rate);
  CL_PrintMiniGLPerfCounter ("NATIVE_FALLBACK_CULL", MGL_PERF_NATIVE_FALLBACK_CULL, rate);
  CL_PrintMiniGLPerfCounter ("NATIVE_FALLBACK_FLAT", MGL_PERF_NATIVE_FALLBACK_FLAT, rate);
  CL_PrintMiniGLPerfCounter ("NATIVE_FALLBACK_SIZE", MGL_PERF_NATIVE_FALLBACK_SIZE, rate);
  CL_PrintMiniGLPerfCounter ("NATIVE_FALLBACK_CLIP", MGL_PERF_NATIVE_FALLBACK_CLIP, rate);
  CL_PrintMiniGLPerfCounter ("QUAD_ATTEMPTS", MGL_PERF_QUAD_ATTEMPTS, rate);
  CL_PrintMiniGLPerfCounter ("QUAD_USED", MGL_PERF_QUAD_USED, rate);
  CL_PrintMiniGLPerfCounter ("QUAD_FALLBACK_CAPS", MGL_PERF_QUAD_FALLBACK_CAPS, rate);
  CL_PrintMiniGLPerfCounter ("QUAD_FALLBACK_STATE", MGL_PERF_QUAD_FALLBACK_STATE, rate);
  CL_PrintMiniGLPerfCounter ("QUAD_FALLBACK_CLIP", MGL_PERF_QUAD_FALLBACK_CLIP, rate);
  CL_PrintMiniGLPerfCounter ("EXECUTE_FAILURES", MGL_PERF_EXECUTE_FAILURES, rate);
  CL_PrintMiniGLPerfCounter ("WAIT_FAILURES", MGL_PERF_WAIT_FAILURES, rate);
  CL_PrintMiniGLPerfCounter ("RHW_CLAMPS", MGL_PERF_RHW_CLAMPS, rate);
  CL_PrintMiniGLWorstFrames (rate);
  CL_PrintMiniGLPerfDerived ();
  if (mgl_perf_log)
  {
    fprintf (mgl_perf_log, "END_TIMEDMO\n");
    fclose (mgl_perf_log);
    mgl_perf_log = NULL;
  }
}
#endif

/*
==============================================================================

DEMO CODE

When a demo is playing back, all NET_SendMessages are skipped, and
NET_GetMessages are read from the demo file.

Whenever cl.time gets past the last received message, another message is
read from the demo file.
==============================================================================
*/

/*
==============
CL_StopPlayback

Called when a demo file runs out, or the user starts a game
==============
*/
void CL_StopPlayback (void)
{
  if (!cls.demoplayback)
    return;

#ifdef AMIGA
  if (cls.demofile) fclose(cls.demofile);
    cls.demofile=0;
#else
  fclose (cls.demofile);
#endif
  cls.demoplayback = false;
  cls.demofile = NULL;
  cls.state = ca_disconnected;

  if (cls.timedemo)
    CL_FinishTimeDemo ();
}

/* JPG - need to fix up the demo message
==============
CL_FixMsg
==============
*/
void CL_FixMsg (int fix)
{
  char s1[7] = "coop 0";
  char s2[7] = "cmd xs";
  char *s;
  int match = 0;
  int c, i;

  s = fix ? s1 : s2;

  MSG_BeginReading ();
  while (1)
  {
    if (msg_badread)
      return;
    if (MSG_ReadByte () != svc_stufftext)
      return;

    while (1)
    {
      c = MSG_ReadChar();
      if (c == -1 || c == 0)
        break;
      if (c == s[match])
      {
        match++;
        if (match == 6)
        {
          for (i = 0 ; i < 6 ; i++)
            net_message.data[msg_readcount - 6 + i] ^= s1[i] ^ s2[i];
          match = 0;
        }
      }
      else
        match = 0;
    }
  }
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
void CL_WriteDemoMessage (void)
{
  int   len;
  int   i;
  float f;

  len = LittleLong (net_message.cursize);
  fwrite (&len, 4, 1, cls.demofile);
  for (i=0 ; i<3 ; i++)
  {
    f = LittleFloat (cl.viewangles[i]);
    fwrite (&f, 4, 1, cls.demofile);
  }
  CL_FixMsg(1); // JPG - some demo things are bad
  fwrite (net_message.data, net_message.cursize, 1, cls.demofile);
  CL_FixMsg(0); // JPG - some demo things are bad
  fflush (cls.demofile);
}

/*
====================
CL_GetMessage

Handles recording and playback of demos, on top of NET_ code
====================
*/
int CL_GetMessage (void)
{
  int   r, i;
  float f;
  
  if  (cls.demoplayback)
  {
  // decide if it is time to grab the next message    
    if (cls.signon == SIGNONS)  // allways grab until fully connected
    {
      if (cls.timedemo)
      {
        if (host_framecount == cls.td_lastframe)
          return 0;   // allready read this frame's message
        cls.td_lastframe = host_framecount;
      // if this is the second frame, grab the real td_starttime
      // so the bogus time on the first frame doesn't count
        if (host_framecount == cls.td_startframe + 1)
        {
          cls.td_starttime = realtime;
#ifdef MINIGL_PERF_DIAGNOSTICS
          glHint (MGL_PERF_COUNTERS_HINT, GL_NICEST);
#endif
        }
      }
      else if ( /* cl.time > 0 && */ cl.time <= cl.mtime[0])
      {
          return 0;   // don't need another message yet
      }
    }
    
  // get the next message
    fread (&net_message.cursize, 4, 1, cls.demofile);
    VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);
    for (i=0 ; i<3 ; i++)
    {
      r = fread (&f, 4, 1, cls.demofile);
      cl.mviewangles[0][i] = LittleFloat (f);
    }
    
    net_message.cursize = LittleLong (net_message.cursize);
    if (net_message.cursize > MAX_MSGLEN)
      Sys_Error ("Demo message > MAX_MSGLEN");
    r = fread (net_message.data, net_message.cursize, 1, cls.demofile);
    if (r != 1)
    {
      CL_StopPlayback ();
      return 0;
    }
  
    return 1;
  }

  while (1)
  {
    r = NET_GetMessage (cls.netcon);
    
    if (r != 1 && r != 2)
      return r;
  
  // discard nop keepalive message
    if (net_message.cursize == 1 && net_message.data[0] == svc_nop)
      Con_Printf ("<-- server to client keepalive\n");
    else
      break;
  }

  if (cls.demorecording)
    CL_WriteDemoMessage ();
  
  return r;
}


/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f (void)
{
  if (cmd_source != src_command)
    return;

  if (!cls.demorecording)
  {
    Con_Printf ("Not recording a demo.\n");
    return;
  }

// write a disconnect message to the demo file
  SZ_Clear (&net_message);
  MSG_WriteByte (&net_message, svc_disconnect);
  CL_WriteDemoMessage ();

// finish up
#ifdef AMIGA
  if (cls.demofile) fclose(cls.demofile);
    cls.demofile=0;
#else
  fclose (cls.demofile);
#endif
  cls.demofile = NULL;
  cls.demorecording = false;
  Con_Printf ("Completed demo\n");
}

/*
====================
CL_Record_f

record <demoname> <map> [cd track]
====================
*/
void CL_Record_f (void)
{
  int   c;
  char  name[MAX_OSPATH];
  int   track;

  if (cmd_source != src_command)
    return;

  c = Cmd_Argc();
  if (c != 2 && c != 3 && c != 4)
  {
    Con_Printf ("record <demoname> [<map> [cd track]]\n");
    return;
  }

  if (strstr(Cmd_Argv(1), ".."))
  {
    Con_Printf ("Relative pathnames are not allowed.\n");
    return;
  }

  if (c == 2 && cls.state == ca_connected)
  {
    Con_Printf("Can not record - already connected to server\nClient demo recording must be started before connecting\n");
    return;
  }

// write the forced cd track number, or -1
  if (c == 4)
  {
    track = atoi(Cmd_Argv(3));
    Con_Printf ("Forcing CD track to %i\n", cls.forcetrack);
  }
  else
    track = -1; 

  sprintf (name, "%s/%s", com_gamedir, Cmd_Argv(1));
  
//
// start the map up
//
  if (c > 2)
    Cmd_ExecuteString ( va("map %s", Cmd_Argv(2)), src_command);
  
//
// open the demo file
//
  COM_DefaultExtension (name, ".dem");

  Con_Printf ("recording to %s.\n", name);
  cls.demofile = fopen (name, "wb");
  if (!cls.demofile)
  {
    Con_Printf ("ERROR: couldn't open.\n");
    return;
  }

  cls.forcetrack = track;
  fprintf (cls.demofile, "%i\n", cls.forcetrack);
  
  cls.demorecording = true;
}


/*
====================
CL_PlayDemo_f

play [demoname]
====================
*/
void CL_PlayDemo_f (void)
{
  char  name[256];
  int c;
  qboolean neg = false;

  if (cmd_source != src_command)
    return;

  if (Cmd_Argc() != 2)
  {
    Con_Printf ("play <demoname> : plays a demo\n");
    return;
  }

//
// disconnect from server
//
  CL_Disconnect ();
  
//
// open the demo file
//
  strcpy (name, Cmd_Argv(1));
  COM_DefaultExtension (name, ".dem");

  Con_Printf ("Playing demo from %s.\n", name);
  COM_FOpenFile (name, &cls.demofile);
  if (!cls.demofile)
  {
    Con_Printf ("ERROR: couldn't open.\n");
    cls.demonum = -1;   // stop demo loop
    return;
  }

  cls.demoplayback = true;
  cls.state = ca_connected;
  cls.forcetrack = 0;

  while ((c = getc(cls.demofile)) != '\n')
    if (c == '-')
      neg = true;
    else
      cls.forcetrack = cls.forcetrack * 10 + (c - '0');

  if (neg)
    cls.forcetrack = -cls.forcetrack;
// ZOID, fscanf is evil
//  fscanf (cls.demofile, "%i\n", &cls.forcetrack);
}

/*
====================
CL_FinishTimeDemo

====================
*/
void CL_FinishTimeDemo (void)
{
  int   frames;
  int   logparm;
  FILE  *resultfile;
  float time;
  
  cls.timedemo = false;
  
// the first frame didn't count
  frames = (host_framecount - cls.td_startframe) - 1;
  time = realtime - cls.td_starttime;
  if (!time)
    time = 1;
  Con_Printf ("%i frames %5.1f seconds %5.1f fps\n", frames, time, frames/time);
#ifdef MINIGL_PERF_DIAGNOSTICS
  CL_PrintMiniGLPerf (frames, time);
#endif
  /* Optional result export, AFTER the timing interval has ended. This
   * avoids stdout/condebug IO during measured gameplay. */
  logparm = COM_CheckParm ("-benchmarklog");
  if (logparm && logparm + 1 < com_argc)
  {
    resultfile = fopen (com_argv[logparm + 1], "w");
    if (resultfile)
    {
      fprintf (resultfile, "%i frames %.3f seconds %.3f fps\n",
               frames, time, frames/time);
      fclose (resultfile);
    }
    else
      Con_Printf ("Could not write timedemo result to %s\n", com_argv[logparm + 1]);
  }
  if (COM_CheckParm ("-benchmarkquit"))
    Sys_Quit ();
  /* Frame profiler report is written after the timing interval (and also
   * from cleanup() for non-benchmark sessions). */
  FP_AutoDump ();
}

/*
====================
CL_TimeDemo_f

timedemo [demoname]
====================
*/
void CL_TimeDemo_f (void)
{
  if (cmd_source != src_command)
    return;

  if (Cmd_Argc() != 2)
  {
    Con_Printf ("timedemo <demoname> : gets demo speeds\n");
    return;
  }

  CL_PlayDemo_f ();
  
// cls.td_starttime will be grabbed at the second frame of the demo, so
// all the loading time doesn't get counted
  
  cls.timedemo = true;
  cls.td_startframe = host_framecount;
  cls.td_lastframe = -1;    // get a new message this frame
}
