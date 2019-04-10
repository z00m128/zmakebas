/* zmakebas - convert a text file containing a speccy Basic program
 *            into an actual program file loadable on a speccy.
 *
 * Public domain by Russell Marks, 1998.
 */

/* warning: this is probably the least structured program ever.
 * I guess that's what comes from hacking something into existence... :-/
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_GETOPT
#include <unistd.h>
#endif
#include <math.h>
#include <ctype.h>


#if defined(__TURBOC__) && !defined(MSDOS)
#define MSDOS
#endif


#define DEFAULT_OUTPUT		"out.tap"

#define REM_TOKEN_NUM		234
#define BIN_TOKEN_NUM		196

/* tokens are stored (and looked for) in reverse speccy-char-set order,
 * to avoid def fn/fn and go to/to screwups. There are two entries for
 * each token - this is to allow for things like "go to" which can
 * actually be entered (thank ghod) as "goto". The one extension to
 * this I've made is that randomize can be entered with -ize or -ise.
 */
char *tokens[]=
  {
  "copy", "",
  "return", "",
  "clear", "",
  "draw", "",
  "cls", "",
  "if", "",
  "randomize", "randomise",
  "save", "",
  "run", "",
  "plot", "",
  "print", "",
  "poke", "",
  "next", "",
  "pause", "",
  "let", "",
  "list", "",
  "load", "",
  "input", "",
  "go sub", "gosub",
  "go to", "goto",
  "for", "",
  "rem", "",
  "dim", "",
  "continue", "",
  "border", "",
  "new", "",
  "restore", "",
  "data", "",
  "read", "",
  "stop", "",
  "llist", "",
  "lprint", "",
  "out", "",
  "over", "",
  "inverse", "",
  "bright", "",
  "flash", "",
  "paper", "",
  "ink", "",
  "circle", "",
  "beep", "",
  "verify", "",
  "merge", "",
  "close #", "close#",
  "open #", "open#",
  "erase", "",
  "move", "",
  "format", "",
  "cat", "",
  "def fn", "deffn",
  "step", "",
  "to", "",
  "then", "",
  "line", "",
  "<>", "",
  ">=", "",
  "<=", "",
  "and", "",
  "or", "",
  "bin", "",
  "not", "",
  "chr$", "",
  "str$", "",
  "usr", "",
  "in", "",
  "peek", "",
  "abs", "",
  "sgn", "",
  "sqr", "",
  "int", "",
  "exp", "",
  "ln", "",
  "atn", "",
  "acs", "",
  "asn", "",
  "tan", "",
  "cos", "",
  "sin", "",
  "len", "",
  "val", "",
  "code", "",
  "val$", "",
  "tab", "",
  "at", "",
  "attr", "",
  "screen$", "",
  "point", "",
  "fn", "",
  "pi", "",
  "inkey$", "",
  "rnd", "",
  "play", "",
  "spectrum", "",
  NULL
  };


/* the whole raw basic file is written to filebuf; no output is generated
 * until the whole program has been converted. This is to allow a TAP
 * to be output on a non-seekable file (stdout).
 */

#ifdef MSDOS
#define MAX_LABELS  500
unsigned char filebuf[32768];
char infile[256],outfile[256];
#else
#define MAX_LABELS  2000
unsigned char filebuf[49152];
char infile[1024],outfile[1024];
#endif

#define MAX_LABEL_LEN	16

/* this is needed for tap files too: */
unsigned char headerbuf[128];

typedef enum { RAW, TAP, PLUS3DOS } OUTPUT_FORMAT;

OUTPUT_FORMAT output_format=TAP;
int use_labels=0;
unsigned int startline=0x8000;
int autostart=10,autoincr=2;
char speccy_filename[11];

int labelend=0;
unsigned char labels[MAX_LABELS][MAX_LABEL_LEN+1];
int label_lines[MAX_LABELS];

unsigned char startlabel[MAX_LABEL_LEN+1];


