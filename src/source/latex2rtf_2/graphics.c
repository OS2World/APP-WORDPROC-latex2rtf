/* graphics.c - routines that handle LaTeX graphics commands

Copyright (C) 2001-2002 The Free Software Foundation

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

This file is available from http://sourceforge.net/projects/latex2rtf/
 
Authors:
    2001-2002 Scott Prahl
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#ifdef UNIX
#include <unistd.h>
#endif
#include "cfg.h"
#include "main.h"
#include "graphics.h"
#include "parser.h"
#include "util.h"
#include "commands.h"
#include "convert.h"
#include "equation.h"

#define POINTS_PER_M 2834.65

/* Little endian macros to convert to and from host format to network byte ordering */
#define LETONS(A) ((((A) & 0xFF00) >> 8) | (((A) & 0x00FF) << 8))
#define LETONL(A) ((((A) & 0xFF000000) >> 24) | (((A) & 0x00FF0000) >>  8) | \
                  (((A) & 0x0000FF00) <<  8) | (((A) & 0x000000FF) << 24) )
/*
Version 1.6 RTF files can include pictures as follows

<pict> 			'{' \pict (<brdr>? & <shading>? & <picttype> & <pictsize> & <metafileinfo>?) <data> '}'
<picttype>		\emfblip | \pngblip | \jpegblip | \macpict | \pmmetafile | \wmetafile 
			 	         | \dibitmap <bitmapinfo> | \wbitmap <bitmapinfo>
<bitmapinfo> 	\wbmbitspixel & \wbmplanes & \wbmwidthbytes
<pictsize> 		(\picw & \pich) \picwgoal? & \pichgoal? \picscalex? & \picscaley? & \picscaled? & \piccropt? & \piccropb? & \piccropr? & \piccropl?
<metafileinfo> 	\picbmp & \picbpp
<data> 			(\bin #BDATA) | #SDATA

\emfblip 				Source of the picture is an EMF (enhanced metafile).
\pngblip 				Source of the picture is a PNG.
\jpegblip 				Source of the picture is a JPEG.
\shppict 				Specifies a Word 97-2000 picture. This is a destination control word.
\nonshppict 			Specifies that Word 97-2000 has written a {\pict destination that it 
						will not read on input. This keyword is for compatibility with other readers.
\macpict                Source of the picture is PICT file (Quickdraw)
\pmmetafileN            Source of the picture is an OS/2 metafile
\wmetafileN             Source of the picture is a Windows metafile
\dibitmapN              Source of the picture is a Windows device-independent bitmap
\wbitmapN               Source of the picture is a Windows device-dependent bitmap
*/

typedef struct _WindowsMetaHeader
{
	unsigned short	FileType;		/* Type of metafile (0=memory, 1=disk) */
	unsigned short	HeaderSize;		/* Size of header in WORDS (always 9) */
	unsigned short	Version;		/* Version of Microsoft Windows used */
	unsigned long	FileSize;		/* Total size of the metafile in WORDs */
	unsigned short	NumOfObjects;	/* Number of objects in the file */
	unsigned long	MaxRecordSize;	/* The size of largest record in WORDs */
	unsigned short	NumOfParams;	/* Not Used (always 0) */
} WMFHEAD;

typedef struct _PlaceableMetaHeader
{
	unsigned long	Key;			/* Magic number (always 0x9AC6CDD7) */
	unsigned short	Handle;			/* Metafile HANDLE number (always 0) */
	short			Left;			/* Left coordinate in twips */
	short			Top;			/* Top coordinate in twips */
	short			Right;			/* Right coordinate in twips */
	short			Bottom;			/* Bottom coordinate in twips */
	unsigned short	Inch;			/* Scaling factor, 1440 => 1:1, 360 => 4:1, 2880 => 1:2 (half size) */
	unsigned long	Reserved;		/* Reserved (always 0) */
	unsigned short	Checksum;		/* Checksum value for previous 10 WORDs */
} PLACEABLEMETAHEADER;

