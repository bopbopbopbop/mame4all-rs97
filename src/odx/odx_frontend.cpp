#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h> 


#include "minimal.h"

#include "odx_frontend_list.h"

#define Y_INCREMENT 12
#ifdef GAMETA
#define VERSION_NUMBER "GAMETA V0.4.4"

#endif
#ifdef RS07
#define VERSION_NUMBER "RS-07 V0.4.4"

#endif

#ifdef RS97
#define VERSION_NUMBER "RFW-97 V0.4.4"
#endif


#ifdef RS07

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272 
#define BMP_SIZE ((SCREEN_HEIGHT*SCREEN_WIDTH)+(256*4)+54)
#define Y_BOTTOM_LINE (SCREEN_HEIGHT-Y_INCREMENT)

#endif

#ifdef RS97

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240 
#define BMP_SIZE ((SCREEN_HEIGHT*SCREEN_WIDTH)+(256*4)+54)
#define Y_BOTTOM_LINE (SCREEN_HEIGHT-Y_INCREMENT)
#endif

#ifdef GAMETA

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320 
#define BMP_SIZE ((SCREEN_HEIGHT*SCREEN_WIDTH)+(256*4)+54)
#define Y_BOTTOM_LINE (SCREEN_HEIGHT-Y_INCREMENT)
#endif


#define X_BUILD		(SCREEN_WIDTH - ((16 * 6)+2))



#define MAX_AMOUNT_OF_GAMES_ONSCREEN 14
#define PAGE_SKIP (MAX_AMOUNT_OF_GAMES_ONSCREEN * 4)
#define FILE_LIST_ROWS (MAX_AMOUNT_OF_GAMES_ONSCREEN - 4)
#define COMPATCORES 1

#define JOY_NUM 0




static unsigned char splash_bmp[BMP_SIZE];
static unsigned char menu_bmp[BMP_SIZE];

#ifdef logging
#define FRONTEND_LOG "frontend.log"
static FILE *errorlog;
#endif


int game_num_avail=0;
static int last_game_selected=0;
char playemu[16] = "mame\0";
char playgame[16] = "builtinn\0";

char mamedir[512];

int odx_freq=600;       /* was 336, default dingoo Mhz */ 
int odx_video_depth=16; /* MAME video depth */
int odx_video_aspect=0; /* was 2, Scale best*/
int odx_video_sync=0;   /* No vsync */
int odx_frameskip=-1;
int odx_sound = 2;
//int odx_volume = 3;
int odx_clock_cpu=100;
int odx_clock_sound=100;
int odx_cpu_cores=1;
int odx_ramtweaks=0;
int odx_cheat=0;
int odx_gsensor=0;

bool want_exit = false, first_run = true;

char romdir[512];

signed int get_romdir(char *result) ;

#ifdef logging
static void logerror(const char *text,...)
{
	if (errorlog)
	{
		va_list arg;
		va_start(arg,text);
		vfprintf(errorlog,text,arg);
		va_end(arg);
	}
}
#endif

static void blit_bmp_8bpp(unsigned char *out, unsigned char *in) 
{
//	SDL_FillRect( layer, NULL, 0 );
	SDL_RWops *rw = SDL_RWFromMem(in, BMP_SIZE);
	SDL_Surface *temp = SDL_LoadBMP_RW(rw, 1);
	SDL_Surface *image;
	image = SDL_DisplayFormat(temp);
	SDL_FreeSurface(temp);
	
	// Display image
 	//SDL_BlitSurface(image, 0, layer, 0);
	SDL_BlitSurface(image, 0, video, 0);
	SDL_FreeSurface(image);
}

static void odx_intro_screen(void) {
	char name[256];
	FILE *f;
	
	#ifdef GAMETA
	sprintf(name,"skins/splash_GAMETA.bmp");
	#endif
	#ifdef RS07
	sprintf(name,"skins/splash_RS07.bmp");
	#endif
	#ifdef RS97
	sprintf(name,"skins/splash_RS97.bmp");
	#endif
	
	#ifdef logging
	//added. was wa, but we want a new file every time
	errorlog = fopen(FRONTEND_LOG,"w");
	#endif
	
	f=fopen(name,"rb");
	if (f) {
		fread(splash_bmp,1,BMP_SIZE,f);
		fclose(f);
	}
	blit_bmp_8bpp(od_screen8,splash_bmp);

	odx_gamelist_text_out(1,ODX_SCREEN_HEIGHT - Y_INCREMENT, VERSION_NUMBER);
	odx_gamelist_text_out(ODX_SCREEN_WIDTH - 110,Y_BOTTOM_LINE - (Y_INCREMENT * 2), "RS97 - bob_fossil");
	
	#ifdef RS07
	odx_gamelist_text_out(ODX_SCREEN_WIDTH - 110,Y_BOTTOM_LINE - Y_INCREMENT, "RS07 - RANDOMIZE");
	#endif
	#ifdef RS97
	odx_gamelist_text_out(ODX_SCREEN_WIDTH - 110,Y_BOTTOM_LINE - Y_INCREMENT, "RFW-97 - RANDOMIZE");
	#endif
	#ifdef GAMETA
	odx_gamelist_text_out(ODX_SCREEN_WIDTH - 110,Y_BOTTOM_LINE - Y_INCREMENT, "GAMETA - RANDOMIZE");
	#endif

	odx_video_flip();
	odx_joystick_press(JOY_NUM);
	
	#ifdef RS07
	sprintf(name,"skins/menu_RS07.bmp");
	#endif
	#ifdef RS97
	sprintf(name,"skins/menu_RS97.bmp");
	#endif
	#ifdef GAMETA
	sprintf(name,"skins/menu_GAMETA.bmp");
	#endif
	f=fopen(name,"rb");
	if (f) {
		fread(menu_bmp,1,BMP_SIZE,f);
		fclose(f);
	}
}

