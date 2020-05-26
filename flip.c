/* ::[[ @(#) flip.c 1.18 89/07/04 16:07:16 ]]:: */
#ifndef LINT
static char sccsid[]="::[[ @(#) flip.c 1.20 2008-07-26 ]]::";
#endif

/*
Copyright 1989 Rahul Dhesi, All rights reserved.

Checksum: 1217582374 (check or update with "brik")
*/

/*
Does newline conversions between **IX and MS-DOS conventions.  Uses a state
machine, so repeated conversions on the same file will do no harm.
Assumes the US ASCII character set in some places (search for 'ASCII').
*/

/* change contents of flip.h as needed */
#include "flip.h"

enum choices   { MSTOIX, IXTOMS, NEITHER };           /* conversion choices */
enum state     { NONE, SAWCR, SAWLF, SAWCRLF, SAWCTRLZ };

void usage PARMS ((void));
void give_help PARMS ((void));
void flip_exit PARMS ((int));
int getopt PARMS ((int argc, char **argv, char *options));
void doarg PARMS ((char *, enum choices));
int dofile PARMS ((char *, enum choices));
char *nextfile PARMS ((int, char *, int));
int ixtoms PARMS ((FILE *, FILE *));
int mstoix PARMS ((FILE *, FILE *));
void error PARMS ((char *, char *));
void setup_sigs PARMS ((void));
void cleanup PARMS ((int));
int mkstemp PARMS ((char *));

#ifdef STDINCLUDE
# include <stdlib.h>
# include <string.h>
#else
void *malloc PARMS ((int));
void exit PARMS ((int));
char *strcpy PARMS ((char *, char *));
#endif

#ifdef IX
#include <sys/types.h>
#include <sys/stat.h>		/* stat and chmod */
#include <unistd.h>
#endif

#ifdef NDEBUG
# define assert(c)
#else
# ifdef STDINCLUDE
#  include <assert.h>
# else
#  define assert(c)  if(!(c)) \
                  fprintf(stderr,"assert error %s:%d\n",__FILE__,__LINE__)
# endif /* STDINCLUDE */
#endif /* NDEBUG */

#ifdef USE_TABLE
char *bintab;
#endif

#ifdef USE_SIG
int got_sig;               /* will indicate if signal received */
#endif

char *myname = NULL;

int exitstat = 0;          /* exit status */
int verbose = 0;           /* set by -v option */
int touch = 0;             /* set by -t option */
int strip = 0;             /* set by -s option */
int bintoo = 0;            /* set by -b option */
int ztrunc = 0;            /* set by -z option */
int use_stdio = 0;	   /* set by "-" filename */

int main (argc, argv)
int argc;
char **argv;

{
   int option;
   extern int optind;
   int i;
   enum choices which = NEITHER;
#ifdef USE_TABLE
#define TABSIZ    256
   char table[TABSIZ];
#endif

#ifdef PICKNAME
   register char *p; /* temp pointer for finding our name */
   register char *arg0 = *argv;
#endif /* PICKNAME */

   SPEC_INIT            /* optional initialization */

#ifdef USE_SIG
   setup_sigs();
#endif

#ifdef PICKNAME
# define STRCMP(a,op,b)    (strcmp(a,b) op 0)
   p = arg0 + strlen(arg0);

   if (p != arg0) {     /* if program name is defined */
      while (p != arg0 && *p != '/' && *p != '\\')
         p--;
      assert ((p - arg0) <= strlen (arg0));
      if (p != arg0)
         p++;

      /* p now points to trailing name component, or nothing */
      myname = p;
      while (*p != '\0' && *p != '.') {   /* ASCII convert to lowercase */
         if (*p >= 'A' && *p <= 'Z') {
            *p = (*p - 'A' + 'a');
         }
         p++;
      }
      if (p != myname && *p == '.')
         *p = '\0';     /* remove trailing .exe or .com under MS-DOS etc. */

      if (STRCMP(myname,==,"toix"))
         which = MSTOIX;
      else if (STRCMP(myname,==,"toms"))
         which = IXTOMS;
   } else
   myname = "flip";
#else
   myname = "flip";
#endif /* PICKNAME */

argv[0] = myname;                /* for use by getopt */

#ifdef USE_TABLE
/* table to find out which characters are binary */
   for (i = 0;  i < TABSIZ;  i++) {
      if ( (i < 7) || (i > 13 && i < 26) || (i > 126)) /*ASCII binary chars*/
         table[i] = 1;
      else
         table[i] = 0;
   }
   bintab = table;
#endif /* USE_TABLE */

   if (argc < 2) {
      usage();
      flip_exit (1);
   }

   while ((option = getopt (argc, argv, "umhvtsbz")) != EOF) {
      switch (option) {
       case 'u': which = MSTOIX;  break;
       case 'm': which = IXTOMS; break;
       case 'h': give_help(); flip_exit (0);
       case 'v': verbose = 1;  break;
       case 't': touch = 1;  break;
       case 's': strip = 1;  break;
       case 'b': bintoo = 1;  break;
       case 'z': ztrunc = 1;  break;
       default:  usage(); flip_exit (1);
      }
   }

   switch (which) {
    case MSTOIX:
      /* printf ("converting to **ix format\n"); */
      break;
    case IXTOMS:
      /* printf ("converting to msdos format\n"); */
      break;
    default:
      fprintf (stderr, "%s: error: -u or -m is required\n", myname);
      flip_exit (1);
      break;
   }

   if (argc <= optind) {
      fprintf (stderr, "%s: error: filenames are needed\n", myname);
      flip_exit (1);
   }

   for (i = optind;  i < argc;  i++)
      doarg (argv[i], which);

   return (exitstat);
}


