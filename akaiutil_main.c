/*
* Copyright (C) 2008-2022 Klaus Michael Indlekofer. All rights reserved.
*
* m.indlekofer@gmx.de
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/* akaiutil: access to AKAI S900/S1000/S3000 filesystems */



#include "commoninclude.h"
#include "akaiutil_io.h"
#include "akaiutil.h"
#include "akaiutil_tar.h"
#include "akaiutil_file.h"
#include "akaiutil_take.h"



/* console user interface */
#ifndef AKAIUTIL_MAIN
#define AKAIUTIL_MAIN(...)			main(__VA_ARGS__)
#endif
#ifndef AKAIUTIL_EXIT
#define AKAIUTIL_EXIT(...)			exit(__VA_ARGS__)
#endif



static void
print_progressbar(u_int end,u_int now)
{
	static double xold; /* must be static */

	if (end==0){
		return;
	}
	if (now>end){
		now=end;
	}

	if (now==0){
		PRINTF_OUT("0         20        40        60        80        100%%\n.");
		xold=0.0;
	}

	for (;((double)now)/(double)end-xold>=0.02;xold+=0.02){
		PRINTF_OUT(".");
	}

	FLUSH_ALL;
}



static void
usage(char *name)
{

	if (name==NULL){
		return;
	}

#ifdef _VISUALCPP
	PRINTF_ERR("usage: %s [-h] [-r] [-F] [-C] [-l <lock-file>] [-o <start-offset>] [-s <pseudo-disk-size>] [-n <pseudo-disk-number>] [-c <cdrom-index> ...] [-p <physdrive-index> ...] [[-f] <floppy-drive> ...] [[-f] <disk-file> ...]\n",name);
	PRINTF_ERR("\t-h\tprint this info\n");
	PRINTF_ERR("\t-r\tread-only mode\n");
	PRINTF_ERR("\t-F\tdisable floppy filesystem for disk-files/CD-ROM drives/physical drives\n");
	PRINTF_ERR("\t-C\tdisable cache\n");
	PRINTF_ERR("\t-l\tlock-file\n");
	PRINTF_ERR("\t-o\tset start offset for disk-file/drive in bytes\n");
	PRINTF_ERR("\t-s\tset pseudo-disk size in KB\n");
	PRINTF_ERR("\t-n\tset max. number of pseudo-disks per disk-file/drive\n");
	PRINTF_ERR("\t-c\tCD-ROM drive\n");
	PRINTF_ERR("\t-p\tphysical drive\n");
	PRINTF_ERR("\t-f\tfloppy drive or disk-file\n");
	PRINTF_ERR("\t\t<floppy-drive> = floppyla: | floppylb: | floppyha: | floppyhb:\n");
#elif defined(__CYGWIN__)
	PRINTF_ERR("usage: %s [-h] [-r] [-F] [-C] [-l <lock-file>] [-o <start-offset>] [-s <pseudo-disk-size>] [-n <pseudo-disk-number>] [-c <cdrom-index> ...] [-p <physdrive-index> ...] [[-f] <disk-file> ...]\n",name);
	PRINTF_ERR("\t-h\tprint this info\n");
	PRINTF_ERR("\t-r\tread-only mode\n");
	PRINTF_ERR("\t-F\tdisable floppy filesystem\n");
	PRINTF_ERR("\t-C\tdisable cache\n");
	PRINTF_ERR("\t-l\tlock-file\n");
	PRINTF_ERR("\t-o\tset start offset for disk-file/drive in bytes\n");
	PRINTF_ERR("\t-s\tset pseudo-disk size in KB\n");
	PRINTF_ERR("\t-n\tset max. number of pseudo-disks per disk-file/drive\n");
	PRINTF_ERR("\t-c\tCD-ROM drive\n");
	PRINTF_ERR("\t-p\tphysical drive\n");
	PRINTF_ERR("\t-f\tdisk-file\n");
#else
	PRINTF_ERR("usage: %s [-h] [-r] [-F] [-C] [-l <lock-file>] [-o <start-offset>] [-s <pseudo-disk-size>] [-n <pseudo-disk-number>] [[-f] <disk-file> ...]\n",name);
	PRINTF_ERR("\t-h\tprint this info\n");
	PRINTF_ERR("\t-r\tread-only mode\n");
	PRINTF_ERR("\t-F\tdisable floppy filesystem\n");
	PRINTF_ERR("\t-C\tdisable cache\n");
	PRINTF_ERR("\t-l\tlock-file\n");
	PRINTF_ERR("\t-o\tset start offset for disk-file/drive in bytes\n");
	PRINTF_ERR("\t-s\tset pseudo-disk size in KB\n");
	PRINTF_ERR("\t-n\tset max. number of pseudo-disks per disk-file/drive\n");
	PRINTF_ERR("\t-f\tdisk-file\n");
#endif
}