static void game_list_init_nocache(void)
{
	char text[512];
	int i;
	FILE *f;
	
	
	snprintf(text,sizeof(text),"%s",romdir);
	
	DIR *d=opendir(text);
	char game[32];
	if (d)
	{
		struct dirent *actual=readdir(d);
		while(actual)
		{
			for (i=0;i<NUMGAMES;i++)
			{
				if (frontend_drivers[i].available==0)
				{
					sprintf(game,"%s.zip",frontend_drivers[i].name);
					if (strcmp(actual->d_name,game)==0)
					{
						frontend_drivers[i].available=1;
						game_num_avail++;
						break;
					}
				}
			}
			actual=readdir(d);
		}
		closedir(d);
	}
	
	if (game_num_avail)
	{
		sprintf(text,"%s/frontend/mame.lst",mamedir);
		remove(text);
		/* sync(); */
		f=fopen(text,"w");
		if (f)
		{
			for (i=0;i<NUMGAMES;i++)
			{
				fputc(frontend_drivers[i].available,f);
			}
			fclose(f);
			/* sync(); */
		}
	}
}

static void game_list_init_cache(void)
{
	char text[512];
	FILE *f;
	int i;
	sprintf(text,"%s/frontend/mame.lst",mamedir);
	f=fopen(text,"r");
	if (f)
	{
		for (i=0;i<NUMGAMES;i++)
		{
			frontend_drivers[i].available=fgetc(f);
			if (frontend_drivers[i].available)
				game_num_avail++;
		}
		fclose(f);
	}
	else
		game_list_init_nocache();
}

static void game_list_init(int argc)
{
	if (argc==1)
		game_list_init_nocache();
	else
		game_list_init_cache();
}

static void game_list_view(int *pos) {

	int i;
	int view_pos;
	int aux_pos=0;
	int screen_y = 48;
	int screen_x = 38;

	/* Draw background image */
	blit_bmp_8bpp(od_screen8,menu_bmp);

	/* draw text */
	odx_gamelist_text_out( 4, 32,"Select ROM");
	odx_gamelist_text_out( 4, Y_BOTTOM_LINE,"A=Select Game/Start  B=Select Rom folder");
	odx_gamelist_text_out( SCREEN_WIDTH - 52, Y_BOTTOM_LINE,"L+R=Exit");
	odx_gamelist_text_out( X_BUILD,2,VERSION_NUMBER);

	/* Check Limits */
	if (*pos<0)
		*pos=game_num_avail-1;
	if (*pos>(game_num_avail-1))
		*pos=0;
					   
	/* Set View Pos */
	if (*pos<MAX_AMOUNT_OF_GAMES_ONSCREEN) { // ALEK 10
		view_pos=0;
	} else {
		if (*pos>game_num_avail-MAX_AMOUNT_OF_GAMES_ONSCREEN) { // ALEK 11
			view_pos=game_num_avail-MAX_AMOUNT_OF_GAMES_ONSCREEN; // ALEK 21
			view_pos=(view_pos<0?0:view_pos);
		} else {
			view_pos=*pos-MAX_AMOUNT_OF_GAMES_ONSCREEN; // ALEK 10
		}
	}

	/* Show List */
	for (i=0;i<NUMGAMES;i++) {
		if (frontend_drivers[i].available==1) {
			if (aux_pos>=view_pos && aux_pos<=view_pos+MAX_AMOUNT_OF_GAMES_ONSCREEN) { // ALEK 20
				odx_gamelist_text_out( screen_x, screen_y, frontend_drivers[i].description);
				if (aux_pos==*pos) {
					odx_gamelist_text_out( screen_x-10, screen_y,">" );
					odx_gamelist_text_out( screen_x-13, screen_y-1,"-" );
				}
				screen_y+=Y_INCREMENT;
			}
			aux_pos++;
		}
	}
}

static void game_list_select (int index, char *game, char *emu) {
	int i;
	int aux_pos=0;
	for (i=0;i<NUMGAMES;i++)
	{
		if (frontend_drivers[i].available==1)
		{
			if(aux_pos==index)
			{
				strcpy(game,frontend_drivers[i].name);
				strcpy(emu,frontend_drivers[i].exe);
				break;
			}
			aux_pos++;
		}
	}
}

static char *game_list_description (int index)
{
	int i;
	int aux_pos=0;
	for (i=0;i<NUMGAMES;i++) {
		if (frontend_drivers[i].available==1) {
			if(aux_pos==index) {
				return(frontend_drivers[i].description);
			}
			aux_pos++;
		   }
	}
	return ((char *)0);
}

