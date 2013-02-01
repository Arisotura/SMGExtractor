/*
    Copyright 2013 Mega-Mario

    This file is part of SMGExtractor.

    SMGExtractor is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    SMGExtractor is distributed in the hope that it will be useful, but WITHOUT ANY 
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS 
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along 
    with SMGExtractor. If not, see http://www.gnu.org/licenses/.
*/

#include <gccore.h>
#include <wiiuse/wpad.h>

#include <fat.h>
#include <sdcard/wiisd_io.h>
#include <di/di.h>
#include "fst.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>


static void *xfb = NULL;
static GXRModeObj *rmode = NULL;


void printerror()
{
	uint32_t err = 0;
	int crapo = DI_GetError(&err);
	if (crapo < 0) printf("ERROR WHILE GETTING ERROR -- %d\n", crapo);
	else if (err != 0) printf("DI_GetError: %d\n", err);
	else printf("DI_GetError: everything is okay\n");
}

bool initdisc() 
{
	if (!fatMountSimple("sd", &__io_wiisd)) return false;
	printf("SD mounted\n");
	
    DI_Mount();
	printf("DImounted\n");
	printerror();
	int oldstatus = -1;
	int status;
    while ((status = DI_GetStatus()) & DVD_INIT) 
	{
		if (oldstatus != status)
		{
			printf("DI STATUS CHANGE %02X -> %02X\n", oldstatus, status);
			oldstatus = status;
		}
		usleep(5000);
	}
	printf("good\n");
    if (DI_GetStatus() & DVD_READY) return FST_Mount();
	printf("blarg\n");
    return false;
}

void deinitdisc()
{
	FST_Unmount();
    DI_Close();
	
	fatUnmount("sd");
}


void dumpdir(char* path)
{
}

void startdump(int what)
{
	int choice = 0;
	
	// AudioRes, EuDutch, EuEnglish, EuFrench, EuGerman, EuItalian, EuSpanish, HomeButton2, LayoutData, ModuleData, MovieData, ObjectData, ParticleData, StageData,
}

int main(int argc, char **argv) 
{
	DI_LoadDVDX(false);
	int lolz = DI_Init();
	VIDEO_Init();
	WPAD_Init();

	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	printf("\x1b[2;0H");
	printf("DI_Init() -> %d\n", lolz);
	printerror();
	
	if (!fatInitDefault()) 
	{
		printf("fatInitDefault failure: terminating\n");
		return 0;
	}
	
	printf("SMGExtractor v1.0 -- by Mega-Mario\n");
	printf("* Press 1 to dump the bare minimum (ObjectData, StageData and the message data)\n");
	printf("* Press 2 to dump everything\n");
	
	printf("(this is a test and doesn't work as advertised-- just press 1 and see what happens)\n");

	while(1) 
	{
		WPAD_ScanPads();

		u32 pressed = WPAD_ButtonsDown(0);
		if (pressed & WPAD_BUTTON_HOME) exit(0);
		if (pressed & WPAD_BUTTON_1)
		{
			printf("going to init the disc...\n");
			if (!initdisc()) { printf("oops, failed\n"); continue; }
			printf("inited!\n");
			
			DIR *pdir;
			struct dirent *pent;
			struct stat statbuf;

			pdir=opendir("fst:/1/StageData");

			if (!pdir){
				printf ("opendir() failure; terminating\n");
				goto error;
			}

			while ((pent=readdir(pdir))!=NULL) {
				stat(pent->d_name,&statbuf);
				if(strcmp(".", pent->d_name) == 0 || strcmp("..", pent->d_name) == 0)
					continue;
				if(S_ISDIR(statbuf.st_mode))
					printf("%s <dir>\n", pent->d_name);
				if(!(S_ISDIR(statbuf.st_mode)))
					printf("%s %lld\n", pent->d_name, statbuf.st_size);
			}
			closedir(pdir);
			
			deinitdisc();
		}
		else if (pressed & WPAD_BUTTON_2)
		{
			printf("not implemented\n");
		}

		VIDEO_WaitVSync();
	}

	return 0;
	
error:
	while(1) 
	{
		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);
		if (pressed & WPAD_BUTTON_HOME) exit(0);
		VIDEO_WaitVSync();
	}
	
	return 0;
}