typedef struct _EnhancedMetaHeader
{
	unsigned long	RecordType;		/* Record type (always 0x00000001)*/
	unsigned long	RecordSize;		/* Size of the record in bytes */
	long			BoundsLeft;		/* Left inclusive bounds */
	long			BoundsTop;		/* Top inclusive bounds */
	long			BoundsRight;	/* Right inclusive bounds */
	long			BoundsBottom;	/* Bottom inclusive bounds */
	long			FrameLeft;		/* Left side of inclusive picture frame */
	long			FrameTop;		/* Top side of inclusive picture frame */
	long			FrameRight;		/* Right side of inclusive picture frame */
	long			FrameBottom;	/* Bottom side of inclusive picture frame */
	unsigned long	Signature;		/* Signature ID (always 0x464D4520) */
	unsigned long	Version;		/* Version of the metafile */
	unsigned long	Size;			/* Size of the metafile in bytes */
	unsigned long	NumOfRecords;	/* Number of records in the metafile */
	unsigned short	NumOfHandles;	/* Number of handles in the handle table */
	unsigned short	Reserved;		/* Not used (always 0) */
	unsigned long	SizeOfDescrip;	/* Size of description string in WORDs */
	unsigned long	OffsOfDescrip;	/* Offset of description string in metafile */
	unsigned long	NumPalEntries;	/* Number of color palette entries */
	long			WidthDevPixels;	/* Width of reference device in pixels */
	long			HeightDevPixels;/* Height of reference device in pixels */
	long			WidthDevMM;		/* Width of reference device in millimeters */
	long			HeightDevMM;	/* Height of reference device in millimeters */
} ENHANCEDMETAHEADER;

typedef struct _EmrFormat
{
    unsigned long 	Signature;    	/* 0x46535045 for EPS, 0x464D4520 for EMF */
    unsigned long 	Version;      	/* EPS version number or 0x00000001 for EMF */
    unsigned long 	Data;         	/* Size of data in bytes */
    unsigned long 	OffsetToData; 	/* Offset to data */
} EMRFORMAT;

typedef struct _GdiCommentMultiFormats
{
    unsigned long 	Identifier;		/* Comment ID (0x43494447) */
    unsigned long 	Comment;		/* Multiformats ID (0x40000004) */
    long  			BoundsLeft;		/* Left side of bounding rectangle */
    long  			BoundsRight;	/* Right side of bounding rectangle */
    long  			BoundsTop;		/* Top side of bounding rectangle */
    long  			BoundsBottom;	/* Bottom side of bounding rectangle */
    unsigned long 	NumFormats;		/* Number of formats in comment */
    EMRFORMAT 		*Data;			/* Array of comment data */
} GDICOMMENTMULTIFORMATS;

static void
my_unlink(char *filename)
/******************************************************************************
     purpose : portable routine to delete filename
 ******************************************************************************/
{
#ifdef UNIX
	unlink(filename);
#endif
}

static
void PicComment(short label, short size, FILE *fp)
{
	short long_comment  = 0x00A1;
	short short_comment = 0x00A0;
	short tag;
	
	tag = (size) ? long_comment : short_comment;
	
	if (g_little_endian) {
		tag   = LETONS(tag);
		label = LETONS(label);
		size  = LETONS(size);
	}
	
	fwrite(&tag,  2,1,fp);
	fwrite(&label,2,1,fp);
	if (size) fwrite(&size, 2,1,fp);
}

static char *
strdup_new_extension(char *s, char *old_ext, char *new_ext)
{
	char *new_name, *p;
	
	p=strstr(s,old_ext);
	if (p==NULL) return NULL;
	
	new_name = strdup_together(s,new_ext);
	p=strstr(new_name,old_ext);
	strcpy(p,new_ext);
	return new_name;
}

static char *
strdup_tmp_path(char *s)
{
	char *tmp, *new_name;
	
	if (s==NULL) return NULL;
	
	tmp = getTmpPath();
	new_name = strdup_together(tmp,s);
	free(tmp);
	return new_name;
}


