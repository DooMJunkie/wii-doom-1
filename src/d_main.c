// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.
//
// DESCRIPTION:
//        DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
//        plus functions to determine game mode (shareware, registered),
//        parse command line parameters, configure game parameters (turbo),
//        and call the startup functions.
//
//-----------------------------------------------------------------------------


#include <ctype.h>
#include <SDL/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "am_map.h"
#include "c_io.h"
#include "config.h"
#include "d_main.h"
#include "d_net.h"
#include "deh_main.h"
#include "deh_str.h"
#include "doomdef.h"
#include "doomfeatures.h"
#include "doomstat.h"
#include "dstrings.h"
#include "d_iwad.h"
#include "f_finale.h"
#include "f_wipe.h"
#include "g_game.h"
#include "gui.h"
#include "hu_stuff.h"
#include "i_joystick.h"
#include "i_sdlmusic.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_config.h"
#include "m_controls.h"
#include "m_menu.h"
#include "m_misc.h"
#include "net_client.h"
#include "net_dedicated.h"
#include "net_query.h"
#include "p_local.h"
#include "p_saveg.h"
#include "p_setup.h"
#include "r_local.h"
#include "s_sound.h"
#include "sounds.h"
#include "st_stuff.h"
#include "sys_wpad.h"
#include "v_video.h"
#include "video.h"
#include "w_merge.h"
#include "w_wad.h"
#include "wi_stuff.h"
#include "z_zone.h"


typedef uint32_t u32;   // < 32bit unsigned integer

// print title for every printed line
char            title[128];

// Location where savegames are stored

char *          savegamedir;

// location of IWAD and WAD files

char *          iwadfile;
char            *pagename;

boolean         nomonsters;     // checkparm of -nomonsters
boolean         start_respawnparm;
boolean         start_fastparm;
boolean         autostart;
boolean         advancedemo;

// Store demo, do not accept any inputs
boolean         storedemo;

// "BFG Edition" version of doom2.wad does not include TITLEPIC.
boolean         bfgedition;

// If true, the main game loop has started.
boolean         main_loop_started = false;

boolean         version13 = false;
boolean         redrawsbar;

int             startepisode;
int             startmap;
int             startloadgame;
int             fsize = 0;
int             fsizerw = 0;
int             wad_message_has_been_shown = 0;
int             dont_show_adding_of_resource_wad = 0;
int             dots_enabled = 0;
int             fps_enabled = 0;
int             display_fps = 0;
int             resource_wad_exists = 0;
int             demosequence;
int             pagetic;
int             runcount = 0;
int             startuptimer;

extern int      mp_skill;
extern int      warpepi;
extern int      warplev;
extern int      mus_engine;
extern int      warped;
extern int      showMessages;
extern int      screenSize;

extern boolean  skillflag;
extern boolean  nomonstersflag;
extern boolean  fastflag;
extern boolean  respawnflag;
extern boolean  warpflag;
extern boolean  multiplayerflag;
extern boolean  deathmatchflag;
extern boolean  altdeathflag;
extern boolean  locallanflag;
extern boolean  searchflag;
extern boolean  queryflag;
extern boolean  dedicatedflag;
extern boolean  opl;
extern boolean  nerve_pwad;
extern boolean  setsizeneeded;
extern boolean  hud;
extern boolean  inhelpscreens;
extern boolean  devparm_nerve;
extern boolean  finale_music;
extern boolean  aiming_help;
extern boolean  devparm_net;
extern boolean  iwad_added;
extern boolean  pwad_added;

extern menu_t*  currentMenu;                          
extern menu_t   CheatsDef;

extern short    itemOn;    // menu item skull is on

skill_t         startskill;

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t     wipegamestate = GS_DEMOSCREEN;


//
// D_ProcessEvents
// Send all the events of the given timestamp down the responder chain
//

void D_ProcessEvents (void)
{
    event_t*        ev;
        
    // IF STORE DEMO, DO NOT ACCEPT INPUT
    if (storedemo)
        return;
        
    while ((ev = D_PopEvent()) != NULL)
    {
        if (M_Responder (ev) || C_Responder (ev))
            continue;     // menu ate the event
        G_Responder (ev);
    }
}

//
// D_Display
//  draw current display, possibly wiping it from the previous
//

void D_Display (void)
{
    static  boolean             viewactivestate = false;
    static  boolean             menuactivestate = false;
    static  boolean             inhelpscreensstate = false;
    static  boolean             fullscreen = false;
    static  gamestate_t         oldgamestate = -1;
    static  int                 borderdrawcount;
    int                         nowtime;
    int                         tics;
    int                         wipestart;
    boolean                     done;
    boolean                     wipe;

    redrawsbar = false;
    
    // change the view size if needed
    if (setsizeneeded)
    {
        R_ExecuteSetViewSize ();
        oldgamestate = -1;                      // force background redraw
        borderdrawcount = 3;
    }

    // save the current screen if about to wipe
    if (gamestate != wipegamestate)
    {
        if(dots_enabled == 1)       // ADDED FOR PSP TO PREVENT CRASH...
            display_ticker = false; // ...UPON WIPING SCREEN WITH ENABLED DISPLAY TICKER
        if(fps_enabled == 1)        // ADDED FOR PSP TO PREVENT CRASH...
            display_fps = 0;        // ...UPON WIPING SCREEN WITH ENABLED DISPLAY TICKER

        wipe = true;
        wipe_StartScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);
    }
    else
    {
        if(dots_enabled == 1)       // ADDED FOR PSP TO PREVENT CRASH...
            display_ticker = true;  // ...UPON WIPING SCREEN WITH ENABLED DISPLAY TICKER
        if(fps_enabled == 1)        // ADDED FOR PSP TO PREVENT CRASH...
            display_fps = 1;        // ...UPON WIPING SCREEN WITH ENABLED DISPLAY TICKER

        wipe = false;
    }

    if (gamestate == GS_LEVEL && gametic)
        HU_Erase();
    
    // do buffered drawing
    switch (gamestate)
    {
      case GS_LEVEL:
        if (!gametic)
            break;
        if (automapactive)
            AM_Drawer ();
        if (wipe || (scaledviewheight != (200 << hires) && fullscreen) ) // HIRES
            redrawsbar = true;
        if (inhelpscreensstate && !inhelpscreens)
            redrawsbar = true;              // just put away the help screen
        ST_Drawer (scaledviewheight == (200 << hires), redrawsbar );     // HIRES

        if(warped == 1)
        {
            paused = true;
            currentMenu = &CheatsDef;
            menuactive = 1;
            itemOn = currentMenu->lastOn;
            warped = 0;
        }

        fullscreen = scaledviewheight == (200 << hires); // CHANGED FOR HIRES
        break;

      case GS_INTERMISSION:
        WI_Drawer ();
        break;

      case GS_FINALE:
        F_Drawer ();
        break;

      case GS_DEMOSCREEN:
        D_PageDrawer ();
        break;

      case GS_CONSOLE:
        break;
    }
    
    // draw buffered stuff to screen
    I_UpdateNoBlit ();
    
    // draw the view directly
    if (gamestate == GS_LEVEL && !automapactive && gametic)
        R_RenderPlayerView (&players[displayplayer]);

    if (gamestate == GS_LEVEL && gametic)
        HU_Drawer ();
    
    // clean up border stuff
    if (gamestate != oldgamestate && gamestate != GS_LEVEL)
        I_SetPalette (W_CacheLumpName (DEH_String("PLAYPAL"),PU_CACHE));

    // see if the border needs to be initially drawn
    if (gamestate == GS_LEVEL && oldgamestate != GS_LEVEL)
    {
        viewactivestate = false;        // view was not active
        R_FillBackScreen ();    // draw the pattern into the back screen
    }

    if(hud && !automapactive && screenSize == 8 && usergame && gamestate == GS_LEVEL)
        ST_drawEx();

    // see if the border needs to be updated to the screen
    if (gamestate == GS_LEVEL && !automapactive && scaledviewwidth != (320 << hires)) // HIRES
    {
        if (menuactive || menuactivestate || !viewactivestate)
            borderdrawcount = 3;
        if (borderdrawcount)
        {
            R_DrawViewBorder ();    // erase old menu stuff
            borderdrawcount--;
        }

    }

    menuactivestate = menuactive;
    viewactivestate = viewactive;
    inhelpscreensstate = inhelpscreens;
    oldgamestate = wipegamestate = gamestate;

    C_Drawer();

    // menus go directly to the screen
    M_Drawer ();          // menu is drawn even on top of everything
    NetUpdate ();         // send out any new accumulation


    // normal update
    if (!wipe)
    {
        I_FinishUpdate ();              // page flip or blit buffer
        return;
    }
    
    // wipe update
    wipe_EndScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);

    wipestart = I_GetTime () - 1;

    do
    {
        do
        {
            nowtime = I_GetTime ();
            tics = nowtime - wipestart;
            I_Sleep(1);
        } while (tics <= 0);
        
        wipestart = nowtime;
        done = wipe_ScreenWipe(wipe_Melt
                               , 0, 0, SCREENWIDTH, SCREENHEIGHT, tics);
        I_UpdateNoBlit ();
        M_Drawer ();                            // menu is drawn even on top of wipes
        I_FinishUpdate ();                      // page flip or blit buffer
    } while (!done);
}

