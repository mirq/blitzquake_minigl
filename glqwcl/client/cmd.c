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
// cmd.c -- Quake script command processing module

#include "quakedef.h"

#ifdef PROXY
#include "teamplay.h"
#endif

void Cmd_ForwardToServer (void);

#define MAX_ALIAS_NAME  32

typedef struct cmd_alias_s
{
#ifdef PROXY
  struct cmd_alias_s	*hash_next; 
#endif
  struct cmd_alias_s *next;

  char  name[MAX_ALIAS_NAME];
  char  *value;
} cmd_alias_t;

#ifndef PROXY
cmd_alias_t  *cmd_alias;
#endif

qboolean  cmd_wait;

cvar_t cl_warncmd = {"cl_warncmd", "0"};

//=============================================================================

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
void Cmd_Wait_f (void)
{
  cmd_wait = true;
}

/*
=============================================================================

            COMMAND BUFFER

=============================================================================
*/

sizebuf_t cmd_text;
byte    cmd_text_buf[8192];

/*
============
Cbuf_Init
============
*/
void Cbuf_Init (void)
{
  cmd_text.data = cmd_text_buf;
  cmd_text.maxsize = sizeof(cmd_text_buf);
}

/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText (char *text)
{
  int   l;
  
  l = Q_strlen (text);

  if (cmd_text.cursize + l >= cmd_text.maxsize)
  {
    Con_Printf ("Cbuf_AddText: overflow\n");
    return;
  }
  SZ_Write (&cmd_text, text, Q_strlen (text));
}