static char *
eps_to_pict(char *s)
/******************************************************************************
     purpose : create a pict file from an EPS file and return file name
               the pict file contains a bitmap version for viewing
               and the original EPS for printing
 ******************************************************************************/
{
	FILE *fp_eps, *fp_pict_bitmap, *fp_pict_eps;
	char *cmd, *p, *pict_bitmap, *pict_eps, *eps, buffer[560];
	long ii, pict_bitmap_size, eps_size;
	short PostScriptBegin = 190;
	short PostScriptEnd   = 191;
	short PostScriptHandle= 192;
	short handle_size;
	unsigned char byte;

	diagnostics(1, "eps_to_pict filename = <%s>", s);
	p = strdup_new_extension(s, ".eps", "a.pict");
	if (p == NULL) {
		p = strdup_new_extension(s, ".EPS", "a.pict");
		if (p == NULL) return NULL;
	}
	pict_bitmap = strdup_tmp_path(p);
	free(p);
	
	p = strdup_new_extension(s, ".eps", ".pict");
	if (p == NULL) {
		p = strdup_new_extension(s, ".EPS", ".pict");
		if (p == NULL) {
			free(pict_bitmap);
			return NULL;
		}
	}
	pict_eps = strdup_tmp_path(p);
	free(p);

	eps = strdup_together(g_home_dir,s);

	/* create a bitmap version of the eps file */
	cmd = (char *) malloc(strlen(eps)+strlen(pict_bitmap)+strlen("convert -crop 0x0  ")+1);
	sprintf(cmd, "convert -crop 0x0 %s %s", eps, pict_bitmap);	
	diagnostics(1,"%s",cmd);
	system(cmd);
	free(cmd);
	
	/* open the eps file and make sure that it is less than 32k */
 	fp_eps = fopen (eps, "rb");
	if (fp_eps==NULL) return NULL;
	fseek(fp_eps, 0, SEEK_END);
  	eps_size = ftell (fp_eps);
  	if (eps_size > 32000) {
  		fclose(fp_eps); 
  		free(pict_eps);
  		diagnostics(WARNING, "EPS file >32K ... using bitmap only");
  		return pict_bitmap;
  	}
  	rewind (fp_eps);
  	diagnostics(WARNING, "eps size is 0x%X bytes", eps_size);
  
	/*open bitmap pict file and get file size */
	fp_pict_bitmap = fopen(pict_bitmap, "rb");
	if (fp_pict_bitmap == NULL) {
		fclose(fp_eps);
  		free(pict_eps);
		return pict_bitmap;
	}
	fseek(fp_pict_bitmap, 0, SEEK_END);
  	pict_bitmap_size = ftell(fp_pict_bitmap);
  	rewind(fp_pict_bitmap);

	/*open new pict file */
	fp_pict_eps = fopen(pict_eps, "w");
	if (fp_pict_eps == NULL) {
		fclose(fp_pict_bitmap); 
		fclose(fp_eps);
  		free(pict_eps);
		return pict_bitmap;
	}

	/*copy header 512 buffer + 40 byte header*/
	fread( &buffer,1,512+40,fp_pict_bitmap);
	fwrite(&buffer,1,512+40,fp_pict_eps);
	
	/* insert comment that allows embedding postscript */
	PicComment(PostScriptBegin,0,fp_pict_eps);
	
	/*copy bitmap 512+40 bytes of header + 2 bytes at end */
	for (ii=512+40+2; ii<pict_bitmap_size; ii++) {
		fread(&byte,1,1,fp_pict_bitmap);
		fwrite(&byte,1,1,fp_pict_eps);
	}
	fclose(fp_pict_bitmap);
	
	/*copy eps graphic (write an even number of bytes) */
	handle_size = eps_size;   
	if (eps_size % 2) 	
		handle_size ++;	
	PicComment(PostScriptHandle,handle_size,fp_pict_eps);
	for (ii=0; ii<eps_size; ii++) {
		fread(&byte,1,1,fp_eps);
		fwrite(&byte,1,1,fp_pict_eps);
	}
	if (eps_size % 2) {
		byte = ' ';
		fwrite(&byte,1,1,fp_pict_eps);
	}		
	fclose(fp_eps);
	
	/*close file*/
	PicComment(PostScriptEnd,0,fp_pict_eps);
	byte = 0x00;
	fwrite(&byte,1,1,fp_pict_eps);
	byte = 0xFF;
	fwrite(&byte,1,1,fp_pict_eps);
	fclose(fp_pict_eps);
	return pict_eps;
}

static char *
eps_to_png(char *eps)
/******************************************************************************
     purpose : create a png file from an EPS file and return file name
 ******************************************************************************/
{
	char *cmd, *s1, *p, *png, *tmp;
	diagnostics(1, "filename = <%s>", eps);

	s1 = strdup(eps);
	if ((p=strstr(s1,".eps")) == NULL && (p=strstr(s1,".EPS")) == NULL) {
		diagnostics(1, "<%s> is not an EPS file", eps);
		free(s1);
		return NULL;
	}

	strcpy(p,".png");
	tmp = getTmpPath();
	png = strdup_together(tmp,s1);
	cmd = (char *) malloc(strlen(eps)+strlen(png)+10);
	sprintf(cmd, "convert %s %s", eps, png);	
	system(cmd);	
	
	free(cmd);
	free(tmp);
	free(s1);
	return png;
}