#ifndef HAVE_GETOPT

/* ok, well here's a crude getopt clone I wrote a few years ago.
 * It's not great, but it's good enough for zmakebas at least.
 */

int optopt=0,opterr=0,optind=1;
char *optarg=NULL;

/* holds offset in current argv[] value */
static int optpos=1;

/* This routine assumes that the caller is pretty sane and doesn't
 * try passing an invalid 'optstring' or varying argc/argv.
 */
int getopt(int argc,char *argv[],char *optstring)
{
char *ptr;

/* check for end of arg list */
if(optind==argc || *(argv[optind])!='-' || strlen(argv[optind])<=1)
  return(-1);

if((ptr=strchr(optstring,argv[optind][optpos]))==NULL)
  return('?');		/* error: unknown option */
else
  {
  optopt=*ptr;
  if(ptr[1]==':')
    {
    if(optind==argc-1) return(':');	/* error: missing option */
    optarg=argv[optind+1];
    optpos=1;
    optind+=2;
    return(optopt);	/* return early, avoiding the normal increment */
    }
  }

/* now increment position ready for next time.
 * no checking is done for the end of args yet - this is done on
 * the next call.
 */
optpos++;
if(optpos>=strlen(argv[optind]))
  {
  optpos=1;
  optind++;
  }

return(optopt);		/* return the found option */
}
#endif	/* !HAVE_GETOPT */


/* dbl2spec() converts a double to an inline-basic-style speccy FP number.
 *
 * usage: dbl2spec(num,&exp,&man);
 *
 * num is double to convert.
 * pexp is an int * to where to return the exponent byte.
 * pman is an unsigned long * to where to return the 4 mantissa bytes.
 * bit 31 is bit 7 of the 0th (first) mantissa byte, and bit 0 is bit 0 of
 * the 3rd (last). As such, the unsigned long returned *must* be written
 * to whatever file in big-endian format to make any sense to a speccy.
 *
 * returns 1 if ok, 0 if exponent too big.
 */
int dbl2spec(double num,int *pexp,unsigned long *pman)
{
int exp;
unsigned long man;

/* check for small integers */
if(num==(long)num && num>=-65535.0 && num<=65535.0)
  {
  /* ignores sign - see below, which applies to ints too. */
  long tmp=(long)fabs(num);
  
  exp=0;
  man=((tmp%256)<<16)|((tmp>>8)<<8);
  }
else
  {
  int f;
  
  /* It appears that the sign bit is always left as 0 when floating-point
   * numbers are embedded in programs, and the speccy appears to use the
   * '-' character to detemine negativity - tests confirm this.
   * As such, we *completely ignore* the sign of the number.
   * exp is 0x80+exponent.
   */
  num=fabs(num);
  
  /* binary standard form goes from 0.50000... to 0.9999...(dec), like
   * decimal which goes from        0.10000... to 0.9999....
   */
  
  /* as such, if the number is >=1, it gets divided by 2, and exp++.
   * And if the number is <0.5, it gets multiplied by 2, and exp--.
   */
  exp=0;
  while(num>=1.0)
    {
    num/=2.0; exp++;
    }

  while(num<0.5)
    {
    num*=2.0; exp--;
    }

  /* so now the number is in binary standard form in exp and num.
   * we check the range of exp... -128 <= exp <= 127.
   * (if outside, we return error (i.e. 0))
   */
   
  if(exp<-128 || exp>127) return(0);
    
  exp=128+exp;
  
  /* so now all we need to do is roll the bits off the mantissa in `num'.
   * we start at the 0.5ths bit at bit 0, and shift left 1 each time
   * round the loop.
   */
  
  num*=2.0;  /* make it so that the 0.5ths bit is the integer part, */
	     /* and the rest is the fractional (0.xxx) part. */
  
  man=0;
  for(f=0;f<32;f++)
    {
    man<<=1;
    man|=(int)num;
    num-=(int)num;
    num*=2.0;
    }

  /* Now, if (int)num is non-zero (well, 1) then we should generally
   * round up 1. We don't do this if it would cause an overflow in the
   * mantissa, though.
   */
   
  if((int)num && man!=0xFFFFFFFF) man++;
    
  /* finally, zero out the top bit */
  man&=0x7FFFFFFF;
  }

/* done */
*pexp=exp; *pman=man;
return(1);
}