static int show_options(char *game)
{
	unsigned long ExKey=0;
	int selected_option=0;
	int x_Pos = 41;
	int y_PosTop = 80;
	int y_Pos = y_PosTop;
	int options_count = 9;
	char text[512];
	FILE *f;
	int i=0;

	/* Read game configuration */
	sprintf(text,"%s/frontend/%s.cfg",mamedir,game);
	f=fopen(text,"r");
	if (f) {
		fscanf(f,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",&odx_freq,&odx_video_depth,&odx_video_aspect,&odx_video_sync,
		&odx_frameskip,&odx_sound,&odx_clock_cpu,&odx_clock_sound,&odx_cpu_cores,&odx_ramtweaks,&i,&odx_cheat,&odx_gsensor);
		fclose(f);
	}
	
	while(1)
	{
		y_Pos = y_PosTop;
	
		/* Draw background image */
		blit_bmp_8bpp(od_screen8,menu_bmp);

		/* draw text */
		odx_gamelist_text_out( 4, 40,"Game Options");
		odx_gamelist_text_out( 4, Y_BOTTOM_LINE,"A=Select Game/Start  B=Back");
		odx_gamelist_text_out( 268, Y_BOTTOM_LINE,"L+R=Exit");
		odx_gamelist_text_out( X_BUILD,2,VERSION_NUMBER);

		/* Draw the options */
		strncpy (text,game_list_description(last_game_selected),33);
		text[32]='\0';
		odx_gamelist_text_out(x_Pos,y_Pos-20,text);

		/* (0) Video Depth */
		y_Pos += Y_INCREMENT;
		switch (odx_video_depth)
		{
			case -1: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Video Depth    Auto"); break;
			case 8:  odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Video Depth    8 bit"); break;
			case 16: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Video Depth    16 bit"); break;
		}
		
		/* (1) Video Aspect */
		y_Pos += Y_INCREMENT;
		switch (odx_video_aspect)
		{
			case 0: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Video Aspect   Unscaled"); break;
			case 1: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Video Aspect   Scale Aspect - Downscale"); break;
			case 2: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Video Aspect   Scale Aspect - Upscale"); break;
			case 3: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Video Aspect   Full Screen"); break;
			case 4: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Video Aspect   Rotate Unscaled"); break;
			case 5: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Video Aspect   Rotate Scale Aspect - Downscale  "); break;
			case 6: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Video Aspect   Rotate Scale Aspect - Upscale"); break;
			case 7: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Video Aspect   Rotate Full Screen"); break;
		}
		
		/* (2) Video Sync */
		y_Pos += Y_INCREMENT;
		switch (odx_video_sync)
		{
			case 1: odx_gamelist_text_out(x_Pos,y_Pos, "Video Sync     VSync"); break;
			case 0: odx_gamelist_text_out(x_Pos,y_Pos, "Video Sync     Normal"); break;
			case 2: odx_gamelist_text_out(x_Pos,y_Pos, "Video Sync     DblBuf"); break;
			case -1: odx_gamelist_text_out(x_Pos,y_Pos,"Video Sync     OFF"); break;
		}
		
		/* (3) Frame-Skip */
		y_Pos += Y_INCREMENT;
		if ((odx_video_sync==-1) && (odx_frameskip==-1)) odx_frameskip=0;
		if(odx_frameskip==-1) {
			odx_gamelist_text_out_fmt(x_Pos,y_Pos, "Frame-Skip     Auto");
		}
		else{
			odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Frame-Skip     %d",odx_frameskip);
		}

		/* (4) Sound */
		y_Pos += Y_INCREMENT;
		switch(odx_sound)
		{
			case 0: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Sound          %s","OFF"); break;
			case 1: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Sound          %s","ON (11 KHz fast)"); break;
			case 2: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Sound          %s","ON (22 KHz fast)"); break;
			case 3: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Sound          %s","ON (33 KHz fast)"); break;
			case 4: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Sound          %s","ON (44 KHz fast)"); break;
			case 5: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Sound          %s","ON (11 KHz fast)"); break;
			case 6: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Sound          %s","ON (15 KHz)"); break;
			case 7: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Sound          %s","ON (22 KHz)"); break;
			case 8: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Sound          %s","ON (33 KHz)"); break;
			case 9: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Sound          %s","ON (44 KHz)"); break;
			case 10: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Sound          %s","ON (11 KHz)"); break;
			case 11: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Sound          %s","ON (15 KHz stereo)"); break;
			case 12: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Sound          %s","ON (22 KHz stereo)"); break;
			case 13: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Sound          %s","ON (33 KHz stereo)"); break;
			case 14: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Sound          %s","ON (44 KHz stereo)"); break;
			case 15: odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Sound          %s","ON (11 KHz stereo)"); break;
		}

		/* (5) CPU Clock */
		y_Pos += Y_INCREMENT;
		odx_gamelist_text_out_fmt(x_Pos,y_Pos,"CPU Clock      %d%%",odx_clock_cpu);

		/* (6) Audio Clock */
		y_Pos += Y_INCREMENT;
		odx_gamelist_text_out_fmt(x_Pos,y_Pos,"Audio Clock    %d%%",odx_clock_sound);

		if(strcmp(playemu, "neomame"))
			{
			/* (7) CPU cores */
			y_Pos += Y_INCREMENT;
			switch (odx_cpu_cores)
				{
				case 0: odx_gamelist_text_out(x_Pos,y_Pos, "CPU FAST cores None"); break;
				case 1: odx_gamelist_text_out(x_Pos,y_Pos, "CPU FAST cores Fame"); break;
				default:odx_gamelist_text_out(x_Pos,y_Pos, "CPU FAST cores None"); odx_cpu_cores = 0; break;
				}
			}
		else
			odx_cpu_cores = 0;


		/* (8) Cheats */
		y_Pos += Y_INCREMENT;
		if (odx_cheat)
			odx_gamelist_text_out(x_Pos,y_Pos,"Cheats         ON");
		else
			odx_gamelist_text_out(x_Pos,y_Pos,"Cheats         OFF");

		//y_Pos += 30;
		//odx_gamelist_text_out(x_Pos,y_Pos,"Press B to confirm, X to return\0");

		/* Show currently selected item */
		odx_gamelist_text_out(x_Pos-16,y_PosTop+(selected_option*Y_INCREMENT)+Y_INCREMENT," >");

		odx_video_flip();
		while (odx_joystick_read(JOY_NUM)) { odx_timer_delay(100); }
		while(!(ExKey=odx_joystick_read(JOY_NUM))) { }
		if(ExKey & OD_DOWN){
			selected_option++;
			selected_option = selected_option % options_count;
		}
		else if(ExKey & OD_UP){
			selected_option--;
			if(selected_option<0)
				selected_option = options_count - 1;
		}
		else if(ExKey & OD_LEFT || ExKey & OD_RIGHT)
		{
			switch(selected_option) {
			case 0:
				switch (odx_video_depth)
				{
					case -1: odx_video_depth=8; break;
					case 8: odx_video_depth=16; break;
					case 16: odx_video_depth=-1; break;
				}
				break;
			case 1:
				if(ExKey & OD_RIGHT)
				{
					odx_video_aspect++;
					if (odx_video_aspect>7)
						odx_video_aspect=0;
				}
				else
				{
					odx_video_aspect--;
					if (odx_video_aspect<0)
						odx_video_aspect=7;
				}
				break;
			case 2:
				odx_video_sync=odx_video_sync+1;
				if (odx_video_sync>2)
					odx_video_sync=-1;
				break;
			case 3:
				if(ExKey & OD_RIGHT)
				{
					odx_frameskip ++;
					if (odx_frameskip>11)
						odx_frameskip=-1;
				}
				else
				{
					odx_frameskip--;
					if (odx_frameskip<-1)
						odx_frameskip=11;
				}
				break;
			case 4:
				if(ExKey & OD_RIGHT)
				{
					odx_sound ++;
					if (odx_sound>15)
						odx_sound=0;
				}
				else
				{
					odx_sound--;
					if (odx_sound<0)
						odx_sound=15;
				}
				break;
			case 5:
				/* "CPU Clock" */
				if(ExKey & OD_RIGHT)
				{
					odx_clock_cpu += 10; /* Add 10% */
					if (odx_clock_cpu > 200) /* 200% is the max */
						odx_clock_cpu = 200;
				}
				else
				{
					odx_clock_cpu -= 10; /* Subtract 10% */
					if (odx_clock_cpu < 10) /* 10% is the min */
						odx_clock_cpu = 10;
				}
				break;
			case 6:
				/* "Audio Clock" */
				if(ExKey & OD_RIGHT)
				{
					odx_clock_sound += 10; /* Add 10% */
					if (odx_clock_sound > 200) /* 200% is the max */
						odx_clock_sound = 200;
				}
				else{
					odx_clock_sound -= 10; /* Subtract 10% */
					if (odx_clock_sound < 10) /* 10% is the min */
						odx_clock_sound = 10;
				}
				break;
			case 7: /* change for fast cpu core */
				odx_cpu_cores=(odx_cpu_cores+1)%2;
				break;
			case 8: /* Are we using cheats */
				odx_cheat=!odx_cheat;
				break;
			}
		}

		if ((ExKey & OD_A) || (ExKey & OD_START) || (ExKey & OD_SELECT)) 
		{
			/* Write game configuration */
			sprintf(text,"%s/frontend/%s.cfg",mamedir,game);
			f=fopen(text,"w");
			if (f) {
				fprintf(f,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",odx_freq,odx_video_depth,odx_video_aspect,odx_video_sync,
				odx_frameskip,odx_sound,odx_clock_cpu,odx_clock_sound,odx_cpu_cores,odx_ramtweaks,i,odx_cheat,odx_gsensor);
				fclose(f);
				/* sync(); */
			}

			/* Selected game will be run */
			return 1;
		}
		else if ((ExKey & OD_X) || (ExKey & OD_Y) || (ExKey & OD_B))
		{
			/* Return To Menu */
			return 0;
		}
	}
}