static char *
eps_to_emf(char *eps)
/******************************************************************************
     purpose : create a wmf file from an EPS file and return file name
 ******************************************************************************/
{
	FILE *fp;
	char *cmd, *s1, *p, *emf, *tmp;
	char ans[50];
	long width, height;
	diagnostics(1, "filename = <%s>", eps);

	s1 = strdup(eps);
	if ((p=strstr(s1,".eps")) == NULL && (p=strstr(s1,".EPS")) == NULL) {
		diagnostics(1, "<%s> is not an EPS file", eps);
		free(s1);
		return NULL;
	}

	strcpy(p,".wmf");
	tmp = getTmpPath();
	emf = strdup_together(tmp,s1);
	
	/* Determine bounding box for EPS file */
	cmd = (char *) malloc(strlen(eps)+strlen("identify -format \"%w %h\" ")+1);
	sprintf(cmd, "identify -format \"%%w %%h\" %s", eps);	
	fp=popen(cmd,"r");	
	fgets(ans, 50, fp);
	sscanf(ans,"%ld %ld",&width,&height);
	pclose(fp);	
 	free(cmd);
	
	fp = fopen(emf, "wb");
	
	/* write ENHANCEDMETAHEADER */
	
	/* write GDICOMMENTMULTIFORMATS */
	
	/* write EMRFORMAT containing EPS */

	free(tmp);
	free(s1);
	fclose(fp);
	return emf;
}


static void 
PutHexFile(FILE *fp)
/******************************************************************************
     purpose : write entire file to RTF as hex
 ******************************************************************************/
{
int i, c;

	i = 0;
	while ((c = fgetc(fp)) != EOF) {
		fprintRTF("%.2x", c);
		if (++i > 126) {		/* keep lines 254 chars long */
			i = 0;
			fprintRTF("\n");
		}	
	}
}

static void 
PutPictFile(char * s, int full_path)
/******************************************************************************
     purpose : Include .pict file in RTF
 ******************************************************************************/
{
FILE *fp;
char *pict;
short buffer[5];
short top, left, bottom, right;
int width, height;

	if (full_path)
		pict = strdup(s);
	else
		pict = strdup_together(g_home_dir, s);		
	diagnostics(1, "PutPictFile <%s>", pict);

	fp = fopen(pict, "rb");
	free(pict);
	if (fp == NULL) return;
	
	if (fseek(fp, 514L, SEEK_SET) || fread(buffer, 2, 4, fp) != 4) {
		diagnostics (WARNING, "Cannot read graphics file <%s>", s);
		fclose(fp);
		return;
	}
	
	top    = buffer[0];
	left   = buffer[1];
	bottom = buffer[2];
	right  = buffer[3];

	width  = right - left;
	height = bottom - top;
	
	if (g_little_endian) {
		top    = LETONS(top);
		bottom = LETONS(bottom);
		left   = LETONS(left);
		right  = LETONS(right);
	}

	diagnostics(1,"top = %d, bottom = %d", top, bottom);
	diagnostics(1,"left = %d, right = %d", left, right);
	diagnostics(1,"width = %d, height = %d", width, height);
	fprintRTF("\n{\\pict\\macpict\\picw%d\\pich%d\n", width, height);

	fseek(fp, -10L, SEEK_CUR);
	PutHexFile(fp);
	fprintRTF("}\n");
	fclose(fp);
}

static void
GetPngSize(char *s, unsigned long *w, unsigned long *h)
/******************************************************************************
     purpose : determine height and width of file
 ******************************************************************************/
{
FILE *fp;
unsigned char buffer[16];
unsigned long width, height;
char reftag[9] = "\211PNG\r\n\032\n";
char refchunk[5] = "IHDR";

	*w = 0;
	*h = 0;
	fp = fopen(s, "rb");
	if (fp == NULL) return;

	if (fread(buffer,1,16,fp)<16) {
		diagnostics (WARNING, "Cannot read graphics file <%s>", s);
		fclose(fp);
		return;
	}

	if (memcmp(buffer,reftag,8)!=0 || memcmp(buffer+12,refchunk,4)!=0) {
		diagnostics (WARNING, "Graphics file <%s> is not a PNG file!", s);
		fclose(fp);
		return;
	}

	if (fread(&width,4,1,fp)!=1 || fread(&height,4,1,fp)!=1) {
		diagnostics (WARNING, "Cannot read graphics file <%s>", s);
		fclose(fp);
		return;
	}

	if (g_little_endian) {
		width  = LETONL(width);
		height = LETONL(height);
	}

	*w = width;
	*h = height;
	fclose(fp);
}

