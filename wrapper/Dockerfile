FROM humancellatlas/upload-validator-base-alpine:22

LABEL maintainer="hca-ingest-dev at ebi.ac.uk"

#RUN yum update -y && yum install -y bzip2-devel ncurses-devel bzip2 zlib-devel git gcc wget make xz-devel && yum clean all
RUN yum update -y && yum install -y bzip2-devel ncurses-devel bzip2 zlib-devel git gcc wget make xz-devel tar && yum clean all
RUN git clone https://github.com/nunofonseca/fastq_utils.git && cd fastq_utils && ./install_deps.sh && make && make install && rm -rf test* && cp bin/* /usr/bin

ADD wrapper /wrapper
RUN chmod +x /wrapper/validation_wrapper.py
RUN ln -s /wrapper/validation_wrapper.py validator