static void odx_exit(char *param)
{
	FILE* filesave;
	char text[512];
	
	snprintf(text,sizeof(text),"%s/frontend/mame.lst",mamedir);
	remove(text);
	
	snprintf(text, sizeof(text), "%s/cfg/romsave.txt", mamedir); 

	if (game_num_avail > 0)
	{
		printf("\nSaving rom directory %s to %s\n", romdir, text);

		filesave = fopen(text, "w+");
		if (filesave)
		{
			fprintf(filesave, romdir);
			fclose(filesave);
		}
	}
	
	
	//sync();
	//odx_deinit();
}

void odx_load_config(void) {
	char curCfg[512];
	FILE *f;
	sprintf(curCfg,"%s/frontend/mame.cfg",mamedir);

	f=fopen(curCfg,"r");
	if (f) {
		fscanf(f,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%[^\n]s",&odx_freq,&odx_video_depth,&odx_video_aspect,&odx_video_sync,
		&odx_frameskip,&odx_sound,&odx_clock_cpu,&odx_clock_sound,&odx_cpu_cores,&odx_ramtweaks,&last_game_selected,&odx_cheat,NULL);
		fclose(f);
	}
	
	//also load rom dir
	//added
	FILE* filesave;
	char text[512];
	snprintf(text, sizeof(text), "%s/cfg/romsave.txt", mamedir); 

	filesave = fopen(text, "r");
	if (filesave)
	{
		fscanf(filesave, "%s",romdir);
		fclose(filesave);
	}
	
	//end of added
	
	#ifdef logging
	//snprintf(text, sizeof(text), "%s, text);
	char errormessage[512];
	snprintf(errormessage, sizeof(errormessage), "save location file is %s \n", text);	
	logerror(errormessage);
	
	snprintf(text, sizeof(text), "%s/cfg/romsave.txt", mamedir); 
	
	filesave = fopen(text, "r");
	if (filesave)
	{
		fscanf(filesave, "%s",text);
		fclose(filesave);
	}
	
	snprintf(errormessage, sizeof(errormessage), "save location in file is %s \n", text);	
	logerror(errormessage);
	
	
	snprintf(errormessage, sizeof(errormessage), "actual save location is %s \n", romdir);	
	logerror(errormessage);
	
	#endif
	
	
}