#ifdef INLINE_EQ_ALIGN

void GetPngOffset(char *file, int *offset)
/*--------------------------------------*/
{ char buf[1024],*p;
  *offset=0;
  strcpy(buf,file);
  if((p=strstr(buf,".png"))!=0)
   { FILE *F;
     strcpy(p,".off");
     if((F=fopen(buf,"r"))!=0) 
           { int x;
             buf[0]=0;      
             fgets(buf,1024,F);
             if(sscanf(buf,"%d",&x)==1)*offset=x;
             fclose(F);
           }
   }
}
  
#endif

void 
PutPngFile(char * s, double scale, int full_path)
/******************************************************************************
     purpose : Include .png file in RTF
 ******************************************************************************/
{
	FILE *fp;
	char *png;
	unsigned long width, height, w, h;
	int iscale;
#ifdef INLINE_EQ_ALIGN
        int offset;
        unsigned long o;
#endif
	if (full_path)
		png = strdup(s);
	else
		png = strdup_together(g_home_dir, s);		
	diagnostics(1, "PutPngFile <%s>",png);

	GetPngSize(png, &width, &height);
#ifdef INLINE_EQ_ALIGN
        GetPngOffset(png,&offset); 
	diagnostics(1,"offset = %ld", offset);
#endif
	diagnostics(1,"width = %ld, height = %ld", width, height);
	if (width==0) return;
	
	fp = fopen(png, "rb");
	free(png);
	if (fp == NULL) return;

	w = (unsigned long)( 100000.0*width  )/ ( 20* POINTS_PER_M );
	h = (unsigned long)( 100000.0*height )/ ( 20* POINTS_PER_M );
#ifdef INLINE_EQ_ALIGN
        o = (unsigned long)( 100000.0*abs(offset)*scale )/ ( 20* POINTS_PER_M );        
        if(g_inline_eq_align&&o!=0)
   	     fprintRTF("\n{\\%s%d\\pict\\pngblip\\picw%ld\\pich%ld",
                          offset>0?"up":"dn", o ,w, h);
        else 
#endif
	fprintRTF("\n{\\pict\\pngblip\\picw%ld\\pich%ld", w, h);
	fprintRTF("\\picwgoal%ld\\pichgoal%ld", width*20, height*20);
	if (scale != 1.0) {
		iscale = (int) (scale * 100);
		fprintRTF("\\picscalex%d\\picscaley%d", iscale, iscale);
	}
	fprintRTF("\n");
	rewind(fp);
	PutHexFile(fp);
	fprintRTF("}\n");
	fclose(fp);
}

static void 
PutJpegFile(char * s)
/******************************************************************************
     purpose : Include .jpeg file in RTF
 ******************************************************************************/
{
	FILE *fp;
	char *jpg;
	unsigned short buffer[2];
	int m,c;
	unsigned short width, height;
	unsigned long w, h;

	jpg = strdup_together(g_home_dir,s);
	fp = fopen(jpg, "rb");
	free(jpg);
	if (fp == NULL) return;

	if ((c=fgetc(fp)) != 0xFF && (c=fgetc(fp)) != 0xD8) {
		fclose(fp);
		diagnostics(WARNING, "<%s> is not really a JPEG file --- skipping");
		return;
	}
	
	do {  /* Look for SOFn tag */
	
		  while (!feof(fp) && fgetc(fp) != 0xFF){}   		/* Find 0xFF byte */
		  
		  while (!feof(fp) && (m=fgetc(fp)) == 0xFF){}  	/* Skip multiple 0xFFs */
		  
	} while (!feof(fp) && m!=0xC0 && m!=0xC1 && m!=0xC2 && m!=0xC3 && m!=0xC5 && m!=0xC6 && m!=0xC7 &&
					      m!=0xC9 && m!=0xCA && m!=0xCB && m!=0xCD && m!=0xCE && m!=0xCF );    
	
	if (fseek(fp, 3, SEEK_CUR) || fread(buffer,2,2,fp) != 2) {
		diagnostics (WARNING, "Cannot read graphics file <%s>", s);
		fclose(fp);
		return;
	}

	width = buffer[1];
	height = buffer[0];

	if (g_little_endian) {
		width  = LETONS(width);
		height = LETONS(height);
	}

	diagnostics(1,"width = %d, height = %d", width, height);

	w = (unsigned long)( 100000.0*width  )/ ( 20* POINTS_PER_M );
	h = (unsigned long)( 100000.0*height )/ ( 20* POINTS_PER_M );
	fprintRTF("\n{\\pict\\jpegblip\\picw%ld\\pich%ld", w, h);
	fprintRTF("\\picwgoal%ld\\pichgoal%ld\n", width*20, height*20);

	rewind(fp);
	PutHexFile(fp);
	fprintRTF("}\n");
	fclose(fp);
}