/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
FIXME: actually change the command buffer to do less copying
============
*/
void Cbuf_InsertText (char *text)
{
  char  *temp;
  int   templen;

// copy off any commands still remaining in the exec buffer
  templen = cmd_text.cursize;
  if (templen)
  {
    temp = Z_Malloc (templen);
    Q_memcpy (temp, cmd_text.data, templen);
    SZ_Clear (&cmd_text);
  }
  else
    temp = NULL;  // shut up compiler
    
// add the entire text of the file
  Cbuf_AddText (text);
  SZ_Write (&cmd_text, "\n", 1);
// add the copied off data
  if (templen)
  {
    SZ_Write (&cmd_text, temp, templen);
    Z_Free (temp);
  }
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute (void)
{
  int   i;
  char  *text;
  char  line[1024];
  int   quotes;
  
  while (cmd_text.cursize)
  {
// find a \n or ; line break
    text = (char *)cmd_text.data;

    quotes = 0;
    for (i=0 ; i< cmd_text.cursize ; i++)
    {
      if (text[i] == '"')
        quotes++;
      if ( !(quotes&1) &&  text[i] == ';')
        break;  // don't break if inside a quoted string
      if (text[i] == '\n')
        break;
    }
      
        
    memcpy (line, text, i);
    line[i] = 0;
    
// delete the text from the command buffer and move remaining commands down
// this is necessary because commands (exec, alias) can insert data at the
// beginning of the text buffer

    if (i == cmd_text.cursize)
      cmd_text.cursize = 0;
    else
    {
      i++;
      cmd_text.cursize -= i;
      Q_memcpy (text, text+i, cmd_text.cursize);
    }

// execute the command line
    Cmd_ExecuteString (line);
    
    if (cmd_wait)
    { // skip out while text still remains in buffer, leaving it
      // for next frame
      cmd_wait = false;
      break;
    }
  }
}

/*
==============================================================================

            SCRIPT COMMANDS

==============================================================================
*/

/*
===============
Cmd_StuffCmds_f

Adds command line parameters as script statements
Commands lead with a +, and continue until a - or another +
quake +prog jctest.qp +cmd amlev1
quake -nosound +cmd amlev1
===============
*/
void Cmd_StuffCmds_f (void)
{
  int   i, j;
  int   s;
  char  *text, *build, c;
    
// build the combined string to parse from
  s = 0;
  for (i=1 ; i<com_argc ; i++)
  {
    if (!com_argv[i])
      continue;   // NEXTSTEP nulls out -NXHost
    s += Q_strlen (com_argv[i]) + 1;
  }
  if (!s)
    return;
    
  text = Z_Malloc (s+1);
  text[0] = 0;
  for (i=1 ; i<com_argc ; i++)
  {
    if (!com_argv[i])
      continue;   // NEXTSTEP nulls out -NXHost
    Q_strcat (text,com_argv[i]);
    if (i != com_argc-1)
      Q_strcat (text, " ");
  }
  
// pull out the commands
  build = Z_Malloc (s+1);
  build[0] = 0;
  
  for (i=0 ; i<s-1 ; i++)
  {
    if (text[i] == '+')
    {
      i++;

      for (j=i ; (text[j] != '+') && (text[j] != '-') && (text[j] != 0) ; j++)
        ;

      c = text[j];
      text[j] = 0;
      
      Q_strcat (build, text+i);
      Q_strcat (build, "\n");
      text[j] = c;
      i = j-1;
    }
  }
  
  if (build[0])
    Cbuf_InsertText (build);
  
  Z_Free (text);
  Z_Free (build);
}


/*
===============
Cmd_Exec_f
===============
*/
void Cmd_Exec_f (void)
{
  char  *f;
  int   mark;

  if (Cmd_Argc () != 2)
  {
    Con_Printf ("exec <filename> : execute a script file\n");
    return;
  }

  // FIXME: is this safe freeing the hunk here???
  mark = Hunk_LowMark ();
  f = (char *)COM_LoadHunkFile (Cmd_Argv(1));
  if (!f)
  {
    Con_Printf ("couldn't exec %s\n",Cmd_Argv(1));
    return;
  }
  if (!Cvar_Command () && (cl_warncmd.value || developer.value))
    Con_Printf ("execing %s\n",Cmd_Argv(1));
  
  Cbuf_InsertText (f);
  Hunk_FreeToLowMark (mark);
}


/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void Cmd_Echo_f (void)
{
  int   i;
  
  for (i=1 ; i<Cmd_Argc() ; i++)
#ifdef PROXY
#ifdef SERVERONLY
    Con_Printf ("%s ",Cmd_Argv(i));
#else
		Con_Printf ("%s ", CL_ParseMacroString(Cmd_Argv(i)));	// Tonik
#endif
#else
    Con_Printf ("%s ",Cmd_Argv(i));
#endif
  Con_Printf ("\n");
}

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/

char *CopyString (char *in)
{
  char  *out;
  
  out = Z_Malloc (strlen(in)+1);
  strcpy (out, in);
  return out;
}

#ifdef PROXY

/* 
============================================================================= 
 
								HASH 
 
============================================================================= 
*/ 
 
/* 
========== 
Key 
========== 
Returns hash key for a string 
*/ 
static int Key (char *name) 
{ 
	int	v; 
	int c; 
 
	v = 0; 
	while ( (c = *name++) != 0 ) 
//		v += *name; 
		v += c &~ 32;	// very lame, but works (case insensitivity) 
 
	return v % 32; 
} 

static cmd_alias_t	*cmd_alias_hash[32]; 
static cmd_alias_t	*cmd_alias; 

cmd_alias_t *Cmd_FindAlias (char *name) 
{ 
	int			key; 
	cmd_alias_t *alias; 
 
	key = Key (name); 
	for (alias = cmd_alias_hash[key] ; alias ; alias = alias->hash_next) 
	{ 
		if (!Q_strcasecmp(name, alias->name)) 
			return alias; 
	} 
	return NULL; 
} 
 
 
// Tonik 31.08.00 --- for message triggers: 
char *Cmd_AliasString (char *name) 
{ 
	int			key; 
	cmd_alias_t *alias; 
 
	key = Key (name); 
	for (alias = cmd_alias_hash[key] ; alias ; alias = alias->hash_next) 
	{ 
		if (!Q_strcasecmp(name, alias->name)) 
			return alias->value; 
	} 
	return NULL; 
} 

void Cmd_Alias_f (void) 
{ 
	cmd_alias_t	*a; 
	char		cmd[1024]; 
	int			i, c; 
	int			key; 
	char		*s; 
	cvar_t		*var; 
 
	if (Cmd_Argc() == 1) 
	{ 
		Con_Printf ("Current alias commands:\n"); 
		for (a = cmd_alias ; a ; a=a->next) 
			Con_Printf ("%s : %s\n", a->name, a->value); 
		return; 
	} 
 
	s = Cmd_Argv(1); 
	if (strlen(s) >= MAX_ALIAS_NAME) 
	{ 
		Con_Printf ("Alias name is too long\n"); 
		return; 
	} 
 
	if ( (var = Cvar_FindVar(s)) != NULL ) { 
//		if (var->created) 
//			Cvar_Delete (var->name); 
//		else { 
//			Con_Printf ("%s is a variable\n"); 
			return; 
//		} 
	} 
 
	key = Key(s); 
 
	// if the alias already exists, reuse it 
	for (a = cmd_alias_hash[key] ; a ; a=a->hash_next) 
	{ 
		if (!Q_strcasecmp(a->name, s)) 
		{ 
			Z_Free (a->value); 
			break; 
		} 
	} 
 
	if (!a) 
	{ 
		a = Z_Malloc (sizeof(cmd_alias_t)); 
		a->next = cmd_alias; 
		cmd_alias = a; 
		a->hash_next = cmd_alias_hash[key]; 
		cmd_alias_hash[key] = a; 
	} 
	strcpy (a->name, s); 
 
// copy the rest of the command line 
	cmd[0] = 0;		// start out with a null string 
	c = Cmd_Argc(); 
	for (i=2 ; i < c ; i++) 
	{ 
		strcat (cmd, Cmd_Argv(i)); 
		if (i != c) 
			strcat (cmd, " "); 
	} 
	strcat (cmd, "\n"); 
	 
	a->value = CopyString (cmd); 
} 
 
 
qboolean Cmd_DeleteAlias (char *name) 
{ 
	cmd_alias_t	*a, *prev; 
	int			key; 
 
	key = Key (name); 
 
	prev = NULL; 
	for (a = cmd_alias_hash[key] ; a ; a = a->hash_next) 
	{ 
		if (!Q_strcasecmp(a->name, name)) 
		{ 
			// unlink from hash 
			if (prev) 
				prev->hash_next = a->hash_next; 
			else 
				cmd_alias_hash[key] = a->hash_next; 
			break; 
		} 
		prev = a; 
	} 
 
	if (!a) 
		return false;	// not found 
 
	prev = NULL; 
	for (a = cmd_alias ; a ; a = a->next) 
	{ 
		if (!Q_strcasecmp(a->name, name)) 
		{ 
			// unlink from alias list 
			if (prev) 
				prev->next = a->next; 
			else 
				cmd_alias = a->next; 
 
			// free 
			Z_Free (a->value); 
			Z_Free (a);			 
			return true; 
		} 
		prev = a; 
	} 
 
	Sys_Error ("Cmd_DeleteAlias: alias list broken"); 
	return false;	// shut up compiler 
} 
 
void Cmd_UnAlias_f (void) 
{ 
	char		*s; 
 
	if (Cmd_Argc() != 2) 
	{ 
		Con_Printf ("unalias <alias>: erase an existing alias\n"); 
		return; 
	} 
 
	s = Cmd_Argv(1); 
	if (strlen(s) >= MAX_ALIAS_NAME) 
	{ 
		Con_Printf ("Alias name is too long\n"); 
		return; 
	} 
 
	if (!Cmd_DeleteAlias(s)) 
		Con_Printf ("Unknown alias \"%s\"\n", s); 
} 
 

#endif

#ifndef PROXY
void Cmd_Alias_f (void)
{
  cmd_alias_t  *a;
  char    cmd[1024];
  int     i, c;
  char    *s;

  if (Cmd_Argc() == 1)
  {
    Con_Printf ("Current alias commands:\n");
    for (a = cmd_alias ; a ; a=a->next)
      Con_Printf ("%s : %s\n", a->name, a->value);
    return;
  }

  s = Cmd_Argv(1);
  if (strlen(s) >= MAX_ALIAS_NAME)
  {
    Con_Printf ("Alias name is too long\n");
    return;
  }

  // if the alias allready exists, reuse it
  for (a = cmd_alias ; a ; a=a->next)
  {
    if (!strcmp(s, a->name))
    {
      Z_Free (a->value);
      break;
    }
  }

  if (!a)
  {
    a = Z_Malloc (sizeof(cmd_alias_t));
    a->next = cmd_alias;
    cmd_alias = a;
  }
  strcpy (a->name, s);  

// copy the rest of the command line
  cmd[0] = 0;   // start out with a null string
  c = Cmd_Argc();
  for (i=2 ; i< c ; i++)
  {
    strcat (cmd, Cmd_Argv(i));
    if (i != c)
      strcat (cmd, " ");
  }
  strcat (cmd, "\n");
  
  a->value = CopyString (cmd);
}
#endif

/*
=============================================================================

          COMMAND EXECUTION

=============================================================================
*/

#ifndef PROXY //moved to cmd.h
typedef struct cmd_function_s
{
  struct cmd_function_s *next;
  char          *name;
  xcommand_t        function;
} cmd_function_t;
#endif

#define MAX_ARGS    80

static  int     cmd_argc;
static  char    *cmd_argv[MAX_ARGS];
static  char    *cmd_null_string = "";
static  char    *cmd_args = NULL;


#ifdef PROXY
static cmd_function_t	*cmd_hash_array[32]; 
#endif

static  cmd_function_t  *cmd_functions;   // possible commands to execute

/*
============
Cmd_Argc
============
*/
int   Cmd_Argc (void)
{
  return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
char  *Cmd_Argv (int arg)
{
  if ( arg >= cmd_argc )
    return cmd_null_string;
  return cmd_argv[arg]; 
}

/*
============
Cmd_Args

Returns a single string containing argv(1) to argv(argc()-1)
============
*/
char    *Cmd_Args (void)
{
  if (!cmd_args)
    return "";
  return cmd_args;
}


/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
============
*/

#ifdef PROXY
#if 1 

void Cmd_TokenizeString (char *text)
{
  int   i;
  
// clear the args from the last string
  for (i=0 ; i<cmd_argc ; i++)
    Z_Free (cmd_argv[i]);
    
  cmd_argc = 0;
  cmd_args = NULL;
  
  while (1)
  {
// skip whitespace up to a /n
	while (*text && *(unsigned char*)text <= ' ' && *text != '\n')	// Tonik 
    {
      text++;
    }
    
    if (*text == '\n')
    { // a newline seperates commands in the buffer
      text++;
      break;
    }

    if (!*text)
      return;
  
    if (cmd_argc == 1)
       cmd_args = text;
      
    text = COM_Parse (text);
    if (!text)
      return;

    if (cmd_argc < MAX_ARGS)
    {
      cmd_argv[cmd_argc] = Z_Malloc (Q_strlen(com_token)+1);
      Q_strcpy (cmd_argv[cmd_argc], com_token);
      cmd_argc++;
    }
  }
  
}
#else 
// Tonik: new TokenizeString (not finished) 
// text must point to a buffer 1024 chars long 
void Cmd_TokenizeString (char *text) 
{ 
	int			idx; 
	int			i, c, len;	// COM_Parse 
	static char	argv_buf[1024]; 
	char		*p; 
	 
	p = argv_buf; 
		 
	cmd_argc = 0; 
	cmd_args = NULL; 
	 
	while (1) 
	{ 
// skip whitespace up to a /n 
//skipwhite: 
		while (*text && *(unsigned char*)text <= ' ' && *text != '\n')	// Tonik 
		{ 
			text++; 
		} 
		 
		if (*text == '\n') 
		{	// a newline seperates commands in the buffer 
			text++; 
			break; 
		} 
 
		if (!*text) 
			return; 
		 
		if (cmd_argc == 1) 
			cmd_args = text; 
 
		// skip // comments 
		if (c=='/' && text[1] == '/') 
		{ 
//			while (*text && *text != '\n') 
//				text++; 
			break; 
		} 
		 
		*p = 0; 
 
		 
		// handle quoted strings specially 
		if (c == '\"') 
		{ 
			text++; 
			while (1) 
			{ 
				c = *text++; 
				if (c=='\"' || !c) 
				{ 
					*p = 0; 
					//return text; 
					goto _return; 
				} 
				*p++ = c; 
				len++; 
			} 
		} 
		 
		// parse a regular word 
		do 
		{ 
			*p++ = c; 
			text++; 
			c = *text; 
		} while (c > 32); 
		 
		*p = 0; 
		 
_return: 
		if (cmd_argc < MAX_ARGS) 
		{ 
			cmd_argv[cmd_argc] = argv_buf + idx; 
			strcpy (cmd_argv[cmd_argc], com_token); 
			idx++; 
			cmd_argc++; 
		} 
	} 
	 
} 
#endif 
#else
void Cmd_TokenizeString (char *text)
{
  int   i;
  
// clear the args from the last string
  for (i=0 ; i<cmd_argc ; i++)
    Z_Free (cmd_argv[i]);
    
  cmd_argc = 0;
  cmd_args = NULL;
  
  while (1)
  {
// skip whitespace up to a /n
    while (*text && *text <= ' ' && *text != '\n')
    {
      text++;
    }
    
    if (*text == '\n')
    { // a newline seperates commands in the buffer
      text++;
      break;
    }

    if (!*text)
      return;
  
    if (cmd_argc == 1)
       cmd_args = text;
      
    text = COM_Parse (text);
    if (!text)
      return;

    if (cmd_argc < MAX_ARGS)
    {
      cmd_argv[cmd_argc] = Z_Malloc (Q_strlen(com_token)+1);
      Q_strcpy (cmd_argv[cmd_argc], com_token);
      cmd_argc++;
    }
  }
  
}
#endif

/*
============
Cmd_AddCommand
============
*/

#ifndef PROXY
void  Cmd_AddCommand (char *cmd_name, xcommand_t function)
{
  cmd_function_t  *cmd;
  
  if (host_initialized) // because hunk allocation would get stomped
    Sys_Error ("Cmd_AddCommand after host_initialized");
    
// fail if the command is a variable name
  if (Cvar_VariableString(cmd_name)[0])
  {
    Con_Printf ("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
    return;
  }
  
// fail if the command already exists
  for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
  {
    if (!Q_strcmp (cmd_name, cmd->name))
    {
      Con_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
      return;
    }
  }

  cmd = Hunk_Alloc (sizeof(cmd_function_t));
  cmd->name = cmd_name;
  cmd->function = function;
  cmd->next = cmd_functions;
  cmd_functions = cmd;
}

/*
============
Cmd_Exists
============
*/
qboolean  Cmd_Exists (char *cmd_name)
{
  cmd_function_t  *cmd;

  for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
  {
    if (!Q_strcmp (cmd_name,cmd->name))
      return true;
  }

  return false;
}

#else

/* 
============ 
Cmd_AddCommand 
============ 
*/
void	Cmd_AddCommand (char *cmd_name, xcommand_t function) 
{ 
	cmd_function_t	*cmd; 
	int	key; 
	 
	if (host_initialized)	// because hunk allocation would get stomped 
		Sys_Error ("Cmd_AddCommand after host_initialized"); 
		 
// fail if the command is a variable name 
	if (Cvar_FindVar(cmd_name)) 
	{ 
		Con_Printf ("Cmd_AddCommand: %s already defined as a var\n", cmd_name); 
		return; 
	} 
	 
	key = Key (cmd_name); 
 
// fail if the command already exists 
	for (cmd=cmd_hash_array[key] ; cmd ; cmd=cmd->hash_next) 
	{ 
		if (!strcmp (cmd_name, cmd->name)) 
		{ 
			Con_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name); 
			return; 
		} 
	} 
 
	cmd = Hunk_Alloc (sizeof(cmd_function_t)); 
	cmd->name = cmd_name; 
	cmd->function = function; 
	cmd->next = cmd_functions; 
	cmd_functions = cmd; 
	cmd->hash_next = cmd_hash_array[key]; 
	cmd_hash_array[key] = cmd; 
} 
 
/* 
============ 
Cmd_Exists 
============ 
*/ 
qboolean	Cmd_Exists (char *cmd_name) 
{ 
	int	key; 
	cmd_function_t	*cmd; 
 
	key = Key (cmd_name); 
	for (cmd=cmd_hash_array[key] ; cmd ; cmd=cmd->hash_next) 
	{ 
		if (!strcmp (cmd_name, cmd->name)) 
			return true; 
	} 
 
	return false; 
} 

// --> Tonik
/* 
============ 
Cmd_FindCommand 
============ 
*/ 
cmd_function_t *Cmd_FindCommand (char *cmd_name) 
{ 
	int	key; 
	cmd_function_t	*cmd; 
 
	key = Key (cmd_name); 
	for (cmd=cmd_hash_array[key] ; cmd ; cmd=cmd->hash_next) 
	{ 
		if (!Q_strcasecmp (cmd_name, cmd->name)) 
			return cmd; 
	} 
 
	return NULL; 
} 
// <-- Tonik 

#endif

/*
============
Cmd_CompleteCommand
============
*/
char *Cmd_CompleteCommand (char *partial)
{
  cmd_function_t  *cmd;
  int       len;
  cmd_alias_t    *a;
  
  len = Q_strlen(partial);
  
  if (!len)
    return NULL;
    
// check for exact match
  for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
    if (!strcmp (partial,cmd->name))
      return cmd->name;
  for (a=cmd_alias ; a ; a=a->next)
    if (!strcmp (partial, a->name))
      return a->name;

// check for partial match
  for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
    if (!strncmp (partial,cmd->name, len))
      return cmd->name;
  for (a=cmd_alias ; a ; a=a->next)
    if (!strncmp (partial, a->name, len))
      return a->name;

  return NULL;
}

#ifndef PROXY
#ifndef SERVERONLY    // FIXME
/*
===================
Cmd_ForwardToServer

adds the current command line as a clc_stringcmd to the client message.
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void Cmd_ForwardToServer (void)
{
  if (cls.state == ca_disconnected)
  {
    Con_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
    return;
  }

  if (cls.demoplayback)
    return;   // not really connected

  MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
  SZ_Print (&cls.netchan.message, Cmd_Argv(0));
  if (Cmd_Argc() > 1)
  {
    SZ_Print (&cls.netchan.message, " ");
    SZ_Print (&cls.netchan.message, Cmd_Args());
  }

}

// don't forward the first argument
void Cmd_ForwardToServer_f (void)
{
  if (cls.state == ca_disconnected)
  {
    Con_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
    return;
  }

  if (Q_strcasecmp(Cmd_Argv(1), "snap") == 0) {
    Cbuf_InsertText ("snap\n");
    return;
  }
  
  if (cls.demoplayback)
    return;   // not really connected

  if (Cmd_Argc() > 1)
  {
    MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
    SZ_Print (&cls.netchan.message, Cmd_Args());
  }
}
#else
void Cmd_ForwardToServer (void)
{
}
#endif
#endif

#ifdef PROXY
#ifndef SERVERONLY		// FIXME 
/* 
=================== 
Cmd_ForwardToServer 
 
adds the current command line as a clc_stringcmd to the client message. 
things like godmode, noclip, etc, are commands directed to the server, 
so when they are typed in at the console, they will need to be forwarded. 
=================== 
*/ 
void Cmd_ForwardToServer (void) 
{ 
	if (cls.state == ca_disconnected) 
	{ 
		Con_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0)); 
		return; 
	} 
 
	if (cls.demoplayback) 
	{ 
		// Tonik: 
		if ( ! Q_strcasecmp(Cmd_Argv(0), "pause")) 
			cl.paused ^= 1;						 
		return; 
	} 
 
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd); 
	SZ_Print (&cls.netchan.message, Cmd_Argv(0)); 
	if (Cmd_Argc() > 1) 
	{ 
		SZ_Print (&cls.netchan.message, " "); 
 
		if (!Q_strcasecmp(Cmd_Argv(0), "say") || 
			!Q_strcasecmp(Cmd_Argv(0), "say_team")) 
		{ 
			char		*s; 
			s = CL_ParseMacroString(Cmd_Args()); 
			if (*s && *s < 32 && *s != 10) 
			{ 
				SZ_Print (&cls.netchan.message, "\""); 
				SZ_Print (&cls.netchan.message, s); 
				SZ_Print (&cls.netchan.message, "\""); 
			} 
			else 
				SZ_Print (&cls.netchan.message, s); 
			return; 
		} 
 
		SZ_Print (&cls.netchan.message, Cmd_Args()); 
	} 
} 
 