/*
Does conversion for one argument, calling dofile with wildcards
expanded.  Updates exitstat in case of error.
*/

void doarg (arg, which)
char *arg;
enum choices which;
{
#ifdef WILDCARD
   char *this_file;

   nextfile (0, arg, 0);
   while ((this_file = nextfile(1, (char *) NULL, 0)) != NULL) {
      exitstat |= dofile (this_file, which);
   }
#else
   exitstat |= dofile (arg, which);
#endif /* WILDCARD */
}

#ifdef USE_SIG
# include <signal.h>
#endif

FILE *outfile;       /* make it visible to both dofile and cleanup */

/*
Here we have filename and an option.  We call a different routine
for each type of conversion.  This way more types of conversions
can be easily added in the future.
*/

int dofile (fname, which)
char *fname;
enum choices which;
{
   FILE *infile;
   char tfname[PATHSIZE];
   SGFTIME timestamp;  /* save file timestamp here, restore later */
   int errstat = 0;
   char *p;             /* temp file ptr */
#ifdef IX
   struct stat ifilestat, ofilestat;
#endif

#ifdef USE_SIG
   if (got_sig)
      flip_exit (INT_EXIT);
#endif

   if(STRCMP(fname,==,"-"))
      use_stdio = 1;
   if(!use_stdio) {
      /* if writable, open for reading */
      if ((infile = fopen (fname, R_PL_B)) != NULL) {
         fclose (infile);
         infile = fopen (fname, RB);
#ifdef IX
	 if (stat (fname, &ifilestat)) {
	   /* can't get the file's permissions */
	   char *buf = malloc(strlen(fname)+strlen(myname)+3);
	   if (buf) {
	     sprintf(buf, "%s: %s", myname, fname);
	     perror(buf);
	   }
	   ifilestat.st_mode = 0644;
	   ifilestat.st_uid = geteuid();
	   ifilestat.st_gid = getegid();
	 }
#endif
      }
   } else {
      infile = stdin;
   }
	   
   if (infile == NULL) {
      error (fname, ": can't open");
      return (1);
   }
   if(use_stdio) {
     outfile = stdout;
   } else {
     /*
       to make temp file in same dir as original, we make p point to
       the filename component of fname, put '\0' there, and strcat the
       temp name to it */
     strcpy (tfname, fname);
     p = tfname + strlen(tfname);
     
     while (p != tfname && *p != '/' && *p != '\\')
       p--;
     if (p != tfname)
       p++;
     *p = '\0';

#define  TEMPLATE    "XXXXXX"
#ifdef IX
     strcat (tfname, TEMPLATE);

     {
       int fd = mkstemp(tfname);
       if (fd == -1 || (outfile = fdopen(fd, WB)) == NULL) {
	 fclose (infile);
	 error (fname, ": skipped, could not open temporary file");
	 return (1);
       }
     }
#else
     {
       char template[7];
       strcpy (template, TEMPLATE);
       strcat (tfname, mktemp (template));
     }

     outfile = fopen (tfname, WB);
     if (outfile == NULL) {
       fclose (infile);
       error (fname, ": skipped, could not open temporary file");
       return (1);
     }
#endif
   }

   if (!touch)
      GETFT (infile, fname, timestamp);      /* save current timestamp */