static void
PutEmfFile(char *s, int full_path)
{
	FILE *fp;
	char *emf;
	unsigned long	RecordType;		/* Record type (always 0x00000001)*/
	unsigned long	RecordSize;		/* Size of the record in bytes */
	long			BoundsLeft;		/* Left inclusive bounds */
	long			BoundsRight;	/* Right inclusive bounds */
	long			BoundsTop;		/* Top inclusive bounds */
	long			BoundsBottom;	/* Bottom inclusive bounds */
	long			FrameLeft;		/* Left side of inclusive picture frame */
	long			FrameRight;		/* Right side of inclusive picture frame */
	long			FrameTop;		/* Top side of inclusive picture frame */
	long			FrameBottom;	/* Bottom side of inclusive picture frame */
	unsigned long	Signature;		/* Signature ID (always 0x464D4520) */
	long			w,h,width,height;
	
	if (full_path)
		emf = strdup(s);
	else
		emf = strdup_together(g_home_dir, s);		
	diagnostics(1, "PutEmfFile <%s>",emf);
	fp = fopen(emf,"rb");
	free(emf);
	if (fp == NULL) return;

/* extract size information*/
	if (fread(&RecordType,4,1,fp)  != 1) goto out;
	if (fread(&RecordSize,4,1,fp)  != 1) goto out;
	if (fread(&BoundsLeft,4,1,fp)  != 1) goto out;
	if (fread(&BoundsTop,4,1,fp)   != 1) goto out;
	if (fread(&BoundsRight,4,1,fp) != 1) goto out;
	if (fread(&BoundsBottom,4,1,fp)!= 1) goto out;
	if (fread(&FrameLeft,4,1,fp)   != 1) goto out;
	if (fread(&FrameRight,4,1,fp)  != 1) goto out;
	if (fread(&FrameTop,4,1,fp)    != 1) goto out;
	if (fread(&FrameBottom,4,1,fp) != 1) goto out;
	if (fread(&Signature,4,1,fp)   != 1) goto out;

	if (!g_little_endian) {
		RecordType   = LETONL(RecordType);
		RecordSize   = LETONL(RecordSize);
		BoundsLeft   = LETONL(BoundsLeft);
		BoundsTop    = LETONL(BoundsTop);
		BoundsRight  = LETONL(BoundsRight);
		BoundsBottom = LETONL(BoundsBottom);
		FrameLeft    = LETONL(FrameLeft);
		FrameRight   = LETONL(FrameRight);
		FrameTop     = LETONL(FrameTop);
		FrameBottom  = LETONL(FrameBottom);
		Signature    = LETONL(Signature);
	}

	if (RecordType != 1 || Signature != 0x464D4520) goto out;
	height = BoundsBottom-BoundsTop;
	width  = BoundsRight-BoundsLeft;
	
	w = (unsigned long)( 100000.0*width  )/ ( 20* POINTS_PER_M );
	h = (unsigned long)( 100000.0*height )/ ( 20* POINTS_PER_M );
	diagnostics(1,"width = %ld, height = %ld", width, height);
	fprintRTF("\n{\\pict\\emfblip\\picw%ld\\pich%ld", w, h);
	fprintRTF("\\picwgoal%ld\\pichgoal%ld\n", width*20, height*20);

/* write file */
	rewind(fp);
	PutHexFile(fp);
	fprintRTF("}\n");
	fclose(fp);
	return;

out:
	diagnostics(WARNING,"Problem with file %s --- not included",s);
	fclose(fp);
}