void odx_save_config(void) {
	char curCfg[512];
	FILE *f;

	/* Write default configuration */
	snprintf(curCfg,sizeof(curCfg),"%s/frontend/mame.cfg",mamedir);
	f=fopen(curCfg,"w");
	if (f) {
		fprintf(f,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s",odx_freq,odx_video_depth,odx_video_aspect,odx_video_sync,
		odx_frameskip,odx_sound,odx_clock_cpu,odx_clock_sound,odx_cpu_cores,odx_ramtweaks,last_game_selected,odx_cheat,romdir);
		fclose(f);
		//fsync(f);
	}
	
	//added
	FILE* filesave;
	char text[512];
	snprintf(text, sizeof(text), "%s/cfg/romsave.txt", mamedir); 

	if (game_num_avail > 0)
	{
		printf("\nSaving rom directory %s to %s\n", romdir, text);

		filesave = fopen(text, "w+");
		if (filesave)
		{
			fprintf(filesave, romdir);
			fclose(filesave);
		}
	}
	//end of added
}

static void select_game(char *emu, char *game)
{
	int i;
	unsigned long ExKey;

	/* No Selected game */
	strcpy(game,"builtinn");

	/* Clean screen */
	odx_video_flip();

	/* Wait until user selects a game */
	while(!want_exit)
	{
		game_list_view(&last_game_selected);
		odx_video_flip();

		if( (odx_joystick_read(JOY_NUM))) odx_timer_delay(100);

		while(!(ExKey=odx_joystick_read(JOY_NUM))) { 
		}

		if ((ExKey & OD_L) && (ExKey & OD_R) )
		{
			odx_save_config();
			odx_exit("");
			want_exit = true;
		}
		else
		{
			if (ExKey & OD_L) last_game_selected-=PAGE_SKIP;
			if (ExKey & OD_R) last_game_selected+=PAGE_SKIP;
			
		}
		if (ExKey & OD_UP) last_game_selected--;
		if (ExKey & OD_DOWN) last_game_selected++;
		if (ExKey & OD_LEFT) last_game_selected-=MAX_AMOUNT_OF_GAMES_ONSCREEN; // ALEK 21
		if (ExKey & OD_RIGHT) last_game_selected+=MAX_AMOUNT_OF_GAMES_ONSCREEN; // ALEK 21
		
		if ((ExKey & OD_A) || (ExKey & OD_START) )
		{
			/* Select the game */
			game_list_select(last_game_selected, game, emu);

			/* Emulation Options */
			if(show_options(game))
			{
				break;
			}
		}
		
		
		/* Get rom directory and reset list after that */
		if ((ExKey & OD_B) )
		{
			get_romdir(romdir);
			game_num_avail = 0;
			for (i=0;i<NUMGAMES;i++)
				frontend_drivers[i].available = 0;
			game_list_init(1);
			last_game_selected = 0;
			odx_video_flip();
		}
		
		
	}
}

char *mame_args[255];
int margc = 0;

