/*
# =========================================================
# Copyright 2012-2020,  Nuno A. Fonseca (nuno dot fonseca at gmail dot com)
#
# This file is part of fastq_utils.
#
# This is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# if not, see <http://www.gnu.org/licenses/>.
#
#
# =========================================================
*/
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <regex.h> 
#include <zlib.h> 

#define HASHSIZE 100000001

#include "fastq.h"


int main(int argc, char **argv) {
  unsigned long paired=0;
  FASTQ_ENTRY *m1=fastq_new_entry(),
    *m2=fastq_new_entry();

  char rname[MAX_LABEL_LENGTH];

  fastq_print_version();
  
  if (argc!=6 && argc!=7 ) {
    fprintf(stderr,"Usage: filterpair fastq1 fastq2 paired1 paired2 unpaired [sorted]\n");
    //fprintf(stderr,"%d",argc);
    exit(PARAMS_ERROR_EXIT_STATUS);
  }
  fprintf(stderr,"%d",argc);
  short sorted=FALSE;
  FASTQ_FILE* fd1=fastq_new(argv[1],FALSE,"r");
  fastq_is_pe(fd1);
  FASTQ_FILE* fd2=fastq_new(argv[2],FALSE,"r");
  fastq_is_pe(fd2);
  
  if ( argc==7 )
    if ( !strcmp(argv[6],"sorted") )
      sorted=TRUE;
  
  
  fprintf(stderr,"HASHSIZE=%u\n",HASHSIZE);
  if ( sorted ) {
    fprintf(stderr,"Assuming sorted fastq files\n");
  }
  //memset(&collisions[0],0,HASHSIZE+1);
  hashtable index=new_hashtable(HASHSIZE);
  index_mem+=sizeof(hashtable);

  fprintf(stderr,"Scanning and indexing all reads from %s\n",fd1->filename);
  fastq_index_readnames(fd1,index,0,FALSE);
  fprintf(stderr,"Scanning complete.\n");

  // print some info
  fprintf(stderr,"Reads indexed: %llu\n",index->n_entries);
  fprintf(stderr,"Memory used in indexing: %ld MB\n",index_mem/1024/1024);
  // 
  char *p1=argv[3];
  char *p2=argv[4];
  char *p3=argv[5];
  FASTQ_FILE* fdw1=fastq_new(p1,FALSE,"w3");
  FASTQ_FILE* fdw2=fastq_new(p2,FALSE,"w3");
  FASTQ_FILE* fdw3=fastq_new(p3,FALSE,"w3");
  unsigned long up2=0;

  if ( fdw1==NULL || fdw2==NULL || fdw3==NULL ) {
    fprintf(stderr,"Unable to create output files\n");
    exit(PARAMS_ERROR_EXIT_STATUS);
  }
  
  // assumes that the fastq files are sorted
  if ( sorted == TRUE ) {
    // index file2
    hashtable index2=new_hashtable(HASHSIZE);
    index_mem+=sizeof(hashtable);

    fprintf(stderr,"Scanning and indexing all reads from %s\n",fd2->filename);
    fastq_index_readnames(fd2,index2,0,FALSE);
    fprintf(stderr,"Scanning complete.\n");

    // print some info
    fprintf(stderr,"Reads indexed: %llu\n",index2->n_entries);
    fprintf(stderr,"Memory used in indexing: %ld MB\n",index_mem/1024/1024);

    // go through file 1 and then through file2
    fastq_rewind(fd1);
    fastq_rewind(fd2);
    // fd1
    unsigned long len=0;
    char *readname=NULL;
    fprintf(stderr,"Filtering %s...\n",fd1->filename);
    while(!gzeof(fd1->fd)) {
      if (fastq_read_next_entry(fd1,m2)==0) break;
      readname=fastq_get_readname(fd1,m2,&rname[0],&len,TRUE);
      // lookup hdr in index
      INDEX_ENTRY* e=fastq_index_lookup_header(index2,readname);
      if (e==NULL) {
	// singleton
	++up2;
	fastq_write_entry(fdw3,m2);
      } else {
	// pair found
	++paired;
	fastq_write_entry(fdw1,m2);
	// remove entry from index
	fastq_index_delete(readname,index2);
      }
      PRINT_READS_PROCESSED(fd1->cline/4,10000);
    }
    // go through file2
    fprintf(stderr,"Filtering %s...\n",fd2->filename);
    while(!gzeof(fd2->fd)) {
      if (fastq_read_next_entry(fd2,m2)==0) break;
      readname=fastq_get_readname(fd2,m2,&rname[0],&len,TRUE);
      // lookup hdr in index
      INDEX_ENTRY* e=fastq_index_lookup_header(index,readname);
      if (e==NULL) {
	// singleton
	++up2;
	fastq_write_entry(fdw3,m2);
      } else {
	// pair found
	fastq_write_entry(fdw2,m2);
	// remove entry from index
	fastq_index_delete(readname,index);
      }
      PRINT_READS_PROCESSED(fd2->cline/4,10000);
    }
  } else {
    // go back to the beginning
    fastq_rewind(fd1);
    fprintf(stderr,"Processing %s\n",fd2->filename);fflush(stderr);
    // TODO: this can be considerably improved
    // requirement: the reads in the output files are sorted
    while(!gzeof(fd2->fd)) {
      if (fastq_read_next_entry(fd2,m2)==0) break;
      unsigned long len;
      char *readname=fastq_get_readname(fd2,m2,&rname[0],&len,TRUE);
      // lookup hdr in index
      INDEX_ENTRY* e=fastq_index_lookup_header(index,readname);
      if (e==NULL) {
	// singleton
	++up2;
	fastq_write_entry(fdw3,m2);
      } else {
	// pair found
	++paired;
	fastq_write_entry(fdw2,m2);
	// assumes that the order is similar to minimize seeks
	fastq_quick_copy_entry(e->entry_start,fd1,fdw1);
	// remove entry from index
	fastq_index_delete(readname,index);
      }
      //fprintf(stderr,"%d\n",fd2->cline);
      PRINT_READS_PROCESSED(fd2->cline/4,10000);
    }
    fprintf(stderr,"\n");
    fprintf(stderr,"Recording %llu unpaired reads from %s\n",index->n_entries,argv[1]);fflush(stderr);
    
    
#ifdef SEEKAPPROACH
    init_hash_traversal(index);
    unsigned long cline=0;
    INDEX_ENTRY* e;
    while((e=(INDEX_ENTRY*)next_hash_object(index))!=NULL) {
      fastq_seek_copy_read(e->entry_start,fd1,fdw3);
      PRINT_READS_PROCESSED(cline,100000);
      ++cline;
    }
    //
#else
    unsigned long remaining=index->n_entries;
    //FASTQ_ENTRY *m1=fastq_new_entry();
    //char rname[MAX_LABEL_LENGTH];
    
    while(!gzeof(fd1->fd) && remaining ) {
      if (fastq_read_next_entry(fd1,m1)==0) break;
      unsigned long len;
      char *readname=fastq_get_readname(fd1,m1,&rname[0],&len,TRUE);
      // lookup hdr in index
      INDEX_ENTRY* e=fastq_index_lookup_header(index,readname);
      if (e!=NULL) {
	//fastq_index_delete(readname,index);
	fastq_write_entry(fdw3,m1);
	remaining--;
      }
      PRINT_READS_PROCESSED(fd1->cline/4,100000);
    }
    fprintf(stderr,"Unpaired from %s: %llu\n",argv[1],index->n_entries);
    fprintf(stderr,"Unpaired from %s: %ld\n",argv[2],up2);
#endif
  }
  fprintf(stderr,"\n");
  fprintf(stderr,"Paired: %ld\n",paired);
  // close
  fastq_destroy(fdw1);
  fastq_destroy(fdw2);
  fastq_destroy(fdw3);
  fastq_destroy(fd1);
  fastq_destroy(fd2);
  if ( paired == 0 ) {
    fprintf(stderr,"!!!WARNING!!! 0 paired reads! are the headers ok?\n");
    exit(FASTQ_FORMAT_ERROR_EXIT_STATUS);
  }
  exit(0);
}