// don't forward the first argument 
void Cmd_ForwardToServer_f (void) 
{ 
	if (cls.state == ca_disconnected) 
	{ 
		Con_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0)); 
		return; 
	} 
 
	if (Q_strcasecmp(Cmd_Argv(1), "snap") == 0) { 
		Cbuf_InsertText ("snap\n"); 
		return; 
	} 
	 
	if (cls.demoplayback) 
		return;		// not really connected 
 
	if (Cmd_Argc() > 1) 
	{ 
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd); 
		SZ_Print (&cls.netchan.message, Cmd_Args()); 
	} 
} 
#else 
void Cmd_ForwardToServer (void) 
{ 
} 
#endif 

// Tonik --> 
void Cmd_CmdList_f (void) 
{ 
	cmd_function_t	*cmd; 
	int	i; 
 
	for (cmd=cmd_functions, i=0 ; cmd ; cmd=cmd->next, i++) 
		Con_Printf("%s\n", cmd->name); 
 
	Con_Printf ("------------\n%d commands\n", i); 
} 
 
/* 
=========== 
Cmd_Z_Cmd_f 
=========== 
_z_cmd <command> 
Just executes the rest of the string. 
Can be used to do some zqwcl-specific action, e.g. "_z_cmd exec tonik_z.cfg" 
*/ 
void Cmd_Z_Cmd_f (void) 
{ 
	Cbuf_InsertText (Cmd_Args()); 
	Cbuf_InsertText ("\n"); 
} 
// <-- Tonik 
 
 