   assert (which == IXTOMS || which == MSTOIX);

#ifdef BIGBUF
   setvbuf (infile,  (char *) NULL, _IOFBF, BIGBUF);
   setvbuf (outfile, (char *) NULL, _IOFBF, BIGBUF);
#endif /* BIGBUF */

   switch (which) {
    case IXTOMS:
      errstat = ixtoms (infile, outfile);
      break;
    case MSTOIX:
      errstat = mstoix (infile, outfile);
      break;
    case NEITHER:
      ;
   }

   fclose (infile);

   switch (errstat) {
    case ERRBINF:
      fclose (outfile);
      DELFILE (tfname);
      fprintf (stderr, "%s: binary file, not converted\n", fname);
      return (1);
      /* break; */  /* unreachable code */
#ifdef USE_SIG
    case ERRSIG:
      fclose (outfile);
      DELFILE (tfname);
      flip_exit (INT_EXIT);
      break;
#endif
    default:
      ;
   }

   assert (errstat == 0);

   if (!ferror(outfile) && fflush(outfile) != EOF && fclose(outfile) != EOF) {
      int moved;
#ifdef IX
      if (stat (tfname, &ofilestat)) {
	/* can't get the file's permissions */
	char *buf = malloc(strlen(tfname)+strlen(myname)+3);
	if (buf) {
	  sprintf(buf, "%s: %s", myname, tfname);
	  perror(buf);
	}
      }
      if (ifilestat.st_gid != ofilestat.st_gid ||
	  ifilestat.st_uid != ofilestat.st_uid)
	if (chown(tfname, ifilestat.st_uid, ifilestat.st_gid)) {
	  /* can't set the file's ownership */
	  char *buf = malloc(strlen(tfname)+strlen(myname)+3);
	  if (buf) {
	    sprintf(buf, "%s: %s", myname, tfname);
	    perror(buf);
	  }
	}
      if (ifilestat.st_mode != ofilestat.st_mode)
	if (chmod(tfname, ifilestat.st_mode)) {
	  /* can't set the file's permissions */
	  char *buf = malloc(strlen(tfname)+strlen(myname)+3);
	  if (buf) {
	    sprintf(buf, "%s: %s", myname, tfname);
	    perror(buf);
	  }
	}
#endif /* IX */
      //      DELFILE (fname);
      if (!use_stdio) {
         moved = MVFILE (tfname, fname);
         if (moved == 0) {
            FILE *fptr;
            if (!touch && (fptr = fopen (fname, RB)) != NULL) {
               SETFT (fptr, fname, timestamp);
               fclose (fptr);
            }
            if (verbose) {
               printf ("%s\n", fname);
               fflush (stdout);
            }
            return (0);
         } else {
            error (fname, ": not converted, could not rename temp file");
            DELFILE (tfname);
            return (1);
         }
      }
   } else {
      fclose (outfile); /* outfile was not closed, so close it here */
      return (1);
   }
   return (1);			/* silences gcc warning */
}

/* convert from ms-dos to **ix format */
int mstoix (infile, outfile)
FILE *infile;           /* input file   */
FILE *outfile;          /* output file  */
{
   int c;
   enum state state = NONE;

   /* lone LF => unchanged, lone CR => unchanged,
      CR LF => LF, ^Z at end means EOF; ^Z elsewhere => unchanged */

   while (1) {       /* break out on EOF only */
      while ((c = getc (infile)) != EOF) {
#ifdef USE_SIG
         if (got_sig)
            return (ERRSIG);
#endif
         if (!bintoo && BINCHAR(c))
            return (ERRBINF);
         if (strip)
            STRIP(c);
         switch (c) {
          case LF:
            CHECK_BREAK
            putc (c, outfile); if (state == SAWCR) state = NONE; break;
          case CR:
            state = SAWCR; break;
          case CTRLZ:
            if (state == SAWCR) putc (CR, outfile);
            state = SAWCTRLZ; goto saweof;
          default:
            if (state == SAWCR) { state = NONE; putc (CR, outfile); }
            putc (c, outfile);
            break;
         }
      }
 saweof:
      /* exit outer loop only on EOF or ^Z as last char */
      if (
          (ztrunc && (state == SAWCTRLZ))
          || (c = getc (infile)) == EOF
         )
         break;
      else
         ungetc (c, infile);
      if (state == SAWCTRLZ)
         putc (CTRLZ, outfile);
   }
   return (0);
}

