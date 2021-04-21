/*
 * =========================================================
 * Copyright 2017-2020,  Nuno A. Fonseca (nuno dot fonseca at gmail dot com)
 *
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * =========================================================
 */
#include <stdio.h>  
#include <math.h>
#include <bam.h>
#include <sam.h>
#include <kstring.h>      

#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <float.h>

#include <regex.h> 
#include <zlib.h> 

#include "sam_tags.h"
#include "fastq.h"


#define BUF_SIZE 10000
typedef enum { R1=0,R2=1,CELL=2,SAMPLE=3,UMI=4,SE=5} FILE_LOC;


char EMPTY_STRING[1]="\0";

char *get_tag(bam1_t *aln,const char tagname[2]) {

  uint8_t *s=bam_aux_get(aln,tagname);
  if (s==0) return(NULL);

  char *s2=bam_aux2Z(s);
  if (s2==0) return(EMPTY_STRING);

  return(s2);
}

void QWRITE(gzFile fd,FILE_LOC type, char*s1,char*s2,char*s3,int add_suffix) {
  char rn_suf[4]="";
  if (add_suffix) // add a suffix
    sprintf(&rn_suf[0],"/%u",(short)type+1);
  GZ_WRITE(fd,"@");
  GZ_WRITE(fd,s1);
  GZ_WRITE(fd,rn_suf);
  GZ_WRITE(fd,"\n");
  GZ_WRITE(fd,s2);
  GZ_WRITE(fd,"\n+\n");
  if (s3!=NULL)
    GZ_WRITE(fd,s3);
  GZ_WRITE(fd,"\n");
}

gzFile get_fp(gzFile *fps, FILE_LOC type, const char *file_prefix) {
  
  if ( fps[type]==NULL ) {
    // open file
    char buf[BUF_SIZE];
    char *ext[]={"_1","_2","_cell","_sample","_umi",""};
    sprintf(buf,"%s%s.fastq.gz",file_prefix,ext[type]);
    fps[type]=fastq_open(buf,"wb");
    fprintf(stderr,"opening %s\n",buf);
  }
  return(fps[type]);
}
/*
char* restore_read_name(char *s) {
  unsigned int i=0;
  while ( s[i]!='\0' ) {
    if ( s[i]=='@' ) s[i]=' ';
    i++;
  }
  return(s);
}
*/
char* my_get_seq(bam1_t *aln, char *buf) {
  unsigned int i,len = aln->core.l_qseq;
  uint8_t *seq = bam1_seq(aln);
  for (i = 0; i < len; ++i)
    buf[i] =  bam_nt16_rev_table[bam1_seqi(seq, i)]; // convert the internal bit representation
  buf[len]='\0';
  return(buf);   
}

char* get_qual(bam1_t *aln,char *buf) {
  unsigned int i,len = aln->core.l_qseq;
  char *seq = bam1_qual(aln);
  for (i = 0; i < len; ++i)
    buf[i] = 33 + seq[i];
  buf[len]='\0';
  return(buf);
  
}


void print_usage(int exit_status) {
    PRINT_ERROR("Usage: bam2fastq --bam in.bam --out fastq_prefix [--verbose --10x|-X]");
    if ( exit_status>=0) exit(exit_status);
}


