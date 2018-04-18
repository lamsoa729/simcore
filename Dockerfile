
FROM jeffmm/u_simcore

ENV REFRESHED_AT 20180416

RUN apt-get -qq update && apt-get install -y \
    build-essential libxmu-dev libxi-dev libgl1-mesa-swx11-dev libosmesa-dev \
    libxau-dev libx11-xcb-dev libxdmcp-dev xorg-dev libglu1-mesa-dev\
    && cd /build \
    && git clone https://github.com/glfw/glfw \
    && git clone https://github.com/nigels-com/glew \
    && cd /build/glew/auto && make && cd /build/glew && make && make install \
    && cd /build/glfw && cmake . && make && make install

ENV LD_LIBRARY_PATH="/build/glew/lib"

WORKDIR /project/simcore

CMD make CFG=release -j8 simcore