void execute_game (char *playemu, char *playgame)
{
	char str[8][64];

	int i=0;

	for(int reset = 0; reset < 255; reset++)
		mame_args[reset] = NULL;

	margc = 0;
	
	// executable
	mame_args[margc]=playemu; margc++;

	// playgame
	mame_args[margc]=playgame; margc++;

	// odx_freq
	//args[margc]="-clock"; n++;
	//sprintf(str[i],"%d",odx_freq);
	//args[margc]=str[i]; i++; n++;

	// odx_video_depth
	//if (odx_video_depth==8)
	//{
	//	mame_args[margc]="-depth"; margc++;
	//	mame_args[margc]="8"; margc++;
	//}
	//if (odx_video_depth==16)
	//{
		mame_args[margc]="-depth"; margc++;
		mame_args[margc]="16"; margc++;
	//}

	// odx_video_aspect
	switch(odx_video_aspect)
		{
		case 0:
			{
			mame_args[margc]="-unscaled"; margc++;
			}		
			break;
			
		case 1:
			{
			// Scale aspect.
			mame_args[margc]="-bestscale"; margc++; //was -aspect
			}
			break;

		case 2:
			{
			// Scale aspect fast.
			mame_args[margc]="-fastaspect"; margc++;
			}
			break;

		case 3:
			{ //fullscreen
			mame_args[margc]="-fullscreen"; margc++; //-aspect
			}
			break;
		
		case 4:
			{//rotate normal
			mame_args[margc]="-unscaled"; margc++;
			}
			break;
		case 5:
			{ // rotate Scale aspect.
			mame_args[margc]="-bestscale"; margc++;
			}		
			break;
		case 6:
			{// rotate Scale aspect fast.
			
			mame_args[margc]="-fastaspect"; margc++; //was -aspect
			}
			break;
		case 7:
			{ //rotate fullscreen
			
			mame_args[margc]="-fullscreen"; margc++;
			}
			break;
		
		}

	mame_args[margc]="-nodirty"; margc++;

	if ((odx_video_aspect>=4) && (odx_video_aspect<=7))
	{
		mame_args[margc]="-rotatecontrols"; margc++;
		mame_args[margc]="-ror"; margc++;
	}

	
	// odx_video_sync
	if (odx_video_sync==1)
	{
		mame_args[margc]="-nodirty"; margc++;
		mame_args[margc]="-waitvsync"; margc++;
	}
	else if ((odx_video_sync==2) || (odx_video_aspect==1) || (odx_video_aspect==9))
	{
		mame_args[margc]="-nodirty"; margc++;
	}
	if (odx_video_sync==-1)
	{
		mame_args[margc]="-nothrottle"; margc++;
	}
	
	// odx_frameskip
	if (odx_frameskip>=0)
	{
		mame_args[margc]="-frameskip"; margc++;
		sprintf(str[i],"%d",odx_frameskip);
		mame_args[margc]=str[i]; i++; margc++;
	}

	// odx_sound
	if (odx_sound==0)
	{
		mame_args[margc]="-soundcard"; margc++;
		mame_args[margc]="0"; margc++;
	}
	if ((odx_sound==1) || (odx_sound==6) || (odx_sound==11))
	{
		mame_args[margc]="-samplerate"; margc++;
		mame_args[margc]="11025"; margc++;
	}
	if ((odx_sound==2) || (odx_sound==7) || (odx_sound==12))
	{
		mame_args[margc]="-samplerate"; margc++;
		mame_args[margc]="22050"; margc++;
	}
	if ((odx_sound==3) || (odx_sound==8) || (odx_sound==13))
	{
		mame_args[margc]="-samplerate"; margc++;
		mame_args[margc]="32000"; margc++;
	}
	if ((odx_sound==4) || (odx_sound==9) || (odx_sound==14))
	{
		mame_args[margc]="-samplerate"; margc++;
		mame_args[margc]="44100"; margc++;
	}
	if ((odx_sound==5) || (odx_sound==10) || (odx_sound==15))
	{
		mame_args[margc]="-samplerate"; margc++;
		mame_args[margc]="11025"; margc++;
	}
	if ((odx_sound>=1) && (odx_sound<=5))
	{
		mame_args[margc]="-fastsound"; margc++;
	}
	if (odx_sound>=11)
	{
		mame_args[margc]="-stereo"; margc++;
	}

	// odx_clock_cpu
	if (odx_clock_cpu!=100)
	{
		mame_args[margc]="-uclock"; margc++;
		sprintf(str[i],"%d",100-odx_clock_cpu);
		mame_args[margc]=str[i]; i++; margc++;
	}

	// odx_clock_sound
	if (odx_clock_cpu!=100)
	{
		mame_args[margc]="-uclocks"; margc++;
		sprintf(str[i],"%d",100-odx_clock_sound);
		mame_args[margc]=str[i]; i++; margc++;
	}
	
	// odx_cpu_cores
	if ((odx_cpu_cores==1) )
	{
		mame_args[margc]="-fame"; margc++;
	}
#if 0	
	if ((odx_cpu_cores==1) || (odx_cpu_cores==3) || (odx_cpu_cores==5))
	{
		mame_args[margc]="-cyclone"; margc++;
	}
	if ((odx_cpu_cores==2) || (odx_cpu_cores==3))
	{
		mame_args[margc]="-drz80"; margc++;
	}
	if ((odx_cpu_cores==4) || (odx_cpu_cores==5))
	{
		mame_args[margc]="-drz80_snd"; margc++;
	}
#endif

	if (odx_ramtweaks)
	{
		mame_args[margc]="-ramtweaks"; margc++;
	}
	
	if (odx_cheat)
	{
		mame_args[margc]="-cheat"; margc++;
	}

	if (odx_video_aspect==24)
	{
		mame_args[margc]="-odx_rotated_video"; margc++;
    	mame_args[margc]="-rol"; margc++;
	}
    if (odx_video_aspect==25)
    {
		mame_args[margc]="-odx_rotated_video"; margc++;
		mame_args[margc]="-rotatecontrols"; margc++;
    }
	
	// Add mame and rom directory
    mame_args[margc]="-mamepath"; margc++;
	mame_args[margc]=mamedir;margc++;
    mame_args[margc]="-rompath"; margc++;;
	mame_args[margc]=romdir;margc++;

	mame_args[margc]=NULL;

#if 0
	for (i=0; i<n; i++)
	{
		fprintf(stderr,"%s ",mame_args[i]);
	}
	fprintf(stderr,"\n");
	fflush(stderr);
#endif
//	odx_deinit();
//	execv(mame_args[0], args);
}
 