int main(int argc, char *argv[])  
{  
  bamFile in; 
  char *bam_file=NULL;
  char *out_file_prefix=NULL;
  int i;

  static int verbose=0;  
  static int help=FALSE;
  static struct option long_options[] = {
    {"verbose", no_argument,       &verbose, TRUE},
    {"help",   no_argument, &help, TRUE},
    {"bam",  required_argument, 0, 'b'},
    {"out",  required_argument, 0, 'o'},
    {"10x",  no_argument, (int*)&__10x_compat,1},
    {0,0,0,0}
  };
  
  fprintf(stderr,"bam2fastq version %s\n",VERSION);
  // process arguments
  while (1) {
    /* getopt_long stores the option index here. */
    int option_index = 0;
    
    int c = getopt_long (argc, argv, "b:o:hX",
			 long_options, &option_index);      
    if (c == -1) // no more options
      break;
    
    switch (c) {
    case 'X':
      __10x_compat=1;
      break;
    case 'b':
      bam_file=optarg;
      break;
    case 'o':
      out_file_prefix=optarg;
      break;
    case 'h':
      help=TRUE;
      break;
    default:
      //print_usage(1);
      break;
    }
  }
  if (help )  print_usage(0);
  
  if ( bam_file == NULL ) print_usage(1);
  if ( out_file_prefix == NULL ) print_usage(1);
  
  
  // Open file and exit if error
  in = strcmp(bam_file, "-")? bam_open(bam_file, "rb") : bam_dopen(fileno(stdin), "rb"); 
  
  if (in == 0 ) {  
    PRINT_ERROR("Failed to open BAM file %s", bam_file);  
    return(PARAMS_ERROR_EXIT_STATUS);  
  }  
  
  gzFile fd[6]={NULL,NULL,NULL,NULL,NULL,NULL};
  
  //
  // 
  bam1_t *aln=bam_init1();
  fprintf(stderr,"Processing %s\n",bam_file);
  // read header
  bam_header_read(in);
  
  // tmp buffers
  char seq_buf[BUF_SIZE];
  char qual_buf[BUF_SIZE];

  
  //
  int is_pe=-1;
  unsigned long long num_alns=0;
  
  while( bam_read1(in,aln)>=0 ) {
    if ( num_alns == ULLONG_MAX ) {       
      PRINT_ERROR("counter overflow (number of alignments) - %llu\n",num_alns);
      exit(3);
    }
     
     ++num_alns;
     if (num_alns%100000==0) { fprintf(stderr,"\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b%llu",num_alns); fflush(stderr); }

     // exclude non-primary alignments
     if ( aln->core.flag & BAM_FSECONDARY) {
       //fprintf(stderr,"Skipping...\n");
       continue;
     }
     // read/qual (se/pe/first/second)
     char *hdr=get_tag(aln,ORIG_RN_TAG);
     char *seq=my_get_seq(aln,&seq_buf[0]);
     char *qual=get_tag(aln,ORIG_QUAL_TAG);
     if ( hdr==NULL ) {
       // ORIG_RN_TAG missing
       // BAM was not generated with fastq2bam
       char *hdr=bam1_qname(aln);
       qual=get_qual(aln,&qual_buf[0]);
       if ( qual == NULL ) {
	 // create a dummy sequence to ensure that the fastq file is valid
	 unsigned int i,len = aln->core.l_qseq;
	 for (i = 0; i < len; ++i)
	   qual_buf[i] =  'H';
	 qual_buf[len]='\0';
	 qual=&qual_buf[0];
       }
       // 
       if (aln->core.flag & BAM_FPAIRED ) {  // SE
	 is_pe=TRUE;
       } else { // PE
	 is_pe=FALSE;
       }
       short write_to=R2;       
       if ( ! is_pe ) {
	 write_to=SE;
       } else if ( (aln->core.flag==(BAM_FUNMAP|BAM_FMUNMAP|BAM_FPAIRED|BAM_FREAD1)) || aln->core.flag & BAM_FUNMAP )  {
	 write_to=R1;
       } 
       QWRITE(get_fp(fd, write_to , out_file_prefix),write_to, hdr,seq,qual,TRUE);
     } else {
       // bam generated by fastq2bam 
       // TODO: add checks?

       is_pe=FALSE;
       if (aln->core.flag & BAM_FPAIRED ) {
	 is_pe=TRUE;
       }

       if (  !is_pe  ||
	    (aln->core.flag & BAM_FREAD1 ) ) {
	 // 
	 short write_to=R1;       
	 if ( ! is_pe )
	   write_to=SE;

	 QWRITE(get_fp(fd, write_to , out_file_prefix),R1, hdr,seq,qual,FALSE);
	 // cell
	 if (get_tag(aln,CELL_TAG)!=NULL)
	   QWRITE(get_fp(fd, CELL, out_file_prefix),CELL, hdr,get_tag(aln,CELL_TAG),get_tag(aln,CELL_QUAL_TAG),FALSE);
	 // umi
	 if (get_tag(aln,GET_UMI_TAG)!=NULL)
	   QWRITE(get_fp(fd, UMI, out_file_prefix),UMI,hdr,get_tag(aln,GET_UMI_TAG),get_tag(aln,GET_UMI_QUAL_TAG),FALSE);
	 // sample
	 if (get_tag(aln,SAMPLE_TAG)!=NULL)
	   QWRITE(get_fp(fd, SAMPLE, out_file_prefix),SAMPLE,hdr,get_tag(aln,SAMPLE_TAG),get_tag(aln,SAMPLE_QUAL_TAG),FALSE);
       } else { //R2
	 QWRITE(get_fp(fd, R2 , out_file_prefix),R2,hdr,seq,qual,FALSE);
       }
     }
  }
  // TODO: catch errors
  for (i=0;i<=5;i++)
    if (fd[i]!=NULL) gzclose(fd[i]);
  
  fprintf(stderr,"\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\n");fflush(stderr);
  bam_destroy1(aln);
  // write output
  fprintf(stderr,"Alignments processed: %llu\n",num_alns);

  return(0);
}