/* convert from **ix to ms-dos format */
int ixtoms (infile, outfile)
FILE *infile;           /* input file   */
FILE *outfile;          /* output file  */
{
   int c;
   enum state state = NONE;

   /* LF => CR LF, but leave CR LF alone */
   while ((c = getc (infile)) != EOF) {
#ifdef USE_SIG
      if (got_sig)
         return (ERRSIG);
#endif
      if (!bintoo && BINCHAR(c))
         return (ERRBINF);
      if (strip)
         STRIP(c);
      switch (c) {
       case LF:
         CHECK_BREAK
         if (state == SAWCR)
            state = NONE;
         else
            putc (CR, outfile);
         putc (LF, outfile);
         break;
       case CR:
         state = SAWCR; putc (c, outfile); break;
       case CTRLZ:
         if (ztrunc)
            return (0);
         /* FALL THROUGH */
       default:
         state = NONE; putc (c, outfile); break;
      }
   }
   return (0);
}

/* set up signal handler for selected signals */
#ifdef USE_SIG
void setup_sigs ()
{
# ifdef SIGPIPE
   if (signal (SIGPIPE, SIG_IGN) != SIG_IGN)
      signal (SIGPIPE, cleanup);
# endif
# ifdef SIGHUP
   if (signal (SIGHUP, SIG_IGN) != SIG_IGN)
      signal (SIGHUP, cleanup);
# endif
# ifdef SIGQUIT
   if (signal (SIGQUIT, SIG_IGN) != SIG_IGN)
      signal (SIGQUIT, cleanup);
# endif
# ifdef SIGINT
   if (signal (SIGINT, SIG_IGN) != SIG_IGN)
      signal (SIGINT, cleanup);
# endif
# ifdef SIGTERM
   if (signal (SIGTERM, SIG_IGN) != SIG_IGN)
      signal (SIGTERM, cleanup);
# endif
}

/* set flag on signal */
void cleanup(sig)
int sig;
{
   signal (sig, SIG_IGN);     /* needed for flaky System V Release 2 */
   got_sig = 1;
   signal (sig, cleanup);     /* ditto */
}
#endif /* USE_SIG */

#define  ERRSIZE     200

/* prints error message via perror */
void error (msg1, msg2)
char *msg1, *msg2;
{
   char buf[ERRSIZE];
   strcpy (buf, myname);
   strcat (buf, ": ");
   strcat (buf, msg1);
   strcat (buf, msg2);
   perror (buf);
   fflush (stderr);
}

/* gives brief usage message */
void usage()
{
   fprintf (stderr,
"usage:  %s [-u | -m] [other options] file ...  (or \"%s -h\" for more help)\n",
 myname, myname);
}

/* gives help screen */

void give_help()
{
printf ("\
File interchange program flip version " VERSION ".  Copyright 1989 Rahul Dhesi,\n\
All rights reserved.  Both noncommercial and commercial copying, use, and\n\
creation of derivative works are permitted in accordance with the\n\
requirements of the GNU license.  This program does newline conversions.\n");

printf ("\n\
   Usage:     flip -umhvtsbz file ...\n\
\n\
One of -u, -m, or -h is required;  others are optional.  See user manual.\n");

printf ("\n\
   -u     convert to **IX format (CR LF => LF, lone CR or LF unchanged,\n\
          trailing control Z removed, embedded control Z unchanged)\n\
   -m     convert to MS-DOS format (lone LF => CR LF, lone CR unchanged)\n\
   -h     give this help message\n\
   -v     be verbose, print filenames as they are processed\n\
   -t     touch files (don't preserve timestamps)\n\
   -s     strip high bit\n\
   -b     convert binary files too (else binary files are left unchanged)\n\
   -z     truncate file at first control Z encountered\n\
");
#ifdef PICKNAME
printf ("\n\
May be invoked as \"toix\" (same as \"flip -u\") or \"toms\" (same as \"flip -m\").\n\
");
#endif
return;
}

/* normal exits go through here -- atexit would be nice but not portable */
void flip_exit(stat)
int stat;
{
   SPEC_EXIT
   exit (stat);
}

/*
special code for **IX -- not in use because can't properly handle
SIGINT while /bin/mv is executing
*/

#ifdef NIX
# ifndef MVFILE
int MVFILE (src, dest)
char *dest;
char *src;
{
   char cmd[2 * PATHSIZE];
   sprintf (cmd, "/bin/mv %s %s", src, dest);
   return (system(cmd));
}
# endif /* MVFILE */
#endif /* NIX */
