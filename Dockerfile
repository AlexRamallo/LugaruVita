FROM debian:latest

RUN apt update
RUN apt -y install \
	python \
	curl \
	wget \
	bzip2 \
	sudo \
	cmake \
	git \
	g++ \
	unzip \
	python3 \
	python3-pip \
	zip \
	imagemagick \
	pkg-config

RUN pip3 install pillow

#Install PVRTexTool
COPY PVRTexToolSetup-2022_R1.run-x64 /PVRTexToolSetup-2022_R1.run-x64
RUN yes | /PVRTexToolSetup-2022_R1.run-x64
#lol
ENV PATH=$PATH:/y/PVRTexTool/CLI/Linux_x86_64/

#Install VitaSDK
ENV VITASDK=/usr/local/vitasdk
ENV PATH=/usr/local/vitasdk/bin:$PATH

RUN git clone https://github.com/vitasdk/vdpm && \
	cd vdpm && \
	./bootstrap-vitasdk.sh && \
	./install-all.sh

#Build and install SDL2-VitaGL port
RUN git clone https://github.com/Northfear/SDL --branch=vitagl && \
	cd SDL && mkdir build && cd build && \
	cmake .. -DBUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake && \
	make install

