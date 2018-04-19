FROM gcc:6.1

ENV REFRESHED_AT 20180418

RUN apt-get -qq update && apt-get install -y \
      cmake \
      libgsl0-dev \
      build-essential \
      libxmu-dev \
      libxi-dev \
      libgl1-mesa-swx11-dev \
      libosmesa-dev \
      libxau-dev \
      libx11-xcb-dev \
      libxdmcp-dev \
      xorg-dev \
      libglu1-mesa-dev \
      && mkdir /build && cd /build \
      && git clone https://github.com/jbeder/yaml-cpp \
      && git clone https://github.com/glfw/glfw \
      && git clone https://github.com/nigels-com/glew \
      && mkdir /build/yaml-cpp/lib \
      && cd /build/yaml-cpp/lib && cmake .. && make \
      && cd /build/glew/auto && make \
      && cd /build/glew && make && make install \
      && cd /build/glfw && cmake . && make && make install

WORKDIR /project/simcore
ENV LD_LIBRARY_PATH="/build/glew/lib"

CMD make CFG=release -j8 simcore