static void
PutWmfFile(char *s)
/******************************************************************************
 purpose   : Insert WMF file (from g_home_dir) into RTF file
 ******************************************************************************/
{
	FILE *fp;
	char *wmf;
	unsigned long	Key;			/* Magic number (always 0x9AC6CDD7) */
	unsigned short	FileType;		/* Type of metafile (0=memory, 1=disk) */
	unsigned short	HeaderSize;		/* Size of header in WORDS (always 9) */
	unsigned short	Handle;			/* Metafile HANDLE number (always 0) */
	short			Left;			/* Left coordinate in twips */
	short			Top;			/* Top coordinate in twips */
	short			Right;			/* Right coordinate in twips */
	short			Bottom;			/* Bottom coordinate in twips */
	int 			width, height;
	
	/* open the proper file */
	wmf = strdup_together(g_home_dir,s);
	diagnostics(1, "PutWmfFile <%s>", wmf);
	fp = fopen(wmf, "rb");
	free(wmf);
	if (fp == NULL) return;

	/* verify file is actually WMF and get size */
	if (fread(&Key,4,1,fp) != 1) goto out;
	if (!g_little_endian) Key  = LETONL(Key);

	if (Key == 0x9AC6CDD7) {		/* file is placeable metafile */
		if (fread(&Handle,2,1,fp) != 1) goto out;
		if (fread(&Left,2,1,fp)   != 1) goto out;
		if (fread(&Top,2,1,fp)    != 1) goto out;
		if (fread(&Right,2,1,fp)  != 1) goto out;
		if (fread(&Bottom,2,1,fp) != 1) goto out;

		if (!g_little_endian) {
			Left   = LETONS(Left);
			Top    = LETONS(Top);
			Right  = LETONS(Right);
			Bottom = LETONS(Bottom);
		}
		
		width  = abs(Right - Left);
		height = abs(Top-Bottom);

	} else {					/* file may be old wmf file with no size */

		rewind(fp);
		if (fread(&FileType,2,1,fp) != 1) goto out;
		if (fread(&HeaderSize,2,1,fp) != 1) goto out;
		
		if (!g_little_endian) {
			FileType  = LETONS(FileType);
			HeaderSize = LETONS(HeaderSize);
		}
	
		if (FileType != 0 && FileType != 1) goto out;
		if (HeaderSize != 9) goto out;
		
		/* real wmf file ... just assume size */
		width = 200;
		height = 200;
	}

	diagnostics(1,"width = %d, height = %d", width, height);
	fprintRTF("\n{\\pict\\wmetafile1\\picw%d\\pich%d\n", width, height);

	rewind(fp);
	PutHexFile(fp);
	fprintRTF("}\n");
	fclose(fp);
	return;

out:
	diagnostics(WARNING,"Problem with file %s --- not included",s);
	fclose(fp);
}

static void
PutEpsFile(char *s)
{
	char *png, *emf, *pict;
	diagnostics(1, "PutEpsFile filename = <%s>", s);

	if (0) {
		png = eps_to_png(s);
		PutPngFile(png,1.0, TRUE);
		my_unlink(png);
		free(png);
	}
	
	if (1) {
		pict = eps_to_pict(s);
		PutPictFile(pict, TRUE);
/*		my_unlink(pict);  */
		free(pict);
	}

	if (0) {
		emf = eps_to_emf(s);
		PutEmfFile(emf, TRUE);
		my_unlink(emf);
		free(emf);
	}
}

static void
PutTiffFile(char *s)
/******************************************************************************
 purpose   : Insert TIFF file (from g_home_dir) into RTF file as a PNG image
 ******************************************************************************/
{
	char *cmd, *tiff, *png, *tmp_png;
	
	diagnostics(1, "filename = <%s>", s);
	png = strdup_new_extension(s, ".tiff", ".png");
	if (png == NULL) {
		png = strdup_new_extension(s, ".TIFF", ".png");
		if (png == NULL) return;
	}
	
	tmp_png = strdup_tmp_path(png);
	tiff = strdup_together(g_home_dir,s);

	cmd = (char *) malloc(strlen(tiff)+strlen(tmp_png)+10);
	sprintf(cmd, "convert %s %s", tiff, tmp_png);	
	system(cmd);
	
	PutPngFile(tmp_png,1.0, TRUE);
	my_unlink(tmp_png);
	
	free(tmp_png);
	free(cmd);
	free(tiff);
	free(png);
}

static void
PutGifFile(char *s)
/******************************************************************************
 purpose   : Insert GIF file (from g_home_dir) into RTF file as a PNG image
 ******************************************************************************/
{
	char *cmd, *gif, *png, *tmp_png;
	
	diagnostics(1, "filename = <%s>", s);
	png = strdup_new_extension(s, ".gif", ".png");
	if (png == NULL) {
		png = strdup_new_extension(s, ".GIF", ".png");
		if (png == NULL) return;
	}
	
	tmp_png = strdup_tmp_path(png);
	gif = strdup_together(g_home_dir,s);
	
	cmd = (char *) malloc(strlen(gif)+strlen(tmp_png)+10);
	sprintf(cmd, "convert %s %s", gif, tmp_png);	
	system(cmd);
	
	PutPngFile(tmp_png, TRUE, 1.0);
	my_unlink(tmp_png);

	free(tmp_png);
	free(cmd);
	free(gif);
	free(png);
}