// dest must point to a 1024-byte buffer 
void Cmd_ExpandString (char *data, char *dest) 
{ 
	unsigned int	c; 
	char	buf[255]; 
	int		i, len; 
	cvar_t	*var, *bestvar; 
	int		quotes = 0; 
 
	len = 0; 
 
// parse a regular word 
	while ( (c = *data) != 0) 
	{ 
		if (c == '"') 
			quotes++; 
		if (c == '$' && !(quotes&1)) 
		{ 
			data++; 
 
			// Copy the text after '$' to a temp buffer 
			i = 0; 
			buf[0] = 0; 
			bestvar = NULL; 
			while ((c = *data) > 32) 
			{ 
				if (c == '$') 
					break; 
				data++; 
				buf[i++] = c; 
				buf[i] = 0; 
				if ( (var = Cvar_FindVar(buf)) != NULL ) 
					bestvar = var; 
			} 
 
			if (bestvar) 
			{ 
				// check buffer size 
				if (len + strlen(bestvar->string) >= 1024-1) 
					break; 
 
				strcpy(&dest[len], bestvar->string); 
				len += strlen(bestvar->string); 
				i = strlen(bestvar->name); 
				while (buf[i]) 
					dest[len++] = buf[i++]; 
			} 
			else 
			{ 
				// no matching cvar name was found 
				dest[len++] = '$'; 
				if (len + strlen(buf) >= 1024-1) 
					break; 
				strcpy (&dest[len], buf); 
				len += strlen(buf); 
			} 
		} 
		else 
		{ 
			dest[len] = c; 
			data++; 
			len++; 
			if (len >= 1024-1) 
				break; 
		} 
	}; 
 
	dest[len] = 0; 
} 
#endif


