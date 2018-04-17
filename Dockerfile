
FROM gcc:6.1

ENV REFRESHED_AT 20180416

RUN apt-get -qq update && apt-get install -y cmake libgsl0-dev \
      && git clone https://github.com/jbeder/yaml-cpp /build/yaml-cpp \
      && mkdir /build/yaml-cpp/lib && cd /build/yaml-cpp/lib \
      && cmake /build/yaml-cpp && make

WORKDIR /project/simcore

CMD make CFG=release NOGRAPH=true -j8 simcore

