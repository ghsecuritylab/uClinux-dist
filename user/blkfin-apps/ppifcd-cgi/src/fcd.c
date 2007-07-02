/*
 *
 *	  Rev:			$Id$
 *	  Revision:		$Revision$
 *	  Source:		$Source$
 *	  Created:		Do Apr 21 11:02:09 CEST	2005
 *	  Author:		Michael	Hennerich
 *	  mail:			hennerich@blackfin.uclinux.org
 *	  Description:	PPI	frame capture driver test code
 *
 *	 Copyright (C) 2005	Michael	Hennerich
 *
 *	 This program is free software;	you	can	redistribute it	and/or modify
 *	 it	under the terms	of the GNU General Public License as published by
 *	 the Free Software Foundation; either version 2	of the License,	or
 *	 (at your option) any later	version.
 *
 *	 This program is distributed in	the	hope that it will be useful,
 *	 but WITHOUT ANY WARRANTY; without even	the	implied	warranty of
 *	 MERCHANTABILITY or	FITNESS	FOR	A PARTICULAR PURPOSE.  See the
 *	 GNU General Public	License	for	more details.
 *
 *	 You should	have received a	copy of	the	GNU	General	Public License
 *	 along with	this program; if not, write	to the Free	Software
 *	 Foundation, Inc., 59 Temple Place,	Suite 330, Boston, MA  02111-1307  USA
 *
 *
 ****************************************************************************
 * MODIFICATION	HISTORY:
 ***************************************************************************/

#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <errno.h>
#include <unistd.h>


#include <strings.h>


#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif

#include "cgivars.h"
#include "htmllib.h"
#include "pflags.h"
#include "adsp-ppifcd.h"

extern int i2c_write_register(char * , unsigned	char , unsigned	char , unsigned	short );
extern int i2c_read_register(char *	, unsigned char	, unsigned char	);
extern int i2c_dump_register(char *	, unsigned char	, unsigned short , unsigned	short );
extern int i2c_scan_bus(char * );
int	set_gpio(char *	, char*	);
void capture(int , char	**,	char **);
void reset_reg(int , char **, char **);
void prg_reg(int , char	**,	char **);
int	getrand(int);
char* itostr  (	 u_int	,  u_char  ,  u_char  ,	u_char);
int	WriteIMG (char *, unsigned long);
//extern void cfa2rgb( unsigned	char *,	unsigned char *	);
void MI350_init(void);


#define	I2C_DEVICE		"/dev/i2c-0"

/****************************************************************************/
#define MT9M001
#undef MT9V022
/****************************************************************************/
#ifdef MT9M001
#define	DEVID			0x5D
#define	WIDTH			1280
#define	HEIGHT			1024
#endif

#define	MICRON_STANDBY	"/dev/pf8"
#define	MICRON_TRIGGER	"/dev/pf6"
#define	MICRON_LED		"/dev/pf11"
#define	FS3				"/dev/pf3"

#undef USETRIGGER

/****************************************************************************/
#ifdef MT9V022
#define DEVID (0xb8>>1)
#define	WIDTH			752
#define	HEIGHT			480
#endif
/****************************************************************************/
 
#define	IMAGESIZE		(WIDTH * HEIGHT)

#define	LSB_WIDTH		(WIDTH	& 0xFF)
#define	MSB_WIDTH		((WIDTH	>> 8) &	0xFF)
#define	LSB_HEIGHT		(HEIGHT	 & 0xFF)
#define	MSB_HEIGHT		((HEIGHT >>	8) & 0xFF)
#define	BPP				8

#define	FILENAME_OUT	"/var/img.bmp"


typedef	struct cgi_info
{
	unsigned int cmd;
	unsigned int reg;
	unsigned int val;

}cgi_info_t;

static cgi_info_t cgiinfo;

enum
{
  CAPTURE, PRGREG, RESETREG
};