/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
FIXME: lookupnoadd the token to speed search?
============
*/
void  Cmd_ExecuteString (char *text)
{ 
  cmd_function_t  *cmd;
  cmd_alias_t    *a;

#ifdef PROXY
	int				key; 
	static	// FIXME 
	char			buf[1024]; 
#endif

#ifdef PROXY
 #if 0
  Cmd_TokenizeString (text);
 #else 
	Cmd_ExpandString (text, buf); 
	Cmd_TokenizeString (buf); 
 #endif 
#else
  Cmd_TokenizeString (text);
#endif
      
// execute the command line
  if (!Cmd_Argc())
    return;   // no tokens

#ifdef PROXY
	key = Key (cmd_argv[0]); 
	// FIXME: make Key () case-insensitive!!! 
// check functions 
	for (cmd=cmd_hash_array[key] ; cmd ; cmd=cmd->hash_next) 
	{ 
		if (!Q_strcasecmp (cmd_argv[0], cmd->name)) 
		{ 
			if (!cmd->function) 
				Cmd_ForwardToServer (); 
			else 
				cmd->function (); 
			return; 
		} 
	} 
 
// check alias 
	for (a=cmd_alias_hash[key] ; a ; a=a->hash_next) 
	{ 
		if (!Q_strcasecmp (cmd_argv[0], a->name)) 
		{ 
			Cbuf_InsertText (a->value); 
			return; 
		} 
	} 

#else

// check functions
  for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
  {
    if (!Q_strcasecmp (cmd_argv[0],cmd->name))
    {
      if (!cmd->function)
        Cmd_ForwardToServer ();
      else
        cmd->function ();
      return;
    }
  }

// check alias
  for (a=cmd_alias ; a ; a=a->next)
  {
    if (!Q_strcasecmp (cmd_argv[0], a->name))
    {
      Cbuf_InsertText (a->value);
      return;
    }
  }

#endif
  
// check cvars
  if (!Cvar_Command () && (cl_warncmd.value || developer.value))
    Con_Printf ("Unknown command \"%s\"\n", Cmd_Argv(0));
  
}



