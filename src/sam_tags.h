/*
# =========================================================
# Copyright 2017-2020,  Nuno A. Fonseca (nuno dot fonseca at gmail dot com)
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
// SAM TAGS 
#define RNAME "*"
#define POS   "0"
#define CIGAR "*"
#define RNEXT "*"
#define PNEXT "0"
#define MAPQ "255" 

#define CELL_TAG "CR" 
#define CELL_QUAL_TAG "CY" 

// SAM specification
#define UMI_TAG "RX" 
#define UMI_QUAL_TAG "QX"

// used by 10x: '[[https://support.10xgenomics.com/single-cell-gene-expression/software/pipelines/latest/output/bam
#define UMI_TAG_10X "UB" 
#define UMI_TAG_QUAL_10X "UY" 

short __10x_compat=0;
#define GET_UMI_TAG ((__10x_compat==0)?UMI_TAG:UMI_TAG_10X)
#define GET_UMI_QUAL_TAG ((__10x_compat==0)?UMI_QUAL_TAG:UMI_TAG_QUAL_10X)

#define SAMPLE_TAG "BC"
#define SAMPLE_QUAL_TAG  "QT"

#define ORIG_RN_TAG "on"
#define ORIG_QUAL_TAG "op" 


// break spec to keep compatibility with some tools
#define GENE_ID_TAG "GX"
#define TRANSCRIPT_ID_TAG "tx"

// intronic, exonic
#define ANNOT_TAG YB