//
// Add configuration file variable bindings.
//

void D_BindVariables(void)
{
    M_BindBaseControls();
}

//
// D-DoomLoop()
// Not a globally visible function,
//  just included for source reference,
//  called by D_DoomMain, never exits.
// Manages timing and IO,
//  calls all ?_Responder, ?_Ticker, and ?_Drawer,
//  calls I_GetTime, I_StartFrame, and I_StartTic
//

void D_DoomLoop (void)
{
    if (demorecording)
        G_BeginRecording ();

    if(usb)
    {
        debugfile = fopen("usb:/apps/wiidoom/debug.txt","w");
        statsfile = fopen("usb:/apps/wiidoom/stats.txt","w");
    }
    else if(sd)
    {
        debugfile = fopen("sd:/apps/wiidoom/debug.txt","w");
        statsfile = fopen("sd:/apps/wiidoom/stats.txt","w");
    }

    main_loop_started = true;

    TryRunTics();

    I_InitGraphics();
    I_EnableLoadingDisk();

    V_RestoreBuffer();
    R_ExecuteSetViewSize();

    D_StartGameLoop();

    while (1)
    {
        // check if the OGG music stopped playing
        if(usergame && gamestate != GS_DEMOSCREEN && gamestate != GS_CONSOLE && !finale_music)
            I_SDL_PollMusic();

        // frame syncronous IO operations
        I_StartFrame ();

        // will run at least one tic
        TryRunTics ();

        // move positional sounds
        S_UpdateSounds (players[consoleplayer].mo);

        // Update display, next frame, with current state.
        if (screenvisible)
            D_Display ();
    }
}

//
// D_PageTicker
// Handles timing for warped projection
//

void D_PageTicker (void)
{
    if (--pagetic < 0)
        D_AdvanceDemo ();
}

//
// D_PageDrawer
//

void D_PageDrawer (void)
{
    V_DrawPatch (0, 0, W_CacheLumpName(pagename, PU_CACHE));
}


//
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
//

void D_AdvanceDemo (void)
{
    advancedemo = true;
}


//
// This cycles through the demo sequences.
// FIXME - version dependend demo numbers?
//

void D_DoAdvanceDemo (void)
{
    players[consoleplayer].playerstate = PST_LIVE;  // not reborn
    advancedemo = false;
    usergame = false;               // no save / end game here
    paused = false;
    gameaction = ga_nothing;

    // The Ultimate Doom executable changed the demo sequence to add
    // a DEMO4 demo.  Final Doom was based on Ultimate, so also
    // includes this change; however, the Final Doom IWADs do not
    // include a DEMO4 lump, so the game bombs out with an error
    // when it reaches this point in the demo sequence.

    // However! There is an alternate version of Final Doom that
    // includes a fixed executable.

    if (gameversion == exe_ultimate || gameversion == exe_final)
      demosequence = (demosequence+1)%7;
    else
      demosequence = (demosequence+1)%6;
    
    switch (demosequence)
    {
      case 0:
        if ( gamemode == commercial )
            pagetic = TICRATE * 11;
        else
            pagetic = 170;
        gamestate = GS_DEMOSCREEN;
        pagename = DEH_String("TITLEPIC");
        if ( gamemode == commercial )
        {
            S_StartMusic(mus_dm2ttl);
        }
        else
        {
            S_StartMusic (mus_intro);
        }
        break;
      case 1:
/*
        if (fsize != 4261144 && fsize != 10401760 && fsize != 10396254 && fsize != 4274218 &&
                fsize != 4207819)
            G_DeferedPlayDemo(DEH_String("demo1"));
        else if(fsize == 4261144)
            G_DeferedPlayDemo(DEH_String("BT14LEV3"));
        else if(fsize == 10401760)
            G_DeferedPlayDemo(DEH_String("demo_3_1"));
        else if(fsize == 10396254)
            G_DeferedPlayDemo(DEH_String("demo_3_1"));
        else if(fsize == 4274218)
            G_DeferedPlayDemo(DEH_String("demo_4_1"));
        else if(fsize == 4207819)
            G_DeferedPlayDemo(DEH_String("BT14LEV3"));
*/
        break;
      case 2:
        pagetic = 200;
        gamestate = GS_DEMOSCREEN;
        pagename = DEH_String("CREDIT");
        break;
      case 3:
/*
        if (fsize != 4261144 && fsize != 10401760 && fsize != 10396254 && fsize != 4274218 &&
                fsize != 4207819)
            G_DeferedPlayDemo(DEH_String("demo2"));
        else if(fsize == 4261144)
            G_DeferedPlayDemo(DEH_String("demo_1_2"));
        else if(fsize == 10401760)
            G_DeferedPlayDemo(DEH_String("demo_2_2"));
        else if(fsize == 10396254)
            G_DeferedPlayDemo(DEH_String("demo_3_2"));
        else if(fsize == 4274218)
            G_DeferedPlayDemo(DEH_String("demo_4_2"));
        else if(fsize == 4207819)
            G_DeferedPlayDemo(DEH_String("demo_5_2"));
*/
        break;
      case 4:
        gamestate = GS_DEMOSCREEN;
        if ( gamemode == commercial)
        {
            pagetic = TICRATE * 11;
            pagename = DEH_String("TITLEPIC");
            S_StartMusic(mus_dm2ttl);
        }
        else
        {
            pagetic = 200;

            if ( gamemode == retail )
              pagename = DEH_String("CREDIT");
            else
            {
                if(fsize != 12361532)
                    pagename = DEH_String("HELP2");
                else
                    pagename = DEH_String("HELP1");
            }
        }
        break;
      case 5:
/*
        if (fsize != 4261144 && fsize != 10401760 && fsize != 10396254 && fsize != 4274218 &&
                fsize != 4207819)
            G_DeferedPlayDemo(DEH_String("demo3"));
        else if(fsize == 4261144)
            G_DeferedPlayDemo(DEH_String("demo_1_3"));
        else if(fsize == 10401760)
            G_DeferedPlayDemo(DEH_String("demo_2_3"));
        else if(fsize == 10396254)
            G_DeferedPlayDemo(DEH_String("demo_3_3"));
        else if(fsize == 4274218)
            G_DeferedPlayDemo(DEH_String("demo_4_3"));
        else if(fsize == 4207819)
            G_DeferedPlayDemo(DEH_String("demo_5_3"));
*/
        break;
        // THE DEFINITIVE DOOM Special Edition demo
      case 6:
//        G_DeferedPlayDemo(DEH_String("demo4"));
        break;
    }

    // The Doom 3: BFG Edition version of doom2.wad does not have a
    // TITLETPIC lump. Use INTERPIC instead as a workaround.
    if (bfgedition && !strcasecmp(pagename, "TITLEPIC"))
    {
        pagename = "INTERPIC";
    }

    C_InstaPopup();       // make console go away
}