#define MAX_FILES 512
typedef struct  {
	char name[255];
	unsigned int type;
} filedirtype;
filedirtype filedir_list[MAX_FILES];

int sort_function(const void *src_str_ptr, const void *dest_str_ptr) {
  filedirtype *p1 = (filedirtype *) src_str_ptr;
  filedirtype *p2 = (filedirtype *) dest_str_ptr;
  
  return strcmp (p1->name, p2->name);
}

signed int get_romdir(char *result) 
{
	unsigned long ExKey;
	
	char current_dir_name[512];
	DIR *current_dir;
	struct dirent *current_file;
	struct stat file_info;
	char current_dir_short[81];
	unsigned int current_dir_length;
	unsigned int num_filedir;

	char *file_name;
	signed int return_value = 1;
	unsigned int repeat;
	unsigned int i;

	unsigned int current_filedir_scroll_value;
	unsigned int current_filedir_selection;
	unsigned int current_filedir_in_scroll;
	unsigned int current_filedir_number;
	
	// Init dir with saved one
	//strcpy(current_dir_name,mamedir);

	while (return_value == 1) {
		current_filedir_in_scroll = 0;
		current_filedir_scroll_value  = 0;
		current_filedir_number = 0;
		current_filedir_selection = 0;
		num_filedir = 0;
		
		getcwd(current_dir_name, 512);
		current_dir = opendir(current_dir_name);
		
		do {
			if(current_dir) current_file = readdir(current_dir); else current_file = NULL;

			if(current_file) {
				file_name = current_file->d_name;

				if((stat(file_name, &file_info) >= 0) && ((file_name[0] != '.') || (file_name[1] == '.'))) {
					if(S_ISDIR(file_info.st_mode)) {
						filedir_list[num_filedir].type = 1; // 1 -> directory
						strcpy(filedir_list[num_filedir].name, file_name);
						num_filedir++;
						
					}
				}
			}
		} while ((current_file) && (num_filedir<MAX_FILES));

		if (num_filedir)
			qsort((void *)filedir_list, num_filedir, sizeof(filedirtype), sort_function);

		closedir(current_dir);

		current_dir_length = strlen(current_dir_name);
		if(current_dir_length > 39) {
			memcpy(current_dir_short, "...", 3);
			memcpy(current_dir_short + 3, current_dir_name + current_dir_length - (39-3), (39-3));
			current_dir_short[39] = 0;
		} else {
			memcpy(current_dir_short, current_dir_name, current_dir_length + 1);
		}
 
		repeat = 1;

		char print_buffer[81];

		while(repeat && !want_exit) {
			blit_bmp_8bpp(od_screen8,menu_bmp);
			
			odx_gamelist_text_out( SCREEN_WIDTH - 138, 40,"Select a ROM directory");
			odx_gamelist_text_out( 4, Y_BOTTOM_LINE - Y_INCREMENT,current_dir_short );
			odx_gamelist_text_out( 4, Y_BOTTOM_LINE,"A=Enter dir START=Select dir");
			odx_gamelist_text_out( SCREEN_WIDTH - 40, Y_BOTTOM_LINE,"B=Quit");
			odx_gamelist_text_out( X_BUILD,2,VERSION_NUMBER);
			
			for(i = 0, current_filedir_number = i + current_filedir_scroll_value; i < FILE_LIST_ROWS; i++, current_filedir_number++) {
#define CHARLEN ((320/6)-2)
				if(current_filedir_number < num_filedir) {
					strncpy(print_buffer+1,filedir_list[current_filedir_number].name, CHARLEN-1);
					print_buffer[0] = ' ';
					if((current_filedir_number == current_filedir_selection))
						print_buffer[0] = '>';
					print_buffer[CHARLEN] = 0;
					odx_gamelist_text_out(4, 40 + Y_INCREMENT +((i + 2) * Y_INCREMENT), print_buffer );
				}
			}
			odx_video_flip();

			// Catch input
			while ( (odx_joystick_read(JOY_NUM))) odx_timer_delay(100);
			while(!(ExKey=odx_joystick_read(JOY_NUM))) { 
			}

			/* L + R = Exit */
			if ((ExKey & OD_L) && (ExKey & OD_R) ) { want_exit = true; odx_exit(""); }
		
			// START - choose directory
			if (ExKey & OD_START) { 
				repeat = 0;
				return_value = 0;
				strcpy(result,current_dir_name);
			}
			
			// A - choose file or enter directory
			if (ExKey & OD_A) { 
				if (filedir_list[current_filedir_selection].type == 1)  { // so it's a directory
					repeat = 0;
					chdir(filedir_list[current_filedir_selection].name);
				}
			}

			// B - exit or back to previous menu
			if (ExKey & OD_B) { 
				return_value = -1;
				repeat = 0;
			}

			// UP - arrow up
			if (ExKey & OD_UP) { 
				if(current_filedir_selection) {
					current_filedir_selection--;
					if(current_filedir_in_scroll == 0) {
						current_filedir_scroll_value--;
					} else {
						current_filedir_in_scroll--;
					}
				}
				else {
					current_filedir_selection = (num_filedir - 1);
					current_filedir_in_scroll = num_filedir> FILE_LIST_ROWS ? (FILE_LIST_ROWS - 1) : num_filedir-1;
					current_filedir_scroll_value = num_filedir> FILE_LIST_ROWS ? (num_filedir - 1)-FILE_LIST_ROWS+1 : 0;
				}
			}

			//DOWN - arrow down
			if (ExKey & OD_DOWN) { 
				if(current_filedir_selection < (num_filedir - 1)) {
					current_filedir_selection++;
					if(current_filedir_in_scroll == (FILE_LIST_ROWS - 1)) {
						current_filedir_scroll_value++;
					} else {
						current_filedir_in_scroll++;
					}
				}
				else {
					current_filedir_selection = 0;
					current_filedir_in_scroll =0;
					current_filedir_scroll_value = 0;
				}
			}
		}
	}
	
	return return_value;
}