static unsigned	char bmphead[] = {
  0x42,	0x4d, 0x36,	0x04, 0x14,	0x00, 0x00,	0x00, 0x00,	0x00, 0x36,	0x04,
  0x00,	0x00, 0x28,	0x00, 0x00,	0x00, LSB_WIDTH, MSB_WIDTH,	0x00, 0x00,	LSB_HEIGHT,	MSB_HEIGHT,
  0x00,	0x00, 0x01,	0x00, BPP, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x14,	0x00, 0xc4,	0x0e, 0x00,	0x00, 0xc4,	0x0e, 0x00,	0x00, 0x00,	0x01,
  0x00,	0x00, 0x00,	0x00, 0x00,	0x00, 0x00,	0x00, 0x00,	0x00, 0x01,	0x01,
  0x01,	0x00, 0x02,	0x02, 0x02,	0x00, 0x03,	0x03, 0x03,	0x00, 0x04,	0x04,
  0x04,	0x00, 0x05,	0x05, 0x05,	0x00, 0x06,	0x06, 0x06,	0x00, 0x07,	0x07,
  0x07,	0x00, 0x08,	0x08, 0x08,	0x00, 0x09,	0x09, 0x09,	0x00, 0x0a,	0x0a,
  0x0a,	0x00, 0x0b,	0x0b, 0x0b,	0x00, 0x0c,	0x0c, 0x0c,	0x00, 0x0d,	0x0d,
  0x0d,	0x00, 0x0e,	0x0e, 0x0e,	0x00, 0x0f,	0x0f, 0x0f,	0x00, 0x10,	0x10,
  0x10,	0x00, 0x11,	0x11, 0x11,	0x00, 0x12,	0x12, 0x12,	0x00, 0x13,	0x13,
  0x13,	0x00, 0x14,	0x14, 0x14,	0x00, 0x15,	0x15, 0x15,	0x00, 0x16,	0x16,
  0x16,	0x00, 0x17,	0x17, 0x17,	0x00, 0x18,	0x18, 0x18,	0x00, 0x19,	0x19,
  0x19,	0x00, 0x1a,	0x1a, 0x1a,	0x00, 0x1b,	0x1b, 0x1b,	0x00, 0x1c,	0x1c,
  0x1c,	0x00, 0x1d,	0x1d, 0x1d,	0x00, 0x1e,	0x1e, 0x1e,	0x00, 0x1f,	0x1f,
  0x1f,	0x00, 0x20,	0x20, 0x20,	0x00, 0x21,	0x21, 0x21,	0x00, 0x22,	0x22,
  0x22,	0x00, 0x23,	0x23, 0x23,	0x00, 0x24,	0x24, 0x24,	0x00, 0x25,	0x25,
  0x25,	0x00, 0x26,	0x26, 0x26,	0x00, 0x27,	0x27, 0x27,	0x00, 0x28,	0x28,
  0x28,	0x00, 0x29,	0x29, 0x29,	0x00, 0x2a,	0x2a, 0x2a,	0x00, 0x2b,	0x2b,
  0x2b,	0x00, 0x2c,	0x2c, 0x2c,	0x00, 0x2d,	0x2d, 0x2d,	0x00, 0x2e,	0x2e,
  0x2e,	0x00, 0x2f,	0x2f, 0x2f,	0x00, 0x30,	0x30, 0x30,	0x00, 0x31,	0x31,
  0x31,	0x00, 0x32,	0x32, 0x32,	0x00, 0x33,	0x33, 0x33,	0x00, 0x34,	0x34,
  0x34,	0x00, 0x35,	0x35, 0x35,	0x00, 0x36,	0x36, 0x36,	0x00, 0x37,	0x37,
  0x37,	0x00, 0x38,	0x38, 0x38,	0x00, 0x39,	0x39, 0x39,	0x00, 0x3a,	0x3a,
  0x3a,	0x00, 0x3b,	0x3b, 0x3b,	0x00, 0x3c,	0x3c, 0x3c,	0x00, 0x3d,	0x3d,
  0x3d,	0x00, 0x3e,	0x3e, 0x3e,	0x00, 0x3f,	0x3f, 0x3f,	0x00, 0x40,	0x40,
  0x40,	0x00, 0x41,	0x41, 0x41,	0x00, 0x42,	0x42, 0x42,	0x00, 0x43,	0x43,
  0x43,	0x00, 0x44,	0x44, 0x44,	0x00, 0x45,	0x45, 0x45,	0x00, 0x46,	0x46,
  0x46,	0x00, 0x47,	0x47, 0x47,	0x00, 0x48,	0x48, 0x48,	0x00, 0x49,	0x49,
  0x49,	0x00, 0x4a,	0x4a, 0x4a,	0x00, 0x4b,	0x4b, 0x4b,	0x00, 0x4c,	0x4c,
  0x4c,	0x00, 0x4d,	0x4d, 0x4d,	0x00, 0x4e,	0x4e, 0x4e,	0x00, 0x4f,	0x4f,
  0x4f,	0x00, 0x50,	0x50, 0x50,	0x00, 0x51,	0x51, 0x51,	0x00, 0x52,	0x52,
  0x52,	0x00, 0x53,	0x53, 0x53,	0x00, 0x54,	0x54, 0x54,	0x00, 0x55,	0x55,
  0x55,	0x00, 0x56,	0x56, 0x56,	0x00, 0x57,	0x57, 0x57,	0x00, 0x58,	0x58,
  0x58,	0x00, 0x59,	0x59, 0x59,	0x00, 0x5a,	0x5a, 0x5a,	0x00, 0x5b,	0x5b,
  0x5b,	0x00, 0x5c,	0x5c, 0x5c,	0x00, 0x5d,	0x5d, 0x5d,	0x00, 0x5e,	0x5e,
  0x5e,	0x00, 0x5f,	0x5f, 0x5f,	0x00, 0x60,	0x60, 0x60,	0x00, 0x61,	0x61,
  0x61,	0x00, 0x62,	0x62, 0x62,	0x00, 0x63,	0x63, 0x63,	0x00, 0x64,	0x64,
  0x64,	0x00, 0x65,	0x65, 0x65,	0x00, 0x66,	0x66, 0x66,	0x00, 0x67,	0x67,
  0x67,	0x00, 0x68,	0x68, 0x68,	0x00, 0x69,	0x69, 0x69,	0x00, 0x6a,	0x6a,
  0x6a,	0x00, 0x6b,	0x6b, 0x6b,	0x00, 0x6c,	0x6c, 0x6c,	0x00, 0x6d,	0x6d,
  0x6d,	0x00, 0x6e,	0x6e, 0x6e,	0x00, 0x6f,	0x6f, 0x6f,	0x00, 0x70,	0x70,
  0x70,	0x00, 0x71,	0x71, 0x71,	0x00, 0x72,	0x72, 0x72,	0x00, 0x73,	0x73,
  0x73,	0x00, 0x74,	0x74, 0x74,	0x00, 0x75,	0x75, 0x75,	0x00, 0x76,	0x76,
  0x76,	0x00, 0x77,	0x77, 0x77,	0x00, 0x78,	0x78, 0x78,	0x00, 0x79,	0x79,
  0x79,	0x00, 0x7a,	0x7a, 0x7a,	0x00, 0x7b,	0x7b, 0x7b,	0x00, 0x7c,	0x7c,
  0x7c,	0x00, 0x7d,	0x7d, 0x7d,	0x00, 0x7e,	0x7e, 0x7e,	0x00, 0x7f,	0x7f,
  0x7f,	0x00, 0x80,	0x80, 0x80,	0x00, 0x81,	0x81, 0x81,	0x00, 0x82,	0x82,
  0x82,	0x00, 0x83,	0x83, 0x83,	0x00, 0x84,	0x84, 0x84,	0x00, 0x85,	0x85,
  0x85,	0x00, 0x86,	0x86, 0x86,	0x00, 0x87,	0x87, 0x87,	0x00, 0x88,	0x88,
  0x88,	0x00, 0x89,	0x89, 0x89,	0x00, 0x8a,	0x8a, 0x8a,	0x00, 0x8b,	0x8b,
  0x8b,	0x00, 0x8c,	0x8c, 0x8c,	0x00, 0x8d,	0x8d, 0x8d,	0x00, 0x8e,	0x8e,
  0x8e,	0x00, 0x8f,	0x8f, 0x8f,	0x00, 0x90,	0x90, 0x90,	0x00, 0x91,	0x91,
  0x91,	0x00, 0x92,	0x92, 0x92,	0x00, 0x93,	0x93, 0x93,	0x00, 0x94,	0x94,
  0x94,	0x00, 0x95,	0x95, 0x95,	0x00, 0x96,	0x96, 0x96,	0x00, 0x97,	0x97,
  0x97,	0x00, 0x98,	0x98, 0x98,	0x00, 0x99,	0x99, 0x99,	0x00, 0x9a,	0x9a,
  0x9a,	0x00, 0x9b,	0x9b, 0x9b,	0x00, 0x9c,	0x9c, 0x9c,	0x00, 0x9d,	0x9d,
  0x9d,	0x00, 0x9e,	0x9e, 0x9e,	0x00, 0x9f,	0x9f, 0x9f,	0x00, 0xa0,	0xa0,
  0xa0,	0x00, 0xa1,	0xa1, 0xa1,	0x00, 0xa2,	0xa2, 0xa2,	0x00, 0xa3,	0xa3,
  0xa3,	0x00, 0xa4,	0xa4, 0xa4,	0x00, 0xa5,	0xa5, 0xa5,	0x00, 0xa6,	0xa6,
  0xa6,	0x00, 0xa7,	0xa7, 0xa7,	0x00, 0xa8,	0xa8, 0xa8,	0x00, 0xa9,	0xa9,
  0xa9,	0x00, 0xaa,	0xaa, 0xaa,	0x00, 0xab,	0xab, 0xab,	0x00, 0xac,	0xac,
  0xac,	0x00, 0xad,	0xad, 0xad,	0x00, 0xae,	0xae, 0xae,	0x00, 0xaf,	0xaf,
  0xaf,	0x00, 0xb0,	0xb0, 0xb0,	0x00, 0xb1,	0xb1, 0xb1,	0x00, 0xb2,	0xb2,
  0xb2,	0x00, 0xb3,	0xb3, 0xb3,	0x00, 0xb4,	0xb4, 0xb4,	0x00, 0xb5,	0xb5,
  0xb5,	0x00, 0xb6,	0xb6, 0xb6,	0x00, 0xb7,	0xb7, 0xb7,	0x00, 0xb8,	0xb8,
  0xb8,	0x00, 0xb9,	0xb9, 0xb9,	0x00, 0xba,	0xba, 0xba,	0x00, 0xbb,	0xbb,
  0xbb,	0x00, 0xbc,	0xbc, 0xbc,	0x00, 0xbd,	0xbd, 0xbd,	0x00, 0xbe,	0xbe,
  0xbe,	0x00, 0xbf,	0xbf, 0xbf,	0x00, 0xc0,	0xc0, 0xc0,	0x00, 0xc1,	0xc1,
  0xc1,	0x00, 0xc2,	0xc2, 0xc2,	0x00, 0xc3,	0xc3, 0xc3,	0x00, 0xc4,	0xc4,
  0xc4,	0x00, 0xc5,	0xc5, 0xc5,	0x00, 0xc6,	0xc6, 0xc6,	0x00, 0xc7,	0xc7,
  0xc7,	0x00, 0xc8,	0xc8, 0xc8,	0x00, 0xc9,	0xc9, 0xc9,	0x00, 0xca,	0xca,
  0xca,	0x00, 0xcb,	0xcb, 0xcb,	0x00, 0xcc,	0xcc, 0xcc,	0x00, 0xcd,	0xcd,
  0xcd,	0x00, 0xce,	0xce, 0xce,	0x00, 0xcf,	0xcf, 0xcf,	0x00, 0xd0,	0xd0,
  0xd0,	0x00, 0xd1,	0xd1, 0xd1,	0x00, 0xd2,	0xd2, 0xd2,	0x00, 0xd3,	0xd3,
  0xd3,	0x00, 0xd4,	0xd4, 0xd4,	0x00, 0xd5,	0xd5, 0xd5,	0x00, 0xd6,	0xd6,
  0xd6,	0x00, 0xd7,	0xd7, 0xd7,	0x00, 0xd8,	0xd8, 0xd8,	0x00, 0xd9,	0xd9,
  0xd9,	0x00, 0xda,	0xda, 0xda,	0x00, 0xdb,	0xdb, 0xdb,	0x00, 0xdc,	0xdc,
  0xdc,	0x00, 0xdd,	0xdd, 0xdd,	0x00, 0xde,	0xde, 0xde,	0x00, 0xdf,	0xdf,
  0xdf,	0x00, 0xe0,	0xe0, 0xe0,	0x00, 0xe1,	0xe1, 0xe1,	0x00, 0xe2,	0xe2,
  0xe2,	0x00, 0xe3,	0xe3, 0xe3,	0x00, 0xe4,	0xe4, 0xe4,	0x00, 0xe5,	0xe5,
  0xe5,	0x00, 0xe6,	0xe6, 0xe6,	0x00, 0xe7,	0xe7, 0xe7,	0x00, 0xe8,	0xe8,
  0xe8,	0x00, 0xe9,	0xe9, 0xe9,	0x00, 0xea,	0xea, 0xea,	0x00, 0xeb,	0xeb,
  0xeb,	0x00, 0xec,	0xec, 0xec,	0x00, 0xed,	0xed, 0xed,	0x00, 0xee,	0xee,
  0xee,	0x00, 0xef,	0xef, 0xef,	0x00, 0xf0,	0xf0, 0xf0,	0x00, 0xf1,	0xf1,
  0xf1,	0x00, 0xf2,	0xf2, 0xf2,	0x00, 0xf3,	0xf3, 0xf3,	0x00, 0xf4,	0xf4,
  0xf4,	0x00, 0xf5,	0xf5, 0xf5,	0x00, 0xf6,	0xf6, 0xf6,	0x00, 0xf7,	0xf7,
  0xf7,	0x00, 0xf8,	0xf8, 0xf8,	0x00, 0xf9,	0xf9, 0xf9,	0x00, 0xfa,	0xfa,
  0xfa,	0x00, 0xfb,	0xfb, 0xfb,	0x00, 0xfc,	0xfc, 0xfc,	0x00, 0xfd,	0xfd,
  0xfd,	0x00, 0xfe,	0xfe, 0xfe,	0x00, 0xff,	0xff, 0xff,	0x00};