//
// D_StartTitle
//

void D_StartTitle (void)
{
    gameaction = ga_nothing;
    demosequence = -1;
    D_AdvanceDemo ();
    C_InstaPopup();       // make console go away
}

// Strings for dehacked replacements of the startup banner
//
// These are from the original source: some of them are perhaps
// not used in any dehacked patches

static char *banners[] =
{
    // doom2.wad
    "                         "
    "DOOM 2: Hell on Earth v%i.%i"
    "                           ",
    // doom1.wad
    "                            "
    "DOOM Shareware Startup v%i.%i"
    "                           ",
    // doom.wad
    "                            "
    "DOOM Registered Startup v%i.%i"
    "                           ",
    // Registered DOOM uses this
    "                          "
    "DOOM System Startup v%i.%i"
    "                          ",
    // doom.wad (Ultimate DOOM)
    "                         "
    "The Ultimate DOOM Startup v%i.%i"
    "                        ",
    // tnt.wad
    "                     "
    "DOOM 2: TNT - Evilution v%i.%i"
    "                           ",
    // plutonia.wad
    "                   "
    "DOOM 2: Plutonia Experiment v%i.%i"
    "                           ",
};

//
// Get game name: if the startup banner has been replaced, use that.
// Otherwise, use the name given
// 

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wchar-subscripts"

static char *GetGameName(char *gamename)
{
    size_t i;
    char *deh_sub;
    
    for (i=0; i<arrlen(banners); ++i)
    {
        // Has the banner been replaced?

        deh_sub = DEH_String(banners[i]);
        
        if (deh_sub != banners[i])
        {
            // Has been replaced
            // We need to expand via printf to include the Doom version 
            // number
            // We also need to cut off spaces to get the basic name

            gamename = Z_Malloc(strlen(deh_sub) + 10, PU_STATIC, 0);
            sprintf(gamename, deh_sub, DOOM_VERSION / 100, DOOM_VERSION % 100);

            while (gamename[0] != '\0' && isspace(gamename[0]))
                strcpy(gamename, gamename+1);

            while (gamename[0] != '\0' && isspace(gamename[strlen(gamename)-1]))
                gamename[strlen(gamename) - 1] = '\0';
            
            return gamename;
        }
    }

    return gamename;
}

// Set the gamedescription string

void D_SetGameDescription(void)
{
    boolean is_freedoom = W_CheckNumForName("FREEDOOM") >= 0,
            is_freedm = W_CheckNumForName("FREEDM") >= 0;

    gamedescription = "Unknown";

    if (logical_gamemission == doom)
    {
        // Doom 1.  But which version?

        if (is_freedoom)
        {
            gamedescription = GetGameName("Freedoom: Phase 1");
        }
        else if (gamemode == retail)
        {
            // Ultimate Doom

            gamedescription = GetGameName("The Ultimate DOOM");
        } 
        else if (gamemode == registered)
        {
            gamedescription = GetGameName("DOOM Registered");
        }
        else if (gamemode == shareware)
        {
            gamedescription = GetGameName("DOOM Shareware");
        }
    }
    else
    {
        // Doom 2 of some kind.  But which mission?

        if (is_freedoom)
        {
            if (is_freedm)
                gamedescription = GetGameName("FreeDM");
            else
                gamedescription = GetGameName("Freedoom: Phase 2");
        }
        else if (logical_gamemission == doom2)
            gamedescription = GetGameName("DOOM 2: Hell on Earth");
        else if (logical_gamemission == pack_plut)
            gamedescription = GetGameName("DOOM 2: Plutonia Experiment"); 
        else if (logical_gamemission == pack_tnt)
            gamedescription = GetGameName("DOOM 2: TNT - Evilution");
        else if (logical_gamemission == pack_nerve)
            gamedescription = GetGameName("DOOM 2: No Rest For The Living");
    }
}

static boolean D_AddFile(char *filename, boolean automatic)
{
    wad_file_t *handle;

    if(gamemode == shareware || load_extra_wad == 1 || version13 == true)
    {
        if(dont_show_adding_of_resource_wad == 0)
        {
            printf("         adding %s\n", filename);
        }
    }
    handle = W_AddFile(filename, automatic);

    return handle != NULL;
}

// Load the Chex Quest dehacked file, if we are in Chex mode.

static void LoadChexDeh(void)
{
    if (gameversion == exe_chex)
    {
        // Look for chex.deh in the same directory as the IWAD file.
        if (load_dehacked == 0)
        {
            printf("\n\n\n");
            printf(" ===============================================================================");
            printf("            !!! UNABLE TO FIND CHEX QUEST DEHACKED FILE (CHEX.DEH) !!!          \n");
            printf("                                                                                \n");
            printf("                THIS DEHACKED FILE IS REQUIRED IN ORDER TO EMULATE              ");
            printf("               CHEX.EXE CORRECTLY.  IT CAN BE FOUND IN YOUR NEAREST             ");
            printf("                         /IDGAMES REPOSITORY MIRROR AT:                         \n");
            printf("                                                                                \n");
            printf("                       UTILS/EXE_EDIT/PATCHES/CHEXDEH.ZIP                       ");
            printf("                                                                                ");
            printf("                                QUITTING NOW ...                                ");
            printf(" ===============================================================================");

            sleep(5);

            I_QuitSerialFail();
        }
    }
}

