#ifndef __COMMONINCLUDE_H
#define __COMMONINCLUDE_H
/*
* Copyright (C) 2008,2010,2012,2019,2020,2021 Klaus Michael Indlekofer. All rights reserved.
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



#ifdef _VISUALCPP



#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
/*#include <unistd.h>*/
/*#include <strings.h>*/
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

typedef unsigned char u_char;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef int ssize_t;
#ifndef SSIZE_MAX
#define SSIZE_MAX INT_MAX
#endif /* !SSIZE_MAX */

#define INT64 __int64
#define U_INT64 unsigned __int64
#define OFF_T __int64
#define OFF64_T __int64

#define OPEN _open
#define CLOSE _close
#define READ _read
#define WRITE _write
/* default lseek and off_t cannot handle offsets>=2GB! */
#define LSEEK _lseeki64
#define LSEEK64 _lseeki64

extern int strcasecmp(const char *s1,const char *s2);
extern int strncasecmp(const char *s1,const char *s2,size_t n);

/* getopt */
/*#include <unistd.h>*/
extern int getopt(int argc,char * const argv[],const char *optstring);
extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;

/* Note: returned value of _snprintf_s() can differ from returned value of snprintf() */
#define SNPRINTF(x,y,...)	_snprintf_s((x),(y),_TRUNCATE,__VA_ARGS__)

/* XXX ignore returned value */
#define SYSTEM(x)	{system(x);}

/* local (external) filesystem */
#include <direct.h>
#define LCHDIR(d)			_chdir(d)
#define LDIR				SYSTEM("dir")
#define LDELFILE(x,y,n)		{_snprintf_s((x),(y),_TRUNCATE,"del /q /f \"%s\"",(n)); SYSTEM(x);}

/* play external WAV file */
#define PLAYWAV_PREPARE(x,y,n)	SNPRINTF((x),(y),"%s",(n))
#define PLAYWAV_START(x)		PlaySoundA((LPCSTR)(x),NULL,SND_ASYNC|SND_FILENAME|SND_NODEFAULT)
#define PLAYWAV_STOP			PlaySoundA(NULL,NULL,0)



#else /* !_VISUALCPP */
/* e.g. UNIX-like systems or cygwin */



#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define INT64 __int64_t
#define U_INT64 __uint64_t
#define OFF_T off_t
#define OFF64_T int64_t

#ifndef NOT_MYLSEEK64
/* own 64bit version of lseek */
extern OFF64_T mylseek64(int filedes,OFF64_T offset,int whence);
#define LSEEK64	mylseek64
#endif

#ifndef __CYGWIN__
/* e.g. UNIX-like systems */
/* getopt */
#include <unistd.h>
extern int getopt(int argc,char * const argv[],const char *optstring);
extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;
#endif /* !__CYGWIN__*/

#define SNPRINTF(x,y,...)	snprintf((x),(y),__VA_ARGS__)

/* XXX ignore returned value */
#define SYSTEM(x)	{if (system(x)<0){;}}

/* local (external) filesystem */
#include <dirent.h>
#define LCHDIR(d)			chdir(d)
#define LDIR				SYSTEM("ls -la")
#define LDELFILE(x,y,n)		{SNPRINTF((x),(y),"rm -f \"%s\"",(n)); SYSTEM(x);}

/* play external WAV file */
#ifndef PLAYWAV_CMD
#ifdef __APPLE__
#define PLAYWAV_CMD		"afplay"
#else /* !__APPLE__ */
/* use PulseAudio */
#define PLAYWAV_CMD		"paplay"
#endif /* !__APPLE__ */
#endif /* !PLAYWAV_CMD */
#define PLAYWAV_PREPARE(x,y,n)	snprintf((x),(y),PLAYWAV_CMD " \"%s\" > /dev/null 2>&1 &",(n))
#define PLAYWAV_START(x)		SYSTEM((const char *)(x))
#define PLAYWAV_STOP			SYSTEM("killall " PLAYWAV_CMD " > /dev/null 2>&1")



#endif /* !_VISUALCPP */



#include <math.h>

/* O_BINARY important for Windows-based systems (esp. cygwin or VC++) */
#ifndef O_BINARY
#define O_BINARY 0
#endif /* !O_BINARY */
#ifndef OPEN
#define OPEN open
#endif
#ifndef CLOSE
#define CLOSE close
#endif
#ifndef READ
#define READ read
#endif
#ifndef WRITE
#define WRITE write
#endif
#ifndef LSEEK
#define LSEEK lseek
#endif
#ifndef LSEEK64
#define LSEEK64 lseek64
#endif

extern void bcopy(const void *src,void *dst,size_t len);
extern void bzero(void *b,size_t len);



#ifndef STRHEX_TO_UINT64
extern U_INT64 my_strhex_to_uint64(char *s);
#define STRHEX_TO_UINT64	my_strhex_to_uint64
#define USE_MY_STRHEX_TO_UINT64
#endif
#ifndef STRDEC_TO_UINT64
extern U_INT64 my_strdec_to_uint64(char *s);
#define STRDEC_TO_UINT64	my_strdec_to_uint64
#define USE_MY_STRDEC_TO_UINT64
#endif



#ifdef UI_INCLUDE
/* external user interface */
#include UI_INCLUDE
#endif



/* console I/O */

#ifndef FGETS
#define FGETS				fgets
#endif
#ifndef PRINTF_OUT
#define PRINTF_OUT(...)		fprintf(stdout,__VA_ARGS__)
#endif
#ifndef FLUSH_OUT
#define FLUSH_OUT			fflush(stdout)
#endif
#ifndef PRINTF_ERR
#define PRINTF_ERR(...)		fprintf(stderr,__VA_ARGS__)
#endif
#ifndef PERROR
#include <errno.h>
#define PERROR(x)			PRINTF_ERR("%s: %s\n",x,strerror(errno))
#endif
#ifndef FLUSH_ALL
#define FLUSH_ALL			fflush(NULL)
#endif



#endif /* !__COMMONINCLUDE_H */