int	main(void)
{
	char **postvars	= NULL;	/* POST	request	data repository	*/
	char **getvars = NULL; /* GET request data repository */
	int	form_method; /*	POST = 1, GET =	0 */
	int	i;


	form_method	= getRequestMethod();


	if(form_method == POST)	{
		getvars	= getGETvars();
		postvars = getPOSTvars();
	} else if(form_method == GET) {
		getvars	= getGETvars();
	}

/* Parse Request */

	if(form_method == POST)	{

/*Preset checkbox settings*/

for	(i=0; postvars[i]; i+= 2) {

	if(strncmp(postvars[i],	"D1", 2) ==	0){	cgiinfo.reg	= atoi(postvars[i +	1]); } else
	if(strncmp(postvars[i],	"T1", 2) ==	0){	cgiinfo.val	= atoi(postvars[i +	1]); } else
	if(strncmp(postvars[i],	"B1", 2) ==	0){	cgiinfo.cmd	= CAPTURE; } else
	if(strncmp(postvars[i],	"B2", 2) ==	0){	cgiinfo.cmd	= RESETREG;	} else
	if(strncmp(postvars[i],	"B3", 2) ==	0){	cgiinfo.cmd	= PRGREG; };

	}
}

	set_gpio(MICRON_STANDBY, "0");

	switch(cgiinfo.cmd)
	{
		case CAPTURE:
		capture(form_method, getvars, postvars);
		break;

		case RESETREG:
		reset_reg(form_method, getvars,	postvars);
		break;

		case PRGREG:
		prg_reg(form_method, getvars, postvars);
		break;

		default:
		break;
	}

	set_gpio(MICRON_STANDBY, "1");

  exit(0);

};