unsigned long grok_hex(unsigned char **ptrp,int textlinenum)
{
static char *hexits="0123456789abcdefABCDEF",*lookup;
unsigned char *ptr=*ptrp;
unsigned long v=0,n;

/* we know the number starts with "0x" and we're pointing to it */

ptr+=2;
if(strchr(hexits,*ptr)==NULL)
  {
  fprintf(stderr,"line %d: bad BIN 0x... number\n",textlinenum);
  exit(1);
  }

while(*ptr && (lookup=strchr(hexits,*ptr))!=NULL)
  {
  n=lookup-hexits;
  if(n>15) n-=6;
  v=v*16+n;
  ptr++;
  }

*ptrp=ptr;
return(v);
}


unsigned long grok_binary(unsigned char **ptrp,int textlinenum)
{
unsigned long v=0;
unsigned char *ptr=*ptrp;

while(isspace(*ptr)) ptr++;

if(*ptr!='0' && *ptr!='1')
  {
  fprintf(stderr,"line %d: bad BIN number\n",textlinenum);
  exit(1);
  }

if(ptr[1]=='x' || ptr[1]=='X')
  {
  *ptrp=ptr;
  return(grok_hex(ptrp,textlinenum));
  }

while(*ptr=='0' || *ptr=='1')
  {
  v*=2;
  v+=*ptr-'0';
  ptr++;
  }

*ptrp=ptr;
return(v);
}


void usage_help()
{
printf("zmakebas - public domain by Russell Marks.\n\n");

printf("usage: zmakebas [-hlpr] [-a line] [-i incr] [-n speccy_filename]\n");
printf("                [-o output_file] [-s line] [input_file]\n\n");

printf("        -a      set auto-start line of basic file (default none).\n");
printf("        -h      give this usage help.\n");
printf("        -i      in labels mode, set line number incr. (default 2).\n");
printf("        -l      use labels rather than line numbers.\n");
printf("        -n      set Spectrum filename (to be given in tape header).");
printf("\n        -o      specify output file (default `%s').\n",
						DEFAULT_OUTPUT);
printf("        -p      output +3DOS file (default is .tap file).\n");            
printf("        -r      output raw headerless file (default is .tap file).\n");
printf("        -s      in labels mode, set starting line number ");
printf("(default 10).\n");
}


/* cmdline option parsing routine.
 */
