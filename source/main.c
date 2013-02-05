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
static int alreadymounted = 0;
static int outmedia = -1;

#define FILEBUF_SIZE 1024*1024
static unsigned char filebuffer[FILEBUF_SIZE];


bool initdisc() 
{
	if (alreadymounted) return true;
	
    DI_Mount();
	printf("Insert your disc if you haven't already.\n");
    while (DI_GetStatus() & (DVD_INIT | DVD_NO_DISC)) 
		usleep(5000);
    if (DI_GetStatus() & DVD_READY) return FST_Mount();
    return false;
}

void deinitdisc()
{
	FST_Unmount();
}


void dumpfile(char* srcpath, char* dstpath)
{
	//printf("\x1b[12;0H");
	//printf("%s                                  \n", srcpath);
	printf("%s\n", srcpath);
	
	FILE* fin = fopen(srcpath, "rb");
	fseek(fin, 0, SEEK_END);
	int len = ftell(fin);
	fseek(fin, 0, SEEK_SET);
	
	FILE* fout = fopen(dstpath, "wb");
	
	int curpos = 0;
	while (curpos < len)
	{
		int thislen = FILEBUF_SIZE;
		if (curpos + thislen > len)
			thislen = len - curpos;
		
		fread(filebuffer, thislen, 1, fin);
		fwrite(filebuffer, thislen, 1, fout);
		curpos += thislen;
	}
	
	fclose(fout);
	fclose(fin);
}

void dumpsubdir(char* srcpath, char* dstpath)
{
	DIR *pdir;
	struct dirent *pent;
	char str1[256], str2[256];

	pdir = opendir(srcpath);
	if (!pdir) return;
	
	//printf("\x1b[11;0H");
	//printf("%s                                  \n", srcpath);
	
	mkdir(dstpath, 0777);

	while ((pent = readdir(pdir)) != NULL) 
	{
		if(strcmp(".", pent->d_name) == 0 || strcmp("..", pent->d_name) == 0)
			continue;
		
		sprintf(str1, "%s/%s", srcpath, pent->d_name);
		sprintf(str2, "%s/%s", dstpath, pent->d_name);
		
		if(pent->d_type & DT_DIR)
			dumpsubdir(str1, str2);
		else
			dumpfile(str1, str2);
	}

	closedir(pdir);
}

void dumpdir(char* srcpath, char* dstpath)
{
	DIR *pdir;
	struct dirent *pent;
	char str1[256], str2[256];

	pdir = opendir(srcpath);
	if (!pdir) return;
	
	//printf("\x1b[10;0H");
	//printf("%s                                  \n", srcpath);
	
	sprintf(str2, "%s:/%s", outmedia==0 ? "sd" : "usb", dstpath);
	mkdir(str2, 0777);

	while ((pent = readdir(pdir)) != NULL) 
	{
		if(strcmp(".", pent->d_name) == 0 || strcmp("..", pent->d_name) == 0)
			continue;
		
		sprintf(str1, "%s/%s", srcpath, pent->d_name);
		sprintf(str2, "%s:/%s/%s", outmedia==0 ? "sd" : "usb", dstpath, pent->d_name);
		
		if(pent->d_type & DT_DIR)
			dumpsubdir(str1, str2);
		else
			dumpfile(str1, str2);
	}

	closedir(pdir);
}

int main(int argc, char **argv) 
{
	int diinit = DI_Init();
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
	if (diinit < 0)
	{
		printf("DI_Init() failed: %d\n", diinit);
		goto error;
	}
	
	if (!fatInitDefault()) 
	{
		printf("fatInitDefault() failed: terminating\n");
		goto error;
	}
	
	if (__io_wiisd.isInserted()) outmedia = 0;
	else if (__io_usbstorage.isInserted()) outmedia = 1;
	
	if (outmedia < 0)
	{
		printf("Failed to find appropriate output media\n");
		goto error;
	}
	
	printf("SMGExtractor v1.0 -- by Mega-Mario\n");
	printf("* Press HOME to return to the system menu\n");
	printf("* Press A to change the output media\n");
	printf("* Press 1 to dump the bare minimum (ObjectData, StageData and the message data)\n");
	printf("* Press 2 to dump all the disc's contents\n");
	
	printf("\x1b[8;0H");
	printf("Output media: %s\n", outmedia==0 ? "SD card    " : "USB storage");

	while(1) 
	{
		WPAD_ScanPads();

		u32 pressed = WPAD_ButtonsDown(0);
		if (pressed & WPAD_BUTTON_HOME)
		{
			deinitdisc();
			DI_Close();
			exit(0);
			return 0;
		}
		
		if (pressed & WPAD_BUTTON_1)
		{
			if (!initdisc())
			{
				printf("Failed to mount the disc\n");
				goto error;
			}
			printf("Disc successfully mounted, now dumping\n");
			
			char str[256];
			sprintf(str, "%s:/SMGFiles", outmedia==0 ? "sd" : "usb");
			mkdir(str, 0777);
			
			dumpdir("fst:/1/ObjectData", "SMGFiles/ObjectData");
			dumpdir("fst:/1/StageData", "SMGFiles/StageData");
			dumpdir("fst:/1/LocalizeData", "SMGFiles/LocalizeData");
			dumpdir("fst:/1/EuDutch", "SMGFiles/EuDutch");
			dumpdir("fst:/1/EuEnglish", "SMGFiles/EuEnglish");
			dumpdir("fst:/1/EuFrench", "SMGFiles/EuFrench");
			dumpdir("fst:/1/EuGerman", "SMGFiles/EuGerman");
			dumpdir("fst:/1/EuItalian", "SMGFiles/EuItalian");
			dumpdir("fst:/1/EuSpanish", "SMGFiles/EuSpanish");
		}
		else if (pressed & WPAD_BUTTON_2)
		{
			if (!initdisc())
			{
				printf("Failed to mount the disc\n");
				goto error;
			}
			printf("Disc successfully mounted, now dumping\n");
			
			dumpdir("fst:/1", "SMGFiles");
			dumpdir("fst:/1_metadata", "SMGFiles");
		}
		else if (pressed & WPAD_BUTTON_A)
		{
			outmedia = outmedia==0 ? 1 : 0;
			printf("\x1b[8;0H");
			printf("Output media: %s\n", outmedia==0 ? "SD card    " : "USB storage");
		}

		VIDEO_WaitVSync();
	}

	return 0;
	
error:
	printf("Press HOME to return to the system menu\n");
	while(1) 
	{
		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);
		if (pressed & WPAD_BUTTON_HOME) 
		{
			deinitdisc();
			DI_Close();
			exit(0);
			return 0;
		}
		VIDEO_WaitVSync();
	}
	
	return 0;
}