void prg_reg(int form_method, char **getvars, char **postvars)
{

			htmlHeader("FCD	Write I2C Reg");
			htmlBody();
			printf("<p><font face=\"Tahoma\" size=\"7\">Wrote Register[0x%X] = 0x%X	\n</font></p>",(unsigned char)cgiinfo.reg,(unsigned	short)cgiinfo.val);
			htmlFooter();
			cleanUp(form_method, getvars, postvars);

			fflush(stdout);

	i2c_write_register(I2C_DEVICE,DEVID,(unsigned char)cgiinfo.reg,(unsigned short)cgiinfo.val);

  return;
}

void reset_reg(int form_method,	char **getvars,	char **postvars)
{

			htmlHeader("FCD	Write I2C Reg");
			htmlBody();
			printf("<p><font face=\"Tahoma\" size=\"7\">Reset Camera Module	to it's	default	\n</font></p>");
			htmlFooter();
			cleanUp(form_method, getvars, postvars);

			fflush(stdout);

	i2c_write_register(I2C_DEVICE,DEVID,0x0D,1);
	i2c_write_register(I2C_DEVICE,DEVID,0x0D,0);

  return;
}

void capture(int form_method, char **getvars, char **postvars)
{

	int	fd;
	char * buffer;

		set_gpio(MICRON_STANDBY, "0");
		set_gpio(FS3, "0");
		set_gpio(MICRON_LED, "1");
		set_gpio(MICRON_TRIGGER, "0");



	/* Allocate	meory for the raw image	and	BMP	Header */
	buffer = (char*) malloc	(IMAGESIZE + sizeof(bmphead));


	/* Copy	the	BMP	header information into	the	buffer */
	(void) memcpy( buffer, bmphead,	sizeof(bmphead)	);

	/* Open	/dev/ppi */
	fd = open("/dev/ppi0", O_RDWR,0);
	if (fd == -1) {

			htmlHeader("FCD	capture");
			htmlBody();
			printf("<p><font face=\"Tahoma\" size=\"7\">Could not open dev\/ppi	: %d \n</font></p>",errno);
			htmlFooter();
			cleanUp(form_method, getvars, postvars);
			fflush(stdout);
		exit(1);
	}

	ioctl(fd, CMD_PPI_SET_PIXELS_PER_LINE, WIDTH);
	ioctl(fd, CMD_PPI_SET_LINES_PER_FRAME, HEIGHT);

#ifdef USETRIGGER
	ioctl(fd, CMD_SET_TRIGGER_GPIO,	TRIGGER_PF6);
#endif

	/* Read	the	raw	image data from	the	PPI	*/
		read(fd,buffer+sizeof(bmphead), HEIGHT * WIDTH);

	/* Close PPI */
	close(fd);

	
	/* When data is transmitted from a	CMOS camera	that uses the Bayer	Color Filter Array
	(Bayer CFA)	we get a stream	of alternating values of Red (R) Green (G) for odd rows
	and	alternating	values of Green	(G)	and	Blue (B) for even rows.

	In order to	construct an RGB picture, we need to calculate Green and Blue values for
	each Red pixel,	Blue and Red values	for	each Green pixel and Red and Green values
	for	each Blue pixel.

	http://www.helicontech.co.il/whitepapers/bayer-rgb.html	*/

	/*For CFA Sensors: cfa2rgb( buffer+sizeof(bmphead) );*/

	/* Write some HTTP and HTML	stuff to the browser at	the	far	end	*/
	htmlHeaderNocache("FCD Demo	Web	Page");
	htmlBody();

	/* Write image back	to file	*/
	WriteIMG(buffer,IMAGESIZE+sizeof(bmphead));

	printf("\n<img border=\"0\"	src=\"/var/img.bmp?id=%s\" width=\"%d\" \
	  height=\"%d\" align=\"left\">\n",itostr	(getrand(6)	,6,1,1),WIDTH,HEIGHT);

	htmlFooter();
	fflush(stdout);

	cleanUp(form_method, getvars, postvars);

	set_gpio(MICRON_LED, "0");

  return;
}


