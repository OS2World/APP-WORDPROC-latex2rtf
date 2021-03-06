/* $Id: main.h,v 1.49 2002/08/05 04:06:00 prahl Exp $ */
#ifndef __MAIN_H
#define __MAIN_H

#ifndef CFGDIR
#define CFGDIR "/usr/local/share/latex2rtf/cfg"
#endif

#ifdef UNIX
#ifndef ENVSEP
#define ENVSEP ':'
#endif
#ifndef PATHSEP
#define PATHSEP '/'
#endif 
#endif

#ifdef MSDOS
#ifndef ENVSEP
#define ENVSEP ';'
#endif
#ifndef PATHSEP
#define PATHSEP '\\'
#endif 
#endif

#if defined(MACINTOSH) || defined(__MWERKS__)
#define HAS_NO_GETOPT
#define ENVSEP '^'
#define PATHSEP ':'
#include "MainMain.h"
#endif

#ifdef HAS_NO_GETOPT
#define getopt my_getopt
#endif

#ifdef HAS_STRDUP
#else
#define strdup my_strdup
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#endif

#define ERROR 0
#define WARNING 1

#define MAXCOMMANDLEN 100

/* available values for alignment */
#define LEFT      'l'
#define RIGHT     'r'
#define CENTERED  'c'
#define JUSTIFIED 'j'

#define PATHMAX 255

/*** error constants ***/
#include <assert.h>
#include <stdio.h>

typedef int     bool;

void            diagnostics(int level, char *format,...);

extern /* @dependent@ */ FILE *fRtf;	/* file pointer to RTF file */
extern			char *g_aux_name;
extern			char *g_bbl_name;
extern			char *g_home_dir;
extern 			char *progname;			/* name of the executable file */

extern bool     GermanMode;
extern bool     FrenchMode;
extern bool     RussianMode;
extern bool     pagenumbering;
extern int      headings;

extern int      g_verbosity_level;
extern int      RecursionLevel;
extern int      indent;
extern char     alignment;

/* table  & tabbing variables */
extern char 	*colFmt;
extern long   	pos_begin_kill;
extern int      tabcounter;
extern int      colCount;
extern int      actCol;
extern int 		g_equation_column;
extern int      tabcounter;

extern bool     twocolumn;
extern bool     titlepage;
extern bool     g_processing_equation;
extern bool     g_processing_preamble;
extern bool     g_processing_figure;
extern bool 	g_processing_table;
extern bool     g_processing_tabbing;
extern bool     g_processing_tabular;
extern bool     g_processing_eqnarray;
extern int		g_processing_arrays;
extern int 		g_processing_fields;
extern int		g_dots_per_inch;

extern int      g_equation_number;
extern bool     g_show_equation_number;
extern int      g_enumerate_depth;
extern bool     g_suppress_equation_number;
extern bool     g_aux_file_missing;
extern int    	g_document_type;
extern char     g_encoding[20];
#ifdef USE_RAW_TEXT
extern int      g_fcharset;
#endif
extern char		*g_figure_label;
extern char		*g_table_label;
extern char		*g_equation_label;
extern char  	*g_section_label;
extern char		*g_config_path;
extern char     g_field_separator;
extern char    *g_preamble;
#ifdef INLINE_EQ_ALIGN
extern bool             g_inline_eq_align;
#endif
#ifdef LATEX_FIGURES
extern bool             g_latex_figures;
#endif
#ifdef USER_SCALE
extern double           g_user_eq_scale; 
extern double           g_user_fig_scale; 
#endif
extern bool		g_equation_inline_rtf;
extern bool		g_equation_display_rtf;
extern bool		g_equation_inline_bitmap;
extern bool		g_equation_display_bitmap;
extern bool		g_equation_comment;
extern bool		g_little_endian;

void fprintRTF(char *format, ...);
void putRtfChar(char cThis);
char *getTmpPath(void);
char *my_strdup(const char *str);
FILE *my_fopen(char *path, char *mode);

void debug_malloc(void);
#endif				/* __MAIN_H */