static void LoadNerveWad(void)
{
    int i;
    char lumpname[9];

    if (gamemission != doom2)
        return;

    if (bfgedition && !modifiedgame)
    {
        // rename level name patch lumps out of the way
        for (i = 0; i < 9; i++)
        {
            M_snprintf (lumpname, 9, "CWILV%2.2d", i);
            lumpinfo[W_GetNumForName(lumpname)].name[0] = 'N';
        }
    }
    else
    {
        i = W_GetNumForName("map01");
        gamemission = pack_nerve;
        DEH_AddStringReplacement ("TITLEPIC", "INTERPIC");
    }
}

static void LoadHacxDeh(void)
{
    // If this is the HACX IWAD, we need to load the DEHACKED lump.
    if (gameversion == exe_hacx)
    {
        if (!DEH_LoadLumpByName("DEHACKED", true, false))
        {
            I_Error("DEHACKED lump not found.  Please check that this is the "
                    "Hacx v1.2 IWAD.");
        }
        else
            C_Printf(" Parsed DEHACKED lump from IWAD file %s\n", target);
    }
}

static void G_CheckDemoStatusAtExit (void)
{
    G_CheckDemoStatus();
}

//
// D_DoomMain
//

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

void D_DoomMain (void)
{
    FILE *fprw;

    char            file[256];

    if(devparm || devparm_net)
//        fsize = 10399316;
        fsize = 14943400;

    printf(" ");

    printf("\n");

    if(usb)
        fprw = fopen("usb:/apps/wiidoom/pspdoom.wad","rb");
    else if(sd)
        fprw = fopen("sd:/apps/wiidoom/pspdoom.wad","rb");

    if(fprw)
    {
        resource_wad_exists = 1;

        fclose(fprw);

        W_CheckSize(0);
    }
    else
    {
        resource_wad_exists = 0;
    }

    if(print_resource_pwad_error)
    {
        printf("\n\n\n\n\n");
        printf(" ===============================================================================");
        printf("                         !!! WRONG RESOURCE PWAD FILE !!!                       ");
        printf("                   PLEASE COPY THE FILE 'PSPDOOM.WAD' THAT CAME                 ");
        printf("                    WITH THIS RELEASE, INTO THE GAME DIRECTORY                  \n");
        printf("                                                                                \n");
        printf("                                QUITTING NOW ...                                ");
        printf(" ===============================================================================");

        sleep(5);

        I_QuitSerialFail();
    }

    if (fsize != 4261144  &&  // DOOM BETA v1.4
        fsize != 4271324  &&  // DOOM BETA v1.5
        fsize != 4211660  &&  // DOOM BETA v1.6
        fsize != 10396254 &&  // DOOM REGISTERED v1.1
        fsize != 10399316 &&  // DOOM REGISTERED v1.2
        fsize != 10401760 &&  // DOOM REGISTERED v1.6
        fsize != 11159840 &&  // DOOM REGISTERED v1.8
        fsize != 12408292 &&  // DOOM REGISTERED v1.9 (THE ULTIMATE DOOM)
        fsize != 12474561 &&  // DOOM REGISTERED (BFG-XBOX360 EDITION)
        fsize != 12487824 &&  // DOOM REGISTERED (BFG-PC EDITION)
        fsize != 12538385 &&  // DOOM REGISTERED (XBOX EDITION)
        fsize != 4207819  &&  // DOOM SHAREWARE v1.0
        fsize != 4274218  &&  // DOOM SHAREWARE v1.1
        fsize != 4225504  &&  // DOOM SHAREWARE v1.2
        fsize != 4225460  &&  // DOOM SHAREWARE v1.25 (SYBEX RELEASE)
        fsize != 4234124  &&  // DOOM SHAREWARE v1.666
        fsize != 4196020  &&  // DOOM SHAREWARE v1.8
        fsize != 14943400 &&  // DOOM 2 REGISTERED v1.666
        fsize != 14824716 &&  // DOOM 2 REGISTERED v1.666 (GERMAN VERSION)
        fsize != 14612688 &&  // DOOM 2 REGISTERED v1.7
        fsize != 14607420 &&  // DOOM 2 REGISTERED v1.8 (FRENCH VERSION)
        fsize != 14604584 &&  // DOOM 2 REGISTERED v1.9
        fsize != 14677988 &&  // DOOM 2 REGISTERED (BFG-PSN EDITION)
        fsize != 14691821 &&  // DOOM 2 REGISTERED (BFG-PC EDITION)
        fsize != 14683458 &&  // DOOM 2 REGISTERED (XBOX EDITION)
        fsize != 18195736 &&  // FINAL DOOM - TNT v1.9 (WITH YELLOW KEYCARD BUG)
        fsize != 18654796 &&  // FINAL DOOM - TNT v1.9 (WITHOUT YELLOW KEYCARD BUG)
        fsize != 18240172 &&  // FINAL DOOM - PLUTONIA v1.9 (WITH DEATHMATCH STARTS)
        fsize != 17420824 &&  // FINAL DOOM - PLUTONIA v1.9 (WITHOUT DEATHMATCH STARTS)
/*
        fsize != 19801320 &&  // FREEDOOM v0.6.4
        fsize != 27704188 &&  // FREEDOOM v0.7 RC 1
        fsize != 27625596 &&  // FREEDOOM v0.7
        fsize != 28144744 &&  // FREEDOOM v0.8 BETA 1
        fsize != 28592816 &&  // FREEDOOM v0.8
        fsize != 19362644 &&  // FREEDOOM v0.8 PHASE 1
*/
        fsize != 28422764 &&  // FREEDOOM v0.8 PHASE 2
        fsize != 12361532 &&  // CHEX QUEST
/*
        fsize != 9745831  &&  // HACX SHAREWARE v1.0
        fsize != 21951805 &&  // HACX REGISTERED v1.0
        fsize != 22102300 &&  // HACX REGISTERED v1.1
*/
        fsize != 19321722)    // HACX REGISTERED v1.2
    {
        printf("\n\n\n\n\n");
        printf(" ===============================================================================");
        printf("            WARNING: DOOM / DOOM 2 / TNT / PLUTONIA IWAD FILE MISSING,          ");
        printf("                         NOT SELECTED OR WRONG IWAD !!!                         \n");
        printf("                                                                                \n");
        printf("                                QUITTING NOW ...                                ");
        printf(" ===============================================================================");

        sleep(5);

        I_QuitSerialFail();
    }
    else if(fsize == 10396254   || // DOOM REGISTERED v1.1
            fsize == 10399316   || // DOOM REGISTERED v1.2
            fsize == 4207819    || // DOOM SHAREWARE v1.0
            fsize == 4274218    || // DOOM SHAREWARE v1.1
            fsize == 4225504    || // DOOM SHAREWARE v1.2
            fsize == 4225460)      // DOOM SHAREWARE v1.25 (SYBEX RELEASE)
    {
        printStyledText(1, 1,CONSOLE_FONT_BLUE,CONSOLE_FONT_YELLOW,CONSOLE_FONT_BOLD,&stTexteLocation,"                          DOOM Operating System v1.2                           ");
    }
    else if(fsize == 4261144    || // DOOM BETA v1.4
            fsize == 4271324    || // DOOM BETA v1.5
            fsize == 4211660    || // DOOM BETA v1.6
            fsize == 10401760   || // DOOM REGISTERED v1.6
            fsize == 11159840   || // DOOM REGISTERED v1.8
            fsize == 4234124    || // DOOM SHAREWARE v1.666
            fsize == 4196020)      // DOOM SHAREWARE v1.8
    {
        printStyledText(1, 1,CONSOLE_FONT_RED,CONSOLE_FONT_WHITE,CONSOLE_FONT_BOLD,&stTexteLocation,"                           DOOM System Startup v1.4                            ");

        version13 = true;
    }
    else if(fsize == 12408292   || // DOOM REGISTERED v1.9 (THE ULTIMATE DOOM)
            fsize == 12538385   || // DOOM REGISTERED (XBOX EDITION)
            fsize == 12487824   || // DOOM REGISTERED (BFG-PC EDITION)
            fsize == 12474561      // DOOM REGISTERED (BFG-XBOX360 EDITION)
/*
                                ||
            fsize == 19362644      // FREEDOOM v0.8 PHASE 1
*/
            )
    {
        printStyledText(1, 1,CONSOLE_FONT_WHITE,CONSOLE_FONT_RED,CONSOLE_FONT_BOLD,&stTexteLocation,"                           DOOM System Startup v1.9                            ");

        version13 = true;
    }
    else if(fsize == 14943400   ||  // DOOM 2 REGISTERED v1.666
            fsize == 14824716   ||  // DOOM 2 REGISTERED v1.666 (GERMAN VERSION)
            fsize == 14612688   ||  // DOOM 2 REGISTERED v1.7
            fsize == 14607420)      // DOOM 2 REGISTERED v1.8 (FRENCH VERSION)
    {
        printStyledText(1, 1,CONSOLE_FONT_RED,CONSOLE_FONT_WHITE,CONSOLE_FONT_BOLD,&stTexteLocation,"                         DOOM 2: Hell on Earth v1.666                          ");

        version13 = true;
    }
    else if(fsize == 14604584   ||  // DOOM 2 REGISTERED v1.9
            fsize == 14677988   ||  // DOOM 2 REGISTERED (BFG-PSN EDITION)
            fsize == 14691821   ||  // DOOM 2 REGISTERED (BFG-PC EDITION)
            fsize == 14683458   ||  // DOOM 2 REGISTERED (XBOX EDITION)
/*
            fsize == 9745831    ||  // HACX SHAREWARE v1.0
            fsize == 21951805   ||  // HACX REGISTERED v1.0
            fsize == 22102300   ||  // HACX REGISTERED v1.1
*/
            fsize == 19321722   ||  // HACX REGISTERED v1.2
/*
            fsize == 19801320   ||  // FREEDOOM v0.6.4
            fsize == 27704188   ||  // FREEDOOM v0.7 RC 1
            fsize == 27625596   ||  // FREEDOOM v0.7
            fsize == 28144744   ||  // FREEDOOM v0.8 BETA 1
            fsize == 28592816   ||  // FREEDOOM v0.8
*/
            fsize == 28422764)      // FREEDOOM v0.8 PHASE 2
    {
        if (
/*
            fsize == 9745831    ||  // HACX SHAREWARE v1.0
            fsize == 21951805   ||  // HACX REGISTERED v1.0
            fsize == 22102300   ||  // HACX REGISTERED v1.1
*/
            fsize == 19321722)      // HACX REGISTERED v1.2

            printStyledText(1, 1, CONSOLE_FONT_WHITE, CONSOLE_FONT_RED,
                            CONSOLE_FONT_BOLD, &stTexteLocation,
            "                          HACX:  Twitch n' Kill v1.2                           ");
        else
            printStyledText(1, 1, CONSOLE_FONT_WHITE, CONSOLE_FONT_RED,
                            CONSOLE_FONT_BOLD, &stTexteLocation,
            "                          DOOM 2: Hell on Earth v1.9                           ");
        version13 = true;
    }
    else if(fsize == 18195736   ||  // FINAL DOOM - TNT v1.9 (WITH YELLOW KEYCARD BUG)
            fsize == 18654796   ||  // FINAL DOOM - TNT v1.9 (WITHOUT YELLOW KEYCARD BUG)
            fsize == 12361532)      // CHEX QUEST
    {
        if(fsize == 12361532)
            printStyledText(1, 1, CONSOLE_FONT_WHITE, CONSOLE_FONT_BLACK,
                            CONSOLE_FONT_BOLD, &stTexteLocation,
            "                            Chex (R) Quest Startup                             ");
        else
            printStyledText(1, 1, CONSOLE_FONT_WHITE, CONSOLE_FONT_BLACK,
                            CONSOLE_FONT_BOLD,&stTexteLocation,
            "                          DOOM 2: TNT Evilution v1.9                           ");
        version13 = true;
    }

    else if(fsize == 18240172   ||  // FINAL DOOM - PLUTONIA v1.9 (WITH DEATHMATCH STARTS)
            fsize == 17420824)      // FINAL DOOM - PLUTONIA v1.9 (WITHOUT DEATHMATCH STARTS)
    {
        printStyledText(1, 1, CONSOLE_FONT_WHITE, CONSOLE_FONT_BLACK,
                        CONSOLE_FONT_BOLD, &stTexteLocation,
        "                       DOOM 2: Plutonia Experiment v1.9                        ");
        version13 = true;
    }

    if (resource_wad_exists == 0)
    {
        printf("\n\n\n\n\n");
        printf(" ===============================================================================");
        printf("              WARNING: RESOURCE PWAD FILE 'PSPDOOM.WAD' MISSING!!!              ");
        printf("               PLEASE COPY THIS FILE INTO THE GAME'S DIRECTORY!!!               \n");
        printf("                                                                                \n");
        printf("                               QUITTING NOW ...                                 ");
        printf(" ===============================================================================");

        sleep(5);

        I_QuitSerialFail();
    }

    Z_Init ();

#ifdef FEATURE_MULTIPLAYER
    //!
    // @category net
    //
    // Start a dedicated server, routing packets but not participating
    // in the game itself.
    //

    if (dedicatedflag && devparm_net)
    {
        printf("Dedicated server mode.\n");
        NET_DedicatedServer();

        // Never returns
    }

    //!
    // @category net
    //
    // Query the Internet master server for a global list of active
    // servers.
    //

    if (searchflag && devparm_net)
    {
        NET_MasterQuery();
        exit(0);
    }

    //!
    // @category net
    //
    // Search the local LAN for running servers.
    //

    if (locallanflag && devparm_net)
    {
        NET_LANQuery();
        exit(0);
    }

#endif

#ifdef FEATURE_DEHACKED
    if(load_dehacked == 1)
        DEH_LoadFile(dehacked_file);

    if(fsize == 19321722)
        DEH_Init();
#endif
    modifiedgame = false;

    //!
    // @vanilla
    //
    // Disable monsters.
    //

    if(nomonstersflag && devparm_net)
        nomonsters = true;

    //!
    // @vanilla
    //
    // Monsters respawn after being killed.
    //

    if(respawnflag && devparm_net)
        respawnparm = true;

    //!
    // @vanilla
    //
    // Monsters move faster.
    //

    if(fastflag && devparm_net)
        fastparm = true;

    //!
    // @category net
    // @vanilla
    //
    // Start a deathmatch game.
    //

    if(deathmatchflag && devparm_net)
        deathmatch = 1;

    //!
    // @category net
    // @vanilla
    //
    // Start a deathmatch 2.0 game.  Weapons do not stay in place and
    // all items respawn after 30 seconds.
    //

    if(altdeathflag && devparm_net)
        deathmatch = 2;

    if (devparm || devparm_net)
    {
        printf(D_DEVSTR);
    }

    // Auto-detect the configuration dir.

    M_SetConfigDir(NULL);

    // init subsystems
    printf(" V_Init: allocate screens.\n");
    V_Init ();

    // Load configuration files before initialising other subsystems.
    printf(" M_LoadDefaults: Load system defaults.\n");
    M_SetConfigFilenames("default.cfg");
    D_BindVariables();
    M_LoadDefaults();

    if (runcount < 32768)
        runcount++;

    respawnparm = false;
    fastparm = false;

    if(!devparm && aiming_help != 0)
        aiming_help = 0;

    if(mus_engine > 1)
        mus_engine = 2;
    else if(mus_engine < 2)
        mus_engine = 1;

    if(mus_engine == 1)
        snd_musicdevice = SNDDEVICE_SB;
    else
        snd_musicdevice = SNDDEVICE_GENMIDI;

    // Save configuration at exit.
    I_AtExit(M_SaveDefaults, false);

    printf(" Z_Init: Init zone memory allocation daemon. \n");
    printf(" heap size: 0x3cdb000 \n");
    printf(" W_Init: Init WADfiles.\n");

    if (fsize == 4207819        ||  // DOOM SHAREWARE v1.0
        fsize == 4274218        ||  // DOOM SHAREWARE v1.1
        fsize == 4225504        ||  // DOOM SHAREWARE v1.2
        fsize == 4225460        ||  // DOOM SHAREWARE v1.25 (SYBEX RELEASE)
        fsize == 4234124        ||  // DOOM SHAREWARE v1.666
        fsize == 4196020        ||  // DOOM SHAREWARE v1.8
        fsize == 4261144        ||  // DOOM BETA v1.4
        fsize == 4271324        ||  // DOOM BETA v1.5
        fsize == 4211660)           // DOOM BETA v1.6
    {
        gamemode = shareware;
        gamemission = doom;
        gameversion = exe_doom_1_9;
        nerve_pwad = false;
    }
    else if(fsize == 10396254   ||  // DOOM REGISTERED v1.1
            fsize == 10399316   ||  // DOOM REGISTERED v1.2
            fsize == 10401760   ||  // DOOM REGISTERED v1.6
            fsize == 11159840)      // DOOM REGISTERED v1.8
    {
        gamemode = registered;
        gamemission = doom;
        gameversion = exe_doom_1_9;
        nerve_pwad = false;
    }
    else if(fsize == 12408292   ||  // DOOM REGISTERED v1.9 (THE ULTIMATE DOOM)
            fsize == 12538385   ||  // DOOM REGISTERED (XBOX EDITION)
            fsize == 12487824   ||  // DOOM REGISTERED (BFG-PC EDITION)
            fsize == 12474561       // DOOM REGISTERED (BFG-XBOX360 EDITION)
/*
                                ||
            fsize == 19362644
*/
                             )      // FREEDOOM v0.8 PHASE 1
    {
        gamemode = retail;
        gamemission = doom;
        gameversion = exe_ultimate;
        nerve_pwad = false;
    }
    else if(fsize == 14943400   ||  // DOOM 2 REGISTERED v1.666
            fsize == 14824716   ||  // DOOM 2 REGISTERED v1.666 (GERMAN VERSION)
            fsize == 14612688   ||  // DOOM 2 REGISTERED v1.7
            fsize == 14607420   ||  // DOOM 2 REGISTERED v1.8 (FRENCH VERSION)
            fsize == 14604584   ||  // DOOM 2 REGISTERED v1.9
            fsize == 14677988   ||  // DOOM 2 REGISTERED (BFG-PSN EDITION)
            fsize == 14691821   ||  // DOOM 2 REGISTERED (BFG-PC EDITION)
            fsize == 14683458   ||  // DOOM 2 REGISTERED (XBOX EDITION)
/*
            fsize == 19801320   ||  // FREEDOOM v0.6.4
            fsize == 27704188   ||  // FREEDOOM v0.7 RC 1
            fsize == 27625596   ||  // FREEDOOM v0.7
            fsize == 28144744   ||  // FREEDOOM v0.8 BETA 1
            fsize == 28592816   ||  // FREEDOOM v0.8
*/
            fsize == 28422764)      // FREEDOOM v0.8 PHASE 2
    {
        gamemode = commercial;
        gamemission = doom2;
        gameversion = exe_doom_1_9;
    }
    else if(fsize == 18195736   ||  // FINAL DOOM - TNT v1.9 (WITH YELLOW KEYCARD BUG)
            fsize == 18654796)      // FINAL DOOM - TNT v1.9 (WITHOUT YELLOW KEYCARD BUG)
    {
        gamemode = commercial;
        gamemission = pack_tnt;
        gameversion = exe_final;
        nerve_pwad = false;
    }
    else if(fsize == 18240172   ||  // FINAL DOOM - PLUTONIA v1.9 (WITH DEATHMATCH STARTS)
            fsize == 17420824)      // FINAL DOOM - PLUTONIA v1.9 (WITHOUT DEATHMATCH STARTS)
    {
        gamemode = commercial;
        gamemission = pack_plut;
        gameversion = exe_final;
        nerve_pwad = false;
    }
    else if(fsize == 12361532)      // CHEX QUEST
    {
        gamemode = shareware;
        gamemission = pack_chex;
        gameversion = exe_chex;
        nerve_pwad = false;
    }
    else if(
/*
            fsize == 9745831    ||  // HACX SHAREWARE v1.0
            fsize == 21951805   ||  // HACX REGISTERED v1.0
            fsize == 22102300   ||  // HACX REGISTERED v1.1
*/
            fsize == 19321722)      // HACX REGISTERED v1.2
    {
        gamemode = commercial;
        gamemission = pack_hacx;
        gameversion = exe_hacx;
        nerve_pwad = false;
    }

    iwad_added = false;
    pwad_added = true;

    startuptimer = I_GetTimeMS();

    if(devparm || devparm_net)
    {
        if(usb)
//            D_AddFile("usb:/apps/wiidoom/IWAD/DOOM/Reg/v12/DOOM.WAD", true);
            D_AddFile("usb:/apps/wiidoom/IWAD/DOOM2/v1666/DOOM2.WAD", true);
        else if(sd)
//            D_AddFile("sd:/apps/wiidoom/IWAD/DOOM/Reg/v12/DOOM.WAD", true);
            D_AddFile("sd:/apps/wiidoom/IWAD/DOOM2/v1666/DOOM2.WAD", true);
    }
    else
        D_AddFile(target, false);

    iwad_added = true;

    if(gamemode != shareware || (gamemode == shareware && gameversion == exe_chex))
    {
        if(load_extra_wad == 1)
        {
            opl = 1;

            if(extra_wad_slot_1_loaded == 1)
                D_AddFile(extra_wad_1, false);

            if(!nerve_pwad)
            {
                if(extra_wad_slot_2_loaded == 1)
                    D_AddFile(extra_wad_2, false);

                if(extra_wad_slot_3_loaded == 1)
                    D_AddFile(extra_wad_3, false);
            }
            modifiedgame = true;
        }
    }

    if(devparm_nerve)
        D_AddFile("usb:/apps/wiidoom/PWAD/DOOM2/NERVE.WAD", true);

    dont_show_adding_of_resource_wad = 0;

    pwad_added = false;

    if(usb)
        W_MergeFile("usb:/apps/wiidoom/pspdoom.wad", true);
    else if(sd)
        W_MergeFile("sd:/apps/wiidoom/pspdoom.wad", true);

    pwad_added = true;

    if(devparm)
        C_Printf(D_DEVSTR);

    C_Printf(" V_Init: allocate screens.\n");
    C_Printf(" M_LoadDefaults: Load system defaults.\n");
    C_Printf(" Z_Init: Init zone memory allocation daemon. \n");
    C_Printf(" heap size: 0x3cdb000 \n");
    C_Printf(" W_Init: Init WADfiles.\n");

    if(show_deh_loading_message == 1)
    {
        printf("         adding %s\n", dehacked_file);
        C_Printf("         adding %s\n", dehacked_file);
    }

    // Debug:
    // W_PrintDirectory();

    I_AtExit(G_CheckDemoStatusAtExit, true);

    // Generate the WAD hash table.  Speed things up a bit.

    W_GenerateHashTable();

    if(fsize == 12361532)
        LoadChexDeh();

    if(
//        fsize == 9745831 || fsize == 21951805 || fsize == 22102300 ||
        fsize == 19321722)
        LoadHacxDeh();

    D_SetGameDescription();
    savegamedir = M_GetSaveGameDir(D_SaveGameIWADName(gamemission));

    if(fsize == 12361532)
    {
        W_CheckSize(1);

        if(print_resource_pwad_error)
        {
            printf("\n\n\n\n\n");
            printf(" ===============================================================================");
            printf("                         !!! WRONG RESOURCE PWAD FILE !!!                       ");
            printf("                   PLEASE COPY THE FILE 'PSPCHEX.WAD' THAT CAME                 ");
            printf("                    WITH THIS RELEASE, INTO THE GAME DIRECTORY                  \n");
            printf("                                                                                \n");
            printf("                                QUITTING NOW ...                                ");
            printf(" ===============================================================================");

            sleep(5);

            I_QuitSerialFail();
        }
        else
        {
            if(usb)
                D_AddFile("usb:/apps/wiidoom/pspchex.wad", true);
            else if(sd)
                D_AddFile("sd:/apps/wiidoom/pspchex.wad", true);
        }
    }
    else if(fsize == 19321722)
    {
        W_CheckSize(2);

        if(print_resource_pwad_error)
        {
            printf("\n\n\n\n\n");
            printf(" ===============================================================================");
            printf("                         !!! WRONG RESOURCE PWAD FILE !!!                       ");
            printf("                   PLEASE COPY THE FILE 'PSPHACX.WAD' THAT CAME                 ");
            printf("                    WITH THIS RELEASE, INTO THE GAME DIRECTORY                  \n");
            printf("                                                                                \n");
            printf("                                QUITTING NOW ...                                ");
            printf(" ===============================================================================");

            sleep(5);

            I_QuitSerialFail();
        }
        else
        {
            if(usb)
                D_AddFile("usb:/apps/wiidoom/psphacx.wad", true);
            else if(sd)
                D_AddFile("sd:/apps/wiidoom/psphacx.wad", true);
        }
    }
    else if(fsize == 28422764)
    {
        W_CheckSize(3);

        if(print_resource_pwad_error)
        {
            printf("\n\n\n\n\n");
            printf(" ===============================================================================");
            printf("                         !!! WRONG RESOURCE PWAD FILE !!!                       ");
            printf("                 PLEASE COPY THE FILE 'PSPFREEDOOM.WAD' THAT CAME               ");
            printf("                    WITH THIS RELEASE, INTO THE GAME DIRECTORY                  \n");
            printf("                                                                                \n");
            printf("                                QUITTING NOW ...                                ");
            printf(" ===============================================================================");

            sleep(5);

            I_QuitSerialFail();
        }
        else
        {
            if(usb)
                D_AddFile("usb:/apps/wiidoom/pspfreedoom.wad", true);
            else if(sd)
                D_AddFile("sd:/apps/wiidoom/pspfreedoom.wad", true);
        }
    }

    if(gamemode == shareware && gameversion != exe_chex)
    {
        printf("         shareware version.\n");
        C_Printf("         shareware version.\n");
    }
    else if((gamemode == shareware && gameversion == exe_chex) || gamemode == registered)
    {
        printf("         registered version.\n");
        C_Printf("         registered version.\n");
    }
    else
    {
        printf("         commercial version.\n");
        C_Printf("         commercial version.\n");
    }
    if(gamemode == retail || gamemode == registered)
    {
        printf(" ===============================================================================");
        printf("                 This version is NOT SHAREWARE, do not distribute!              ");
        printf("             Please report software piracy to the SPA: 1-800-388-PIR8           ");
        printf(" ===============================================================================");
        C_Printf(" ===============================================================================\n");
        C_Printf("                 This version is NOT SHAREWARE, do not distribute!              \n");
        C_Printf("             Please report software piracy to the SPA: 1-800-388-PIR8           \n");
        C_Printf(" ===============================================================================\n");
    }
    else if(gamemode == commercial)
    {
        printf(" ===============================================================================");
        printf("                                Do not distribute!                              ");
        printf("             Please report software piracy to the SPA: 1-800-388-PIR8           ");
        printf(" ===============================================================================");
        C_Printf(" ===============================================================================\n");
        C_Printf("                                Do not distribute!                              \n");
        C_Printf("             Please report software piracy to the SPA: 1-800-388-PIR8           \n");
        C_Printf(" ===============================================================================\n");
    }

    if(modifiedgame)
    {
        while(1)
        {
            if(wad_message_has_been_shown == 1)
                goto skip_showing_message;
        
            printf(" ===============================================================================");
            printf("    ATTENTION:  This version of DOOM has been modified.  If you would like to   ");
            printf("   get a copy of the original game, call 1-800-IDGAMES or see the readme file.  ");
            printf("            You will not receive technical support for modified games.          ");
            printf("                             press enter to continue                            ");
            printf(" ===============================================================================");
            C_Printf(" ===============================================================================");
            C_Printf("    ATTENTION:  This version of DOOM has been modified.  If you would like to   ");
            C_Printf("   get a copy of the original game, call 1-800-IDGAMES or see the readme file.  ");
            C_Printf("            You will not receive technical support for modified games.          ");
            C_Printf("                             press enter to continue                            ");
            C_Printf(" ===============================================================================");

            skip_showing_message:
            {
            }

            u32 buttons = WaitButtons();

            if (buttons & WPAD_CLASSIC_BUTTON_A)
                break;

            WaitButtons();

            wad_message_has_been_shown = 1;
        }
    }

    // Check for -file in shareware
    if (modifiedgame)
    {
        // These are the lumps that will be checked in IWAD,
        // if any one is not present, execution will be aborted.
        char name[23][8]=
        {
            "e2m1","e2m2","e2m3","e2m4","e2m5","e2m6","e2m7","e2m8","e2m9",
            "e3m1","e3m3","e3m3","e3m4","e3m5","e3m6","e3m7","e3m8","e3m9",
            "dphoof","bfgga0","heada1","cybra1","spida1d1"
        };
        int i;
        
        if ( gamemode == shareware && gameversion != exe_chex)
            I_Error(DEH_String("\nYou cannot -file with the shareware "
                               "version. Register!"));

        // Check for fake IWAD with right name,
        // but w/o all the lumps of the registered version. 
        if (gamemode == registered)
            for (i = 0;i < 23; i++)
                if (W_CheckNumForName(name[i])<0)
                    I_Error(DEH_String("\nThis is not the registered version."));
    }

#ifdef FEATURE_MULTIPLAYER
    NET_Init ();
#endif

    // Initial netgame startup. Connect to server etc.
    D_ConnectNetGame();

    start_respawnparm = respawnparm;
    start_fastparm = fastparm;

    // get skill / episode / map from parms
    startskill = sk_medium;
    startepisode = 1;
    startmap = 1;

    if(devparm || devparm_net)
        autostart = true;
    else
        autostart = false;

    //!
    // @arg <skill>
    // @vanilla
    //
    // Set the game skill, 1-5 (1: easiest, 5: hardest).  A skill of
    // 0 disables all monsters.
    //

    if (skillflag && devparm_net)
    {
        startskill = mp_skill;
        autostart = true;
    }

    //!
    // @arg <n>
    // @vanilla
    //
    // Start playing on episode n (1-4)
    //

    timelimit = 0;

    //! 
    // @arg <n>
    // @category net
    // @vanilla
    //
    // For multiplayer games: exit each level after n minutes.
    //

    if (warpflag && devparm_net)
    {
        if (gamemode == commercial)
            startmap = warplev;
        else
        {
            startepisode = warpepi;

            startmap = 1;
        }
        autostart = true;
    }

    // Undocumented:
    // Invoked by setup to test the controls.
    // Not loading a game
    startloadgame = -1;

    printf(" M_Init: Init miscellaneous info.\n");
    C_Printf(" M_Init: Init miscellaneous info.\n");
    M_Init ();

    if(gameversion == exe_chex)
    {
        printf(" R_Init: Init Chex(R) Quest refresh daemon - ");
        C_Printf(" R_Init: Init Chex(R) Quest refresh daemon - ");
    }
    else
    {
        printf(" R_Init: Init DOOM refresh daemon - ");
        C_Printf(" R_Init: Init DOOM refresh daemon - ");
    }
    R_Init ();

    printf("\n P_Init: Init Playloop state.\n");
    C_Printf(" P_Init: Init Playloop state.\n");
    P_Init ();

    printf(" I_Init: Setting up machine state.\n");
    C_Printf(" I_Init: Setting up machine state.\n");

    printf(" I_StartupDPMI\n");
    C_Printf(" I_StartupDPMI\n");

    printf(" I_StartupMouse\n");
    C_Printf(" I_StartupMouse\n");

    printf(" I_StartupJoystick\n");
    C_Printf(" I_StartupJoystick\n");

    printf(" I_StartupKeyboard\n");
    C_Printf(" I_StartupKeyboard\n");

    printf(" I_StartupTimer\n");
    C_Printf(" I_StartupTimer\n");
    I_InitTimer();

    printf(" I_StartupSound\n");
    C_Printf(" I_StartupSound\n");

    printf(" calling DMX_Init\n");
    C_Printf(" calling DMX_Init\n");

    printf(" D_CheckNetGame: Checking network game status.\n");
    C_Printf(" D_CheckNetGame: Checking network game status.\n");
    D_CheckNetGame ();

    printf(" S_Init: Setting up sound.\n");
    C_Printf(" S_Init: Setting up sound.\n");
    S_Init (sfxVolume * 8, musicVolume * 8);

    printf(" HU_Init: Setting up heads up display.\n");
    C_Printf(" HU_Init: Setting up heads up display.\n");
    HU_Init ();

    printf(" ST_Init: Init status bar.\n");
    C_Printf(" ST_Init: Init status bar.\n");
    ST_Init ();

    // If Doom II without a MAP01 lump, this is a store demo.
    // Moved this here so that MAP01 isn't constantly looked up
    // in the main loop.

    if (gamemode == commercial && W_CheckNumForName("map01") < 0)
        storedemo = true;

    // Doom 3: BFG Edition includes modified versions of the classic
    // IWADs which can be identified by an additional DMENUPIC lump.
    // Furthermore, the M_GDHIGH lumps have been modified in a way that
    // makes them incompatible to Vanilla Doom and the modified version
    // of doom2.wad is missing the TITLEPIC lump.
    // We specifically check for DMENUPIC here, before PWADs have been
    // loaded which could probably include a lump of that name.

    if (W_CheckNumForName("dehacked") >= 0)
        C_Printf(" Parsed DEHACKED lump\n");

    if (W_CheckNumForName("dmenupic") >= 0)
    {
        bfgedition = true;

        // BFG Edition changes the names of the secret levels to
        // censor the Wolfenstein references. It also has an extra
        // secret level (MAP33). In Vanilla Doom (meaning the DOS
        // version), MAP33 overflows into the Plutonia level names
        // array, so HUSTR_33 is actually PHUSTR_1.

        DEH_AddStringReplacement(HUSTR_31, "level 31: idkfa");
        DEH_AddStringReplacement(HUSTR_32, "level 32: keen");
        DEH_AddStringReplacement(PHUSTR_1, "level 33: betray");

        // The BFG edition doesn't have the "low detail" menu option (fair
        // enough). But bizarrely, it reuses the M_GDHIGH patch as a label
        // for the options menu (says "Fullscreen:"). Why the perpetrators
        // couldn't just add a new graphic lump and had to reuse this one,
        // I don't know.
        //
        // The end result is that M_GDHIGH is too wide and causes the game
        // to crash. As a workaround to get a minimum level of support for
        // the BFG edition IWADs, use the "ON"/"OFF" graphics instead.

        DEH_AddStringReplacement("M_GDHIGH", "M_MSGON");
        DEH_AddStringReplacement("M_GDLOW", "M_MSGOFF");
    }

    if(nerve_pwad)
        LoadNerveWad();

    if (startloadgame >= 0)
    {
        strcpy(file, P_SaveGameFile(startloadgame));
        G_LoadGame (file);
    }
        
    if (gameaction != ga_loadgame )
    {
        if (autostart || netgame)
            G_InitNew (startskill, startepisode, startmap);
        else
            D_StartTitle ();                // start up intro loop
    }

    startuptimer = I_GetTimeMS() - startuptimer;

    C_Printf(" Startup took %02i:%02i:%02i.%i to complete.\n",
        (startuptimer / (1000 * 60 * 60)) % 24,
        (startuptimer / (1000 * 60)) % 60,
        (startuptimer / 1000) % 60,
        (startuptimer % 1000) / 10);

    D_DoomLoop ();  // never returns
}