void parse_options(int argc,char *argv[])
{
int done=0;

done=0;
opterr=0;
startlabel[0]=0;

do
  switch(getopt(argc,argv,"a:hi:ln:o:prs:"))
    {
    case 'a':
      if(*optarg=='@')
        if(strlen(optarg+1)>MAX_LABEL_LEN)
          fprintf(stderr,"Auto-start label too long\n"),exit(1);
        else
          strcpy(startlabel,optarg+1);
      else
        {
        startline=(unsigned int)atoi(optarg);
        if(startline>9999)
          fprintf(stderr,"Auto-start line must be in the range 0 to 9999.\n"),
            exit(1);
        }
      break;
    case 'h':	/* usage help */
      usage_help();
      exit(1);
    case 'i':
      autoincr=(int)atoi(optarg);
      /* this is unnecessarily restrictive but keeps things a bit sane */
      if(autoincr<1 || autoincr>1000)
        fprintf(stderr,"Label line incr. must be in the range 1 to 1000.\n"),
          exit(1);
      break;
    case 'l':
      use_labels=1;
      break;
    case 'n':
      strncpy(speccy_filename,optarg,10);
      speccy_filename[10]=0;
      break;
    case 'o':
      strcpy(outfile,optarg);
      break;
    case 'p':	/* output plus3dos */
      output_format=PLUS3DOS; break;      
    case 'r':	/* output raw file */
      output_format=RAW; break;
    case 's':
      autostart=(int)atoi(optarg);
      if(autostart<0 || autostart>9999)
        fprintf(stderr,"Label start line must be in the range 0 to 9999.\n"),
          exit(1);
      break;
    case '?':
      switch(optopt)
        {
        case 'a':
          fprintf(stderr,"The `a' option takes a line number arg.\n");
          break;
        case 'i':
          fprintf(stderr,"The `i' option takes a line incr. arg.\n");
          break;
        case 'n':
          fprintf(stderr,"The `n' option takes a Spectrum filename arg.\n");
          break;
        case 'o':
          fprintf(stderr,"The `o' option takes a filename arg.\n");
          break;
        case 's':
          fprintf(stderr,"The `s' option takes a line number arg.\n");
          break;
        default:
          fprintf(stderr,"Option `%c' not recognised.\n",optopt);
        }
      exit(1);
    case -1:
      done=1;
    }
while(!done);

if(optind<argc-1)	/* two or more remaining args */
  usage_help(),exit(1);

if(optind==argc-1)	/* one remaining arg */
  strcpy(infile,argv[optind]);
}


int grok_block(unsigned char *ptr,int textlinenum)
{
static char *lookup[]=
  {
  "  ", " '", "' ", "''", " .", " :", "'.", "':",
  ". ", ".'", ": ", ":'", "..", ".:", ":.", "::",
  NULL
  };
char **lptr;
int f=128,v=-1;
            
for(lptr=lookup;*lptr!=NULL;lptr++,f++)
  {
  if(strncmp(ptr+1,*lptr,2)==0)
    {
    v=f;
    break;
    }
  }

if(v==-1)
  {
  fprintf(stderr,"line %d: invalid block graphics escape\n",textlinenum);
  exit(1);
  }

return(v);
}