int
AKAIUTIL_MAIN(int argc, char **argv)
{
	int op;
	int readonly;
	int floppyenable;
#ifdef _VISUALCPP
	HANDLE lockh;
	OVERLAPPED lockov;
#else /* !_VISUALCPP */
	int lockfd;
	struct flock lockfl;
#endif /* !_VISUALCPP */
	int lockflag;
	OFF64_T startoff;
	u_int pseudodisksize;
	u_int pseudodisknum;
#define CURWAVNAMEMAXLEN	256 /* XXX */
	static char curwavname[CURWAVNAMEMAXLEN];
#define CURWAVCMDBUFSIZ		(CURWAVNAMEMAXLEN+64) /* XXX */
	static char curwavcmdbuf[CURWAVCMDBUFSIZ];
	int restartflag;
	u_int i;
	U_INT64 j;
	int mainret;

	PRINTF_OUT("\n"
			   "****************************************************\n"
			   "*                                                  *\n"
			   "*  AKAI S900/S1000/S3000 File Manager, Rev. 4.6.7  *\n"
			   "*                                                  *\n"
			   "****************************************************\n"
			   "\n"
			   "Copyright (c) 2008-2022 Klaus Michael Indlekofer. All rights reserved.\n"
			   "\n");
#ifdef _VISUALCPP
	PRINTF_OUT("This product uses fdrawcmd.sys developed by Simon Owen.\n"\
			   "\n"
			   "This product includes getopt,strncasecmp developed by\n"
			   "the University of California, Berkeley and its contributors.\n"
			   "Copyright (c) 1987-2002 The Regents of the University of California.\n"
			   "All rights reserved.\n"
			   "\n");
#endif /* _VISUALCPP */
	PRINTF_OUT("\n");

	if (argc<=1){
		usage(argv[0]);
		AKAIUTIL_EXIT(1);
	}

	disk_num=0; /* no disks so far */
	init_blk_cache(); /* init cache */
	blk_cache_enable=1; /* enable cache */
#ifdef _VISUALCPP
	if (fldr_init()<0){
		mainret=1; /* error */
		goto main_exit;
	}
#endif /* _VISUALCPP */

	/* no current external WAV file so far */
	curwavname[0]='\0';
	curwavcmdbuf[0]='\0';

	mainret=0; /* no error so far */

	PRINTF_OUT("opening disks\n");

	/* parse arguments */
	readonly=0; /* R/W so far */
	floppyenable=1; /* start with floppy filesystem for disk-files/CD-ROM drives/physical drives allowed */
#ifdef _VISUALCPP
	lockh=INVALID_HANDLE_VALUE; /* no lock-file so far */
	bzero(&lockov,sizeof(OVERLAPPED));
	lockov.Offset=0; /* XXX offset 0 (lower DWORD) */
	lockov.OffsetHigh=0; /* XXX offset 0 (upper DWORD) */
	lockov.hEvent=0;
#else /* !_VISUALCPP */
	lockfd=-1; /* no lock-file so far */
	bzero(&lockfl,sizeof(struct flock));
	lockfl.l_whence=SEEK_SET;
	lockfl.l_start=0; /* XXX offset 0 */
	lockfl.l_len=1; /* XXX 1 byte */
#endif /* !_VISUALCPP */
	lockflag=0; /* lock not acquired yet */
	startoff=0;
	pseudodisksize=0; /* 0 means: pseudo-disk size is not specified */
	pseudodisknum=0; /* 0 means: max. number of pseudo-disks is not specified */
#if defined(_VISUALCPP)||defined(__CYGWIN__)
#define OPT_STRING "hrFCl:o:s:n:c:p:f:"
#else
#define OPT_STRING "hrFCl:o:s:n:f:"
#endif
	while ((op=getopt(argc,argv,OPT_STRING))!=EOF){
		switch (op){
		case 'h':
			usage(argv[0]);
			goto main_exit;
		case 'r':
			/* Note: -r option shall affect all disk-files/drives */
			/* Note: CD-ROM drive are always read-only (see below), independent of -r option */
			if ((!readonly)&&(disk_num>0)){
				PRINTF_ERR("\n-r option must be prior to any disk-file/drive arguments\n");
				mainret=1; /* error */
				goto main_exit;
			}
			readonly=1; /* read-only mode */
			break;
		case 'F':
			/* Note: -F option shall affect all disk-files/drives */
			if (floppyenable&&(disk_num>0)){
				PRINTF_ERR("\n-F option must be prior to any disk-file/drive arguments\n");
				mainret=1; /* error */
				goto main_exit;
			}
			floppyenable=0; /* disable floppy filesystem for disk-files/CD-ROM drives/physical drives */
			break;
		case 'C':
			/* Note: -C option must be prior to any disk I/O */
			if (blk_cache_enable&&(disk_num>0)){
				PRINTF_ERR("\n-C option must be prior to any disk-file/drive arguments\n");
				mainret=1; /* error */
				goto main_exit;
			}
			blk_cache_enable=0; /* disable cache */
			/* Note: command CMD_ENABLECACHE may enable cache again */
			break;
		case 'l':
			if (lockflag){
				PRINTF_ERR("\n-l option must not be used multiple times\n");
				mainret=1; /* error */
				goto main_exit;
			}
			/* Note: -l option must be prior to any disk I/O */
			if (disk_num>0){
				PRINTF_ERR("\n-l option must be prior to any disk-file/drive arguments\n");
				mainret=1; /* error */
				goto main_exit;
			}
#ifdef _VISUALCPP
			/* open lock-file */
			lockh=CreateFileA(optarg,GENERIC_WRITE,FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,0,NULL);
			if (lockh==INVALID_HANDLE_VALUE){
				PRINTF_ERR("cannot open lock-file\n");
				mainret=1; /* error */
				goto main_exit;
			}
			/* acquire lock */
			if (LockFileEx(lockh,LOCKFILE_EXCLUSIVE_LOCK,0,1,0,&lockov)==FALSE){ /* XXX 1 byte */
				PRINTF_ERR("cannot acquire lock\n");
				mainret=1; /* error */
				goto main_exit;
			}
#else /* !_VISUALCPP */
			/* open lock-file */
			if ((lockfd=OPEN(optarg,O_WRONLY|O_CREAT|O_BINARY,0666))<0){
				PERROR("open lock-file");
				mainret=1; /* error */
				goto main_exit;
			}
			/* acquire lock */
			lockfl.l_type=F_WRLCK; /* exclusive lock */
			if (fcntl(lockfd,F_SETLKW,&lockfl)<0){
				PRINTF_ERR("cannot acquire lock\n");
				mainret=1; /* error */
				goto main_exit;
			}
#endif /* !_VISUALCPP */
			lockflag=1;
			break;
		case 'o':
			if (strncmp(optarg,"0x",2)==0){
				startoff=(OFF64_T)STRHEX_TO_UINT64(optarg+2);
			}else{
				startoff=(OFF64_T)STRDEC_TO_UINT64(optarg);
			}
			break;
		case 's':
			if (strncmp(optarg,"0x",2)==0){
				j=STRHEX_TO_UINT64(optarg+2)*1024;
			}else{
				j=STRDEC_TO_UINT64(optarg)*1024;
			}
			if (j>0xffffffff){ /* XXX 32bit overflow protection */
				PRINTF_ERR("pseudo-disk size is too large\n");
				mainret=1; /* error */
				goto main_exit;
			}
			pseudodisksize=(u_int)j;
			/* Note: pseudodisksize>AKAI_DISKSIZE_MAX is allowed */
			/* Note: pseudodisksize==0 is allowed and means: pseudo-disk size is not specified */
			break;
		case 'n':
			pseudodisknum=(u_int)atoi(optarg);
			if (pseudodisknum>PSEUDODISK_NUM_MAX){
				PRINTF_ERR("max. number of pseudo-disks is too large\n");
				mainret=1; /* error */
				goto main_exit;
			}
			/* Note: pseudodisknum==0 is allowed and means: max. number of pseudo-disks is not specified */
			break;
#if defined(_VISUALCPP)||defined(__CYGWIN__)
		case 'c':
			/* open CD-ROM drive */
			sprintf(dirnamebuf,"\\\\.\\cdrom%i",atoi(optarg));
			if (open_disk(dirnamebuf,1,startoff,pseudodisksize,pseudodisknum)<0){ /* 1: read-only */
				mainret=1; /* error */
				goto main_exit;
			}
			break;
		case 'p':
			/* open physical drive, Note: use with care!!! */
			sprintf(dirnamebuf,"\\\\.\\physicaldrive%i",atoi(optarg));
			if (open_disk(dirnamebuf,readonly,startoff,pseudodisksize,pseudodisknum)<0){
				mainret=1; /* error */
				goto main_exit;
			}
			break;
#endif
		case 'f':
			/* open floppy drive or disk-file */
			if (open_disk(optarg,readonly,startoff,pseudodisksize,pseudodisknum)<0){
				mainret=1; /* error */
				goto main_exit;
			}
			break;
		case '?':
			/* fall through */
		default:
			mainret=1; /* error */
			break;
		}
	}
	if ((mainret!=0)||(optind>argc)){
		usage(argv[0]);
		mainret=1; /* error */
		goto main_exit;
	}

	/* remaining arguments: floppy drives or disk-files */
	for (;optind<argc;optind++){
		/* open floppy drive or disk-file */
		if (open_disk(argv[optind],readonly,startoff,pseudodisksize,pseudodisknum)<0){
			mainret=1; /* error */
			goto main_exit;
		}
	}

	if (disk_num==0){
		PRINTF_ERR("no disks\n");
		mainret=1; /* error */
		goto main_exit;
	}
	PRINTF_OUT("done\n");
	FLUSH_ALL;

	restartflag=1;

main_restart: /* restart with opened disks */
	FLUSH_ALL;

	if (blk_cache_enable){ /* cache enabled? */
		if (flush_blk_cache()<0){
			/* XXX try once again */
			PRINTF_ERR("trying again to flush cache\n");
			FLUSH_ALL;
			flush_blk_cache(); /* XXX if error, too late */
		}
		free_blk_cache();
	}

	if (restartflag){
		PLAYWAV_STOP; /* stop playback of current external WAV file (if currently running) */
		/* no current external WAV file */
		curwavname[0]='\0';
		curwavcmdbuf[0]='\0';
		akai_clear_filetag(curfiltertag,AKAI_FILE_TAGFREE); /* clear all filter tags */
		dirnamebuf[0]='\0'; /* no previous directory given */
	}

	/* scan disks */
	if (restartflag){
		PRINTF_OUT("\nscanning disks\n");
	}
	part_num=0; /* no partitions found so far */
	for (i=0;i<disk_num;i++){
		if (restartflag){
			PRINTF_OUT("disk%u\r",i);
		}
		FLUSH_ALL;
#ifdef _VISUALCPP
		if (disk[i].fldrn>=0){ /* is floppy drive? */
			/* Note: ignore floppyenable */
			if (akai_scan_floppy(&disk[i])<0){
#if 0 /* XXX ignore error */
				mainret=1; /* error */
				goto main_exit;
#endif
			}
		}else /* no floppy drive */
#endif /* _VISUALCPP */
		{
			if (akai_scan_disk(&disk[i],floppyenable)<0){
#if 0 /* XXX ignore error */
				mainret=1; /* error */
				goto main_exit;
#endif
			}
		}
	}
	if (restartflag){
		PRINTF_OUT("done        \n");
	}
	if (disk_num>=DISK_NUM_MAX){
		PRINTF_OUT("\nmax. number (%u) of disks reached\n",DISK_NUM_MAX);
	}
	if (part_num>=PART_NUM_MAX){
		PRINTF_OUT("\nmax. number (%u) of partitions reached\n",PART_NUM_MAX);
	}

	if (restartflag){
		/* print disks */
		PRINTF_OUT("\n");
		akai_list_alldisks(0,NULL);
		FLUSH_ALL;
	}

	/* set current directory */
	/* Note: upon (re)start, dirnamebuf[] contains path of previous directory (or '\0' if none) */
	if (dirnamebuf[0]!='\0'){ /* previous directory given? */
		if (change_curdir(dirnamebuf,0,NULL,1)<0){ /* NULL,1: check last */
			PRINTF_ERR("\nprevious directory not found\n");
			FLUSH_ALL;
			dirnamebuf[0]='\0'; /* no previous directory given anymore */
		}
	}
	if (dirnamebuf[0]=='\0'){ /* no previous directory given? */
		change_curdir_home(); /* XXX ignore error */
	}

	/* command interpreter */
	{
#define CMDLEN 256 /* XXX */
		char cmd[CMDLEN+1]; /* +1 for '\0' */
#define CMDTOKMAX 16
		char *(cmdtok[CMDTOKMAX]);
		int cmdtoknr;
		int i;
		char *p;

		enum cmd_e{
			CMD_HELP,
			CMD_EXIT,
			CMD_RESTART,
			CMD_RESTARTKEEP,
			CMD_DINFO,
			CMD_DF,
			CMD_CD,
			CMD_CDI,
			CMD_DIR,
			CMD_DIRREC,
			CMD_LSTAGS,
			CMD_INITTAGS,
			CMD_RENTAG,
			CMD_CDINFO,
			CMD_VCDINFO,
			CMD_SETCDINFO,
			CMD_LCD,
			CMD_LDIR,
			CMD_LSFAT,
			CMD_LSFATI,
			CMD_LSTFATI,
			CMD_INFOI,
			CMD_INFOALL,
			CMD_TINFOI,
			CMD_TINFOALL,
			CMD_DEL,
			CMD_DELI,
			CMD_TDELI,
			CMD_REN,
			CMD_RENI,
			CMD_SETOSVERI,
			CMD_SETOSVERALL,
			CMD_SETUNCOMPRI,
			CMD_UPDATEUNCOMPRI,
			CMD_UPDATEUNCOMPRALL,
			CMD_SAMPLE900UNCOMPR,
			CMD_SAMPLE900UNCOMPRI,
			CMD_SAMPLE900UNCOMPRALL,
			CMD_SAMPLE900COMPR,
			CMD_SAMPLE900COMPRI,
			CMD_SAMPLE900COMPRALL,
			CMD_FIXRAMNAME,
			CMD_FIXRAMNAMEI,
			CMD_FIXRAMNAMEALL,
			CMD_CLRTAGI,
			CMD_CLRTAGALL,
			CMD_SETTAGI,
			CMD_SETTAGALL,
			CMD_TRENI,
			CMD_CLRFILTERTAG,
			CMD_SETFILTERTAG,
			CMD_COPY,
			CMD_COPYI,
			CMD_COPYVOL,
			CMD_COPYVOLI,
			CMD_COPYPART,
			CMD_COPYTAGS,
			CMD_WIPEVOL,
			CMD_WIPEVOLI,
			CMD_DELVOL,
			CMD_DELVOLI,
			CMD_FORMATFLOPPYL9,
			CMD_FORMATFLOPPYL1,
			CMD_FORMATFLOPPYL3,
			CMD_FORMATFLOPPYH9,
			CMD_FORMATFLOPPYH1,
			CMD_FORMATFLOPPYH3,
			CMD_WIPEFLOPPY,
			CMD_FORMATHARDDISK9,
			CMD_FORMATHARDDISK1,
			CMD_FORMATHARDDISK3,
			CMD_FORMATHARDDISK3CD,
			CMD_WIPEPART,
			CMD_WIPEPART3CD,
			CMD_FIXPART,
			CMD_FIXHARDDISK,
			CMD_SCANBADBLKSPART,
			CMD_MARKBADBLKSPART,
			CMD_SCANBADBLKSDISK,
			CMD_MARKBADBLKSDISK,
			CMD_GETDISK,
			CMD_PUTDISK,
			CMD_GETPART,
			CMD_PUTPART,
			CMD_GETTAGS,
			CMD_PUTTAGS,
			CMD_GETVOLPARAM,
			CMD_PUTVOLPARAM,
			CMD_RENVOL,
			CMD_RENVOLI,
			CMD_SETOSVERVOL,
			CMD_SETOSVERVOLI,
			CMD_SETLNUM,
			CMD_SETLNUMI,
			CMD_LSPARAM,
			CMD_LSPARAMI,
			CMD_INITPARAM,
			CMD_INITPARAMI,
			CMD_SETPARAM,
			CMD_SETPARAMI,
			CMD_GETPARAM,
			CMD_GETPARAMI,
			CMD_PUTPARAM,
			CMD_PUTPARAMI,
			CMD_GET,
			CMD_GETI,
			CMD_GETALL,
			CMD_SAMPLE2WAV,
			CMD_SAMPLE2WAVI,
			CMD_SAMPLE2WAVALL,
			CMD_PUT,
			CMD_WAV2SAMPLE,
			CMD_WAV2SAMPLE9,
			CMD_WAV2SAMPLE9C,
			CMD_WAV2SAMPLE1,
			CMD_WAV2SAMPLE3,
			CMD_TGETI,
			CMD_TGETALL,
			CMD_TAKE2WAVI,
			CMD_TAKE2WAVALL,
			CMD_TPUT,
			CMD_WAV2TAKE,
			CMD_TARC,
			CMD_TARCWAV,
			CMD_TARX,
			CMD_TARX9,
			CMD_TARX1,
			CMD_TARX3,
			CMD_TARX3CD,
			CMD_TARXWAV,
			CMD_TARXWAV9,
			CMD_TARXWAV9C,
			CMD_TARXWAV1,
			CMD_TARXWAV3,
			CMD_MKVOL,
			CMD_MKVOL9,
			CMD_MKVOL1,
			CMD_MKVOL3,
			CMD_MKVOL3CD,
			CMD_MKVOLI,
			CMD_MKVOLI9,
			CMD_MKVOLI1,
			CMD_MKVOLI3,
			CMD_MKVOLI3CD,
			CMD_DIRCACHE,
			CMD_DISABLECACHE,
			CMD_ENABLECACHE,
			CMD_LOCK,
			CMD_UNLOCK,
			CMD_PLAYWAV,
			CMD_STOPWAV,
			CMD_DELWAV,
			CMD_NULL
		};
		enum cmd_e cmdnr;

		struct cmdtab_s{
			enum cmd_e cmdnr;
			char *cmdstr;
			int cmdtokmin;
			int cmdtokmax;
			char *cmduse;
			char *cmdhelp;
		}cmdtab[]={
			{CMD_HELP,"help",1,2,"[<cmd>]","print help information for a command"},
			{CMD_HELP,"man",1,2,NULL,NULL},
			{CMD_EXIT,"exit",1,1,"","exit program"},
			{CMD_EXIT,"quit",1,1,NULL,NULL},
			{CMD_EXIT,"bye",1,1,NULL,NULL},
			{CMD_EXIT,"q",1,1,NULL,NULL},
			{CMD_RESTART,"restart",1,1,"","restart program (with opened disks)"},
			{CMD_RESTARTKEEP,"restartkeep",1,1,"","restart program (with opened disks, keep current directory path etc.)"},
			{CMD_RESTARTKEEP,"restart.",1,1,NULL,NULL},
			{CMD_RESTARTKEEP,"sync",1,1,NULL,NULL},
			{CMD_DF,"df",1,1,"","print disk info"},
			{CMD_DINFO,"dinfo",1,1,"","print current directory info"},
			{CMD_DINFO,"pwd",1,1,NULL,NULL},
			{CMD_CD,"cd",1,2,"[<path>]","change current directory"},
			{CMD_CDI,"cdi",2,2,"<volume-index>","change current volume"},
			{CMD_DIR,"dir",1,2,"[<path>]","list directory"},
			{CMD_DIR,"ls",1,2,NULL,NULL},
			{CMD_DIRREC,"dirrec",1,1,"","list current directory recursively"},
			{CMD_DIRREC,"lsrec",1,1,NULL,NULL},
			{CMD_LSTAGS,"lstags",1,1,"","list tags in partition"},
			{CMD_LSTAGS,"dirtags",1,1,NULL,NULL},
			{CMD_INITTAGS,"inittags",1,1,"","initialize tags of disk or partition"},
			{CMD_RENTAG,"rentag",3,3,"<tag-index> <tag-name>","rename tag in partition"},
			{CMD_CDINFO,"cdinfo",1,1,"","print CD3000 CD-ROM info"},
			{CMD_VCDINFO,"vcdinfo",1,1,"","print verbose CD3000 CD-ROM info"},
			{CMD_SETCDINFO,"setcdinfo",1,2,"[<cdlabel>]","set CD3000 CD-ROM info of disk or partition"},
			{CMD_LCD,"lcd",2,2,"<dir-path>","change current local (external) directory"},
			{CMD_LDIR,"ldir",1,1,"","list files in current local (external) directory"},
			{CMD_LDIR,"lls",1,1,NULL,NULL},
			{CMD_LSFAT,"lsfat",1,1,"","list FAT of partition"},
			{CMD_LSFATI,"lsfati",2,2,"<file-index>","print FAT-chain of file"},
			{CMD_LSTFATI,"lstfati",2,2,"<take-index>","print FAT-chain of DD take"},
			{CMD_INFOI,"infoi",2,2,"<file-index>","print information for file"},
			{CMD_INFOALL,"infoall",1,1,"","print information for all files in current volume"},
			{CMD_TINFOI,"tinfoi",2,2,"<take-index>","print information for DD take"},
			{CMD_TINFOALL,"tinfoall",1,1,"","print information for all DD takes in current partition"},
			{CMD_DEL,"del",2,2,"<file-path>","delete file"},
			{CMD_DEL,"rm",2,2,NULL,NULL},
			{CMD_DELI,"deli",2,2,"<file-index>","delete file"},
			{CMD_DELI,"rmi",2,2,NULL,NULL},
			{CMD_TDELI,"tdeli",2,2,"<take-index>","delete DD take"},
			{CMD_TDELI,"trmi",2,2,NULL,NULL},
			{CMD_REN,"ren",3,4,"<old-file-path> <new-file-path> [<new-file-index>]","rename/move file"},
			{CMD_REN,"mv",3,4,NULL,NULL},
			{CMD_RENI,"reni",3,4,"<old-file-index> <new-file-path> [<new-file-index>]","rename/move file"},
			{CMD_RENI,"mvi",3,4,NULL,NULL},
			{CMD_SETOSVERI,"setosveri",3,3,"<file-index> <new-os-version>","set OS version of file in S1000/S3000 volume"},
			{CMD_SETOSVERALL,"setosverall",2,2,"<new-os-version>","set OS version of all files in current S1000/S3000 volume"},
			{CMD_SETUNCOMPRI,"setuncompri",3,3,"<file-index> <new-uncompr>","set uncompr value of compressed file in S900 volume"},
			{CMD_UPDATEUNCOMPRI,"updateuncompri",2,2,"<file-index>","update uncompr value of compressed file in S900 volume"},
			{CMD_UPDATEUNCOMPRALL,"updateuncomprall",1,1,"","update uncompr value of all compressed files in current S900 volume"},
			{CMD_SAMPLE900UNCOMPR,"sample900uncompr",2,3,"<file-path> [<dest-vol-path>]","uncompress S900 compressed sample file"},
			{CMD_SAMPLE900UNCOMPR,"s9uncompr",2,3,NULL,NULL},
			{CMD_SAMPLE900UNCOMPRI,"sample900uncompri",2,3,"<file-index> [<dest-vol-path>]","uncompress S900 compressed sample file"},
			{CMD_SAMPLE900UNCOMPRI,"s9uncompri",2,3,NULL,NULL},
			{CMD_SAMPLE900UNCOMPRALL,"sample900uncomprall",1,2,"[<dest-vol-path>]","uncompress all S900 compressed sample files in current volume"},
			{CMD_SAMPLE900UNCOMPRALL,"s9uncomprall",1,2,NULL,NULL},
			{CMD_SAMPLE900COMPR,"sample900compr",2,3,"<file-path> [<dest-vol-path>]","compress S900 non-compressed sample file"},
			{CMD_SAMPLE900COMPR,"s9compr",2,3,NULL,NULL},
			{CMD_SAMPLE900COMPRI,"sample900compri",2,3,"<file-index> [<dest-vol-path>]","compress S900 non-compressed sample file"},
			{CMD_SAMPLE900COMPRI,"s9compri",2,3,NULL,NULL},
			{CMD_SAMPLE900COMPRALL,"sample900comprall",1,2,"[<dest-vol-path>]","compress all S900 non-compressed sample files in current volume"},
			{CMD_SAMPLE900COMPRALL,"s9comprall",1,2,NULL,NULL},
			{CMD_FIXRAMNAME,"fixramname",2,2,"<file-path>","fix name in file header"},
			{CMD_FIXRAMNAMEI,"fixramnamei",2,2,"<file-index>","fix name in file header"},
			{CMD_FIXRAMNAMEALL,"fixramnameall",1,1,"","fix name in file header of all files in current volume"},
			{CMD_CLRTAGI,"clrtagi",3,3,"<file-index> {<tag-index>|all}","untag file"},
			{CMD_CLRTAGALL,"clrtagall",2,2,"{<tag-index>|all}","untag all files in current volume"},
			{CMD_SETTAGI,"settagi",3,3,"<file-index> <tag-index>","tag file"},
			{CMD_SETTAGALL,"settagall",2,2,"<tag-index>","tag all files in current volume"},
			{CMD_TRENI,"treni",3,3,"<take-index> <new-name>","rename DD take"},
			{CMD_TRENI,"tmvi",3,3,NULL,NULL},
			{CMD_CLRFILTERTAG,"clrfiltertag",2,2,"{<tag-index>|all}","remove tag from file filter"},
			{CMD_SETFILTERTAG,"setfiltertag",2,2,"<tag-index>","add tag to file filter"},
			{CMD_COPY,"copy",3,4,"<src-file-path> <new-file-path> [<new-file-index>]","copy file"},
			{CMD_COPY,"cp",3,4,NULL,NULL},
			{CMD_COPYI,"copyi",3,4,"<src-file-index> <new-file-path> [<new-file-index>]","copy file"},
			{CMD_COPYI,"cpi",3,4,NULL,NULL},
			{CMD_COPYVOL,"copyvol",3,3,"<src-volume-path> <new-volume-path>","copy volume (with all of its files)"},
			{CMD_COPYVOL,"cpvol",3,3,NULL,NULL},
			{CMD_COPYVOLI,"copyvoli",3,3,"<src-volume-index> <new-volume-path>","copy volume (with all of its files)"},
			{CMD_COPYVOLI,"cpvoli",3,3,NULL,NULL},
			{CMD_COPYPART,"copypart",3,3,"<src-partition-path> <dst-partition-path>","copy all volumes of a partition"},
			{CMD_COPYPART,"cppart",3,3,NULL,NULL},
			{CMD_COPYTAGS,"copytags",3,3,"<src-partition-path> <dst-partition-path>","copy all tags of a partition"},
			{CMD_COPYTAGS,"cptags",3,3,NULL,NULL},
			{CMD_WIPEVOL,"wipevol",2,2,"<volume-path>","delete all files in volume"},
			{CMD_WIPEVOL,"wipedir",2,2,NULL,NULL},
			{CMD_WIPEVOL,"rmall",2,2,NULL,NULL},
			{CMD_WIPEVOLI,"wipevoli",2,2,"<volume-index>","delete all files in volume"},
			{CMD_WIPEVOLI,"wipediri",2,2,NULL,NULL},
			{CMD_WIPEVOLI,"rmalli",2,2,NULL,NULL},
			{CMD_DELVOL,"delvol",2,2,"<volume-path>","delete volume and all of its files"},
			{CMD_DELVOL,"deldir",2,2,NULL,NULL},
			{CMD_DELVOL,"rmvol",2,2,NULL,NULL},
			{CMD_DELVOL,"rmdir",2,2,NULL,NULL},
			{CMD_DELVOLI,"delvoli",2,2,"<volume-index>","delete volume and all of its files"},
			{CMD_DELVOLI,"deldiri",2,2,NULL,NULL},
			{CMD_DELVOLI,"rmvoli",2,2,NULL,NULL},
			{CMD_DELVOLI,"rmdiri",2,2,NULL,NULL},
			{CMD_FORMATFLOPPYL9,"formatfloppyl9",1,1,"","format/erase low-density floppy for S900/S950, create filesystem"},
			{CMD_FORMATFLOPPYL9,"formatfl9",1,1,NULL,NULL},
			{CMD_FORMATFLOPPYL1,"formatfloppyl1",1,1,"","format/erase low-density floppy for S1000, create filesystem"},
			{CMD_FORMATFLOPPYL1,"formatfl1",1,1,NULL,NULL},
			{CMD_FORMATFLOPPYL3,"formatfloppyl3",1,1,"","format/erase low-density floppy for S3000, create filesystem"},
			{CMD_FORMATFLOPPYL3,"formatfl3",1,1,NULL,NULL},
			{CMD_FORMATFLOPPYH9,"formatfloppyh9",1,1,"","format/erase high-density floppy for S950, create filesystem"},
			{CMD_FORMATFLOPPYH9,"formatfh9",1,1,NULL,NULL},
			{CMD_FORMATFLOPPYH1,"formatfloppyh1",1,1,"","format/erase high-density floppy for S1000, create filesystem"},
			{CMD_FORMATFLOPPYH1,"formatfh1",1,1,NULL,NULL},
			{CMD_FORMATFLOPPYH3,"formatfloppyh3",1,1,"","format/erase high-density floppy for S3000, create filesystem"},
			{CMD_FORMATFLOPPYH3,"formatfh3",1,1,NULL,NULL},
			{CMD_WIPEFLOPPY,"wipefloppy",1,1,"","create filesystem on floppy"},
			{CMD_WIPEFLOPPY,"newfsfloppy",1,1,NULL,NULL},
			{CMD_FORMATHARDDISK9,"formatharddisk9",1,2,"[<total-size>[M]|SH205|SUPRA20M|SUPRA30M|SUPRA60M]","create filesystem on harddisk for S900/S950 (size in blocks or MB, or drive name)"},
			{CMD_FORMATHARDDISK9,"formathd9",1,2,NULL,NULL},
			{CMD_FORMATHARDDISK9,"newfsharddisk9",1,2,NULL,NULL},
			{CMD_FORMATHARDDISK9,"newfshd9",1,2,NULL,NULL},
			{CMD_FORMATHARDDISK1,"formatharddisk1",1,3,"[<part-size>[M] [<total-size>[M]]]","create filesystem on harddisk for S1000 (size in blocks or MB)"},
			{CMD_FORMATHARDDISK1,"formathd1",1,3,NULL,NULL},
			{CMD_FORMATHARDDISK1,"newfsharddisk1",1,3,NULL,NULL},
			{CMD_FORMATHARDDISK1,"newfshd1",1,3,NULL,NULL},
			{CMD_FORMATHARDDISK3,"formatharddisk3",1,3,"[<part-size>[M] [<total-size>[M]]]","create filesystem on harddisk for S3000 or CD3000 (size in blocks or MB)"},
			{CMD_FORMATHARDDISK3,"formathd3",1,3,NULL,NULL},
			{CMD_FORMATHARDDISK3,"newfsharddisk3",1,3,NULL,NULL},
			{CMD_FORMATHARDDISK3,"newfshd3",1,3,NULL,NULL},
			{CMD_FORMATHARDDISK3CD,"formatharddisk3cd",1,3,"[<part-size>[M] [<total-size>[M]]]","create filesystem on harddisk for CD3000 CD-ROM (size in blocks or MB)"},
			{CMD_FORMATHARDDISK3CD,"formatcd",1,3,NULL,NULL},
			{CMD_FORMATHARDDISK3CD,"newfsharddisk3cd",1,3,NULL,NULL},
			{CMD_FORMATHARDDISK3CD,"newfscd",1,3,NULL,NULL},
			{CMD_WIPEPART,"wipepart",1,2,"[<partition-path>]","create filesystem in harddisk partition"},
			{CMD_WIPEPART,"newfspart",1,2,NULL,NULL},
			{CMD_WIPEPART3CD,"wipepart3cd",1,2,"[<partition-path>]","create filesystem in harddisk partition for CD3000 CD-ROM"},
			{CMD_WIPEPART3CD,"newfspart3cd",1,2,NULL,NULL},
			{CMD_FIXPART,"fixpart",1,2,"[<partition-path>]","fix filesystem in harddisk partition"},
			{CMD_FIXHARDDISK,"fixharddisk",1,2,"[<disk-path>]","fix filesystem on harddisk"},
			{CMD_SCANBADBLKSPART,"scanbadblkspart",1,2,"[<partition-path>]","scan for bad blocks/clusters in partition"},
			{CMD_MARKBADBLKSPART,"markbadblkspart",1,2,"[<partition-path>]","mark free bad blocks/clusters in partition"},
			{CMD_SCANBADBLKSDISK,"scanbadblksdisk",1,2,"[<disk-path>]","scan for bad blocks/clusters in all partitions of disk"},
			{CMD_MARKBADBLKSDISK,"markbadblksdisk",1,2,"[<disk-path>]","mark free bad blocks/clusters in all partitions of disk"},
			{CMD_GETDISK,"getdisk",2,2,"<file-name>","get disk (to external file)"},
			{CMD_GETDISK,"dget",2,2,NULL,NULL},
			{CMD_GETDISK,"dexport",2,2,NULL,NULL},
			{CMD_PUTDISK,"putdisk",2,2,"<file-name>","put disk (from external file)"},
			{CMD_PUTDISK,"dput",2,2,NULL,NULL},
			{CMD_PUTDISK,"dimport",2,2,NULL,NULL},
			{CMD_GETPART,"getpart",2,3,"[<partition-path>] <file-name>","get partition (to external file)"},
			{CMD_GETPART,"pget",2,3,NULL,NULL},
			{CMD_GETPART,"pexport",2,3,NULL,NULL},
			{CMD_PUTPART,"putpart",2,3,"<file-name> [<partition-path>]","put partition (from external file)"},
			{CMD_PUTPART,"pput",2,3,NULL,NULL},
			{CMD_PUTPART,"pimport",2,3,NULL,NULL},
			{CMD_GETTAGS,"gettags",2,2,"<file-name>","get tags from partition (to external file)"},
			{CMD_GETTAGS,"tagsget",2,2,NULL,NULL},
			{CMD_GETTAGS,"tagsexport",2,2,NULL,NULL},
			{CMD_PUTTAGS,"puttags",2,2,"<file-name>","put tags to partition (from external file)"},
			{CMD_PUTTAGS,"tagsput",2,2,NULL,NULL},
			{CMD_PUTTAGS,"tagsimport",2,2,NULL,NULL},
			{CMD_RENVOL,"renvol",3,3,"<old-path> <new-name>","rename volume"},
			{CMD_RENVOL,"rendir",3,3,NULL,NULL},
			{CMD_RENVOL,"mvvol",3,3,NULL,NULL},
			{CMD_RENVOL,"mvdir",3,3,NULL,NULL},
			{CMD_RENVOLI,"renvoli",3,3,"<volume-index> <new-name>","rename volume"},
			{CMD_RENVOLI,"rendiri",3,3,NULL,NULL},
			{CMD_RENVOLI,"mvvoli",3,3,NULL,NULL},
			{CMD_RENVOLI,"mvdiri",3,3,NULL,NULL},
			{CMD_SETOSVERVOL,"setosvervol",2,2,"<new-os-version>","set OS version of current volume"},
			{CMD_SETOSVERVOLI,"setosvervoli",3,3,"<volume-index> <new-os-version>","set OS version of volume"},
			{CMD_SETLNUM,"setlnum",2,2,"<new-load-number>","set load number of current volume (OFF for none)"},
			{CMD_SETLNUMI,"setlnumi",3,3,"<volume-index> <new-load-number>","set load number of volume (OFF for none)"},
			{CMD_LSPARAM,"lsparam",1,1,"","list parameters in current volume"},
			{CMD_LSPARAMI,"lsparami",2,2,"<volume-index>","list parameters in volume"},
			{CMD_INITPARAM,"initparam",1,1,"","initialize parameters in current volume"},
			{CMD_INITPARAMI,"initparami",2,2,"<volume-index>","initialize parameters in volume"},
			{CMD_SETPARAM,"setparam",3,3,"<par-index> <par-value>","set parameters in current volume"},
			{CMD_SETPARAMI,"setparami",4,4,"<volume-index> <par-index> <par-value>","set parameters in volume"},
			{CMD_GETPARAM,"getparam",2,2,"<file-name>","get parameters from current volume (to external file)"},
			{CMD_GETPARAM,"paramget",2,2,NULL,NULL},
			{CMD_GETPARAM,"paramexport",2,2,NULL,NULL},
			{CMD_GETPARAMI,"getparami",3,3,"<volume-index> <file-name>","get parameters from volume (to external file)"},
			{CMD_GETPARAMI,"paramgeti",3,3,NULL,NULL},
			{CMD_GETPARAMI,"paramexporti",3,3,NULL,NULL},
			{CMD_PUTPARAM,"putparam",2,2,"<file-name>","put parameters to current volume (from external file)"},
			{CMD_PUTPARAM,"paramput",2,2,NULL,NULL},
			{CMD_PUTPARAM,"paramimport",2,2,NULL,NULL},
			{CMD_PUTPARAMI,"putparami",3,3,"<volume-index> <file-name>","put parameters to volume (from external file)"},
			{CMD_PUTPARAMI,"paramputi",3,3,NULL,NULL},
			{CMD_PUTPARAMI,"paramimporti",3,3,NULL,NULL},
			{CMD_GET,"get",2,4,"<file-path> [<begin-byte> [<end-byte>]]","get file (to external)"},
			{CMD_GET,"export",2,4,NULL,NULL},
			{CMD_GETI,"geti",2,4,"<file-index> [<begin-byte> [<end-byte>]]","get file (to external)"},
			{CMD_GETI,"exporti",2,4,NULL,NULL},
			{CMD_GETALL,"getall",1,1,"","get all files (to external)"},
			{CMD_GETALL,"exportall",1,1,NULL,NULL},
			{CMD_SAMPLE2WAV,"sample2wav",2,2,"<file-path>","convert sample file into external WAV file"},
			{CMD_SAMPLE2WAV,"s2wav",2,2,NULL,NULL},
			{CMD_SAMPLE2WAV,"getwav",2,2,NULL,NULL},
			{CMD_SAMPLE2WAVI,"sample2wavi",2,2,"<file-index>","convert sample file into external WAV file"},
			{CMD_SAMPLE2WAVI,"s2wavi",2,2,NULL,NULL},
			{CMD_SAMPLE2WAVI,"getwavi",2,2,NULL,NULL},
			{CMD_SAMPLE2WAVALL,"sample2wavall",1,1,NULL,"convert all sample files into external WAV files"},
			{CMD_SAMPLE2WAVALL,"s2wavall",1,1,NULL,NULL},
			{CMD_SAMPLE2WAVALL,"getwavall",1,1,NULL,NULL},
			{CMD_PUT,"put",2,3,"[<volume-path>/]<file-name> [<file-index>]","put file (from external, use * for all local files)"},
			{CMD_PUT,"import",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE,"wav2sample",2,3,"[<volume-path>/]<wav-file> [<file-index>]","convert external WAV file into sample file (use *.wav for all local WAV files)"},
			{CMD_WAV2SAMPLE,"wav2s",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE,"putwav",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE9,"wav2sample9",2,3,"[<volume-path>/]<wav-file> [<file-index>]","convert external WAV file into S900 non-compressed sample file (use *.wav for all local WAV files)"},
			{CMD_WAV2SAMPLE9,"wav2s9",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE9,"putwav9",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE9C,"wav2sample9c",2,3,"[<volume-path>/]<wav-file> [<file-index>]","convert external WAV file into S900 compressed sample file (use *.wav for all local WAV files)"},
			{CMD_WAV2SAMPLE9C,"wav2s9c",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE9C,"putwav9c",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE1,"wav2sample1",2,3,"[<volume-path>/]<wav-file> [<file-index>]","convert external WAV file into S1000 sample file (use *.wav for all local WAV files)"},
			{CMD_WAV2SAMPLE1,"wav2s1",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE1,"putwav1",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE3,"wav2sample3",2,3,"[<volume-path>/]<wav-file> [<file-index>]","convert external WAV file into S3000 sample file (use *.wav for all local WAV files)"},
			{CMD_WAV2SAMPLE3,"wav2s3",2,3,NULL,NULL},
			{CMD_WAV2SAMPLE3,"putwav3",2,3,NULL,NULL},
			{CMD_TGETI,"tgeti",2,2,"<take-index>","get DD take (to external)"},
			{CMD_TGETI,"texporti",2,2,NULL,NULL},
			{CMD_TGETALL,"tgetall",1,1,"","get all DD takes (to external)"},
			{CMD_TGETALL,"texportall",1,1,NULL,NULL},
			{CMD_TAKE2WAVI,"take2wavi",2,2,"<take-index>","convert DD take into external WAV file"},
			{CMD_TAKE2WAVI,"t2wavi",2,2,NULL,NULL},
			{CMD_TAKE2WAVI,"tgetwavi",2,2,NULL,NULL},
			{CMD_TAKE2WAVI,"getwavti",2,2,NULL,NULL},
			{CMD_TAKE2WAVALL,"take2wavall",1,1,NULL,"convert all DD takes into external WAV files"},
			{CMD_TAKE2WAVALL,"t2wavall",1,1,NULL,NULL},
			{CMD_TAKE2WAVALL,"tgetwavall",1,1,NULL,NULL},
			{CMD_TAKE2WAVALL,"getwavtall",1,1,NULL,NULL},
			{CMD_TPUT,"tput",2,2,"<take-file>","put DD take (from external, use *"AKAI_DDTAKE_FNAMEEND" for all local DD take files)"},
			{CMD_TPUT,"timport",2,2,NULL,NULL},
			{CMD_WAV2TAKE,"wav2take",2,2,"<wav-file>","convert external WAV file into DD take (use *.wav for all local WAV files)"},
			{CMD_WAV2TAKE,"wav2t",2,2,NULL,NULL},
			{CMD_WAV2TAKE,"tputwav",2,2,NULL,NULL},
			{CMD_WAV2TAKE,"putwavt",2,2,NULL,NULL},
			{CMD_TARC,"tarc",2,2,"<tar-file>","tar c from current directory (to external)"},
			{CMD_TARC,"target",2,2,NULL,NULL},
			{CMD_TARC,"gettar",2,2,NULL,NULL},
			{CMD_TARCWAV,"tarcwav",2,2,"<tar-file>","tar c from current directory (to external) with WAV conversion"},
			{CMD_TARCWAV,"targetwav",2,2,NULL,NULL},
			{CMD_TARCWAV,"gettarwav",2,2,NULL,NULL},
			{CMD_TARX,"tarx",2,2,"<tar-file>","tar x in current directory (from external)"},
			{CMD_TARX,"tarput",2,2,NULL,NULL},
			{CMD_TARX,"puttar",2,2,NULL,NULL},
			{CMD_TARX9,"tarx9",2,2,"<tar-file>","tar x in current directory (from external) for S900"},
			{CMD_TARX9,"tarput9",2,2,NULL,NULL},
			{CMD_TARX9,"puttar9",2,2,NULL,NULL},
			{CMD_TARX1,"tarx1",2,2,"<tar-file>","tar x in current directory (from external) for S1000"},
			{CMD_TARX1,"tarput1",2,2,NULL,NULL},
			{CMD_TARX1,"puttar1",2,2,NULL,NULL},
			{CMD_TARX3,"tarx3",2,2,"<tar-file>","tar x in current directory (from external) for S3000 or CD3000"},
			{CMD_TARX3,"tarput3",2,2,NULL,NULL},
			{CMD_TARX3,"puttar3",2,2,NULL,NULL},
			{CMD_TARX3CD,"tarx3cd",2,2,"<tar-file>","tar x in current directory (from external) for CD3000 CD-ROM"},
			{CMD_TARX3CD,"tarput3cd",2,2,NULL,NULL},
			{CMD_TARX3CD,"puttar3cd",2,2,NULL,NULL},
			{CMD_TARXWAV,"tarxwav",2,2,"<tar-file>","tar x in current directory (from external) with WAV conversion"},
			{CMD_TARXWAV,"tarputwav",2,2,NULL,NULL},
			{CMD_TARXWAV,"puttarwav",2,2,NULL,NULL},
			{CMD_TARXWAV9,"tarxwav9",2,2,"<tar-file>","tar x in current directory (from external) with WAV conversion to S900 compressed sample"},
			{CMD_TARXWAV9,"tarputwav9",2,2,NULL,NULL},
			{CMD_TARXWAV9,"puttarwav9",2,2,NULL,NULL},
			{CMD_TARXWAV9C,"tarxwav9c",2,2,"<tar-file>","tar x in current directory (from external) with WAV conversion to S900 compressed sample"},
			{CMD_TARXWAV9C,"tarputwav9c",2,2,NULL,NULL},
			{CMD_TARXWAV9C,"puttarwav9c",2,2,NULL,NULL},
			{CMD_TARXWAV1,"tarxwav1",2,2,"<tar-file>","tar x in current directory (from external) with WAV conversion to S1000 sample"},
			{CMD_TARXWAV1,"tarputwav1",2,2,NULL,NULL},
			{CMD_TARXWAV1,"puttarwav1",2,2,NULL,NULL},
			{CMD_TARXWAV3,"tarxwav3",2,2,"<tar-file>","tar x in current directory (from external) with WAV conversion to S3000 sample"},
			{CMD_TARXWAV3,"tarputwav3",2,2,NULL,NULL},
			{CMD_TARXWAV3,"puttarwav3",2,2,NULL,NULL},
			{CMD_MKVOL,"mkvol",1,2,"[<volume-path>]","create new volume"},
			{CMD_MKVOL,"mkdir",1,2,NULL,NULL},
			{CMD_MKVOL9,"mkvol9",1,2,"[<volume-path>]","create new volume for S900"},
			{CMD_MKVOL9,"mkdir9",1,2,NULL,NULL},
			{CMD_MKVOL1,"mkvol1",1,3,"[<volume-path> [<load-number>]]","create new volume for S1000"},
			{CMD_MKVOL1,"mkdir1",1,3,NULL,NULL},
			{CMD_MKVOL3,"mkvol3",1,3,"[<volume-path> [<load-number>]]","create new volume for S3000 or CD3000"},
			{CMD_MKVOL3,"mkdir3",1,3,NULL,NULL},
			{CMD_MKVOL3CD,"mkvol3cd",1,3,"[<volume-path> [<load-number>]]","create new volume for CD3000 CD-ROM"},
			{CMD_MKVOL3CD,"mkdir3cd",1,3,NULL,NULL},
			{CMD_MKVOLI,"mkvoli",2,3,"<volume-index> [<volume-name>]","create new volume at index"},
			{CMD_MKVOLI,"mkdiri",2,3,NULL,NULL},
			{CMD_MKVOLI9,"mkvoli9",2,3,"<volume-index> [<volume-name>]","create new volume for S900 at index"},
			{CMD_MKVOLI9,"mkdiri9",2,3,NULL,NULL},
			{CMD_MKVOLI1,"mkvoli1",2,4,"<volume-index> [<volume-name> [<load-number>]]","create new volume for S1000 at index"},
			{CMD_MKVOLI1,"mkdiri1",2,4,NULL,NULL},
			{CMD_MKVOLI3,"mkvoli3",2,4,"<volume-index> [<volume-name> [<load-number>]]","create new volume for S3000 or CD3000 at index"},
			{CMD_MKVOLI3,"mkdiri3",2,4,NULL,NULL},
			{CMD_MKVOLI3CD,"mkvoli3cd",2,4,"<volume-index> [<volume-name> [<load-number>]]","create new volume for CD3000 CD-ROM at index"},
			{CMD_MKVOLI3CD,"mkdiri3cd",2,4,NULL,NULL},
			{CMD_LOCK,"lock",1,1,"","acquire lock"},
			{CMD_UNLOCK,"unlock",1,1,"","release lock"},
			{CMD_DIRCACHE,"dircache",1,1,"","print cache information"},
			{CMD_DIRCACHE,"lscache",1,1,NULL,NULL},
			{CMD_DISABLECACHE,"disablecache",1,1,"","disable cache"},
			{CMD_ENABLECACHE,"enablecache",1,1,"","enable cache"},
			{CMD_PLAYWAV,"playwav",1,2,"[<wav-name>]","start playback of current external WAV file"},
			{CMD_PLAYWAV,"p",1,2,NULL,NULL},
			{CMD_STOPWAV,"stopwav",1,1,"","stop playback of current external WAV file"},
			{CMD_STOPWAV,"s",1,1,NULL,NULL},
			{CMD_DELWAV,"delwav",1,1,"","delete current external WAV file"},
			{CMD_DELWAV,"rmwav",1,1,NULL,NULL},
			{CMD_NULL,NULL,0,0,NULL,NULL}
		};

		if (restartflag){
			if (readonly){
				PRINTF_OUT("\n--- read-only mode ---\n");
			}else{
				PRINTF_OUT("\n--- read/write mode ---\n");
			}
			PRINTF_OUT("\ntry \"help\" for infos\n\n");
		}
		
		for (;;){
			/* get command */
			curdir_info(0,NULL);
			PRINTF_OUT(" > ");
			FLUSH_ALL;
			if ((FGETS(cmd,CMDLEN,stdin)==NULL)
				||(strlen(cmd)==0)){
				goto main_parser_next;
			}
			cmd[strlen(cmd)-1]='\0';
			if (strlen(cmd)==0){
				goto main_parser_next;
			}
			/* parse command line */
			for (i=0;i<CMDTOKMAX;i++){
				if (i==0){
					p=(char *)strtok(cmd," \t");
					if (p==NULL){
						/* no tokens at all */
						goto main_parser_next;
					}
					cmdtok[i]=p;
				}else{
					p=(char *)strtok(NULL," \t");
					if (p==NULL){
						/* done */
						break; /* parse command line loop */
					}
					cmdtok[i]=p;
				}
			}
			cmdtoknr=i; /* number of tokens */
#ifdef DEBUGPARSER
			PRINTF_OUT("parser: cmdtoknr: %i\n",cmdtoknr);
			for (i=0;i<cmdtoknr;i++){
				PRINTF_OUT("%i: \"%s\"\n",i,cmdtok[i]);
			}
			PRINTF_OUT("\n");
#endif
			
			/* find command */
			cmdnr=CMD_NULL; /* nothing found yet */
			for (i=0;cmdtab[i].cmdnr!=CMD_NULL;i++){
				if (cmdtab[i].cmdstr!=NULL){
					if (strcmp(cmdtab[i].cmdstr,cmdtok[0])==0){
						/* found command string */
						cmdnr=cmdtab[i].cmdnr;
						break; /* done */
					}
				}
			}

			/* check number of arguments */
			if ((cmdnr!=CMD_NULL)&&((cmdtoknr<cmdtab[i].cmdtokmin)||(cmdtoknr>cmdtab[i].cmdtokmax))){
				for (i=0;cmdtab[i].cmdnr!=CMD_NULL;i++){
					if ((cmdtab[i].cmdnr==cmdnr)&&(cmdtab[i].cmdstr!=NULL)&&(cmdtab[i].cmduse!=NULL)){
						PRINTF_OUT("usage: %s %s\n\n",cmdtab[i].cmdstr,cmdtab[i].cmduse);
						break;
					}
				}
				goto main_parser_next;
			}

			/* execute command */
			switch (cmdnr){
			case CMD_EXIT:
				goto main_exit;
			case CMD_RESTART:
			case CMD_RESTARTKEEP:
				FLUSH_ALL;
				if (cmdnr==CMD_RESTARTKEEP){
					curdir_info(0,dirnamebuf); /* save path of current directory */
					PRINTF_OUT("\n");
					restartflag=0;
				}else{
					PRINTF_OUT("\nrestarting program\n\n");
					restartflag=1;
				}
				goto main_restart;
			case CMD_HELP:
				if (cmdtoknr==1){
					int i,j,k;

					PRINTF_OUT("\ncommands:\n");
					k=0;
					for (i=0,j=0;cmdtab[i].cmdnr!=CMD_NULL;i++){
						if (cmdtab[i].cmdstr!=NULL){
							if (j%5==0){
								putchar('\n');
								k=0;
							}
							for (;k<((j%5)*15+2);k++){
								putchar(' ');
							}
							PRINTF_OUT("%s ",cmdtab[i].cmdstr);
							k+=(int)strlen(cmdtab[i].cmdstr)+1;
							j++;
						}
					}
					PRINTF_OUT("\n\ntry \"help <cmd>\" for more information about <cmd>\n\n");
				}else if (cmdtoknr==2){
					int i,flag;

					/* find command */
					cmdnr=CMD_NULL; /* nothing found yet */
					for (i=0;cmdtab[i].cmdnr!=CMD_NULL;i++){
						if (cmdtab[i].cmdstr!=NULL){
							if (strcmp(cmdtab[i].cmdstr,cmdtok[1])==0){
								/* found command string */
								cmdnr=cmdtab[i].cmdnr;
								break; /* done */
							}
						}
					}
					if (cmdnr==CMD_NULL){
						PRINTF_OUT("unknown command, try \"help\"\n\n");
						goto main_parser_next;
					}
					PRINTF_OUT("\nusage: ");
					flag=0;
					for (i=0;cmdtab[i].cmdnr!=CMD_NULL;i++){
						if (cmdtab[i].cmdnr==cmdnr){
							if (flag==0){
								/* first */
								if (cmdtab[i].cmdstr!=NULL){
									if (cmdtab[i].cmduse!=NULL){
										PRINTF_OUT("%s %s\n",cmdtab[i].cmdstr,cmdtab[i].cmduse);
									}else{
										PRINTF_OUT("%s\n",cmdtab[i].cmdstr);
									}
								}
								if (cmdtab[i].cmdhelp!=NULL){
									PRINTF_OUT("\n%s\n",cmdtab[i].cmdhelp);
								}
								flag=1;
							}else{
								/* found alias */
								if (flag==1){
									PRINTF_OUT("\naliases:\n");
									flag=2;
								}
								if (cmdtab[i].cmdstr!=NULL){
									PRINTF_OUT("  %s\n",cmdtab[i].cmdstr);
								}
							}
						}
					}
					PRINTF_OUT("\n");
				}
				break;
			case CMD_DF:
				{
					u_int di;

					PRINTF_OUT("\n");
					akai_list_alldisks(0,NULL);
					for (di=0;di<disk_num;di++){
						akai_list_disk(&disk[di],0,NULL);
					}
				}
				break;
			case CMD_DINFO:
				PRINTF_OUT("\n");
				curdir_info(1,NULL); /* 1: verbose */
				break;
			case CMD_CD:
				if (cmdtoknr>=2){
					if (change_curdir(cmdtok[1],0,NULL,1)<0){ /* NULL,1: check last */
						PRINTF_ERR("directory not found\n");
					}
				}else{
					change_curdir_home(); /* XXX ignore error */
				}
				break;
			case CMD_CDI:
				{
					u_int vi;

					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						PRINTF_ERR("must be on sampler partition level\n");
						goto main_parser_next;
					}
					/* index */
					vi=(u_int)atoi(cmdtok[1]);
					if ((vi<1)||(vi>curpartp->volnummax)){
						PRINTF_ERR("invalid volume index\n");
						goto main_parser_next;
					}
					if (change_curdir(NULL,vi-1,NULL,1)<0){ /* NULL,1: check last */
						PRINTF_ERR("directory not found\n");
					}
				}
				break;
			case CMD_DIR:
				save_curdir(0); /* 0: no modifications */
				if (cmdtoknr>=2){
					if (change_curdir(cmdtok[1],0,NULL,1)<0){ /* NULL,1: check last */
						PRINTF_ERR("directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
				}
				PRINTF_OUT("\n");
				list_curdir(0); /* 0: non-recursive */
				restore_curdir();
				break;
			case CMD_DIRREC:
				PRINTF_OUT("\n");
				list_curdir(1); /* 1: recursive */
				break;
			case CMD_LSTAGS:
				if (curpartp!=NULL){
					PRINTF_OUT("\n");
					akai_list_tags(curpartp);
				}
				PRINTF_OUT("\n");
				list_curfiltertags();
				PRINTF_OUT("\n");
				break;
			case CMD_INITTAGS:
				{
					u_int i;

					if ((curdiskp!=NULL)&&(curpartp==NULL)){ /* on disk level? */
						if (curdiskp->readonly){
							PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
							goto main_parser_next;
						}
						/* all S1000/S3000 harddisk sampler partitions on current disk */
						for (i=0;i<part_num;i++){
							/* check if same disk and correct partition type */
							if ((part[i].diskp!=curdiskp)||(part[i].type!=PART_TYPE_HD)){
								continue; /* next */
							}
							/* now, S1000/S3000 harddisk sampler partition */
							PRINTF_OUT("partition %c:\n",part[i].letter);
							akai_rename_tag(&part[i],NULL,0,1); /* 1: wipe, XXX ignore error */
						}
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							PRINTF_ERR("must be on disk level or sampler partition level\n");
							goto main_parser_next;
						}
						if (curdiskp->readonly){
							PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
							goto main_parser_next;
						}
						akai_rename_tag(curpartp,NULL,0,1); /* 1: wipe */
					}
					break;
				}
			case CMD_RENTAG:
				{
					u_int ti;

					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						PRINTF_ERR("must be on sampler partition level\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					/* index */
					ti=(u_int)atoi(cmdtok[1]);
					if ((ti<1)||(ti>AKAI_PARTHEAD_TAGNUM)){
						PRINTF_ERR("invalid volume index\n");
						goto main_parser_next;
					}
					akai_rename_tag(curpartp,cmdtok[2],ti-1,0); /* 0: don't wipe */
				}
				break;
			case CMD_CDINFO:
			case CMD_VCDINFO:
				if (check_curnosamplerpart()){ /* not on sampler partition level? */
					PRINTF_ERR("must be on sampler partition level\n");
					goto main_parser_next;
				}
				PRINTF_OUT("\n");
				akai_print_cdinfo(curpartp,(cmdnr==CMD_VCDINFO)?1:0);
				PRINTF_OUT("\n");
				break;
			case CMD_SETCDINFO:
				{
					u_int i;

					if ((curdiskp!=NULL)&&(curpartp==NULL)){ /* on disk level? */
						if (curdiskp->readonly){
							PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
							goto main_parser_next;
						}
						/* all harddisk sampler partitions on current disk */
						for (i=0;i<part_num;i++){
							/* check if same disk and correct partition type */
							if ((part[i].diskp!=curdiskp)||(part[i].type!=PART_TYPE_HD)){
								continue; /* next */
							}
							/* now, S1000/S3000 harddisk sampler partition */
							PRINTF_OUT("partition %c:\n",part[i].letter);
							akai_set_cdinfo(&part[i],(cmdtoknr>=2)?cmdtok[1]:NULL); /* XXX ignore error */
						}
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							PRINTF_ERR("must be on disk level or on sampler partition level\n");
							goto main_parser_next;
						}
						if (curdiskp->readonly){
							PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
							goto main_parser_next;
						}
						akai_set_cdinfo(curpartp,(cmdtoknr>=2)?cmdtok[1]:NULL);
					}
					break;
				}
			case CMD_LCD:
				if (LCHDIR(cmdtok[1])<0){
					PRINTF_ERR("invalid directory path\n");
				}
				break;
			case CMD_LDIR:
				LDIR; /* XXX no error check */
				break;
			case CMD_LSFAT:
				if ((curdiskp==NULL)||(curpartp==NULL)){
					PRINTF_ERR("must be inside a partition\n");
					goto main_parser_next;
				}
				PRINTF_OUT("\n");
				print_fat(curpartp);
				PRINTF_OUT("\n");
				break;
			case CMD_LSFATI:
				{
					struct file_s tmpfile;
					u_int fi;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("must be inside a volume\n");
						goto main_parser_next;
					}
					/* index */
					fi=(u_int)atoi(cmdtok[1]);
					if ((fi<1)||(fi>curvolp->fimax)){
						PRINTF_ERR("invalid file index\n");
						goto main_parser_next;
					}
					/* find file in current volume */
					if (akai_get_file(curvolp,&tmpfile,fi-1)<0){
						PRINTF_ERR("file not found\n");
						goto main_parser_next;
					}
					PRINTF_OUT("\n");
					print_fatchain(curpartp,tmpfile.bstart);
					PRINTF_OUT("\n");
				}
				break;
			case CMD_LSTFATI:
				{
					u_int ti;
					u_int cstarts,cstarte;

					if (check_curnoddpart()){ /* not on DD partition level? */
						PRINTF_ERR("must be inside a DD partition\n");
						goto main_parser_next;
					}
					/* index */
					ti=(u_int)atoi(cmdtok[1]);
					if ((ti<1)||(ti>AKAI_DDTAKE_MAXNUM)){
						PRINTF_ERR("invalid take index\n");
						goto main_parser_next;
					}
					/* take */
					if (curpartp->head.dd.take[ti-1].stat==AKAI_DDTAKESTAT_FREE){ /* free? */
						PRINTF_ERR("take not found\n");
						goto main_parser_next;
					}
					PRINTF_OUT("\n");
					cstarts=(curpartp->head.dd.take[ti-1].cstarts[1]<<8)
						    +curpartp->head.dd.take[ti-1].cstarts[0];
					if (cstarts!=0){ /* sample not empty? */
						PRINTF_OUT("sample:\n");
						print_ddfatchain(curpartp,cstarts);
						PRINTF_OUT("\n");
					}
					cstarte=(curpartp->head.dd.take[ti-1].cstarte[1]<<8)
						    +curpartp->head.dd.take[ti-1].cstarte[0];
					if (cstarte!=0){ /* envelope not empty? */
						PRINTF_OUT("envelope:\n");
						print_ddfatchain(curpartp,cstarte);
						PRINTF_OUT("\n");
					}
				}
				break;
			case CMD_INFOI:
				{
					struct file_s tmpfile;
					u_int fi;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("must be inside a volume\n");
						goto main_parser_next;
					}
					/* index */
					fi=(u_int)atoi(cmdtok[1]);
					if ((fi<1)||(fi>curvolp->fimax)){
						PRINTF_ERR("invalid file index\n");
						goto main_parser_next;
					}
					/* find file in current volume */
					if (akai_get_file(curvolp,&tmpfile,fi-1)<0){
						PRINTF_ERR("file not found\n");
						goto main_parser_next;
					}
					/* print info */
					PRINTF_OUT("\n");
					curdir_info(0,NULL);
					PRINTF_OUT("/");
					if (akai_file_info(&tmpfile,1)<1){ /* 1: verbose */
						PRINTF_OUT("\n");
					}
				}
				break;
			case CMD_INFOALL:
				{
					struct file_s tmpfile;
					u_int sfi;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("must be inside a volume\n");
						goto main_parser_next;
					}
					/* all files in current volume */
					for (sfi=0;sfi<curvolp->fimax;sfi++){
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,sfi)<0){
							continue; /* next */
						}
						/* print info */
						PRINTF_OUT("\n");
						curdir_info(0,NULL);
						PRINTF_OUT("/");
						akai_file_info(&tmpfile,1); /* 1: verbose */
					}
				}
				break;
			case CMD_TINFOI:
				{
					u_int ti;

					if (check_curnoddpart()){ /* not on DD partition level? */
						PRINTF_ERR("must be inside a DD partition\n");
						goto main_parser_next;
					}
					/* index */
					ti=(u_int)atoi(cmdtok[1]);
					if ((ti<1)||(ti>AKAI_DDTAKE_MAXNUM)){
						PRINTF_ERR("invalid take index\n");
						goto main_parser_next;
					}
					/* take */
					if (curpartp->head.dd.take[ti-1].stat==AKAI_DDTAKESTAT_FREE){ /* free? */
						PRINTF_ERR("take not found\n");
						goto main_parser_next;
					}
					/* print info */
					PRINTF_OUT("\n");
					curdir_info(0,NULL);
					PRINTF_OUT("/");
					if (akai_ddtake_info(curpartp,ti-1,1)<1){ /* 1: verbose */
						PRINTF_OUT("\n");
					}
				}
				break;
			case CMD_TINFOALL:
				{
					u_int ti;

					if (check_curnoddpart()){ /* not on DD partition level? */
						PRINTF_ERR("must be inside a DD partition\n");
						goto main_parser_next;
					}
					/* all DD takes in current DD partition */
					for (ti=0;ti<AKAI_DDTAKE_MAXNUM;ti++){
						/* take */
						if (curpartp->head.dd.take[ti].stat==AKAI_DDTAKESTAT_FREE){ /* free? */
							continue; /* next */
						}
						/* print info */
						PRINTF_OUT("\n");
						curdir_info(0,NULL);
						PRINTF_OUT("/");
						akai_ddtake_info(curpartp,ti,1); /* 1: verbose */
					}
				}
				break;
			case CMD_DEL:
				{
					struct file_s tmpfile;

					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
						PRINTF_ERR("directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("must be a file in a sampler volume\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						restore_curdir();
						goto main_parser_next;
					}
					if (strlen(dirnamebuf)==0){
						PRINTF_ERR("invalid file name\n");
						restore_curdir();
						goto main_parser_next;
					}
					/* find file in current directory */
					if (akai_find_file(curvolp,&tmpfile,dirnamebuf)<0){
						PRINTF_ERR("file not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					/* delete file */
					if (akai_delete_file(&tmpfile)<0){
						PRINTF_ERR("delete error\n");
					}
					restore_curdir();
				}
				break;
			case CMD_DELI:
				{
					struct file_s tmpfile;
					u_int fi;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("must be inside a volume\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					/* index */
					fi=(u_int)atoi(cmdtok[1]);
					if ((fi<1)||(fi>curvolp->fimax)){
						PRINTF_ERR("invalid file index\n");
						goto main_parser_next;
					}
					/* find file in current volume */
					if (akai_get_file(curvolp,&tmpfile,fi-1)<0){
						PRINTF_ERR("file not found\n");
						goto main_parser_next;
					}
					/* delete file */
					if (akai_delete_file(&tmpfile)<0){
						PRINTF_ERR("delete error\n");
					}
				}
				break;
			case CMD_TDELI:
				{
					u_int ti;

					if (check_curnoddpart()){ /* not on DD partition level? */
						PRINTF_ERR("must be inside a DD partition\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					/* index */
					ti=(u_int)atoi(cmdtok[1]);
					if ((ti<1)||(ti>AKAI_DDTAKE_MAXNUM)){
						PRINTF_ERR("invalid take index\n");
						goto main_parser_next;
					}
					/* delete take */
					if (akai_delete_ddtake(curpartp,ti-1)<0){
						PRINTF_ERR("cannot delete take\n");
					}
				}
				break;
			case CMD_REN:
			case CMD_RENI:
				{
					struct file_s tmpfile,dummyfile;
					struct vol_s tmpvol;
					u_int sfi,dfi;

					/* source */
					if (cmdnr==CMD_RENI){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("must be inside a volume\n");
							goto main_parser_next;
						}
						/* source file index */
						sfi=(u_int)atoi(cmdtok[1]);
						if ((sfi<1)||(sfi>curvolp->fimax)){
							PRINTF_ERR("invalid source file index\n");
							goto main_parser_next;
						}
						/* find file in current volume */
						/* must save current volume for tmpfile, since change_curdir below will possibly change volume */
						akai_copy_structvol(curvolp,&tmpvol);
						if (akai_get_file(&tmpvol,&tmpfile,sfi-1)<0){
							PRINTF_ERR("file not found\n");
							goto main_parser_next;
						}
					}else{
						save_curdir(0); /* 0: no modifications */
						if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
							PRINTF_ERR("source directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("source must be a file in a sampler volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (strlen(dirnamebuf)==0){
							PRINTF_ERR("invalid file name\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* find file in current directory */
						/* must save current volume for tmpfile */
						akai_copy_structvol(curvolp,&tmpvol);
						if (akai_find_file(&tmpvol,&tmpfile,dirnamebuf)<0){
							PRINTF_ERR("file not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						restore_curdir();
					}
					/* destination */
					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[2],0,dirnamebuf,0)<0){
						PRINTF_ERR("destination directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("destination must be inside a volume\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						restore_curdir();
						goto main_parser_next;
					}
					if (strlen(dirnamebuf)==0){ /* empty destination name? */
						/* take source name */
						strcpy(dirnamebuf,tmpfile.name);
					}
					if (cmdtoknr>=4){
						/* destination file index */
						dfi=(u_int)atoi(cmdtok[3]);
						if ((dfi<1)||(dfi>curvolp->fimax)){
							PRINTF_ERR("invalid destination file index\n");
							restore_curdir();
							goto main_parser_next;
						}
						dfi--;
						/* XXX akai_rename_file() below will check if index is free */
					}else{
						dfi=AKAI_CREATE_FILE_NOINDEX;
#if 1
						/* check if new file name already used */
						if (akai_find_file(curvolp,&dummyfile,dirnamebuf)==0){
							PRINTF_ERR("file name already used\n");
							restore_curdir();
							goto main_parser_next;
						}
#endif
					}
					/* rename file */
					if (akai_rename_file(&tmpfile,dirnamebuf,curvolp,dfi,NULL,tmpfile.osver)<0){
						PRINTF_ERR("cannot rename file\n");
					}
					/* fix name in header (if necessary) */
					akai_fixramname(&tmpfile); /* ignore error */
					restore_curdir();
				}
				break;
			case CMD_SETOSVERI:
			case CMD_SETUNCOMPRI:
			case CMD_UPDATEUNCOMPRI:
				{
					struct file_s tmpfile;
					u_int fi;
					u_int osver;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("must be inside a volume\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					/* index */
					fi=(u_int)atoi(cmdtok[1]);
					if ((fi<1)||(fi>curvolp->fimax)){
						PRINTF_ERR("invalid file index\n");
						goto main_parser_next;
					}
					/* find file in current volume */
					if (akai_get_file(curvolp,&tmpfile,fi-1)<0){
						PRINTF_ERR("file not found\n");
						goto main_parser_next;
					}
					if (cmdnr==CMD_SETOSVERI){
						if (curvolp->type==AKAI_VOL_TYPE_S900){
							PRINTF_ERR("must be an S1000/S3000 volume\n");
							goto main_parser_next;
						}
						/* OS version of file in S1000/S3000 volume */
						if (strstr(cmdtok[2],".")!=NULL){
							/* format: "XX.XX" */
							osver=(u_int)(100.0*atof(cmdtok[2]));
							osver=(((osver/100))<<8)|(osver%100);
						}else{
							/* integer format */
							osver=(u_int)atoi(cmdtok[2]);
						}
					}else{
						if (curvolp->type!=AKAI_VOL_TYPE_S900){
							PRINTF_ERR("must be an S900 volume\n");
							goto main_parser_next;
						}
						if (tmpfile.osver==0){
							PRINTF_ERR("not a compressed file\n");
							goto main_parser_next;
						}
						if (cmdtoknr>=3){
							/* given uncompr value in integer format */
							osver=(u_int)atoi(cmdtok[2]);
						}else{
							/* update uncompr value */
							if (akai_s900comprfile_updateuncompr(&tmpfile)<0){
								PRINTF_ERR("cannot set uncompr value\n");
							}
							goto main_parser_next; /* done */
						}
					}
					/* set osver of file */
					/* Note: akai_create_file() will correct osver if necessary */
					if (akai_rename_file(&tmpfile,NULL,curvolp,AKAI_CREATE_FILE_NOINDEX,NULL,osver)<0){
						if (cmdnr==CMD_SETOSVERI){
							PRINTF_ERR("cannot set OS version\n");
						}else{
							PRINTF_ERR("cannot set uncompr value\n");
						}
					}
				}
				break;
			case CMD_SETOSVERALL:
				{
					struct file_s tmpfile;
					u_int fi;
					u_int osver;
					u_int fcount;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("must be inside a volume\n");
						goto main_parser_next;
					}
					if (curvolp->type==AKAI_VOL_TYPE_S900){
						PRINTF_ERR("must be an S1000/S3000 volume\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					/* OS version of file in S1000/S3000 volume */
					if (strstr(cmdtok[2],".")!=NULL){
						/* format: "XX.XX" */
						osver=(u_int)(100.0*atof(cmdtok[2]));
						osver=(((osver/100))<<8)|(osver%100);
					}else{
						/* integer format */
						osver=(u_int)atoi(cmdtok[2]);
					}
					/* all files in current volume */
					fcount=0;
					for (fi=0;fi<curvolp->fimax;fi++){
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,fi)<0){
							continue; /* next */
						}
						PRINTF_OUT("updating \"%s\"\n",tmpfile.name);
						FLUSH_ALL;
						/* set osver of file */
						/* Note: akai_create_file() will correct osver if necessary */
						if (akai_rename_file(&tmpfile,NULL,curvolp,AKAI_CREATE_FILE_NOINDEX,NULL,osver)<0){
							PRINTF_ERR("cannot set OS version\n");
							continue; /* next */
						}
						fcount++;
					}
					FLUSH_ALL;
					PRINTF_OUT("updated %u file(s)\n",fcount);
				}
				break;
			case CMD_UPDATEUNCOMPRALL:
				{
					struct file_s tmpfile;
					u_int fi;
					u_int fcount;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("must be inside a volume\n");
						goto main_parser_next;
					}
					if (curvolp->type!=AKAI_VOL_TYPE_S900){
						PRINTF_ERR("must be an S900 volume\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					/* all files in current volume */
					fcount=0;
					for (fi=0;fi<curvolp->fimax;fi++){
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,fi)<0){
							continue; /* next */
						}
						if (tmpfile.osver!=0){ /* compressed file? */
							PRINTF_OUT("updating \"%s\"\n",tmpfile.name);
							FLUSH_ALL;
							/* update uncompr value */
							if (akai_s900comprfile_updateuncompr(&tmpfile)<0){
								PRINTF_ERR("cannot set uncompr value\n");
								continue; /* next */
							}
							fcount++;
						}
					}
					FLUSH_ALL;
					PRINTF_OUT("updated %u file(s)\n",fcount);
				}
				break;
			case CMD_SAMPLE900UNCOMPR:
			case CMD_SAMPLE900UNCOMPRI:
			case CMD_SAMPLE900COMPR:
			case CMD_SAMPLE900COMPRI:
				{
					struct file_s tmpfile;
					struct vol_s tmpvol;
					struct vol_s destvol;
					struct vol_s *volp;
					int destreadonly;
					u_int destindex;
					u_int fi;

					destreadonly=1; /* XXX */
					destindex=0; /* XXX */
					if ((cmdnr==CMD_SAMPLE900UNCOMPRI)||(cmdnr==CMD_SAMPLE900COMPRI)){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("must be inside a volume\n");
							goto main_parser_next;
						}
						destreadonly=curdiskp->readonly;
						destindex=curdiskp->index;
						/* index */
						fi=(u_int)atoi(cmdtok[1]);
						if ((fi<1)||(fi>curvolp->fimax)){
							PRINTF_ERR("invalid file index\n");
							goto main_parser_next;
						}
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,fi-1)<0){
							PRINTF_ERR("file not found\n");
							goto main_parser_next;
						}
					}else{
						save_curdir(0); /* 0: no modifications */
						if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
							PRINTF_ERR("directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("must be a file in a sampler volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						destreadonly=curdiskp->readonly;
						destindex=curdiskp->index;
						if (strlen(dirnamebuf)==0){
							PRINTF_ERR("invalid file name\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* find file in current directory */
						/* must save current volume for tmpfile */
						akai_copy_structvol(curvolp,&tmpvol);
						if (akai_find_file(&tmpvol,&tmpfile,dirnamebuf)<0){
							PRINTF_ERR("file not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						restore_curdir();
					}
					if (cmdtoknr>=3){
						save_curdir(0); /* 0: no modifications */
						/* given destination volume */
						if (change_curdir(cmdtok[2],0,NULL,1)<0){ /* NULL,1: check last */
							PRINTF_ERR("destination volume not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("destination must be a volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						destreadonly=curdiskp->readonly;
						destindex=curdiskp->index;
						/* must save current volume */
						akai_copy_structvol(curvolp,&destvol);
						volp=&destvol;
						restore_curdir();
					}else{
						/* replace file in same volume */
						volp=NULL;
					}
					if (destreadonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",destindex);
						goto main_parser_next;
					}
					save_curdir(1); /* 1: could be modifications */
					if ((cmdnr==CMD_SAMPLE900UNCOMPR)||(cmdnr==CMD_SAMPLE900UNCOMPRI)){
						PRINTF_OUT("uncompressing \"%s\"\n",tmpfile.name);
						FLUSH_ALL;
						/* uncompress S900 compressed sample file */
						akai_sample900_compr2noncompr(&tmpfile,volp); /* ignore error */
					}else{
						PRINTF_OUT("compressing \"%s\"\n",tmpfile.name);
						FLUSH_ALL;
						/* compress S900 non-compressed sample file */
						akai_sample900_noncompr2compr(&tmpfile,volp); /* ignore error */
					}
					restore_curdir();
				}
				break;
			case CMD_SAMPLE900UNCOMPRALL:
			case CMD_SAMPLE900COMPRALL:
				{
					struct file_s tmpfile;
					struct vol_s destvol;
					struct vol_s *volp;
					int destreadonly;
					u_int destindex;
					u_int fi;
					u_int fcount;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("must be inside a volume\n");
						goto main_parser_next;
					}
					destreadonly=curdiskp->readonly;
					destindex=curdiskp->index;
					if (cmdtoknr>=2){
						save_curdir(0); /* 0: no modifications */
						/* given destination volume */
						if (change_curdir(cmdtok[1],0,NULL,1)<0){ /* NULL,1: check last */
							PRINTF_ERR("destination volume not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("destination must be a volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						destreadonly=curdiskp->readonly;
						destindex=curdiskp->index;
						/* must save current volume */
						akai_copy_structvol(curvolp,&destvol);
						volp=&destvol;
						restore_curdir();
					}else{
						/* replace files in same volume */
						volp=NULL;
					}
					if (destreadonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",destindex);
						goto main_parser_next;
					}
					/* all files in current volume */
					save_curdir(1); /* 1: could be modifications */
					fcount=0;
					for (fi=0;fi<curvolp->fimax;fi++){
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,fi)<0){
							continue; /* next */
						}
						if (cmdnr==CMD_SAMPLE900UNCOMPRALL){
							if ((tmpfile.type!=AKAI_SAMPLE900_FTYPE)||(tmpfile.osver==0)){ /* not S900 compressed sample file? */
								continue; /* next */
							}
							PRINTF_OUT("uncompressing \"%s\"\n",tmpfile.name);
							FLUSH_ALL;
							/* uncompress S900 compressed sample file */
							if (akai_sample900_compr2noncompr(&tmpfile,volp)<0){
								continue; /* next */
							}
							fcount++;
						}else{
							if ((tmpfile.type!=AKAI_SAMPLE900_FTYPE)||(tmpfile.osver!=0)){ /* not S900 non-compressed sample file? */
								continue; /* next */
							}
							PRINTF_OUT("compressing \"%s\"\n",tmpfile.name);
							FLUSH_ALL;
							/* compress S900 non-compressed sample file */
							if (akai_sample900_noncompr2compr(&tmpfile,volp)<0){
								continue; /* next */
							}
							fcount++;
						}
					}
					restore_curdir();
					FLUSH_ALL;
					if (cmdnr==CMD_SAMPLE900UNCOMPRALL){
						PRINTF_OUT("uncompressed %u file(s)\n",fcount);
					}else{
						PRINTF_OUT("compressed %u file(s)\n",fcount);
					}
				}
				break;
			case CMD_FIXRAMNAME:
			case CMD_FIXRAMNAMEI:
				{
					struct file_s tmpfile;
					struct vol_s tmpvol;
					u_int fi;

					if (cmdnr==CMD_FIXRAMNAMEI){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("must be inside a volume\n");
							goto main_parser_next;
						}
						if (curdiskp->readonly){
							PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
							goto main_parser_next;
						}
						/* index */
						fi=(u_int)atoi(cmdtok[1]);
						if ((fi<1)||(fi>curvolp->fimax)){
							PRINTF_ERR("invalid file index\n");
							goto main_parser_next;
						}
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,fi-1)<0){
							PRINTF_ERR("file not found\n");
							goto main_parser_next;
						}
					}else{
						save_curdir(0); /* 0: no modifications */
						if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
							PRINTF_ERR("directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("must be a file in a sampler volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (curdiskp->readonly){
							PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
							restore_curdir();
							goto main_parser_next;
						}
						if (strlen(dirnamebuf)==0){
							PRINTF_ERR("invalid file name\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* find file in current directory */
						/* must save current volume for tmpfile */
						akai_copy_structvol(curvolp,&tmpvol);
						if (akai_find_file(&tmpvol,&tmpfile,dirnamebuf)<0){
							PRINTF_ERR("file not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						restore_curdir();
					}
					/* fix name in header (if necessary) */
					akai_fixramname(&tmpfile); /* ignore error */
				}
				break;
			case CMD_FIXRAMNAMEALL:
				{
					struct file_s tmpfile;
					u_int fi;
					u_int fcount;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("must be inside a volume\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					/* all files in current volume */
					fcount=0;
					for (fi=0;fi<curvolp->fimax;fi++){
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,fi)<0){
							continue; /* next */
						}
						PRINTF_OUT("fixing \"%s\"\n",tmpfile.name);
						FLUSH_ALL;
						/* fix name in header (if necessary) */
						if (akai_fixramname(&tmpfile)<0){
							continue; /* next */
						}
						fcount++;
					}
					FLUSH_ALL;
					PRINTF_OUT("fixed %u file(s)\n",fcount);
				}
				break;
			case CMD_CLRTAGI:
			case CMD_SETTAGI:
				{
					struct file_s tmpfile;
					u_int fi,ti;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("must be inside a volume\n");
						goto main_parser_next;
					}
					if (curvolp->type==AKAI_VOL_TYPE_S900){
						PRINTF_ERR("no tags for this volume type\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					/* file index */
					fi=(u_int)atoi(cmdtok[1]);
					if ((fi<1)||(fi>curvolp->fimax)){
						PRINTF_ERR("invalid file index\n");
						goto main_parser_next;
					}
					/* find file in current volume */
					if (akai_get_file(curvolp,&tmpfile,fi-1)<0){
						PRINTF_ERR("file not found\n");
						goto main_parser_next;
					}
					/* tag index */
					if ((cmdnr==CMD_CLRTAGI)&&(strcasecmp(cmdtok[2],"all")==0)){
						/* clear all */
						if (tmpfile.osver<=AKAI_OSVER_S1100MAX){ /* XXX */
							ti=AKAI_FILE_TAGS1000;
						}else{
							ti=AKAI_FILE_TAGFREE;
						}
					}else{
						ti=(u_int)atoi(cmdtok[2]);
						if ((ti<1)||(ti>AKAI_PARTHEAD_TAGNUM)){
							PRINTF_ERR("invalid tag index\n");
							goto main_parser_next;
						}
					}
					/* set/clear tag */
					if (cmdnr==CMD_SETTAGI){
						if (akai_set_filetag(tmpfile.tag,ti)<0){
							goto main_parser_next;
						}
					}else{
						if (akai_clear_filetag(tmpfile.tag,ti)<0){
							goto main_parser_next;
						}
					}
					/* update file */
					if (akai_rename_file(&tmpfile,NULL,NULL,AKAI_CREATE_FILE_NOINDEX,tmpfile.tag,tmpfile.osver)<0){
						PRINTF_ERR("cannot update file\n");
					}
				}
				break;
			case CMD_CLRTAGALL:
			case CMD_SETTAGALL:
				{
					struct file_s tmpfile;
					u_int fi,ti;
					u_int fcount;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("must be inside a volume\n");
						goto main_parser_next;
					}
					if (curvolp->type==AKAI_VOL_TYPE_S900){
						PRINTF_ERR("no tags for this volume type\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					/* tag index */
					if ((cmdnr==CMD_CLRTAGALL)&&(strcasecmp(cmdtok[1],"all")==0)){
						/* clear all */
						ti=AKAI_FILE_TAGFREE;
						/* Note: ti will be adjusted below if necessary */
					}else{
						ti=(u_int)atoi(cmdtok[1]);
						if ((ti<1)||(ti>AKAI_PARTHEAD_TAGNUM)){
							PRINTF_ERR("invalid tag index\n");
							goto main_parser_next;
						}
					}
					/* all files in current volume */
					fcount=0;
					for (fi=0;fi<curvolp->fimax;fi++){
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,fi)<0){
							continue; /* next */
						}
						/* adjust tag index if necessary */
						if ((cmdnr==CMD_CLRTAGALL)&&((ti==AKAI_FILE_TAGFREE)||(ti==AKAI_FILE_TAGS1000))){
							/* clear all */
							if (tmpfile.osver<=AKAI_OSVER_S1100MAX){ /* XXX */
								ti=AKAI_FILE_TAGS1000;
							}else{
								ti=AKAI_FILE_TAGFREE;
							}
						}
						PRINTF_OUT("updating \"%s\"\n",tmpfile.name);
						FLUSH_ALL;
						/* set/clear tag */
						if (cmdnr==CMD_SETTAGALL){
							if (akai_set_filetag(tmpfile.tag,ti)<0){
								PRINTF_ERR("cannot set tag\n");
								continue; /* next */
							}
						}else{
							if (akai_clear_filetag(tmpfile.tag,ti)<0){
								PRINTF_ERR("cannot clear tag\n");
								continue; /* next */
							}
						}
						/* update file */
						if (akai_rename_file(&tmpfile,NULL,NULL,AKAI_CREATE_FILE_NOINDEX,tmpfile.tag,tmpfile.osver)<0){
							PRINTF_ERR("cannot update file\n");
							continue; /* next */
						}
						fcount++;
					}
					FLUSH_ALL;
					PRINTF_OUT("updated %u file(s)\n",fcount);
				}
				break;
			case CMD_TRENI:
				{
					u_int ti;

					if (check_curnoddpart()){ /* not on DD partition level? */
						PRINTF_ERR("must be inside a DD partition\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					/* index */
					ti=(u_int)atoi(cmdtok[1]);
					if ((ti<1)||(ti>AKAI_DDTAKE_MAXNUM)){
						PRINTF_ERR("invalid take index\n");
						goto main_parser_next;
					}
					/* rename take */
					if (akai_rename_ddtake(curpartp,ti-1,cmdtok[2])<0){
						PRINTF_ERR("cannot rename take\n");
					}
				}
				break;
			case CMD_CLRFILTERTAG:
			case CMD_SETFILTERTAG:
				{
					u_int ti;

					/* tag index */
					if ((cmdnr==CMD_CLRFILTERTAG)&&(strcasecmp(cmdtok[1],"all")==0)){
						ti=AKAI_FILE_TAGFREE; /* clear all */
					}else{
						ti=(u_int)atoi(cmdtok[1]);
						if ((ti<1)||(ti>AKAI_PARTHEAD_TAGNUM)){
							PRINTF_ERR("invalid tag index\n");
							goto main_parser_next;
						}
					}
					/* set/clear tag */
					if (cmdnr==CMD_SETFILTERTAG){
						if (akai_set_filetag(curfiltertag,ti)<0){
							goto main_parser_next;
						}
					}else{
						if (akai_clear_filetag(curfiltertag,ti)<0){
							goto main_parser_next;
						}
					}
				}
				break;
			case CMD_COPY:
			case CMD_COPYI:
				{
					struct file_s tmpfile,dstfile;
					struct vol_s tmpvol;
					u_int sfi,dfi;

					/* source */
					if (cmdnr==CMD_COPYI){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("must be inside a volume\n");
							goto main_parser_next;
						}
						/* source file index */
						sfi=(u_int)atoi(cmdtok[1]);
						if ((sfi<1)||(sfi>curvolp->fimax)){
							PRINTF_ERR("invalid source file index\n");
							goto main_parser_next;
						}
						/* find file in current volume */
						/* must save current volume for tmpfile, since change_curdir below will possibly change volume */
						akai_copy_structvol(curvolp,&tmpvol);
						if (akai_get_file(&tmpvol,&tmpfile,sfi-1)<0){
							PRINTF_ERR("file not found\n");
							goto main_parser_next;
						}
					}else{
						save_curdir(0); /* 0: no modifications */
						if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
							PRINTF_ERR("source directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("source must be a file in a sampler volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (strlen(dirnamebuf)==0){
							PRINTF_ERR("invalid file name\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* find file in current directory */
						/* must save current volume for tmpfile */
						akai_copy_structvol(curvolp,&tmpvol);
						if (akai_find_file(&tmpvol,&tmpfile,dirnamebuf)<0){
							PRINTF_ERR("source file not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						restore_curdir();
					}
					/* destination */
					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[2],0,dirnamebuf,0)<0){
						PRINTF_ERR("destination directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("destination must be inside a volume\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						restore_curdir();
						goto main_parser_next;
					}
					if (strlen(dirnamebuf)==0){ /* empty destination name? */
						/* take source name */
						strcpy(dirnamebuf,tmpfile.name);
					}
					if (cmdtoknr>=4){
						/* destination file index */
						dfi=(u_int)atoi(cmdtok[3]);
						if ((dfi<1)||(dfi>curvolp->fimax)){
							PRINTF_ERR("invalid destination file index\n");
							restore_curdir();
							goto main_parser_next;
						}
						dfi--;
						/* XXX copy_file() below will check if index is free */
					}else{
						dfi=AKAI_CREATE_FILE_NOINDEX;
					}
					/* copy file */
					if (copy_file(&tmpfile,curvolp,&dstfile,dfi,dirnamebuf,1)<0){ /* 1: overwrite */
						PRINTF_ERR("cannot copy file\n");
					}
					/* fix name in header (if necessary) */
					akai_fixramname(&dstfile); /* ignore error */
					restore_curdir();
				}
				break;
			case CMD_COPYVOL:
			case CMD_COPYVOLI:
				{
					struct vol_s tmpvol;
					u_int vi;

					/* source */
					if (cmdnr==CMD_COPYVOLI){
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							PRINTF_ERR("must be on sampler partition level\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							PRINTF_ERR("invalid source volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							PRINTF_ERR("source directory not found\n");
							goto main_parser_next;
						}
					}else{
						save_curdir(0); /* 0: no modifications */
						if (change_curdir(cmdtok[1],0,NULL,1)<0){ /* NULL,1: check last */
							PRINTF_ERR("source directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("source must be a volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* must save current volume, since change_curdir below will possibly change volume */
						akai_copy_structvol(curvolp,&tmpvol);
						restore_curdir();
					}
					/* destination */
					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[2],0,dirnamebuf,0)<0){
						PRINTF_ERR("destination directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						PRINTF_ERR("destination must be a volume\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						restore_curdir();
						goto main_parser_next;
					}
					if (strlen(dirnamebuf)==0){ /* empty destination name? */
						/* take source name */
						strcpy(dirnamebuf,tmpvol.name);
					}
					if ((strcmp(dirnamebuf,".")==0)||(strcmp(dirnamebuf,"..")==0)){
						PRINTF_ERR("invalid destination volume name\n");
						restore_curdir();
						goto main_parser_next;
					}
					/* copy */
					if (copy_vol_allfiles(&tmpvol,curpartp,dirnamebuf,1,1)<0){ /* 1: overwrite, 1: verbose */
						PRINTF_ERR("copy error\n");
					}
					restore_curdir();
				}
				break;
			case CMD_COPYPART:
				{
					struct part_s *tmppartp;

					/* source */
					save_curdir(0); /* 0: no modifications */
					if (change_curdir(cmdtok[1],0,NULL,1)<0){ /* NULL,1: check last */
						PRINTF_ERR("source directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						PRINTF_ERR("source must be a sampler partition\n");
						restore_curdir();
						goto main_parser_next;
					}
					tmppartp=curpartp; /* source partition */
					restore_curdir();
					/* destination */
					/* Note: allow invalid destination partition!!! */
					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[2],0,NULL,0)<0){ /* NULL,0: don't check last */
						PRINTF_ERR("destination directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						PRINTF_ERR("destination must be a sampler partition\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						restore_curdir();
						goto main_parser_next;
					}
					/* copy */
					if (copy_part_allvols(tmppartp,curpartp,1,2)<0){ /* 1: overwrite, 2: verbose */
						PRINTF_ERR("copy error\n");
					}
					restore_curdir();
				}
				break;
			case CMD_COPYTAGS:
				{
					struct part_s *tmppartp;

					/* source */
					save_curdir(0); /* 0: no modifications */
					if (change_curdir(cmdtok[1],0,NULL,1)<0){ /* NULL,1: check last */
						PRINTF_ERR("source directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						PRINTF_ERR("source must be a sampler partition\n");
						restore_curdir();
						goto main_parser_next;
					}
					tmppartp=curpartp; /* source partition */
					restore_curdir();
					/* destination */
					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[2],0,NULL,1)<0){ /* NULL,1: check last */
						PRINTF_ERR("destination directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						PRINTF_ERR("destination must be a sampler partition\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						restore_curdir();
						goto main_parser_next;
					}
					/* copy */
					if (copy_tags(tmppartp,curpartp)<0){
						PRINTF_ERR("copy error\n");
					}
					restore_curdir();
				}
				break;
			case CMD_WIPEVOL:
			case CMD_DELVOL:
				{

					/* Note: allow invalid volume to be deleted!!! */
					/* Note: might delete original current volume!!! */
					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[1],0,NULL,0)<0){ /* NULL,0: don't check last */
						PRINTF_ERR("directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("must be a volume\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						restore_curdir();
						goto main_parser_next;
					}
					/* wipe or delete volume */
					if (akai_wipe_vol(curvolp,(cmdnr==CMD_DELVOL)?1:0)<0){
						PRINTF_ERR("delete error\n");
					}
					restore_curdir();
				}
				break;
			case CMD_WIPEVOLI:
			case CMD_DELVOLI:
				{
					struct vol_s tmpvol;
					u_int vi;

					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						PRINTF_ERR("must be on sampler partition level\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					/* index */
					vi=(u_int)atoi(cmdtok[1]);
					if ((vi<1)||(vi>curpartp->volnummax)){
						PRINTF_ERR("invalid volume index\n");
						goto main_parser_next;
					}
					/* find volume in current partition */
					if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
						PRINTF_ERR("volume not found\n");
						goto main_parser_next;
					}
					/* Note: volume may be corrupt */
					/* wipe or delete volume */
					if (akai_wipe_vol(&tmpvol,(cmdnr==CMD_DELVOLI)?1:0)<0){
						PRINTF_ERR("delete error\n");
					}
				}
				break;
			case CMD_FORMATFLOPPYL9:
			case CMD_FORMATFLOPPYL1:
			case CMD_FORMATFLOPPYL3:
			case CMD_FORMATFLOPPYH9:
			case CMD_FORMATFLOPPYH1:
			case CMD_FORMATFLOPPYH3:
				{
					int ret;
					int lodensflag,s3000flag,s900flag;

					if (curdiskp==NULL){
						PRINTF_ERR("must be on a disk\n");
						goto main_parser_next;
					}
					if ((curdiskp->type!=DISK_TYPE_FLL)&&(curdiskp->type!=DISK_TYPE_FLH)){ /* not a floppy? */
						PRINTF_ERR("must be a floppy\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					if (blk_cache_enable){ /* cache enabled? */
						flush_blk_cache(); /* XXX if error, maybe next time more luck */
					}

					switch (cmdnr){
					case CMD_FORMATFLOPPYL9:
						lodensflag=1;
						s3000flag=0;
						s900flag=1;
						break;
					case CMD_FORMATFLOPPYL1:
						lodensflag=1;
						s3000flag=0;
						s900flag=0;
						break;
					case CMD_FORMATFLOPPYL3:
						lodensflag=1;
						s3000flag=1;
						s900flag=0;
						break;
					case CMD_FORMATFLOPPYH9:
						lodensflag=0;
						s3000flag=0;
						s900flag=1;
						break;
					case CMD_FORMATFLOPPYH1:
						lodensflag=0;
						s3000flag=0;
						s900flag=0;
						break;
					case CMD_FORMATFLOPPYH3:
					default:
						lodensflag=0;
						s3000flag=1;
						s900flag=0;
						break;
					}

#ifdef _VISUALCPP
					if (curdiskp->fldrn>=0){ /* is floppy drive? */
						if ((lodensflag&&(curdiskp->type==DISK_TYPE_FLH))
							||((!lodensflag)&&(curdiskp->type==DISK_TYPE_FLL))){ /* incompatible? */
							PRINTF_ERR("disk%u: chosen filesystem is incompatible with floppy type\n",curdiskp->index);
							goto main_parser_next;
						}
						/* low-level format */
						PRINTF_OUT("\nlow-level formatting disk%u as %s floppy\n",
							curdiskp->index,lodensflag?"low-density":"high-density");
						FLUSH_ALL;
						if (fldr_format(curdiskp->fldrn)<0){
#if 0 /* XXX ignore error, continue */
							PRINTF_ERR("format error\n");
							FLUSH_ALL;
							PRINTF_OUT("\nrestarting program\n\n");
							restartflag=1;
							goto main_restart;
#endif
						}
					}else
#endif /* _VISUALCPP */
					{
						u_char fbuf[AKAI_FL_BLOCKSIZE];
						u_int i;

						/* erase whole disk */
						PRINTF_OUT("\nerasing disk%u\n",curdiskp->index);
						FLUSH_ALL;
						bzero(fbuf,AKAI_FL_BLOCKSIZE);
						for (i=0;i<curdiskp->bsize;i++){
							PRINTF_OUT("\rblock 0x%04x",i);
							FLUSH_ALL;
							/* Note: checked for floppy drive above -> not a floppy drive here */
							/*       -> no need to check if floppy inserted here */
							/* write block */
							if (io_blks(curdiskp->fd,
#ifdef _VISUALCPP
										curdiskp->fldrn,
#endif /* _VISUALCPP */
										curdiskp->startoff,
										fbuf,
										i,
										1,
										curdiskp->blksize,
										0,IO_BLKS_WRITE)<0){ /* 0: don't alloc cache */
								PRINTF_OUT(" - cannot write block\n");
								FLUSH_ALL;
#if 0 /* XXX ignore error, continue */
								PRINTF_ERR("format error\n");
								FLUSH_ALL;
								PRINTF_OUT("\nrestarting program\n\n");
								restartflag=1;
								goto main_restart;
#endif
							}
						}
						PRINTF_OUT("\rdone        \n");
					}

					/* create filesystem */
					PRINTF_OUT("\ncreating filesystem on disk%u\n",curdiskp->index);
					FLUSH_ALL;
					ret=akai_wipe_floppy(curdiskp,lodensflag,s3000flag,s900flag);
					/* Note: blksize might have changed now!!! */
					if (ret!=0){
						PRINTF_ERR("format error\n");
						FLUSH_ALL;
					}
					if (ret<0){ /* non-fatal? */
						goto main_parser_next;
					}
					/* must exit (or restart) now!!! */
					if (ret>0){ /* fatal? */
						mainret=1; /* error */
						goto main_exit;
					}
					FLUSH_ALL;

#if 1
					/* scan for bad blocks/clusters */
					PRINTF_OUT("\nscanning for bad blocks\n");
					/* Note: akai_wipe_floppy() above has initialized part[0] */
					if (akai_scanbad(&part[0],1)<0){ /* 1: mark bad blocks */
						PRINTF_OUT("invalid partition\n");
					}else{
						PRINTF_OUT("\rdone                           \n");
					}
#endif

					PRINTF_OUT("\nrestarting program\n\n");
					restartflag=1;
					goto main_restart;
				}
				break;
			case CMD_WIPEFLOPPY:
				{
					int ret;

					if (check_curnosamplervol() /* not inside a sampler volume? */
						||((curdiskp->type!=DISK_TYPE_FLL)&&(curdiskp->type!=DISK_TYPE_FLH))){ /* or not a floppy? */
						PRINTF_ERR("must be inside a floppy volume\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					if (blk_cache_enable){ /* cache enabled? */
						flush_blk_cache(); /* XXX if error, maybe next time more luck */
					}

					/* create filesystem */
					PRINTF_OUT("\ncreating filesystem on disk%u\n",curdiskp->index);
					FLUSH_ALL;
					ret=akai_wipe_floppy(curdiskp,
							curpartp->type==PART_TYPE_FLL,
							(curvolp->type==AKAI_VOL_TYPE_S3000)||(curvolp->type==AKAI_VOL_TYPE_CD3000),
							(curvolp->type==AKAI_VOL_TYPE_S900));
					/* Note: blksize might have changed now!!! */
					if (ret!=0){
						PRINTF_ERR("format error\n");
						FLUSH_ALL;
					}
					if (ret<0){ /* non-fatal? */
						goto main_parser_next;
					}
					/* must exit (or restart) now!!! */
					if (ret>0){ /* fatal? */
						mainret=1; /* error */
						goto main_exit;
					}
					FLUSH_ALL;

					PRINTF_OUT("\nrestarting program\n\n");
					restartflag=1;
					goto main_restart;
				}
				break;
			case CMD_FORMATHARDDISK9:
				{
					u_int l;
					u_int totb;
					int ret;

					if (curdiskp==NULL){
						PRINTF_ERR("must be on a disk\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
#ifdef _VISUALCPP
					if (curdiskp->fldrn>=0){
						PRINTF_ERR("disk%u: chosen filesystem is incompatible with floppy\n",curdiskp->index);
						goto main_parser_next;
					}
#endif /* _VISUALCPP */
					if (blk_cache_enable){ /* cache enabled? */
						flush_blk_cache(); /* XXX if error, maybe next time more luck */
					}

					/* total size to format */
					if (cmdtoknr>=2){
						if (strcasecmp(cmdtok[1],"SH205")==0){
							totb=AKAI_HD9_SH205SIZE;
						}else if (strcasecmp(cmdtok[1],"SUPRA20M")==0){
							totb=AKAI_HD9_SUPRA20MSIZE;
						}else if (strcasecmp(cmdtok[1],"SUPRA30M")==0){
							totb=AKAI_HD9_SUPRA30MSIZE;
						}else if (strcasecmp(cmdtok[1],"SUPRA60M")==0){
							totb=AKAI_HD9_SUPRA60MSIZE;
						}else{
							l=(u_int)strlen(cmdtok[1]);
							if ((l>=1)&&((cmdtok[1][l-1]=='M')||(cmdtok[1][l-1]=='m'))){ /* in MB? */
								cmdtok[1][l-1]='\0'; /* XXX remove letter */
								l=(1024*1024)/AKAI_HD_BLOCKSIZE; /* MB */
							}else{
								l=1; /* block */
							}
							totb=l*(u_int)atoi(cmdtok[1]);
						}
					}else{
						totb=AKAI_HD9_MAXSIZE; /* maximum */
					}
					/* Note: akai_wipe_harddisk9() will truncate totb if required */

					/* create filesystem */
					PRINTF_OUT("\ncreating filesystem on disk%u\n",curdiskp->index);
					FLUSH_ALL;
					ret=akai_wipe_harddisk9(curdiskp,totb);
					/* Note: blksize might have changed now!!! */
					if (ret!=0){
						PRINTF_ERR("format error\n");
						FLUSH_ALL;
					}
					if (ret<0){ /* non-fatal? */
						goto main_parser_next;
					}
					/* must exit (or restart) now!!! */
					if (ret>0){ /* fatal? */
						mainret=1; /* error */
						goto main_exit;
					}
					FLUSH_ALL;
					PRINTF_OUT("\nrestarting program\n\n");
					restartflag=1;
					goto main_restart;
				}
				break;
			case CMD_FORMATHARDDISK1:
			case CMD_FORMATHARDDISK3:
			case CMD_FORMATHARDDISK3CD:
				{
					u_int l,bsize,totb;
					int s3000flag;
					int cdromflag;
					int ret;

					if (curdiskp==NULL){
						PRINTF_ERR("must be on a disk\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
#ifdef _VISUALCPP
					if (curdiskp->fldrn>=0){
						PRINTF_ERR("disk%u: chosen filesystem is incompatible with floppy\n",curdiskp->index);
						goto main_parser_next;
					}
#endif /* _VISUALCPP */
					if (blk_cache_enable){ /* cache enabled? */
						flush_blk_cache(); /* XXX if error, maybe next time more luck */
					}

					/* partition size */
					if (cmdtoknr>=2){
						l=(u_int)strlen(cmdtok[1]);
						if ((l>=1)&&((cmdtok[1][l-1]=='M')||(cmdtok[1][l-1]=='m'))){ /* in MB? */
							cmdtok[1][l-1]='\0'; /* XXX remove letter */
							l=(1024*1024)/AKAI_HD_BLOCKSIZE; /* MB */
						}else{
							l=1; /* block */
						}
						bsize=l*(u_int)atoi(cmdtok[1]);
					}else{
						bsize=AKAI_PART_MAXSIZE; /* maximum */
					}
					/* Note: akai_wipe_harddisk() will truncate bsize if required */
					/* total size to format */
					if (cmdtoknr>=3){
						l=(u_int)strlen(cmdtok[2]);
						if ((l>=1)&&((cmdtok[2][l-1]=='M')||(cmdtok[2][l-1]=='m'))){ /* in MB? */
							cmdtok[2][l-1]='\0'; /* XXX remove letter */
							l=(1024*1024)/AKAI_HD_BLOCKSIZE; /* MB */
						}else{
							l=1; /* block */
						}
						totb=l*(u_int)atoi(cmdtok[2]);
					}else{
						totb=AKAI_HD_MAXSIZE; /* maximum */
					}
					/* Note: akai_wipe_harddisk() will truncate totb if required */
					if (cmdnr==CMD_FORMATHARDDISK3){
						s3000flag=1;
						cdromflag=0;
					}else if (cmdnr==CMD_FORMATHARDDISK3CD){
						s3000flag=1;
						cdromflag=1;
					}else{
						s3000flag=0;
						cdromflag=0;
					}
					/* create filesystem */
					PRINTF_OUT("\ncreating filesystem on disk%u\n",curdiskp->index);
					FLUSH_ALL;
					ret=akai_wipe_harddisk(curdiskp,bsize,totb,s3000flag,cdromflag);
					/* Note: blksize might have changed now!!! */
					if (ret!=0){
						PRINTF_ERR("format error\n");
						FLUSH_ALL;
					}
					if (ret<0){ /* non-fatal? */
						goto main_parser_next;
					}
					/* must exit (or restart) now!!! */
					if (ret>0){ /* fatal? */
						mainret=1; /* error */
						goto main_exit;
					}
					FLUSH_ALL;
					PRINTF_OUT("\nrestarting program\n\n");
					restartflag=1;
					goto main_restart;
				}
				break;
			case CMD_WIPEPART:
			case CMD_WIPEPART3CD:
			case CMD_FIXPART:
				{
					int wipeflag;
					int cdromflag;

					/* Note: allow invalid partition to be fixed/wiped!!! */
					save_curdir(1); /* 1: could be modifications */
					if (cmdtoknr>=2){
						if (change_curdir(cmdtok[1],0,NULL,0)<0){ /* NULL,0: don't check last */
							PRINTF_ERR("directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnopart()){ /* not on partition level (sampler or DD)? */
							PRINTF_ERR("must be a partition\n");
							restore_curdir();
							goto main_parser_next;
						}
					}else{
						if ((curdiskp==NULL)||(curpartp==NULL)){
							PRINTF_ERR("must be inside a partition\n");
							restore_curdir();
							goto main_parser_next;
						}
					}
					if ((curpartp->type==PART_TYPE_FLL)||(curpartp->type==PART_TYPE_FLH)){ /* floppy? */
						PRINTF_ERR("must not be a floppy\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						restore_curdir();
						goto main_parser_next;
					}
					if (cmdnr==CMD_WIPEPART){
						wipeflag=1;
						cdromflag=0;
					}else if (cmdnr==CMD_WIPEPART3CD){
						wipeflag=1;
						cdromflag=1;
					}else{
						wipeflag=0;
						cdromflag=0;
					}

					/* create or fix filesystem, supply part[] to fix partition table if necessary */
					PRINTF_OUT("\n%s filesystem in disk%u partition %c\n",
								wipeflag?"creating":"fixing",curdiskp->index,curpartp->letter);
					if (akai_wipe_part(curpartp,wipeflag,&part[0],part_num,cdromflag)<0){
						PRINTF_ERR("failed\n");
					}
					if (cmdnr==CMD_WIPEPART3CD){
						/* XXX init tags (checked by cdinfo) */
						akai_rename_tag(curpartp,NULL,0,1); /* 1: wipe */
					}
					restore_curdir();
				}
				break;
			case CMD_FIXHARDDISK:
				{
					u_int pi;

					/* Note: allow invalid disk to be fixed!!! */
					save_curdir(1); /* 1: could be modifications */
					if (cmdtoknr>=2){
						if (change_curdir(cmdtok[1],0,NULL,0)<0){ /* NULL,0: don't check last */
							PRINTF_ERR("directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
					}
					if (curdiskp==NULL){
						PRINTF_ERR("must be on a disk\n");
						restore_curdir();
						goto main_parser_next;
					}
					if ((curdiskp->type==DISK_TYPE_FLL)||(curdiskp->type==DISK_TYPE_FLH)){ /* floppy? */
						PRINTF_ERR("must not be a floppy\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						restore_curdir();
						goto main_parser_next;
					}

					/* fix filesystem */
					PRINTF_OUT("\nfixing filesystem on disk%u\n",curdiskp->index);
					/* scan partitions */
					for (pi=0;pi<part_num;pi++){
						if (part[pi].diskp!=curdiskp){ /* not on same disk? */
							continue; /* next partition */
						}
						if (part[pi].type==PART_TYPE_DD){
							continue; /* next partition */
						}
						/* Note: allow invalid partitions to be fixed */
						/* fix partition, supply part[] to fix partition table if necessary */
						if (akai_wipe_part(&part[pi],0,&part[0],part_num,0)<0){ /* 0: fix only */
							PRINTF_ERR("cannot fix filesystem in partition %c\n",part[pi].letter);
							/* XXX ignore, continue with others */
						}
					}
					restore_curdir();
				}
				break;
			case CMD_SCANBADBLKSPART:
			case CMD_MARKBADBLKSPART:
				{
					save_curdir(1); /* 1: could be modifications */
					if (cmdtoknr>=2){
						if (change_curdir(cmdtok[1],0,NULL,0)<0){ /* NULL,0: don't check last */
							PRINTF_ERR("directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnopart()){ /* not on partition level (sampler or DD)? */
							PRINTF_ERR("must be a partition\n");
							restore_curdir();
							goto main_parser_next;
						}
					}else{
						if ((curdiskp==NULL)||(curpartp==NULL)){
							PRINTF_ERR("must be inside a partition\n");
							restore_curdir();
							goto main_parser_next;
						}
					}
					if ((cmdnr==CMD_MARKBADBLKSPART)&&(curdiskp->readonly)){
						PRINTF_ERR("disk%u: read-only, cannot mark bad blocks/clusters\n",curdiskp->index);
						restore_curdir();
						goto main_parser_next;
					}
					/* scan for bad blocks/clusters */
					if (curpartp->type==PART_TYPE_DD){
						PRINTF_OUT("\nscanning for bad clusters\n");
					}else{
						PRINTF_OUT("\nscanning for bad blocks\n");
					}
					if (akai_scanbad(curpartp,cmdnr==CMD_MARKBADBLKSPART)<0){
						PRINTF_OUT("invalid partition\n");
					}else{
						PRINTF_OUT("\rdone                           \n");
					}
					restore_curdir();
				}
				break;
			case CMD_SCANBADBLKSDISK:
			case CMD_MARKBADBLKSDISK:
				{
					u_int pi,pnr;

					save_curdir(1); /* 1: could be modifications */
					if (cmdtoknr>=2){
						if (change_curdir(cmdtok[1],0,NULL,0)<0){ /* NULL,0: don't check last */
							PRINTF_ERR("directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
					}
					if (curdiskp==NULL){
						PRINTF_ERR("must be on a disk\n");
						restore_curdir();
						goto main_parser_next;
					}
					if ((cmdnr==CMD_MARKBADBLKSDISK)&&(curdiskp->readonly)){
						PRINTF_ERR("disk%u: read-only, cannot mark bad blocks/clusters\n",curdiskp->index);
						restore_curdir();
						goto main_parser_next;
					}
					for (pi=0,pnr=0;pi<part_num;pi++){
						if (part[pi].diskp!=curdiskp){ /* not on same disk? */
							continue; /* next partition */
						}
						/* scan for bad blocks/clusters */
						if (part[pi].type==PART_TYPE_DD){
							PRINTF_OUT("\nscanning for bad clusters in /disk%u/DD\n",curdiskp->index);
							if (part[pi].letter!=0){
								PRINTF_OUT("%u",(u_int)part[pi].letter);
							}
						}else{
							PRINTF_OUT("\nscanning for bad blocks in /disk%u/%c\n",curdiskp->index,part[pi].letter);
						}
						if (akai_scanbad(&part[pi],cmdnr==CMD_MARKBADBLKSDISK)<0){
							PRINTF_OUT("invalid partition\n");
						}else{
							PRINTF_OUT("\rdone                           \n");
						}
						pnr++;
					}
					if (pnr==0){
						PRINTF_OUT("\ndisk%u: no partitions\n",curdiskp->index);
					}
					restore_curdir();
				}
				break;
			case CMD_GETDISK:
				{
					int outfd;
					u_int blk;
					u_char fbuf[AKAI_HD_BLOCKSIZE];

					save_curdir(1); /* 1: could be modifications */
					if (curdiskp==NULL){
						PRINTF_ERR("must be on a disk\n");
						restore_curdir();
						goto main_parser_next;
					}
					/* create external file */
					if ((outfd=OPEN(cmdtok[1],O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
						PERROR("open");
						restore_curdir();
						goto main_parser_next;
					}
					/* export */
					PRINTF_OUT("\n");
					/* Note: possible error in akai_io_blks() below is ignored */
					/*       -> should remove current content of buffers first */
					bzero(fbuf,curdiskp->blksize);
#ifdef _VISUALCPP
					if (curdiskp->fldrn>=0){ /* is floppy drive? */
						bzero(fldr_secbuf,AKAI_FL_SECSIZE);
					}
#endif /* _VISUALCPP */
					for (blk=0;blk<curdiskp->bsize;blk++){
						print_progressbar(curdiskp->bsize,blk);
#ifdef _VISUALCPP
						/* check if floppy drive but no floppy inserted */
						if ((curdiskp->fldrn>=0)&&(fldr_checkfloppyinserted(curdiskp->fldrn)!=0)){
							PRINTF_ERR("error: no floppy inserted\n\n");
							break;
						}
#endif /* _VISUALCPP */
						/* read block */
						if (io_blks(curdiskp->fd,
#ifdef _VISUALCPP
									curdiskp->fldrn,
#endif /* _VISUALCPP */
									curdiskp->startoff,
									fbuf,
									blk,
									1,
									curdiskp->blksize,
									0,IO_BLKS_READ)<0){ /* 0: don't alloc cache */
#if 1
							/* Note: error -> fbuf could contain leftover */
							PRINTF_ERR("\nerror in block 0x%08x\n\n",blk);
							FLUSH_ALL;
							print_progressbar(curdiskp->bsize,0); /* 0: draw scale again */
							if (blk>0){
								print_progressbar(curdiskp->bsize,blk); /* draw dots again */
							}
							/* XXX ignore error, write current fbuf */
#else
							break;
#endif
						}
						/* write block */
						if (WRITE(outfd,fbuf,curdiskp->blksize)!=(int)curdiskp->blksize){
							PERROR("write");
							break;
						}
						if ((blk+1)>=curdiskp->bsize){ /* end? */
							PRINTF_OUT("."); /* dot for 100% */
						}
					}
					PRINTF_OUT("\n\n");
					CLOSE(outfd);
					restore_curdir();
				}
				break;
			case CMD_PUTDISK:
				{
					int ret;
					int inpfd;
					struct stat instat;
					u_int size,bsize;
					u_int blk;
					u_char fbuf[AKAI_HD_BLOCKSIZE];

					if (blk_cache_enable){ /* cache enabled? */
						flush_blk_cache(); /* XXX if error, maybe next time more luck */
					}

					save_curdir(1); /* 1: could be modifications */
					if (curdiskp==NULL){
						PRINTF_ERR("must be on a disk\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						restore_curdir();
						goto main_parser_next;
					}
					/* open external file */
					if ((inpfd=akai_openreadonly_extfile(cmdtok[1]))<0){
						PERROR("open");
						restore_curdir();
						goto main_parser_next;
					}
					/* get file size */
					if (fstat(inpfd,&instat)<0){
						PERROR("stat");
						CLOSE(inpfd);
						restore_curdir();
						goto main_parser_next;
					}
					size=(u_int)instat.st_size; /* size in bytes */
					/* check blocksize */
					if (curdiskp->blksize==0){
						if ((curdiskp->type==DISK_TYPE_HD9)||(curdiskp->type==DISK_TYPE_HD)){
							curdiskp->blksize=AKAI_HD_BLOCKSIZE; /* XXX */
						}else{
							curdiskp->blksize=AKAI_FL_BLOCKSIZE; /* XXX */
						}
					}
					bsize=size/curdiskp->blksize; /* size in blocks, round down (blksize>0 see above) */
					/* check bsize */
					if (bsize>curdiskp->bsize){
						bsize=curdiskp->bsize;
						PRINTF_ERR("truncated to disk size\n");
						FLUSH_ALL;
					}
					curdiskp->bsize=bsize; /* new partition size */
					/* import */
					ret=0;
					PRINTF_OUT("\n");
					for (blk=0;blk<curdiskp->bsize;blk++){
						print_progressbar(curdiskp->bsize,blk);
						/* read block */
						if (READ(inpfd,fbuf,curdiskp->blksize)!=(int)curdiskp->blksize){
							PERROR("read");
							ret=1;
							break;
						}
						/* write block */
						if (io_blks(curdiskp->fd,
#ifdef _VISUALCPP
									curdiskp->fldrn,
#endif /* _VISUALCPP */
									curdiskp->startoff,
									fbuf,
									blk,
									1,
									curdiskp->blksize,
									0,IO_BLKS_WRITE)<0){ /* 0: don't alloc cache */
							ret=1;
							break;
						}
						if ((blk+1)>=curdiskp->bsize){ /* end? */
							PRINTF_OUT("."); /* dot for 100% */
						}
					}
					PRINTF_OUT("\n\n");
					CLOSE(inpfd);

					/* must exit (or restart) now!!! */
					if (ret>0){ /* fatal? */
						mainret=1; /* error */
						goto main_exit;
					}
					FLUSH_ALL;
					PRINTF_OUT("\nrestarting program\n\n");
					restartflag=1;
					goto main_restart;
				}
				break;
			case CMD_GETPART:
				{
					int outfd;
					u_int blk;
					u_char fbuf[AKAI_HD_BLOCKSIZE];
					char *fname;

					/* Note: allow invalid partition to be exported!!! */
					save_curdir(1); /* 1: could be modifications */
					if (cmdtoknr>=3){
						if (change_curdir(cmdtok[1],0,NULL,0)<0){ /* NULL,0: don't check last */
							PRINTF_ERR("directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnopart()){ /* not on partition level (sampler or DD)? */
							PRINTF_ERR("must be a partition\n");
							restore_curdir();
							goto main_parser_next;
						}
					}else{
						if ((curdiskp==NULL)||(curpartp==NULL)){
							PRINTF_ERR("must be inside a partition\n");
							restore_curdir();
							goto main_parser_next;
						}
					}
					/* create external file */
					if (cmdtoknr>=3){
						fname=cmdtok[2];
					}else{
						fname=cmdtok[1];
					}
					if ((outfd=OPEN(fname,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
						PERROR("open");
						restore_curdir();
						goto main_parser_next;
					}
					/* export */
					PRINTF_OUT("\n");
					/* Note: possible error in akai_io_blks() below is ignored */
					/*       -> should remove current content of buffers first */
					bzero(fbuf,curdiskp->blksize);
#ifdef _VISUALCPP
					if (curdiskp->fldrn>=0){ /* is floppy drive? */
						bzero(fldr_secbuf,AKAI_FL_SECSIZE);
					}
#endif /* _VISUALCPP */
					for (blk=0;blk<curpartp->bsize;blk++){
						print_progressbar(curpartp->bsize,blk);
#ifdef _VISUALCPP
						/* check if floppy drive but no floppy inserted */
						if ((curdiskp->fldrn>=0)&&(fldr_checkfloppyinserted(curdiskp->fldrn)!=0)){
							PRINTF_ERR("error: no floppy inserted\n\n");
							break;
						}
#endif /* _VISUALCPP */
						/* read block */
						if (akai_io_blks(curpartp,fbuf,
										 blk,
										 1,
										 0,IO_BLKS_READ)<0){ /* 0: don't alloc cache */
#if 1
							/* Note: error -> fbuf could contain leftover */
							PRINTF_ERR("\nerror in block 0x%08x\n\n",blk);
							FLUSH_ALL;
							print_progressbar(curpartp->bsize,0); /* 0: draw scale again */
							if (blk>0){
								print_progressbar(curpartp->bsize,blk); /* draw dots again */
							}
							/* XXX ignore error, write current fbuf */
#else
							break;
#endif
						}
						/* write block */
						if (WRITE(outfd,fbuf,curpartp->blksize)!=(int)curpartp->blksize){
							PERROR("write");
							break;
						}
						if ((blk+1)>=curpartp->bsize){ /* end? */
							PRINTF_OUT("."); /* dot for 100% */
						}
					}
					PRINTF_OUT("\n\n");
					CLOSE(outfd);
					restore_curdir();
				}
				break;
			case CMD_PUTPART:
				{
					int ret;
					int inpfd;
					struct stat instat;
					u_int size,bsize;
					u_int blk;
					u_char fbuf[AKAI_HD_BLOCKSIZE];
					struct akai_parthead_s tmppart;

					if (blk_cache_enable){ /* cache enabled? */
						flush_blk_cache(); /* XXX if error, maybe next time more luck */
					}

					/* Note: allow import to invalid partition!!! */
					save_curdir(1); /* 1: could be modifications */
					if (cmdtoknr>=3){
						if (change_curdir(cmdtok[2],0,NULL,0)<0){ /* NULL,0: don't check last */
							PRINTF_ERR("directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnopart()){ /* not on partition level (sampler or DD)? */
							PRINTF_ERR("must be a partition\n");
							restore_curdir();
							goto main_parser_next;
						}
					}else{
						if ((curdiskp==NULL)||(curpartp==NULL)){
							PRINTF_ERR("must be inside a partition\n");
							restore_curdir();
							goto main_parser_next;
						}
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						restore_curdir();
						goto main_parser_next;
					}
					if (curpartp->bstart>curdiskp->bsize){
						PRINTF_ERR("invalid partition start\n");
						restore_curdir();
						goto main_parser_next;
					}
					/* open external file */
					if ((inpfd=akai_openreadonly_extfile(cmdtok[1]))<0){
						PERROR("open");
						restore_curdir();
						goto main_parser_next;
					}
					/* get file size */
					if (fstat(inpfd,&instat)<0){
						PERROR("stat");
						CLOSE(inpfd);
						restore_curdir();
						goto main_parser_next;
					}
					size=(u_int)instat.st_size; /* size in bytes */
					/* check blocksize */
					if (curpartp->blksize==0){
						if ((curdiskp->type==DISK_TYPE_HD9)||(curdiskp->type==DISK_TYPE_HD)){
							curdiskp->blksize=AKAI_HD_BLOCKSIZE; /* XXX */
						}else{
							curdiskp->blksize=AKAI_FL_BLOCKSIZE; /* XXX */
						}
						curpartp->blksize=curdiskp->blksize;
					}
					bsize=size/curpartp->blksize; /* size in blocks, round down (blksize>0 see above) */
					/* check bsize */
					if ((curdiskp->type==DISK_TYPE_FLL)&&(bsize>AKAI_FLL_SIZE)){
						bsize=AKAI_FLL_SIZE;
						PRINTF_ERR("truncated to max. partition size\n");
					}else if ((curdiskp->type==DISK_TYPE_FLH)&&(bsize>AKAI_FLH_SIZE)){
						bsize=AKAI_FLH_SIZE;
						PRINTF_ERR("truncated to max. partition size\n");
					}else if ((curdiskp->type==DISK_TYPE_HD9)&&(bsize>AKAI_HD9_MAXSIZE)){
						bsize=AKAI_HD9_MAXSIZE;
						PRINTF_ERR("truncated to max. partition size\n");
					}else if ((curdiskp->type==DISK_TYPE_HD)&&(bsize>AKAI_PART_MAXSIZE)){
						bsize=AKAI_PART_MAXSIZE;
						PRINTF_ERR("truncated to max. partition size\n");
					}
					if (curpartp->bstart+bsize>curdiskp->bsize){
						bsize=curdiskp->bsize-curpartp->bstart;
						PRINTF_ERR("truncated to remaining disk size\n");
					}
					if (bsize>curpartp->bsize){
#if 1
						PRINTF_ERR("import larger than current partition size, ignored\n");
#else
						bsize=pp->bsize;
						PRINTF_ERR("truncated to current partition size\n");
#endif
					}
					FLUSH_ALL;
					curpartp->bsize=bsize; /* new partition size */
					/* import */
					ret=0;
					PRINTF_OUT("\n");
					for (blk=0;blk<curpartp->bsize;blk++){
						print_progressbar(curpartp->bsize,blk);
#if 1
						if ((curpartp->type==PART_TYPE_HD)&&(curpartp->index==0) /* first partition on S1000/S3000 harddisk? */
							&&(blk==0)&&(curpartp->bsize>=AKAI_PARTHEAD_BLKS)){ /* in header? */
							/* read partition header */
							if (READ(inpfd,(u_char *)&tmppart,AKAI_PARTHEAD_BLKS*curpartp->blksize)
								!=(int)(AKAI_PARTHEAD_BLKS*curpartp->blksize)){
									PERROR("read");
									ret=1;
									break;
							}
							/* transfer partition table */
							bcopy(&(curpartp->head.hd.parttab),
								  &(tmppart.parttab),
								  sizeof(struct akai_parttab_s));
							/* write partition header */
							if (akai_io_blks(curpartp,(u_char *)&tmppart,
											 0, /* partition header block */
											 AKAI_PARTHEAD_BLKS,
											 0,IO_BLKS_WRITE)<0){ /* 0: don't alloc cache */
								ret=1;
								break;
							}
							blk=AKAI_PARTHEAD_BLKS-1;
							continue;
						}
#endif
						/* read block */
						if (READ(inpfd,fbuf,curpartp->blksize)!=(int)curpartp->blksize){
							PERROR("read");
							ret=1;
							break;
						}
						/* write block */
						if (akai_io_blks(curpartp,fbuf,
										 blk,
										 1,
										 0,IO_BLKS_WRITE)<0){ /* 0: don't alloc cache */
							ret=1;
							break;
						}
						if ((blk+1)>=curpartp->bsize){ /* end? */
							PRINTF_OUT("."); /* dot for 100% */
						}
					}
					PRINTF_OUT("\n\n");
					CLOSE(inpfd);

					/* must exit (or restart) now!!! */
					if (ret>0){ /* fatal? */
						mainret=1; /* error */
						goto main_exit;
					}
					FLUSH_ALL;
					PRINTF_OUT("\nrestarting program\n\n");
					restartflag=1;
					goto main_restart;
				}
				break;
			case CMD_GETTAGS:
				{
					int outfd;
					u_int size;

					if ((curdiskp==NULL)||(curpartp==NULL)||(curpartp->type!=PART_TYPE_HD)){
						PRINTF_ERR("must be inside an S1000/S3000 harddisk sampler partition\n");
						goto main_parser_next;
					}
					if (strncmp(AKAI_PARTHEAD_TAGSMAGIC,(char *)curpartp->head.hd.tagsmagic,4)!=0){
						PRINTF_ERR("no tags in partition\n");
						goto main_parser_next;
					}
					/* create external file */
					if ((outfd=OPEN(cmdtok[1],O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
						PERROR("open");
						goto main_parser_next;
					}
					/* export tags-file */
					size=4+AKAI_PARTHEAD_TAGNUM*AKAI_NAME_LEN; /* tags magic and tag names */;
					if (WRITE(outfd,(void *)curpartp->head.hd.tagsmagic,size)!=(int)size){
						PERROR("write");
					}
					CLOSE(outfd);
				}
				break;
			case CMD_PUTTAGS:
				{
					int inpfd;
					struct stat instat;
					u_int size;

					if ((curdiskp==NULL)||(curpartp==NULL)||(curpartp->type!=PART_TYPE_HD)){
						PRINTF_ERR("must be inside an S1000/S3000 harddisk sampler partition\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					/* open external file */
					if ((inpfd=akai_openreadonly_extfile(cmdtok[1]))<0){
						PERROR("open");
						goto main_parser_next;
					}
					/* get file size */
					if (fstat(inpfd,&instat)<0){
						PERROR("stat");
						CLOSE(inpfd);
						goto main_parser_next;
					}
					size=(u_int)instat.st_size;
					if (size!=(4+AKAI_PARTHEAD_TAGNUM*AKAI_NAME_LEN)){ /* invalid file size? (Note: tags magic and tag names) */
						PRINTF_ERR("invalid file size of tags-file\n");
						CLOSE(inpfd);
						goto main_parser_next;
					}
					/* read tags-file into partition header */
					if (READ(inpfd,(void *)curpartp->head.hd.tagsmagic,size)!=(int)size){
						PERROR("read");
						CLOSE(inpfd);
						goto main_parser_next;
					}
					CLOSE(inpfd);
					/* XXX no check if valid tags magic */
					/* write partition header */
					if (akai_io_blks(curpartp,(u_char *)&curpartp->head.hd,
									 0,
									 AKAI_PARTHEAD_BLKS,
									 1,IO_BLKS_WRITE)<0){ /* 1: allocate cache if possible */
						PRINTF_ERR("cannot write partition header\n");
						goto main_parser_next;
					}
					PRINTF_OUT("tags imported\n");
				}
				break;
			case CMD_RENVOL:
			case CMD_RENVOLI:
				{
					struct vol_s *vp;
					struct vol_s tmpvol;
					u_int vi;

					if ((strcmp(cmdtok[2],".")==0)||(strcmp(cmdtok[2],"..")==0)){
						PRINTF_ERR("invalid volume name\n");
						goto main_parser_next;
					}

					/* Note: allow invalid volume to be renamed!!! */
					if (cmdnr==CMD_RENVOL){
						save_curdir(1); /* 1: could be modifications */
						if (change_curdir(cmdtok[1],0,NULL,0)<0){ /* NULL,0: don't check last */
							PRINTF_ERR("directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("must be a volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						vp=curvolp;
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							PRINTF_ERR("must be on sampler partition level\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							PRINTF_ERR("invalid volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							PRINTF_ERR("volume not found\n");
							goto main_parser_next;
						}
						vp=&tmpvol;
					}
#if 1
					if (curpartp!=NULL){
						struct vol_s tmpvol2;

						/* check if new volume name already used */
						if (akai_find_vol(curpartp,&tmpvol2,cmdtok[2])==0){
							PRINTF_ERR("volume name already used\n");
							if (cmdnr==CMD_RENVOL){
								restore_curdir();
							}
							goto main_parser_next;
						}
					}
#endif
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						if (cmdnr==CMD_RENVOL){
							restore_curdir();
						}
						goto main_parser_next;
					}
					/* rename volume */
					if (akai_rename_vol(vp,cmdtok[2],vp->lnum,vp->osver,NULL)<0){
						PRINTF_ERR("rename error\n");
					}
					if (cmdnr==CMD_RENVOL){
						restore_curdir();
					}
				}
				break;
			case CMD_SETOSVERVOL:
			case CMD_SETOSVERVOLI:
				{
					struct vol_s *vp;
					struct vol_s tmpvol;
					u_int vi;
					char *osvername;
					u_int osver;

					if (cmdnr==CMD_SETOSVERVOL){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("must be inside a volume\n");
							goto main_parser_next;
						}
						if ((curpartp->type==PART_TYPE_HD9)||(curpartp->type==PART_TYPE_HD)
							||(curvolp->type==AKAI_VOL_TYPE_S900)){
							PRINTF_ERR("must be an S1000/S3000 floppy volume\n");
							goto main_parser_next;
						}
						vp=curvolp;
						osvername=cmdtok[1];
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							PRINTF_ERR("must be on sampler partition level\n");
							goto main_parser_next;
						}
						if ((curpartp->type==PART_TYPE_HD9)||(curpartp->type==PART_TYPE_HD)
							||(curvolp->type==AKAI_VOL_TYPE_S900)){
							PRINTF_ERR("must be an S1000/S3000 floppy volume\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							PRINTF_ERR("invalid volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							PRINTF_ERR("volume not found\n");
							goto main_parser_next;
						}
						vp=&tmpvol;
						osvername=cmdtok[2];
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					/* OS version */
					if (strstr(osvername,".")!=NULL){
						/* format: "XX.XX" */
						osver=(u_int)(100.0*atof(osvername));
						osver=(((osver/100))<<8)|(osver%100);
					}else{
						/* integer format */
						osver=(u_int)atoi(osvername);
					}
					/* set OS version in volume */
					/* Note: akai_rename_file() will correct osver if necessary */
					if (akai_rename_vol(vp,NULL,vp->lnum,osver,NULL)<0){
						PRINTF_ERR("cannot set OS version\n");
					}
				}
				break;
			case CMD_SETLNUM:
			case CMD_SETLNUMI:
				{
					struct vol_s *vp;
					struct vol_s tmpvol;
					u_int vi;
					char *lnumname;
					u_int lnum;

					if (cmdnr==CMD_SETLNUM){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("must be inside a volume\n");
							goto main_parser_next;
						}
						if (curpartp->type!=PART_TYPE_HD){
							PRINTF_ERR("must be an S1000/S3000 harddisk volume\n");
							goto main_parser_next;
						}
						vp=curvolp;
						lnumname=cmdtok[1];
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							PRINTF_ERR("must be on sampler partition level\n");
							goto main_parser_next;
						}
						if (curpartp->type!=PART_TYPE_HD){
							PRINTF_ERR("must be an S1000/S3000 harddisk volume\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							PRINTF_ERR("invalid volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							PRINTF_ERR("volume not found\n");
							goto main_parser_next;
						}
						vp=&tmpvol;
						lnumname=cmdtok[2];
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					/* load number */
					lnum=akai_get_lnum(lnumname);
					/* set load number in volume */
					if (akai_rename_vol(vp,NULL,lnum,vp->osver,NULL)<0){
						PRINTF_ERR("cannot set load number\n");
					}
				}
				break;
			case CMD_LSPARAM:
			case CMD_LSPARAMI:
				{
					struct vol_s *vp;
					struct vol_s tmpvol;
					u_int vi;

					if (cmdnr==CMD_LSPARAM){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("must be inside a volume\n");
							goto main_parser_next;
						}
						vp=curvolp;
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							PRINTF_ERR("must be on sampler partition level\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							PRINTF_ERR("invalid volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							PRINTF_ERR("volume not found\n");
							goto main_parser_next;
						}
						vp=&tmpvol;
					}
					if (vp->param!=NULL){
						PRINTF_OUT("\n");
						akai_list_volparam(vp->param,1); /* 1: list parameter values */
						PRINTF_OUT("\n");
					}else{
						PRINTF_ERR("volume has no parameters\n");
					}
				}
				break;
			case CMD_INITPARAM:
			case CMD_INITPARAMI:
				{
					struct vol_s *vp;
					struct vol_s tmpvol;
					u_int vi;

					if (cmdnr==CMD_INITPARAM){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("must be inside a volume\n");
							goto main_parser_next;
						}
						vp=curvolp;
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							PRINTF_ERR("must be on sampler partition level\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							PRINTF_ERR("invalid volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							PRINTF_ERR("volume not found\n");
							goto main_parser_next;
						}
						vp=&tmpvol;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					if (vp->param!=NULL){
						/* default parameters */
						akai_init_volparam(vp->param,(vp->type==AKAI_VOL_TYPE_S3000)||(vp->type==AKAI_VOL_TYPE_CD3000));
						/* update volume */
						if (akai_rename_vol(vp,NULL,vp->lnum,vp->osver,vp->param)<0){
							PRINTF_ERR("cannot update volume\n");
							goto main_parser_next;
						}
					}else{
						PRINTF_ERR("volume has no parameters\n");
					}
				}
				break;
			case CMD_SETPARAM:
			case CMD_SETPARAMI:
				{
					struct vol_s *vp;
					struct vol_s tmpvol;
					u_int vi;
					char *piname,*pvname;
					u_int pi,pv;

					if (cmdnr==CMD_SETPARAM){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("must be inside a volume\n");
							goto main_parser_next;
						}
						vp=curvolp;
						piname=cmdtok[1];
						pvname=cmdtok[2];
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							PRINTF_ERR("must be on sampler partition level\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							PRINTF_ERR("invalid volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							PRINTF_ERR("volume not found\n");
							goto main_parser_next;
						}
						vp=&tmpvol;
						piname=cmdtok[2];
						pvname=cmdtok[3];
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					if (vp->param!=NULL){
						/* parameter index */
						pi=(u_int)atoi(piname);
						if (pi>=sizeof(struct akai_volparam_s)){
							PRINTF_ERR("invalid parameter index\n");
							goto main_parser_next;
						}
						/* parameter value */
						pv=(u_int)atoi(pvname);
						if (pv>=0xff){
							PRINTF_ERR("invalid parameter value\n");
							goto main_parser_next;
						}
						/* set parameter */
						((u_char *)vp->param)[pi]=(u_char)pv;
						/* update volume */
						if (akai_rename_vol(vp,NULL,vp->lnum,vp->osver,vp->param)<0){
							PRINTF_ERR("cannot update volume\n");
							goto main_parser_next;
						}
					}else{
						PRINTF_ERR("volume has no parameters\n");
					}
				}
				break;
			case CMD_GETPARAM:
			case CMD_GETPARAMI:
				{
					struct vol_s *vp;
					struct vol_s tmpvol;
					u_int vi;
					int outfd;
					u_int size;
					char *fname;

					if (cmdnr==CMD_GETPARAM){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("must be inside a volume\n");
							goto main_parser_next;
						}
						vp=curvolp;
						fname=cmdtok[1];
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							PRINTF_ERR("must be on sampler partition level\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							PRINTF_ERR("invalid volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							PRINTF_ERR("volume not found\n");
							goto main_parser_next;
						}
						vp=&tmpvol;
						fname=cmdtok[2];
					}
					if (vp->param!=NULL){
						/* create external file */
						if ((outfd=OPEN(fname,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
							PERROR("open");
							goto main_parser_next;
						}
						/* export volparam-file */
						size=sizeof(struct akai_volparam_s);
						if (WRITE(outfd,(void *)vp->param,size)!=(int)size){
							PERROR("write");
						}
						CLOSE(outfd);
					}else{
						PRINTF_ERR("volume has no parameters\n");
					}
				}
				break;
			case CMD_PUTPARAM:
			case CMD_PUTPARAMI:
				{
					struct vol_s *vp;
					struct vol_s tmpvol;
					u_int vi;
					int inpfd;
					struct stat instat;
					u_int size;
					char *fname;

					if (cmdnr==CMD_PUTPARAM){
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("must be inside a volume\n");
							goto main_parser_next;
						}
						vp=curvolp;
						fname=cmdtok[1];
					}else{
						if (check_curnosamplerpart()){ /* not on sampler partition level? */
							PRINTF_ERR("must be on sampler partition level\n");
							goto main_parser_next;
						}
						/* index */
						vi=(u_int)atoi(cmdtok[1]);
						if ((vi<1)||(vi>curpartp->volnummax)){
							PRINTF_ERR("invalid volume index\n");
							goto main_parser_next;
						}
						/* find volume in current partition */
						if (akai_get_vol(curpartp,&tmpvol,vi-1)<0){
							PRINTF_ERR("volume not found\n");
							goto main_parser_next;
						}
						vp=&tmpvol;
						fname=cmdtok[2];
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					if (vp->param!=NULL){
						/* open external file */
						if ((inpfd=akai_openreadonly_extfile(fname))<0){
							PERROR("open");
							goto main_parser_next;
						}
						/* get file size */
						if (fstat(inpfd,&instat)<0){
							PERROR("stat");
							CLOSE(inpfd);
							goto main_parser_next;
						}
						size=(u_int)instat.st_size;
						if (size!=sizeof(struct akai_volparam_s)){ /* invalid file size? */
							PRINTF_ERR("invalid file size of volparam-file\n");
							CLOSE(inpfd);
							goto main_parser_next;
						}
						/* read volparam-file into volume */
						if (READ(inpfd,(void *)vp->param,size)!=(int)size){
							PERROR("read");
							CLOSE(inpfd);
							goto main_parser_next;
						}
						CLOSE(inpfd);
						/* update volume */
						if (akai_rename_vol(vp,NULL,vp->lnum,vp->osver,vp->param)<0){
							PRINTF_ERR("cannot update volume\n");
							goto main_parser_next;
						}
						PRINTF_OUT("volume parameters imported\n");
					}else{
						PRINTF_ERR("volume has no parameters\n");
					}
				}
				break;
			case CMD_GET:
			case CMD_GETI:
			case CMD_SAMPLE2WAV:
			case CMD_SAMPLE2WAVI:
				{
					struct file_s tmpfile;
					struct vol_s tmpvol;
					u_int sfi;
					int outfd;
					u_int begin,end;
					char *wavname;

					if ((cmdnr==CMD_GET)||(cmdnr==CMD_SAMPLE2WAV)){
						save_curdir(0); /* 0: no modifications */
						if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
							PRINTF_ERR("directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("must be a file in a sampler volume\n");
							restore_curdir();
							goto main_parser_next;
						}
						if (strlen(dirnamebuf)==0){
							PRINTF_ERR("invalid file name\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* find file in current directory */
						/* must save current volume for tmpfile */
						akai_copy_structvol(curvolp,&tmpvol);
						if (akai_find_file(&tmpvol,&tmpfile,dirnamebuf)<0){
							PRINTF_ERR("file not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						restore_curdir();
					}else{
						if (check_curnosamplervol()){ /* not inside a sampler volume? */
							PRINTF_ERR("must be inside a volume\n");
							goto main_parser_next;
						}
						/* index */
						sfi=(u_int)atoi(cmdtok[1]);
						if ((sfi<1)||(sfi>curvolp->fimax)){
							PRINTF_ERR("invalid file index\n");
							goto main_parser_next;
						}
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,sfi-1)<0){
							PRINTF_ERR("file not found\n");
							goto main_parser_next;
						}
					}
					PRINTF_OUT("exporting \"%s\"\n",tmpfile.name);
					FLUSH_ALL;
					if ((cmdnr==CMD_GET)||(cmdnr==CMD_GETI)){
						/* create external file */
						if ((outfd=OPEN(tmpfile.name,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
							PERROR("open");
							goto main_parser_next;
						}
						/* limits */
						if (cmdtoknr>=3){
							begin=(u_int)atoi(cmdtok[2]);
						}else{
							begin=0;
						}
						if (cmdtoknr>=4){
							end=(u_int)atoi(cmdtok[3]);
						}else{
							end=tmpfile.size;
						}
						/* export file */
						if (akai_read_file(outfd,NULL,&tmpfile,begin,end)<0){
							PRINTF_ERR("export error\n");
						}
						CLOSE(outfd);
					}else{
						/* export file to WAV */
#if 1
						PLAYWAV_STOP; /* stop playback of current external WAV file (if currently running) */
						/* no current external WAV file */
						curwavname[0]='\0';
						curwavcmdbuf[0]='\0';
#endif
						wavname=NULL;
						if (akai_sample2wav(&tmpfile,-1,NULL,&wavname,SAMPLE2WAV_ALL)!=0){ /* error or not valid sample file? */
							PRINTF_ERR("cannot export file to WAV\n");
						}
						if ((wavname!=NULL)&&(wavname[0]!='\0')){
							/* use name of last exported WAV file */
							SNPRINTF(curwavname,CURWAVNAMEMAXLEN,"%s",wavname);
							PLAYWAV_PREPARE(curwavcmdbuf,CURWAVCMDBUFSIZ,curwavname);
						}
					}
				}
				break;
			case CMD_GETALL:
				{
					struct file_s tmpfile;
					u_int sfi;
					int outfd;
					u_int fcount;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("must be inside a volume\n");
						goto main_parser_next;
					}
					/* all files in current volume */
					fcount=0;
					for (sfi=0;sfi<curvolp->fimax;sfi++){
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,sfi)<0){
							continue; /* next */
						}
						/* export file */
						PRINTF_OUT("exporting \"%s\"\n",tmpfile.name);
						FLUSH_ALL;
						/* create external file */
						if ((outfd=OPEN(tmpfile.name,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
							PERROR("open");
							goto main_parser_next;
						}
						/* export file */
						if (akai_read_file(outfd,NULL,&tmpfile,0,tmpfile.size)==0){
							fcount++;
						}else{
							PRINTF_ERR("export error\n");
							/* XXX continue */
						}
						CLOSE(outfd);
					}
					FLUSH_ALL;
					PRINTF_OUT("exported %u file(s)\n",fcount);
				}
				break;
			case CMD_SAMPLE2WAVALL:
				{
					struct file_s tmpfile;
					u_int sfi;
					u_int fcount;
					char *wavname;

					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("must be inside a volume\n");
						goto main_parser_next;
					}
#if 1
					PLAYWAV_STOP; /* stop playback of current external WAV file (if currently running) */
					/* no current external WAV file */
					curwavname[0]='\0';
					curwavcmdbuf[0]='\0';
#endif
					/* all files in current volume */
					fcount=0;
					wavname=NULL;
					for (sfi=0;sfi<curvolp->fimax;sfi++){
						/* find file in current volume */
						if (akai_get_file(curvolp,&tmpfile,sfi)<0){
							continue; /* next */
						}
						/* export file */
						PRINTF_OUT("exporting \"%s\"\n",tmpfile.name);
						FLUSH_ALL;
						if (akai_sample2wav(&tmpfile,-1,NULL,&wavname,SAMPLE2WAV_ALL)==0){
							fcount++;
						} /* XXX else: continue */
					}
					if ((wavname!=NULL)&&(wavname[0]!='\0')){
						/* use name of last exported WAV file */
						SNPRINTF(curwavname,CURWAVNAMEMAXLEN,"%s",wavname);
						PLAYWAV_PREPARE(curwavcmdbuf,CURWAVCMDBUFSIZ,curwavname);
					}
					FLUSH_ALL;
					PRINTF_OUT("exported %u file(s)\n",fcount);
				}
				break;
			case CMD_PUT:
				{
					struct file_s tmpfile;
					int inpfd;
					struct stat instat;
					u_int size;
					u_int dfi;
					u_int fcount;
#ifdef _VISUALCPP
					HANDLE ha;
					WIN32_FIND_DATAA fd;
#else /* !_VISUALCPP */
					DIR *dp;
					struct dirent *dep;
#endif /* !_VISUALCPP */
					int allflag;
					char *name;

#ifdef _VISUALCPP
					ha=INVALID_HANDLE_VALUE;
#else /* !_VISUALCPP */
					dp=NULL;
#endif /* !_VISUALCPP */

					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
						PRINTF_ERR("destination directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("destination must be inside a volume\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						restore_curdir();
						goto main_parser_next;
					}
					if (strlen(dirnamebuf)==0){
						PRINTF_ERR("invalid file name\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (cmdtoknr>=3){
						/* index */
						dfi=(u_int)atoi(cmdtok[2]);
						if ((dfi<1)||(dfi>curvolp->fimax)){
							PRINTF_ERR("invalid file index\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* XXX akai_create_file() below will check if index is free */
					}else{
						dfi=AKAI_CREATE_FILE_NOINDEX;
					}
					if (strcasecmp(dirnamebuf,"*")==0){
						if (cmdtoknr>=3){
							PRINTF_ERR("ignoring given file index\n");
							FLUSH_ALL;
							dfi=AKAI_CREATE_FILE_NOINDEX;
						}
						/* all files */
						allflag=1;
#ifdef _VISUALCPP
						/* open local directory and get first directory entry */
						ha=FindFirstFileA("*",&fd);
						if (ha==INVALID_HANDLE_VALUE){
							PRINTF_ERR("error: cannot read local directory\n");
							restore_curdir();
							goto main_parser_next;
						}
#else /* !_VISUALCPP */
						/* open local directory */
						dp=opendir(".");
						if (dp==NULL){
							PRINTF_ERR("error: cannot read local directory\n");
							restore_curdir();
							goto main_parser_next;
						}
#endif /* !_VISUALCPP */
						name=NULL;
					}else{
						/* single WAV file */
						allflag=0;
						name=dirnamebuf;
					}
					/* import file(s) */
					fcount=0;
					for (;;){
						if (allflag){
#ifdef _VISUALCPP
							if (name==NULL){
								name=fd.cFileName;
							}else{
								/* get next directory entry */
								if (((int)FindNextFileA(ha,&fd))==0){ /* end? */
									break; /* done */
								}
							}
							if (FILE_ATTRIBUTE_DIRECTORY&fd.dwFileAttributes){ /* directory? */
								continue; /* next */
							}
#else /* !_VISUALCPP */
							/* get next directory entry */
							dep=readdir(dp);
							if (dep==NULL){ /* end? */
								break; /* done */
							}
							if (dep->d_type!=DT_REG){ /* not regular file? */
								continue; /* next */
							}
							name=dep->d_name;
#endif /* !_VISUALCPP */
#if 1
							{
								static u_char anamebuf[AKAI_NAME_LEN+4+1]; /* +4 for ".<type>", +1 for '\0' */
								u_int osver;

								/* skip file with invalid name/type */
								if (ascii2akai_filename(name,anamebuf,&osver,curvolp->type==AKAI_VOL_TYPE_S900)==AKAI_FTYPE_FREE){
									PRINTF_OUT("invalid file name/type, skipping \"%s\"\n",name);
									FLUSH_ALL;
									continue; /* next */
								}
							}
#endif
						}
						if (allflag){
							PRINTF_OUT("importing \"%s\"\n",name);
							FLUSH_ALL;
						}
						/* open external file */
						if ((inpfd=akai_openreadonly_extfile(name))<0){
							PERROR("open");
							break; /* exit */
						}
						/* get file size */
						if (fstat(inpfd,&instat)<0){
							PERROR("stat");
							CLOSE(inpfd);
							break; /* exit */
						}
						size=(u_int)instat.st_size;
						/* check if destination file already exists */
						if (akai_find_file(curvolp,&tmpfile,name)==0){
							/* exists */
							/* delete file */
							if (akai_delete_file(&tmpfile)<0){
								PRINTF_ERR("cannot overwrite existing file\n");
								CLOSE(inpfd);
								break; /* exit */
							}
						}
						/* create destination file */
						/* Note: akai_create_file() will correct osver if necessary */
						if (akai_create_file(curvolp,&tmpfile,size,
											 dfi,
											 name,
											 (curvolp->type==AKAI_VOL_TYPE_S900)?0:curvolp->osver, /* default: from volume */
											 NULL)<0){
							PRINTF_ERR("cannot create file\n");
							CLOSE(inpfd);
							break; /* exit */
						}
						/* import file */
						if (akai_write_file(inpfd,NULL,&tmpfile,0,tmpfile.size)<0){
							PRINTF_ERR("import error\n");
							CLOSE(inpfd);
							break; /* exit */
						}
						CLOSE(inpfd);
						if ((curvolp->type==AKAI_VOL_TYPE_S900)&&(tmpfile.osver!=0)){ /* compressed file in S900 volume? */
							/* update uncompr value */
							akai_s900comprfile_updateuncompr(&tmpfile); /* ignore error */
						}
						/* fix name in header (if necessary) */
						akai_fixramname(&tmpfile); /* ignore error */
						if (!allflag){
							break; /* done */
						}
						fcount++;
					}
					restore_curdir();
#ifdef _VISUALCPP
					if (ha!=INVALID_HANDLE_VALUE){
						/* close local directory */
						FindClose(ha);
					}
#else /* !_VISUALCPP */
					if (dp!=NULL){
						/* close local directory */
						closedir(dp);
					}
#endif /* !_VISUALCPP */
					if (allflag){
						FLUSH_ALL;
						PRINTF_OUT("imported %u file(s)\n",fcount);
					}
				}
				break;
			case CMD_WAV2SAMPLE:
			case CMD_WAV2SAMPLE9:
			case CMD_WAV2SAMPLE9C:
			case CMD_WAV2SAMPLE1:
			case CMD_WAV2SAMPLE3:
				{
					u_int nlen;
					u_int dfi;
					u_int type;
					u_int fcount;
					int err;
#ifdef _VISUALCPP
					HANDLE ha;
					WIN32_FIND_DATAA fd;
#else /* !_VISUALCPP */
					DIR *dp;
					struct dirent *dep;
#endif /* !_VISUALCPP */
					int allflag;
					char *name;

#if 1
					PLAYWAV_STOP; /* stop playback of current external WAV file (if currently running) */
					/* no current external WAV file */
					curwavname[0]='\0';
					curwavcmdbuf[0]='\0';
#endif
#ifdef _VISUALCPP
					ha=INVALID_HANDLE_VALUE;
#else /* !_VISUALCPP */
					dp=NULL;
#endif /* !_VISUALCPP */
					save_curdir(1); /* 1: could be modifications */
					if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
						PRINTF_ERR("destination directory not found\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (check_curnosamplervol()){ /* not inside a sampler volume? */
						PRINTF_ERR("destination must be inside a volume\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						restore_curdir();
						goto main_parser_next;
					}
					if (cmdtoknr>=3){
						/* index */
						dfi=(u_int)atoi(cmdtok[2]);
						if ((dfi<1)||(dfi>curvolp->fimax)){
							PRINTF_ERR("invalid file index\n");
							restore_curdir();
							goto main_parser_next;
						}
						/* XXX akai_create_file() in akai_wav2sample() below will check if index is free */
					}else{
						dfi=AKAI_CREATE_FILE_NOINDEX;
					}
					/* sample file type */
					if ((cmdnr==CMD_WAV2SAMPLE9)||(cmdnr==CMD_WAV2SAMPLE9C)){
						type=AKAI_SAMPLE900_FTYPE;
					}else if (cmdnr==CMD_WAV2SAMPLE1){
						type=AKAI_SAMPLE1000_FTYPE;
					}else if (cmdnr==CMD_WAV2SAMPLE3){
						type=AKAI_SAMPLE3000_FTYPE;
					}else{
						type=AKAI_FTYPE_FREE; /* invalid: derive sample file type from volume type/file osver */
					}
					if (strcasecmp(dirnamebuf,"*.wav")==0){
						/* all WAV files */
						allflag=1;
#ifdef _VISUALCPP
						/* open local directory and get first directory entry */
						ha=FindFirstFileA("*",&fd);
						if (ha==INVALID_HANDLE_VALUE){
							PRINTF_ERR("error: cannot read local directory\n");
							restore_curdir();
							goto main_parser_next;
						}
#else /* !_VISUALCPP */
						/* open local directory */
						dp=opendir(".");
						if (dp==NULL){
							PRINTF_ERR("error: cannot read local directory\n");
							restore_curdir();
							goto main_parser_next;
						}
#endif /* !_VISUALCPP */
						name=NULL;
					}else{
						/* single WAV file */
						/* check external WAV file name (and modify given file name if required) */
						/* Note: string in dirnamebuf may be modified here */
						if (akai_check_extwavname(dirnamebuf)<0){
							restore_curdir();
							goto main_parser_next;
						}
						allflag=0;
						name=dirnamebuf;
					}
					/* import WAV file(s) */
					fcount=0;
					for (;;){
						if (allflag){
#ifdef _VISUALCPP
							if (name==NULL){
								name=fd.cFileName;
							}else{
								/* get next directory entry */
								if (((int)FindNextFileA(ha,&fd))==0){ /* end? */
									break; /* done */
								}
							}
							if (FILE_ATTRIBUTE_DIRECTORY&fd.dwFileAttributes){ /* directory? */
								continue; /* next */
							}
#else /* !_VISUALCPP */
							/* get next directory entry */
							dep=readdir(dp);
							if (dep==NULL){ /* end? */
								break; /* done */
							}
							if (dep->d_type!=DT_REG){ /* not regular file? */
								continue; /* next */
							}
							name=dep->d_name;
#endif /* !_VISUALCPP */
							nlen=(u_int)strlen(name);
							if ((nlen<4)||(strncasecmp(name+nlen-4,".wav",4)!=0)){ /* not a WAV file? */
								continue; /* next */
							}
						}
						/* read and parse WAV header, create sample header, import samples */
						PRINTF_OUT("importing \"%s\"\n",name);
						FLUSH_ALL;
						/* Note: akai_wav2sample() will correct osver if necessary */
						err=akai_wav2sample(-1,name,
											curvolp,
											dfi,
											type,
											(cmdnr==CMD_WAV2SAMPLE9C),
											curvolp->osver, /* default: osver from volume */
											NULL, /* no tags */
											NULL,
											WAV2SAMPLE_OPEN|WAV2SAMPLE_OVERWRITE);
						FLUSH_ALL;
						if (err<0){
							PRINTF_ERR("WAV import error\n");
							break; /* exit */
						}
						if (!allflag){
							break; /* done */
						}
						if (err==0){
							fcount++;
						}
					}
					restore_curdir();
#ifdef _VISUALCPP
					if (ha!=INVALID_HANDLE_VALUE){
						/* close local directory */
						FindClose(ha);
					}
#else /* !_VISUALCPP */
					if (dp!=NULL){
						/* close local directory */
						closedir(dp);
					}
#endif /* !_VISUALCPP */
					if ((name!=NULL)&&(name[0]!='\0')){
						/* use name of last imported WAV file */
						SNPRINTF(curwavname,CURWAVNAMEMAXLEN,"%s",name);
						PLAYWAV_PREPARE(curwavcmdbuf,CURWAVCMDBUFSIZ,curwavname);
					}
					if (allflag){
						FLUSH_ALL;
						PRINTF_OUT("imported %u WAV file(s)\n",fcount);
					}
				}
				break;
			case CMD_TGETI:
				{
					u_int ti;
					int outfd;
					u_int cstarts,cstarte;
					u_int csizes,csizee;
					struct akai_ddtake_s t;
					char fnamebuf[AKAI_NAME_LEN+4+1]; /* name (ASCII), +4 for ".<type>", +1 for '\0' */

					if (check_curnoddpart()){ /* not on DD partition level? */
						PRINTF_ERR("must be inside a DD partition\n");
						goto main_parser_next;
					}
					/* index */
					ti=(u_int)atoi(cmdtok[1]);
					if ((ti<1)||(ti>AKAI_DDTAKE_MAXNUM)){
						PRINTF_ERR("invalid take index\n");
						goto main_parser_next;
					}
					/* take */
					if (curpartp->head.dd.take[ti-1].stat==AKAI_DDTAKESTAT_FREE){ /* free? */
						PRINTF_ERR("take not found\n");
						goto main_parser_next;
					}
					/* create external file */
					akai2ascii_name(curpartp->head.dd.take[ti-1].name,fnamebuf,0); /* 0: not S900 */
					strcpy(fnamebuf+strlen(fnamebuf),AKAI_DDTAKE_FNAMEEND);
					PRINTF_OUT("exporting take %u to \"%s\"\n",ti,fnamebuf);
					FLUSH_ALL;
					if ((outfd=OPEN(fnamebuf,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
						PERROR("open");
						goto main_parser_next;
					}
					/* clusters */
					cstarts=(curpartp->head.dd.take[ti-1].cstarts[1]<<8)
						    +curpartp->head.dd.take[ti-1].cstarts[0];
					cstarte=(curpartp->head.dd.take[ti-1].cstarte[1]<<8)
						    +curpartp->head.dd.take[ti-1].cstarte[0];
					/* determine csizes (sample clusters) and csizee (envelope clusters) */
					if (cstarts!=0){ /* sample not empty? */
						csizes=akai_count_ddfatchain(curpartp,cstarts);
					}else{
						csizes=0;
					}
					if (cstarte!=0){ /* envelope not empty? */
						csizee=akai_count_ddfatchain(curpartp,cstarte);
					}else{
						csizee=0;
					}
					/* get DD take header (DD directory entry) */
					bcopy(&curpartp->head.dd.take[ti-1],&t,sizeof(struct akai_ddtake_s));
					/* XXX use cstarts/cstarte fields in DD take header for csizes/csizee */
					t.cstarts[1]=0xff&(csizes>>8);
					t.cstarts[0]=0xff&csizes;
					t.cstarte[1]=0xff&(csizee>>8);
					t.cstarte[0]=0xff&csizee;
					/* export take */
					if (akai_export_take(outfd,curpartp,&t,csizes,csizee,cstarts,cstarte)<0){
						PRINTF_ERR("cannot export take\n");
					}
					CLOSE(outfd);
				}
				break;
			case CMD_TGETALL:
				{
					u_int ti;
					int outfd;
					u_int cstarts,cstarte;
					u_int csizes,csizee;
					struct akai_ddtake_s t;
					char fnamebuf[AKAI_NAME_LEN+4+1]; /* name (ASCII), +4 for ".<type>", +1 for '\0' */
					u_int tcount;

					if (check_curnoddpart()){ /* not on DD partition level? */
						PRINTF_ERR("must be inside a DD partition\n");
						goto main_parser_next;
					}
					/* all takes in current DD partition */
					tcount=0;
					if (check_curnoddpart()){ /* not on DD partition level? */
						PRINTF_ERR("must be inside a DD partition\n");
						goto main_parser_next;
					}
					/* all DD takes in current DD partition */
					tcount=0;
					for (ti=0;ti<AKAI_DDTAKE_MAXNUM;ti++){
						/* take */
						if (curpartp->head.dd.take[ti].stat==AKAI_DDTAKESTAT_FREE){ /* free? */
							continue; /* next */
						}
						/* create external file */
						akai2ascii_name(curpartp->head.dd.take[ti].name,fnamebuf,0); /* 0: not S900 */
						strcpy(fnamebuf+strlen(fnamebuf),AKAI_DDTAKE_FNAMEEND);
						PRINTF_OUT("exporting take %u to \"%s\"\n",ti+1,fnamebuf);
						FLUSH_ALL;
						if ((outfd=OPEN(fnamebuf,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
							PERROR("open");
							goto main_parser_next;
						}
						/* clusters */
						cstarts=(curpartp->head.dd.take[ti].cstarts[1]<<8)
							    +curpartp->head.dd.take[ti].cstarts[0];
						cstarte=(curpartp->head.dd.take[ti].cstarte[1]<<8)
								+curpartp->head.dd.take[ti].cstarte[0];
						/* determine csizes (sample clusters) and csizee (envelope clusters) */
						if (cstarts!=0){ /* sample not empty? */
							csizes=akai_count_ddfatchain(curpartp,cstarts);
						}else{
							csizes=0;
						}
						if (cstarte!=0){ /* envelope not empty? */
							csizee=akai_count_ddfatchain(curpartp,cstarte);
						}else{
							csizee=0;
						}
						/* get DD take header (DD directory entry) */
						bcopy(&curpartp->head.dd.take[ti],&t,sizeof(struct akai_ddtake_s));
						/* XXX use cstarts/cstarte fields in DD take header for csizes/csizee */
						t.cstarts[1]=0xff&(csizes>>8);
						t.cstarts[0]=0xff&csizes;
						t.cstarte[1]=0xff&(csizee>>8);
						t.cstarte[0]=0xff&csizee;
						/* export take */
						if (akai_export_take(outfd,curpartp,&t,csizes,csizee,cstarts,cstarte)==0){
							tcount++;
						}else{
							PRINTF_ERR("export error\n");
							/* XXX continue */
						}
						CLOSE(outfd);
					}
					FLUSH_ALL;
					PRINTF_OUT("exported %u take(s)\n",tcount);
				}
				break;
			case CMD_TAKE2WAVI:
				{
					u_int ti;
					char *wavname;

					if (check_curnoddpart()){ /* not on DD partition level? */
						PRINTF_ERR("must be inside a DD partition\n");
						goto main_parser_next;
					}
					/* index */
					ti=(u_int)atoi(cmdtok[1]);
					if ((ti<1)||(ti>AKAI_DDTAKE_MAXNUM)){
						PRINTF_ERR("invalid take index\n");
						goto main_parser_next;
					}
#if 1
					PLAYWAV_STOP; /* stop playback of current external WAV file (if currently running) */
					/* no current external WAV file */
					curwavname[0]='\0';
					curwavcmdbuf[0]='\0';
#endif
					/* export take to WAV */
					PRINTF_OUT("exporting take %u\n",ti);
					FLUSH_ALL;
					wavname=NULL;
					if (akai_take2wav(curpartp,ti-1,-1,NULL,&wavname,TAKE2WAV_ALL)<0){
						PRINTF_ERR("cannot export take to WAV\n");
					}
					if ((wavname!=NULL)&&(wavname[0]!='\0')){
						/* use name of last exported WAV file */
						SNPRINTF(curwavname,CURWAVNAMEMAXLEN,"%s",wavname);
						PLAYWAV_PREPARE(curwavcmdbuf,CURWAVCMDBUFSIZ,curwavname);
					}
				}
				break;
			case CMD_TAKE2WAVALL:
				{
					u_int ti;
					u_int tcount;
					char *wavname;

					if (check_curnoddpart()){ /* not on DD partition level? */
						PRINTF_ERR("must be inside a DD partition\n");
						goto main_parser_next;
					}
#if 1
					PLAYWAV_STOP; /* stop playback of current external WAV file (if currently running) */
					/* no current external WAV file */
					curwavname[0]='\0';
					curwavcmdbuf[0]='\0';
#endif
					/* all DD takes in current DD partition */
					tcount=0;
					wavname=NULL;
					for (ti=0;ti<AKAI_DDTAKE_MAXNUM;ti++){
						/* take */
						if (curpartp->head.dd.take[ti].stat==AKAI_DDTAKESTAT_FREE){ /* free? */
							continue; /* next */
						}
						/* export take to WAV */
						PRINTF_OUT("exporting take %u\n",ti+1);
						FLUSH_ALL;
						if (akai_take2wav(curpartp,ti,-1,NULL,&wavname,TAKE2WAV_ALL)==0){
							tcount++;
						} /* XXX else: continue */
					}
					if ((wavname!=NULL)&&(wavname[0]!='\0')){
						/* use name of last exported WAV file */
						SNPRINTF(curwavname,CURWAVNAMEMAXLEN,"%s",wavname);
						PLAYWAV_PREPARE(curwavcmdbuf,CURWAVCMDBUFSIZ,curwavname);
					}
					FLUSH_ALL;
					PRINTF_OUT("exported %u take(s)\n",tcount);
				}
				break;
			case CMD_TPUT:
				{
					u_int nlen;
					u_int ti;
					int inpfd;
					u_int csizes,csizee;
					struct akai_ddtake_s t;
					u_int tcount;
#ifdef _VISUALCPP
					HANDLE ha;
					WIN32_FIND_DATAA fd;
#else /* !_VISUALCPP */
					DIR *dp;
					struct dirent *dep;
#endif /* !_VISUALCPP */
					int allflag;
					char *name;

#ifdef _VISUALCPP
					ha=INVALID_HANDLE_VALUE;
#else /* !_VISUALCPP */
					dp=NULL;
#endif /* !_VISUALCPP */

					if (check_curnoddpart()){ /* not on DD partition level? */
						PRINTF_ERR("must be inside a DD partition\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					if (strcasecmp(cmdtok[1],"*"AKAI_DDTAKE_FNAMEEND)==0){
						/* all take files */
						allflag=1;
#ifdef _VISUALCPP
						/* open local directory and get first directory entry */
						ha=FindFirstFileA("*",&fd);
						if (ha==INVALID_HANDLE_VALUE){
							PRINTF_ERR("error: cannot read local directory\n");
							restore_curdir();
							goto main_parser_next;
						}
#else /* !_VISUALCPP */
						/* open local directory */
						dp=opendir(".");
						if (dp==NULL){
							PRINTF_ERR("error: cannot read local directory\n");
							restore_curdir();
							goto main_parser_next;
						}
#endif /* !_VISUALCPP */
						name=NULL;
					}else{
						/* single take file */
						allflag=0;
						name=cmdtok[1];
					}
					/* import take(s) */
					tcount=0;
					for (;;){
						if (allflag){
#ifdef _VISUALCPP
							if (name==NULL){
								name=fd.cFileName;
							}else{
								/* get next directory entry */
								if (((int)FindNextFileA(ha,&fd))==0){ /* end? */
									break; /* done */
								}
							}
							if (FILE_ATTRIBUTE_DIRECTORY&fd.dwFileAttributes){ /* directory? */
								continue; /* next */
							}
#else /* !_VISUALCPP */
							/* get next directory entry */
							dep=readdir(dp);
							if (dep==NULL){ /* end? */
								break; /* done */
							}
							if (dep->d_type!=DT_REG){ /* not regular file? */
								continue; /* next */
							}
							name=dep->d_name;
#endif /* !_VISUALCPP */
							nlen=(u_int)strlen(name);
							if ((nlen<3)||(strncasecmp(name+nlen-3,AKAI_DDTAKE_FNAMEEND,3)!=0)){ /* not a TK file? */
								continue; /* next */
							}
						}
						/* find free index */
						for (ti=0;ti<AKAI_DDTAKE_MAXNUM;ti++){
							/* take */
							if (curpartp->head.dd.take[ti].stat==AKAI_DDTAKESTAT_FREE){ /* free? */
								break; /* done */
							}
						}
						if (ti==AKAI_DDTAKE_MAXNUM){ /* no free index? */
							PRINTF_ERR("no free take index\n");
							break; /* exit */
						}
						/* import take */
						PRINTF_OUT("importing \"%s\" to take %u\n",name,ti+1);
						FLUSH_ALL;
						/* open external file */
						if ((inpfd=akai_openreadonly_extfile(name))<0){
							PERROR("open");
							break; /* exit */
						}
						/* get DD take header (DD directory entry) */
						if (READ(inpfd,&t,sizeof(struct akai_ddtake_s))!=(int)sizeof(struct akai_ddtake_s)){
							PERROR("read");
							CLOSE(inpfd);
							break; /* exit */
						}
						/* get csizes (sample clusters) and csizee (envelope clusters) */
						/* XXX use cstarts/cstarte fields in DD take header for csizes/csizee */
						csizes=(t.cstarts[1]<<8)
							   +t.cstarts[0];
						csizee=(t.cstarte[1]<<8)
							   +t.cstarte[0];
						/* Note: akai_import_take() keeps name in DD take header */
						/* Note: don't check if name already used, create new DD take */
						if (akai_import_take(inpfd,curpartp,&t,ti,csizes,csizee)!=0){
							PRINTF_ERR("import error\n");
							CLOSE(inpfd);
							break; /* exit */
						}
						CLOSE(inpfd);
						if (!allflag){
							break; /* done */
						}
						tcount++;
					}
#ifdef _VISUALCPP
					if (ha!=INVALID_HANDLE_VALUE){
						/* close local directory */
						FindClose(ha);
					}
#else /* !_VISUALCPP */
					if (dp!=NULL){
						/* close local directory */
						closedir(dp);
					}
#endif /* !_VISUALCPP */
					if (allflag){
						FLUSH_ALL;
						PRINTF_OUT("imported %u take(s)\n",tcount);
					}
				}
				break;
			case CMD_WAV2TAKE:
				{
					u_int nlen;
					u_int ti;
					u_int tcount;
					int err;
#ifdef _VISUALCPP
					HANDLE ha;
					WIN32_FIND_DATAA fd;
#else /* !_VISUALCPP */
					DIR *dp;
					struct dirent *dep;
#endif /* !_VISUALCPP */
					int allflag;
					char *name;

#if 1
					PLAYWAV_STOP; /* stop playback of current external WAV file (if currently running) */
					/* no current external WAV file */
					curwavname[0]='\0';
					curwavcmdbuf[0]='\0';
#endif
#ifdef _VISUALCPP
					ha=INVALID_HANDLE_VALUE;
#else /* !_VISUALCPP */
					dp=NULL;
#endif /* !_VISUALCPP */
					if (check_curnoddpart()){ /* not on DD partition level? */
						PRINTF_ERR("must be inside a DD partition\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					if (strcasecmp(cmdtok[1],"*.wav")==0){
						/* all WAV files */
						allflag=1;
#ifdef _VISUALCPP
						/* open local directory and get first directory entry */
						ha=FindFirstFileA("*",&fd);
						if (ha==INVALID_HANDLE_VALUE){
							PRINTF_ERR("error: cannot read local directory\n");
							goto main_parser_next;
						}
#else /* !_VISUALCPP */
						/* open local directory */
						dp=opendir(".");
						if (dp==NULL){
							PRINTF_ERR("error: cannot read local directory\n");
							goto main_parser_next;
						}
#endif /* !_VISUALCPP */
						name=NULL;
					}else{
						/* single WAV file */
						/* check external WAV file name (and modify given file name if required) */
						/* Note: string in cmdtok[1] may be modified here */
						if (akai_check_extwavname(cmdtok[1])<0){
							goto main_parser_next;
						}
						allflag=0;
						name=cmdtok[1];
					}
					/* import WAV file(s) */
					tcount=0;
					for (;;){
						if (allflag){
#ifdef _VISUALCPP
							if (name==NULL){
								name=fd.cFileName;
							}else{
								/* get next directory entry */
								if (((int)FindNextFileA(ha,&fd))==0){ /* end? */
									break; /* done */
								}
							}
							if (FILE_ATTRIBUTE_DIRECTORY&fd.dwFileAttributes){ /* directory? */
								continue; /* next */
							}
#else /* !_VISUALCPP */
							/* get next directory entry */
							dep=readdir(dp);
							if (dep==NULL){ /* end? */
								break; /* done */
							}
							if (dep->d_type!=DT_REG){ /* not regular file? */
								continue; /* next */
							}
							name=dep->d_name;
#endif /* !_VISUALCPP */
							nlen=(u_int)strlen(name);
							if ((nlen<4)||(strncasecmp(name+nlen-4,".wav",4)!=0)){ /* not a WAV file? */
								continue; /* next */
							}
						}
						/* find free index */
						for (ti=0;ti<AKAI_DDTAKE_MAXNUM;ti++){
							/* take */
							if (curpartp->head.dd.take[ti].stat==AKAI_DDTAKESTAT_FREE){ /* free? */
								break; /* done */
							}
						}
						if (ti==AKAI_DDTAKE_MAXNUM){ /* no free index? */
							PRINTF_ERR("no free take index\n");
							break; /* exit */
						}
						/* read and parse WAV header, create take, import samples */
						PRINTF_OUT("importing \"%s\" to take %u\n",name,ti+1);
						FLUSH_ALL;
						/* Note: don't check if name already used, create new DD take */
						err=akai_wav2take(-1,name,
										  curpartp,
										  ti,
										  NULL,
										  WAV2TAKE_OPEN);
						FLUSH_ALL;
						if (err<0){
							PRINTF_ERR("WAV import error\n");
							break; /* exit */
						}
						if (!allflag){
							break; /* done */
						}
						if (err==0){
							tcount++;
						}
					}
#ifdef _VISUALCPP
					if (ha!=INVALID_HANDLE_VALUE){
						/* close local directory */
						FindClose(ha);
					}
#else /* !_VISUALCPP */
					if (dp!=NULL){
						/* close local directory */
						closedir(dp);
					}
#endif /* !_VISUALCPP */
					if ((name!=NULL)&&(name[0]!='\0')){
						/* use name of last imported WAV file */
						SNPRINTF(curwavname,CURWAVNAMEMAXLEN,"%s",name);
						PLAYWAV_PREPARE(curwavcmdbuf,CURWAVCMDBUFSIZ,curwavname);
					}
					if (allflag){
						FLUSH_ALL;
						PRINTF_OUT("imported %u WAV file(s)\n",tcount);
					}
				}
				break;
			case CMD_TARC:
			case CMD_TARCWAV:
				{
					int outfd;
					u_int flags;

					/* create tar-file */
					if ((outfd=OPEN(cmdtok[1],O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666))<0){
						PERROR("open");
						goto main_parser_next;
					}
					/* flags */
					if (cmdnr==CMD_TARCWAV){
#if 1
						PLAYWAV_STOP; /* stop playback of current external WAV file (if currently running) */
						/* no current external WAV file */
						curwavname[0]='\0';
						curwavcmdbuf[0]='\0';
#endif
						flags=TAR_EXPORT_WAV;
					}else{
						flags=0;
					}
					/* export tar-file */
					if (tar_export_curdir(outfd,1,flags)<0){ /* 1: verbose */
						PRINTF_ERR("tar error\n");
					}
					if (tar_export_tailzero(outfd)<0){
						PRINTF_ERR("tar error\n");
					}
					CLOSE(outfd);
				}
				break;
			case CMD_TARX:
			case CMD_TARX9:
			case CMD_TARX1:
			case CMD_TARX3:
			case CMD_TARX3CD:
			case CMD_TARXWAV:
			case CMD_TARXWAV9:
			case CMD_TARXWAV9C:
			case CMD_TARXWAV1:
			case CMD_TARXWAV3:
				{
					int inpfd;
					u_int vtype;
					u_int flags;

					/* XXX check for read-only mode actually not necessary here, */
					/*     will be checked within tar_import_curdir() */
					if ((curdiskp!=NULL)&&curdiskp->readonly){
							PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
							goto main_parser_next;
					}
					/* Note: if curdiskp==NULL: cannot check for read-only mode here */
					/*       because different disks may have different readonly values */
					/*       and don't know here which disk is contained within the tar-file */
					/* open tar-file */
					if ((inpfd=akai_openreadonly_extfile(cmdtok[1]))<0){
						PERROR("open");
						goto main_parser_next;
					}
					/* volume type */
					if (cmdnr==CMD_TARX9){
						vtype=AKAI_VOL_TYPE_S900; /* force to S900 */
					}else if (cmdnr==CMD_TARX1){
						vtype=AKAI_VOL_TYPE_S1000; /* force to S1000 */
					}else if (cmdnr==CMD_TARX3){
						vtype=AKAI_VOL_TYPE_S3000; /* force to S3000 or CD3000 */
					}else if (cmdnr==CMD_TARX3CD){
						vtype=AKAI_VOL_TYPE_CD3000; /* force to CD3000 CD-ROM */
					}else{
						vtype=AKAI_VOL_TYPE_INACT; /* INACT: auto-detect */
					}
					/* flags */
					if (cmdnr==CMD_TARXWAV){
						flags=TAR_IMPORT_WAV;
					}else if (cmdnr==CMD_TARXWAV9){
						flags=TAR_IMPORT_WAV|TAR_IMPORT_WAVS9;
					}else if (cmdnr==CMD_TARXWAV9C){
						flags=TAR_IMPORT_WAV|TAR_IMPORT_WAVS9C;
					}else if (cmdnr==CMD_TARXWAV1){
						flags=TAR_IMPORT_WAV|TAR_IMPORT_WAVS1;
					}else if (cmdnr==CMD_TARXWAV3){
						flags=TAR_IMPORT_WAV|TAR_IMPORT_WAVS3;
					}else{
						flags=0;
					}
#if 1
					if ((TAR_IMPORT_WAV&flags)!=0){
						PLAYWAV_STOP; /* stop playback of current external WAV file (if currently running) */
						/* no current external WAV file */
						curwavname[0]='\0';
						curwavcmdbuf[0]='\0';
					}
#endif
					/* import tar-file */
					if (tar_import_curdir(inpfd,vtype,1,flags)<0){ /* 1: verbose */
						PRINTF_ERR("tar error\n");
					}
					CLOSE(inpfd);
				}
				break;
			case CMD_MKVOL:
			case CMD_MKVOL9:
			case CMD_MKVOL1:
			case CMD_MKVOL3:
			case CMD_MKVOL3CD:
				{
					struct vol_s tmpvol;
					u_int lnum;
					u_int vtype;

					save_curdir(1); /* 1: could be modifications */
					if (cmdtoknr>=2){
						if (change_curdir(cmdtok[1],0,dirnamebuf,0)<0){
							PRINTF_ERR("directory not found\n");
							restore_curdir();
							goto main_parser_next;
						}
						if ((strlen(dirnamebuf)==0)
							||(strcmp(dirnamebuf,".")==0)||(strcmp(dirnamebuf,"..")==0)){
								PRINTF_ERR("invalid volume name\n");
								restore_curdir();
								goto main_parser_next;
						}
					}
					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						PRINTF_ERR("must be on sampler partition level\n");
						restore_curdir();
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						restore_curdir();
						goto main_parser_next;
					}
#if 1
					if (cmdtoknr>=2){
						/* check if new volume name already used */
						if (akai_find_vol(curpartp,&tmpvol,dirnamebuf)==0){
							PRINTF_ERR("volume name already used\n");
							restore_curdir();
							goto main_parser_next;
						}
					}
#endif
					/* load number */
					if (cmdtoknr>=3){
						lnum=akai_get_lnum(cmdtok[2]);
					}else{
						lnum=AKAI_VOL_LNUM_OFF;
					}
					/* volume type */
					if (cmdnr==CMD_MKVOL){
						/* derive volume type from partition type */
						if (curpartp->type==PART_TYPE_HD9){
							vtype=AKAI_VOL_TYPE_S900;
						}else{
							if (strncmp(AKAI_PARTHEAD_TAGSMAGIC,(char *)curpartp->head.hd.tagsmagic,4)!=0){
								vtype=AKAI_VOL_TYPE_S3000;
							}else{
								/* default */
								vtype=AKAI_VOL_TYPE_S1000;
							}
						}
					}else if (cmdnr==CMD_MKVOL9){
						vtype=AKAI_VOL_TYPE_S900;
					}else if (cmdnr==CMD_MKVOL3){
						vtype=AKAI_VOL_TYPE_S3000;
					}else if (cmdnr==CMD_MKVOL3CD){
						vtype=AKAI_VOL_TYPE_CD3000;
					}else{
						vtype=AKAI_VOL_TYPE_S1000;
					}
					/* create volume */
					if (akai_create_vol(curpartp,&tmpvol,
										vtype,
										AKAI_CREATE_VOL_NOINDEX,
										(cmdtoknr>=2)?dirnamebuf:NULL,
										lnum,
										NULL)<0){
						PRINTF_ERR("cannot create volume\n");
					}
					restore_curdir();
				}
				break;
			case CMD_MKVOLI9:
			case CMD_MKVOLI1:
			case CMD_MKVOLI3:
			case CMD_MKVOLI3CD:
				{
					struct vol_s tmpvol;
					u_int vi;
					u_int lnum;
					u_int vtype;

					if ((cmdtoknr>=3)
						&&((strcmp(cmdtok[2],".")==0)||(strcmp(cmdtok[2],"..")==0))){
						PRINTF_ERR("invalid volume name\n");
						goto main_parser_next;
					}

					if (check_curnosamplerpart()){ /* not on sampler partition level? */
						PRINTF_ERR("must be on sampler partition level\n");
						goto main_parser_next;
					}
					if (curdiskp->readonly){
						PRINTF_ERR("disk%u: read-only, cannot write\n",curdiskp->index);
						goto main_parser_next;
					}
					/* index */
					vi=(u_int)atoi(cmdtok[1]);
					if ((vi<1)||(vi>curpartp->volnummax)){
						PRINTF_ERR("invalid volume index\n");
						goto main_parser_next;
					}
					/* load number */
					if (cmdtoknr>=4){
						lnum=akai_get_lnum(cmdtok[3]);
					}else{
						lnum=AKAI_VOL_LNUM_OFF;
					}
					/* volume type */
					if (cmdnr==CMD_MKVOLI){
						/* derive volume type from partition type */
						if (curpartp->type==PART_TYPE_HD9){
							vtype=AKAI_VOL_TYPE_S900;
						}else{
							if (strncmp(AKAI_PARTHEAD_TAGSMAGIC,(char *)curpartp->head.hd.tagsmagic,4)!=0){
								vtype=AKAI_VOL_TYPE_S3000;
							}else{
								/* default */
								vtype=AKAI_VOL_TYPE_S1000;
							}
						}
					}else if (cmdnr==CMD_MKVOLI9){
						vtype=AKAI_VOL_TYPE_S900;
					}else if (cmdnr==CMD_MKVOLI3){
						vtype=AKAI_VOL_TYPE_S3000;
					}else if (cmdnr==CMD_MKVOLI3CD){
						vtype=AKAI_VOL_TYPE_CD3000;
					}else{
						vtype=AKAI_VOL_TYPE_S1000;
					}
					/* create volume */
					if (akai_create_vol(curpartp,&tmpvol,
										vtype,
										vi-1,
										(cmdtoknr>=3)?cmdtok[2]:NULL,
										lnum,
										NULL)<0){
						PRINTF_ERR("cannot create volume\n");
					}
				}
				break;
			case CMD_DIRCACHE:
				if (blk_cache_enable){ /* cache enabled? */
					PRINTF_OUT("\n");
					print_blk_cache();
					PRINTF_OUT("\n");
				}else{
					PRINTF_OUT("cache is not enabled\n\n");
				}
				break;
			case CMD_DISABLECACHE:
				if (blk_cache_enable){ /* cache enabled? */
					if (flush_blk_cache()<0){
						/* XXX try once again */
						PRINTF_ERR("trying again to flush cache\n");
						FLUSH_ALL;
						flush_blk_cache(); /* XXX if error, too late */
					}
					free_blk_cache();
					blk_cache_enable=0; /* disable cache */
				}
				break;
			case CMD_ENABLECACHE:
				if (!blk_cache_enable){ /* cache disabled? */
					init_blk_cache(); /* XXX actually not necessary here (after init_blk_cache() or free_blk_cache()) */
					blk_cache_enable=1; /* enable cache */
				}
				break;
			case CMD_LOCK:
#ifdef _VISUALCPP
				if (lockh!=INVALID_HANDLE_VALUE){
					if (!lockflag){
						/* acquire lock */
						if (LockFileEx(lockh,LOCKFILE_EXCLUSIVE_LOCK,0,1,0,&lockov)==FALSE){ /* XXX 1 byte */
							PRINTF_ERR("cannot acquire lock\n");
						}else{
							lockflag=1;
						}
					}else{
						PRINTF_ERR("lock already acquired\n");
					}
#else /* !_VISUALCPP */
				if (lockfd>=0){
					if (!lockflag){
						/* acquire lock */
						lockfl.l_type=F_WRLCK; /* exclusive lock */
						if (fcntl(lockfd,F_SETLKW,&lockfl)<0){
							PRINTF_ERR("cannot acquire lock\n");
						}else{
							lockflag=1;
						}
					}else{
						PRINTF_ERR("lock already acquired\n");
					}
#endif /* !_VISUALCPP */
				}else{
					PRINTF_ERR("no lock-file\n");
				}
				break;
			case CMD_UNLOCK:
#ifdef _VISUALCPP
				if (lockh!=INVALID_HANDLE_VALUE){
					if (lockflag){
						/* release lock */
						if (UnlockFileEx(lockh,0,1,0,&lockov)==FALSE){ /* XXX 1 byte */
							PRINTF_ERR("cannot release lock\n");
						}else{
							lockflag=0;
						}
					}else{
						PRINTF_ERR("lock currently not acquired\n");
					}
#else /* !_VISUALCPP */
				if (lockfd>=0){
					if (lockflag){
						/* release lock */
						lockfl.l_type=F_UNLCK;
						if (fcntl(lockfd,F_SETLKW,&lockfl)<0){
							PRINTF_ERR("cannot release lock\n");
						}else{
							lockflag=0;
						}
					}else{
						PRINTF_ERR("lock currently not acquired\n");
					}
#endif /* !_VISUALCPP */
				}else{
					PRINTF_ERR("no lock-file\n");
				}
				break;
			case CMD_PLAYWAV:
				PLAYWAV_STOP; /* stop playback of current external WAV file (if currently running) */
				if (cmdtoknr>=2){
					/* given WAV file name */
					/* check external WAV file name (and modify given file name if required) */
					/* Note: string in cmdtok[1] may be modified here */
					if (akai_check_extwavname(cmdtok[1])<0){
						/* no current external WAV file */
						curwavname[0]='\0';
						curwavcmdbuf[0]='\0';
						goto main_parser_next;
					}
					SNPRINTF(curwavname,CURWAVNAMEMAXLEN,"%s",cmdtok[1]);
					PLAYWAV_PREPARE(curwavcmdbuf,CURWAVCMDBUFSIZ,curwavname);
				}
				if (curwavcmdbuf[0]!='\0'){
					/* start playback of current external WAV file */
					PLAYWAV_START(curwavcmdbuf); /* XXX no error check */
				}else{
					PRINTF_OUT("no current WAV file\n");
				}
				break;
			case CMD_STOPWAV:
				PLAYWAV_STOP; /* stop playback of current external WAV file (if currently running) */
				break;
			case CMD_DELWAV:
				PLAYWAV_STOP; /* stop playback of current external WAV file (if currently running) */
				if (curwavname[0]!='\0'){
					/* check external WAV file name (and modify given file name if required) */
					/* Note: string in curwavname may be modified here */
					if (akai_check_extwavname(curwavname)==0){
						/* delete current external WAV file */
						LDELFILE(curwavcmdbuf,CURWAVCMDBUFSIZ,curwavname); /* XXX no error check */
					}
					/* no current external WAV file */
					curwavname[0]='\0';
					curwavcmdbuf[0]='\0';
				}else{
					PRINTF_OUT("no current WAV file\n");
				}
				break;
			default:
				PRINTF_OUT("unknown command, try \"help\"\n\n");
				break;
			}
main_parser_next:
#if 1 /* XXX flush cache every now and then */
			if (blk_cache_enable){ /* cache enabled? */
				flush_blk_cache(); /* XXX if error, maybe next time more luck */
			}
#endif
			PRINTF_OUT("\n");
#ifdef DEBUG
			if (blk_cache_enable){ /* cache enabled? */
				print_blk_cache();
			}
#endif
			FLUSH_ALL;
		} /* main command loop */
	} /* command interpreter */

main_exit:

	FLUSH_ALL;

	PLAYWAV_STOP; /* stop playback of current external WAV file (if currently running) */

	if (blk_cache_enable){ /* cache enabled? */
		if (flush_blk_cache()<0){
			/* XXX try once again */
			PRINTF_ERR("trying again to flush cache\n");
			FLUSH_ALL;
			flush_blk_cache(); /* XXX if error, too late */
		}
		free_blk_cache();
	}

	/* close all disk-files/drives */
	close_alldisks();

#ifdef _VISUALCPP
	fldr_end();
	if (lockh!=INVALID_HANDLE_VALUE){
		if (lockflag){
			/* release lock */
			UnlockFileEx(lockh,0,1,0,&lockov); /* XXX 1 byte, XXX ignore error */
			lockflag=0;
		}
		/* close lock-file */
		CloseHandle(lockh);
		lockh=INVALID_HANDLE_VALUE;
	}
#else /* !_VISUALCPP */
	if (lockfd>=0){
		if (lockflag){
			/* release lock */
			lockfl.l_type=F_UNLCK;
			fcntl(lockfd,F_SETLKW,&lockfl); /* XXX ignore error */
			lockflag=0;
		}
		/* close lock-file */
		CLOSE(lockfd);
		lockfd=-1;
	}
#endif /* !_VISUALCPP */

	AKAIUTIL_EXIT(mainret);
}



/* EOF */