int	WriteIMG (char * buffer, unsigned long lSize)
{
  FILE * pFile;
  pFile	= fopen	(FILENAME_OUT ,	"w");
  fwrite (buffer , 1 , lSize , pFile);

  // terminate

  free (buffer);
  fclose (pFile);
  return 0;
}


int	set_gpio(char *	flag, char*	value)
{
	int	fd0;

	fd0	= open(flag, O_RDWR,0);
	if (fd0	== -1) {
		printf("%s open	error %d\n",flag,errno);
		return -1;
	}

		ioctl(fd0, SET_FIO_DIR,	OUTPUT);
		ioctl(fd0, SET_FIO_POLAR, 0);
		ioctl(fd0, SET_FIO_EDGE, 0);
		ioctl(fd0, SET_FIO_BOTH, 0);
		ioctl(fd0, SET_FIO_INEN, 0);

	write(fd0,value,sizeof("0"));

	close(fd0);

	return 0;
}

#define	OUT_DEC	1		//Converts the number based	on the decimal format
#define	OUT_BIN	2		// Converts	the	number based on	the	binary format
#define	OUT_HEX	3		//Converts the number based	on the hexadecimal format

char *
itostr (u_int iNumber, u_char cDigits, u_char cMode, u_char	cDec_mode)
{
  static char cBuffer[31];
  char c, cMod;
  u_int	iDivisor = 10;

  if (cDigits >	30)
	{
	  cBuffer[0] = '\0';
	}
  else
	{
	  if (iNumber)
	{
	  switch (cMode)
		{
		case OUT_BIN:
		  iDivisor = 2;
		  break;

		case OUT_DEC:
		  iDivisor = 10;
		  break;

		case OUT_HEX:
		  iDivisor = 16;
		  break;
		}

	  for (c = cDigits;	c >	0; c--)
		{
		  cMod = iNumber % iDivisor;
		  if (cMode	== OUT_HEX)
		{
		  if (cMod > 9)
			cBuffer[c -	1] = cMod +	55;
		  else
			cBuffer[c -	1] = cMod +	48;
		}
		  else
		{
		  if (cMode	== OUT_DEC)
			{
			  if ((!iNumber) &&	(cDec_mode))
			cBuffer[c -	1] = ' ';
			  else
			cBuffer[c -	1] = cMod +	48;
			}
		  else
			cBuffer[c -	1] = cMod +	48;
		}
		  iNumber /= iDivisor;
		}

	  cBuffer[cDigits] = '\0';
	}
	  else
	{
	  if ((cMode ==	OUT_DEC) &&	(cDec_mode))
		{
		  for (c = 0; c	< cDigits; c++)
		cBuffer[(unsigned char)	c] = ' ';
		  cBuffer[cDigits -	1] = '0';
		  cBuffer[cDigits] = '\0';
		}
	  else
		{
		  for (c = 0; c	< cDigits; c++)
		cBuffer[(unsigned char)	c] = '0';
		  cBuffer[(unsigned	char) c] = '\0';
		}
	}
	}

  return (cBuffer);
};


int
getrand	(int max)
{

  int j;
  struct timeval tv;

  if (gettimeofday (&tv, NULL) != 0)
	{
	  printf ("Error getting time\n");
	}

  srand	(tv.tv_sec);
  j	= 1	+ (int)	((float) max * rand	() / (23457	+ 1.0));

  return j;
};