int main(int argc,char *argv[])
{
#ifdef MSDOS
static unsigned char buf[512],lcasebuf[512],outbuf[1024];
#else
static unsigned char buf[2048],lcasebuf[2048],outbuf[4096];
#endif
int f,toknum,toklen,linenum,linelen,in_quotes,in_rem,lastline;
char **tarrptr;
unsigned char *ptr,*ptr2,*linestart,*outptr,*remptr,*fileptr,*asciiptr;
double num;
int num_exp;
unsigned long num_mantissa,num_ascii;
int textlinenum;
int chk=0;
int alttok;
int passnum=1;
unsigned int siz;
FILE *in=stdin,*out=stdout;

strcpy(speccy_filename,"");
strcpy(infile,"-");
strcpy(outfile,DEFAULT_OUTPUT);

parse_options(argc,argv);

if(strcmp(infile,"-")!=0 && (in=fopen(infile,"r"))==NULL)
  fprintf(stderr,"Couldn't open input file.\n"),exit(1);


fileptr=filebuf;
linenum=-1;		/* to set lastline */

/* we make one pass if using line numbers, two if using labels */

do
  {
  if(use_labels) linenum=autostart-autoincr;
  textlinenum=0;
  if(passnum>1 && fseek(in,0L,SEEK_SET)!=0)
    {
    fprintf(stderr,"Need seekable input for label support\n");
    exit(1);
    }
  
  while(fgets(buf+1,sizeof(buf)-1,in)!=NULL)
    {
    buf[0]=32;		/* just in case, for all the ptr[-1] stuff */
    textlinenum++;
    lastline=linenum;
    
    if(buf[strlen(buf)-1]=='\n') buf[strlen(buf)-1]=0;
    
    /* allow for (shell-style) comments which don't appear in the program,
     * and also ignore blank lines.
     */
    if(buf[1]==0 || buf[1]=='#') continue;
    
    /* check for line continuation */
    while(buf[strlen(buf)-1]=='\\')
      {
      f=strlen(buf)-1;
      fgets(buf+f,sizeof(buf)-f,in);
      textlinenum++;
      if(buf[strlen(buf)-1]=='\n') buf[strlen(buf)-1]=0;
      }
    
    if(strlen(buf)>=sizeof(buf)-MAX_LABEL_LEN-1)
      {
      /* this is nasty, but given how the label substitution works it's
       * probably the safest thing to do.
       */
      fprintf(stderr,"line %d: line too big for input buffer\n",textlinenum);
      exit(1);
      }
    
    /* get line number (or assign one) */
    if(use_labels)
      {
      linestart=buf;
      /* assign a line number */
      linenum+=autoincr;
      if(linenum>9999)
        fprintf(stderr,"Generated line number is >9999 - %s\n",
          (autostart>1 || autoincr>1)?"try using `-s 1 -i 1'"
                                     :"too many lines!"),
          exit(1);
      }
    else
      {
      ptr=buf;
      while(isspace(*ptr)) ptr++;
      if(!isdigit(*ptr))
        {
        fprintf(stderr,"line %d: missing line number\n",textlinenum);
        exit(1);
        }
      linenum=(int)strtol(ptr,(char **)&linestart,10);
    
      if(linenum<=lastline)
        {
        fprintf(stderr,"line %d: line no. not greater than previous one\n",
        	textlinenum);
        exit(1);
        }
      }
    
    if(linenum<0 || linenum>9999)
      {
      fprintf(stderr,"line %d: line no. out of range\n",textlinenum);
      exit(1);
      }
    
    /* lose remaining spaces */
    while(isspace(*linestart)) linestart++;
    
    /* check there's no line numbers on label-using programs */
    if(use_labels && isdigit(*linestart))
      {
      fprintf(stderr,"line %d: line number used in labels mode\n",
      	textlinenum);
      exit(1);
      }
    
    if(use_labels && *linestart=='@')
      {
      if((ptr=strchr(linestart,':'))==NULL)
        {
        fprintf(stderr,"line %d: incomplete token definition\n",textlinenum);
        exit(1);
        }
      if(ptr-linestart-1>MAX_LABEL_LEN)
        {
        fprintf(stderr,"line %d: token too long\n",textlinenum);
        exit(1);
        }
      if(passnum==1)
        {
        *ptr=0;
        label_lines[labelend]=linenum;
        strcpy(labels[labelend++],linestart+1);
        if(labelend>=MAX_LABELS)
          {
          fprintf(stderr,"line %d: too many labels\n",textlinenum);
          exit(1);
          }
        for(f=0;f<labelend-1;f++)
          if(strcmp(linestart+1,labels[f])==0)
            {
            fprintf(stderr,"iine %d: attempt to redefine label\n",textlinenum);
            exit(1);
            }
        *ptr=':';
        }
      
      linestart=ptr+1;
      while(isspace(*linestart)) linestart++;
      
      /* if now blank, don't bother inserting an actual line here;
       * instead, fiddle linenum so the next line will have the
       * same number.
       */
      if(*linestart==0)
        {
        linenum-=autoincr;
        continue;
        }
      }
    
    if(use_labels && passnum==1) continue;
    
    /* make token comparison copy of line. this has lowercase letters and
     * blanked-out strings.
     */
    ptr=linestart; in_quotes=0;
    ptr2=lcasebuf;
    while(*ptr)
      {
      if(*ptr=='"') in_quotes=!in_quotes;
      if(in_quotes && *ptr!='"')
        *ptr2++=32;
      else
        *ptr2++=tolower(*ptr);
      ptr++;
      }
    *ptr2=0;
    
    /* now convert any token without letters either side to the correct
     * speccy token number. (Any space this leaves in the string is replaced
     * by 0x01 chars.)
     *
     * However, we need to check for REM first. If found, no token/num stuff
     * is performed on the line after that point.
     */
    remptr=NULL;
    
    if((ptr=strstr(lcasebuf,"rem"))!=NULL &&
       !isalpha(ptr[-1]) && !isalpha(ptr[3]))
      {
      ptr2=linestart+(ptr-lcasebuf);
      /* endpoint for checks must be here, then. */
      remptr=ptr2;
      *remptr=*ptr=0;
      /* the zero will be replaced with the REM token later */
      ptr2[1]=ptr[1]=ptr2[2]=ptr[2]=1;
      /* absorb at most one trailing space */
      if(ptr[3]==' ') ptr2[3]=ptr[3]=1;
      }
    
    toknum=256; alttok=1;
    for(tarrptr=tokens;*tarrptr!=NULL;tarrptr++)
      {
      if(alttok) toknum--;
      alttok=!alttok;
      if(**tarrptr==0) continue;
      toklen=strlen(*tarrptr);
      ptr=lcasebuf;
      while((ptr=strstr(ptr,*tarrptr))!=NULL)
        {
        /* check it's not in the middle of a word etc., except for
         * <>, <=, >=.
         */
        if((*tarrptr)[0]=='<' || (*tarrptr)[1]=='=' ||
           (!isalpha(ptr[-1]) && !isalpha(ptr[toklen])))
          {
          ptr2=linestart+(ptr-lcasebuf);
          /* the token text is overwritten in the lcase copy too, to
           * avoid problems with e.g. go to/to.
           */
          *ptr2=*ptr=toknum;
          for(f=1;f<toklen;f++) ptr2[f]=ptr[f]=1;
          /* absorb trailing spaces too */
          while(ptr2[f]==' ')
            ptr2[f++]=1;
          
          /* for BIN, we need the token right before the number. */
          if(toknum==BIN_TOKEN_NUM)
            {
            *ptr2=*ptr=1;
            ptr2[f-1]=ptr[f-1]=toknum;
            }
          }
        
        ptr+=toklen;
        }
      }
    
    if(use_labels)
      {
      /* replace @label with matching number.
       * this expands labels in strings too, since:
       * 1. it seems reasonable to assume you might want this;
       * 2. it makes the code a bit simpler :-) ;
       * 3. you can use the escape `\@' to get a literal `@' anyway.
       */
      
      ptr=linestart;
      while((ptr=strchr(ptr,'@'))!=NULL)
        {
        if(ptr[-1]=='\\')
          {
          ptr++;
          continue;
          }
        
        /* the easiest way to spot them is to try matching against
         * each label in turn. It's gross, but at least it's sane
         * and doesn't restrict what you can have as a label.
         * We also test that the char after a match is not a printable
         * ascii char (other than space or colon), to prevent matches
         * against the shorter of two possibilities.
         */
        ptr++;
        for(f=0;f<labelend;f++)
          {
          int len=strlen(labels[f]);
          if(memcmp(labels[f],ptr,len)==0 &&
            (ptr[len]<33 || ptr[len]>126 || ptr[len]==':'))
            {
            unsigned char numbuf[20];
            
            /* this could be optimised to use a single memmove(), but
             * at least this way it's clear(er) what's happening.
             */
            /* switch text for label. first, remove text */
            memmove(ptr-1,ptr+len,strlen(ptr+len)+1);
            /* make number string */
            sprintf(numbuf,"%d",label_lines[f]);
            len=strlen(numbuf);
            /* insert room for number string */
            ptr--;
            memmove(ptr+len,ptr,strlen(ptr)+1);
            memcpy(ptr,numbuf,len);
            ptr+=len;
            break;
            }
          }
        if(f==labelend)
          {
          fprintf(stderr,"line %d: undefined label\n",textlinenum);
          exit(1);
          }
        }
      }
    
    if(remptr) *remptr=REM_TOKEN_NUM;
    
    /* remove 0x01s, deal with backslash things, and add numbers */
    ptr=linestart;
    outptr=outbuf;
    in_rem=in_quotes=0;
    
    while(*ptr)
      {
      if(outptr>outbuf+sizeof(outbuf)-10)
        {
        fprintf(stderr,"line %d: line too big\n",textlinenum);
        exit(1);
        }
      
      if(*ptr=='"') in_quotes=!in_quotes;
      
      /* as well as 0x01 chars, we skip tabs. */
      if(*ptr==1 || *ptr==9 || (!in_quotes && !in_rem && *ptr==' '))
        {
        ptr++;
        continue;
        }
      
      if(*ptr==REM_TOKEN_NUM) in_rem=1;
      
      if(*ptr=='\\')
        {
        if(isalpha(ptr[1]) && strchr("VWXYZvwxyz",ptr[1])==NULL)
          *outptr++=144+tolower(ptr[1])-'a';
        else
          switch(ptr[1])
            {
            case '\\':	*outptr++='\\';	break;
            case '@':	*outptr++='@';	break;
            case '*':	*outptr++=127;	break;	/* copyright symbol */
            case '\'': case '.': case ':': case ' ': /* block graphics char */
              *outptr++=grok_block(ptr,textlinenum);
              ptr++;
              break;
            case '{': /* directly specify output code */
              /* find end of number */
              asciiptr=strchr(ptr+2,'}');
              if(asciiptr==NULL)
                {
                fprintf(stderr,
                  "line %d: unclosed brace in eight-bit character code\n",
                        textlinenum);
                exit(1);
                }
              /* parse number in decimal, octal or hex */
              num_ascii=strtoul(ptr+2, NULL, 0);
              if(num_ascii<0 || num_ascii>255)
                {
                fprintf(stderr,
                  "line %d: eight-bit character code out of range\n",
                        textlinenum);
                exit(1);
                }
              *outptr++=(char)num_ascii;
              /* set pointer to the second char from the end, so we're in the
               * right place when we skip forward two chars below
               */
              ptr=asciiptr-1;
              break;
            default:
              fprintf(stderr,
                "line %d: warning: unknown escape `%c', inserting literally\n",
                textlinenum,ptr[1]);
              *outptr++=ptr[1];
            }
        ptr+=2;
        continue;
        }
      
      /* spot any numbers (because we have to add the inline FP
       * representation). We do this largely by relying on strtod(),
       * so that we only have to find the start - i.e. a non-alpha char,
       * an optional `-' or `+', an optional `.', then a digit.
       */
      if(!in_rem && !in_quotes && !isalpha(ptr[-1]) &&
         (isdigit(*ptr) ||
          ((*ptr=='-' || *ptr=='+' || *ptr=='.') && isdigit(ptr[1])) ||
          ((*ptr=='-' || *ptr=='+') && ptr[1]=='.' && isdigit(ptr[2]))))
        {
        if(ptr[-1]!=BIN_TOKEN_NUM)
          /* we have a number. parse with strtod(). */
          num=strtod(ptr,(char **)&ptr2);
        else
          {
          /* however, if the number was after a BIN token, the inline
           * number must match the binary number, e.g. BIN 1001 would be
           * followed by an inline 9.
           */
          ptr2=ptr;
          num=(double)grok_binary(&ptr2,textlinenum);
          }
        
        /* output text of number */
        memcpy(outptr,ptr,ptr2-ptr);
        outptr+=ptr2-ptr;
        
        *outptr++=0x0e;
        if(!dbl2spec(num,&num_exp,&num_mantissa))
          {
          fprintf(stderr,"line %d: exponent out of range (number too big)\n",
            textlinenum);
          exit(1);
          }
        *outptr++=num_exp;
        *outptr++=(num_mantissa>>24);
        *outptr++=(num_mantissa>>16);
        *outptr++=(num_mantissa>>8);
        *outptr++=(num_mantissa&255);
        ptr=ptr2;
        }
      else
        {
        /* if not number, just output char */
        *outptr++=*ptr++;
        }
      }
    
    *outptr++=0x0d;	/* add terminating CR */
    
    /* output line */
    linelen=outptr-outbuf;
    if(fileptr+4+linelen>=filebuf+sizeof(filebuf))
      {
      /* the program would be too big to load into a speccy long before
       * you'd get this error, but FWIW...
       */
      fprintf(stderr,"program too big!\n");
      exit(1);
      }
    *fileptr++=(linenum>>8);
    *fileptr++=(linenum&255);
    *fileptr++=(linelen&255);
    *fileptr++=(linelen>>8);
    memcpy(fileptr,outbuf,linelen);
    fileptr+=linelen;
    }	/* end of pass-making while() */
  
  passnum++;
  }	/* end of do..while() pass loop */
while(use_labels && passnum<=2);

if(in!=stdin) fclose(in);


/* we only need to do this if outputting a .tap, but might as well do
 * it to check the label's ok anyway.
 */
if(*startlabel)
  {
  /* this is nearly a can't happen error, but FWIW... */
  if(!use_labels)
    fprintf(stderr,"Auto-start label specified, but not using labels!\n"),
      exit(1);
  for(f=0;f<labelend;f++)
    if(strcmp(startlabel,labels[f])==0)
      {
      startline=label_lines[f]; break;
      }
  if(f==labelend)
    fprintf(stderr,"Auto-start label is undefined\n"),exit(1);
  }


/* write output file */

if(strcmp(outfile,"-")!=0 && (out=fopen(outfile,"wb"))==NULL)
  fprintf(stderr,"Couldn't open output file.\n"),exit(1);

switch(output_format)
  {
case PLUS3DOS:
    siz=fileptr-filebuf;
  
    /* make header */
    memset(headerbuf, 0, sizeof headerbuf);
    memcpy(headerbuf, "PLUS3DOS\032\001", 10);

    headerbuf[11] =  (siz + 128)        & 255;
    headerbuf[12] = ((siz + 128) >> 8)  & 255;
    headerbuf[13] = ((siz + 128) >> 16) & 255;
    headerbuf[14] = ((siz + 128) >> 24) & 255;
    headerbuf[15] = 0;	/* BASIC */
    headerbuf[16]=(siz&255);
    headerbuf[17]=(siz/256);
    headerbuf[18]=(startline&255);
    headerbuf[19]=(startline/256);
    headerbuf[20]=(siz&255);
    headerbuf[21]=(siz/256);
  
    chk = 0;
    for (f=0;f<127;f++) chk += headerbuf[f];
    fwrite(headerbuf,1,127,out);
    fputc(chk,out);
    break;
case TAP:
    siz=fileptr-filebuf;
  
     /* make header */
    headerbuf[0]=0;
    for(f=strlen(speccy_filename);f<10;f++)
      speccy_filename[f]=32;
    strncpy(headerbuf+1,speccy_filename,10);
    headerbuf[11]=(siz&255);
    headerbuf[12]=(siz/256);
    headerbuf[13]=(startline&255);
    headerbuf[14]=(startline/256);
    headerbuf[15]=(siz&255);
    headerbuf[16]=(siz/256);
    
    /* write header */
    fprintf(out,"%c%c%c",19,0,chk=0);
    for(f=0;f<17;f++) chk^=headerbuf[f];
    fwrite(headerbuf,1,17,out);
    fputc(chk,out);
    
    /* write (most of) tap bit for data block */
    fprintf(out,"%c%c%c",(siz+2)&255,(siz+2)>>8,chk=255);
    for(f=0;f<siz;f++) chk^=filebuf[f];
    break;

    case RAW:
    break;
  }

fwrite(filebuf,1,fileptr-filebuf,out);

if(output_format == TAP)
  fputc(chk,out);

if(out!=stdout) fclose(out);

exit(0);
}