void
PutLatexFile(char *s)
/******************************************************************************
 purpose   : Convert LaTeX to Bitmap and insert in RTF file
 ******************************************************************************/
{
	char *png, *cmd;
	int err, cmd_len;
	unsigned long width, height;
	unsigned long max=32767/20;
	int resolution = g_dots_per_inch*2; /*points per inch */
	
	diagnostics(4, "Entering PutLatexFile");

	png = strdup_together(s,".png");

	cmd_len = strlen(s)+25;
	if (g_home_dir)
		cmd_len += strlen(g_home_dir);

	cmd = (char *) malloc(cmd_len);

	/* iterate until png is small enough for Word */
	do {
		resolution /= 2;
		if (g_home_dir==NULL)
			sprintf(cmd, "latex2png -d %d %s", resolution, s);	
		else
			sprintf(cmd, "latex2png -d %d -H \"%s\" %s", resolution, g_home_dir, s);	

		err = system(cmd);
		diagnostics(1, "cmd = <%s>", cmd);
		if (err==0){
			GetPngSize(png, &width, &height);
			diagnostics(4, "png size width=%d height =%d", width, height);
		}
#ifdef MY_CHANGES
                if(width>max||height>max)
                   { int rw=((resolution*max)/width)*2,
                         rh=((resolution*max)/height)*2; 
                     resolution=rw<rh?rw:rh;
                   }
#endif
	} while (!err && resolution>10 && ( (width>max) || (height>max)) );
	
	if (err==0)
#ifdef USER_SCALE
             {  double usc=getUserPngScale();
                if(usc<=0)usc=1.0;
		PutPngFile(png,usc*72.0/resolution,TRUE);
             }   
#else
		PutPngFile(png,72.0/resolution,TRUE);
#endif	
	free(png);
	free(cmd);
}


void 
CmdGraphics(int code)
/*
\includegraphics[parameters]{filename}

where parameters is a comma-separated list of any of the following: 
bb=llx lly urx ury (bounding box),
width=h_length,
height=v_length,
angle=angle,
scale=factor,
clip=true/false,
draft=true/false.
*/
{
	char           *options;
	char           *filename;

	/* could be \includegraphics*[0,0][5,5]{file.pict} */

	options = getBracketParam();
	if (options) free(options);

	options = getBracketParam();
	if (options) free(options);
	
	filename = getBraceParam();

	if (strstr(filename, ".pict") || strstr(filename, ".PICT"))
		PutPictFile(filename, FALSE);
		
	else if (strstr(filename, ".png")  || strstr(filename, ".PNG"))
		PutPngFile(filename, 1.0, FALSE);

	else if (strstr(filename, ".gif")  || strstr(filename, ".GIF"))
		PutGifFile(filename);

	else if (strstr(filename, ".emf")  || strstr(filename, ".EMF"))
		PutEmfFile(filename, FALSE);

	else if (strstr(filename, ".wmf")  || strstr(filename, ".WMF"))
		PutWmfFile(filename);

	else if (strstr(filename, ".eps")  || strstr(filename, ".EPS"))
		PutEpsFile(filename);

	else if (strstr(filename, ".ps")  || strstr(filename, ".PS"))
		PutEpsFile(filename);

	else if (strstr(filename, ".tiff")  || strstr(filename, ".TIFF"))
		PutTiffFile(filename);

	else if (strstr(filename, ".jpg")  || strstr(filename, ".JPG") ||
		strstr(filename, ".jpeg") || strstr(filename, ".JPEG"))
		PutJpegFile(filename);

	else 
		diagnostics(WARNING, "Conversion of '%s' not supported", filename);
	
	free(filename);
}

void 
CmdPicture(int code)
/******************************************************************************
  purpose: handle \begin{picture} ... \end{picture}
           by converting to png image and inserting
 ******************************************************************************/
{
	char *pre, *post, *picture;	
	
	if (code & ON) {
		pre = strdup("\\begin{picture}");
		post = strdup("\\end{picture}");
		picture=getTexUntil(post,0);
		WriteLatexAsBitmap(pre,picture,post);
		ConvertString(post);  /* to balance the \begin{picture} */
		free(pre);
		free(post);
		free(picture);
	}
}