/*
================
Cmd_CheckParm

Returns the position (1 to argc-1) in the command's argument list
where the given parameter apears, or 0 if not present
================
*/
int Cmd_CheckParm (char *parm)
{
  int i;
  
  if (!parm)
    Sys_Error ("Cmd_CheckParm: NULL");

  for (i = 1; i < Cmd_Argc (); i++)
    if (! Q_strcasecmp (parm, Cmd_Argv (i)))
      return i;
      
  return 0;
}

/*
============
Cmd_Init
============
*/
void Cmd_Init (void)
{
//
// register our commands
//
  Cmd_AddCommand ("stuffcmds",Cmd_StuffCmds_f);
  Cmd_AddCommand ("exec",Cmd_Exec_f);
  Cmd_AddCommand ("echo",Cmd_Echo_f);
  Cmd_AddCommand ("alias",Cmd_Alias_f);
  Cmd_AddCommand ("wait", Cmd_Wait_f);

#ifdef PROXY
	Cmd_AddCommand ("cmdlist", Cmd_CmdList_f); 
	Cmd_AddCommand ("unalias", Cmd_UnAlias_f); 
	Cmd_AddCommand ("_z_cmd", Cmd_Z_Cmd_f); 
#endif

#ifndef SERVERONLY
  Cmd_AddCommand ("cmd", Cmd_ForwardToServer_f);
#endif
}