void gethomedir() {
	char text[512];
	
	FILE *fileread, *ftest;

	snprintf(mamedir, sizeof(mamedir), "%s/.mame4all", getenv("HOME"));
	mkdir(mamedir,0755); // create $HOME/.program if it doesn't exist
	snprintf(text, sizeof(text), "%s/frontend/",mamedir); 
	mkdir(text,0755); 
	snprintf(text, sizeof(text), "%s/nvram/",mamedir); 
	mkdir(text,0755); 
	snprintf(text, sizeof(text), "%s/hi/",mamedir); 
	mkdir(text,0755); 
	snprintf(text, sizeof(text), "%s/cfg/",mamedir); 
	mkdir(text,0755); 
	snprintf(text, sizeof(text), "%s/memcard/",mamedir);
	mkdir(text,0755); 
	snprintf(text, sizeof(text), "%s/snap/",mamedir); 
	mkdir(text,0755); 

	snprintf(text, sizeof(text), "%s/cfg/romsave.txt",mamedir); 
	
	#ifdef logging
	//logs
	logerror(text);
	#endif

	
	fileread = fopen(text, "r");
	if (fileread)
	{
		fscanf(fileread, "%s",romdir);
		fclose(fileread);
	}
		
		/**
	snprintf(text, sizeof(text), "%s/test.tmp",romdir); 
	ftest = fopen(text, "w+");
	if (ftest == NULL)
	{
		printf("Folder does not exist, revert to default");
		snprintf(romdir, sizeof(romdir), "./roms");
	}
	else
	{
		remove(text);
		fclose(ftest);
	
	}
	*/
}

int do_frontend ()
{
	char curDir[512];

	odx_video_color8(0,0,0,0);
	odx_video_color8(255,255,255,255);
	odx_video_setpalette();

	odx_clear_video();

	while(odx_joystick_read(JOY_NUM)) { odx_timer_delay(100); }

	gethomedir();
	
	if(first_run)
	{
		/* get initial home directory */
		
		/* Open dingux Initialization */
		//odx_init(1000,16,44100,16,0,60);

		/* Show intro screen */
		odx_intro_screen();

		
		#ifdef logging
		char errormessage[14];
		snprintf(errormessage,"at firstrun \n",errormessage);
		logerror(errormessage);	
		#endif
		
		
		/* Read default configuration */
		odx_load_config();

		/* Initialize list of available games */
		game_list_init(0);
		if (game_num_avail==0)
		{
			/* save current dir */
			getcwd(curDir, 256);

			/* Check for rom dir */
			while (game_num_avail == 0)
			{
				odx_gamelist_text_out(10, 20, "Error: No available games found !");
				odx_gamelist_text_out(10, 40, "Press a key to select a rom directory");
				odx_video_flip();
				odx_joystick_press(JOY_NUM);
				// !!!
				if (get_romdir(romdir) == -1)
				{
					want_exit = true;
					break;
				}
				else
				{
					game_list_init(0);
				}
			}

			/* go back to default dir to avoid issue when launching mame after */
			chdir(curDir);
		}

		first_run = false;
	}

	if(!want_exit)
		{
		/* Select Game */
		select_game(playemu,playgame);

		/* Write default configuration */
		odx_save_config();

		/* Execute Game */
		execute_game (playemu,playgame);
	}

	odx_printf_init();

	return want_exit ? 0 : 1;
}
