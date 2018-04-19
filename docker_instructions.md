
To build the image, do

$ docker build -t "jeffmm/u_simcore:latest" .

To run the image (which currently defaults to using Make to build simcore for release), do

$ docker run --rm -v $(PWD):/project/simcore jeffmm/u_simcore:latest

To run simcore, 

$ docker run --rm -v $(PWD):/project/simcore jeffmm/u_simcore:latest ./simcore params.yaml

To shell into the container:

$ docker run -it -v $(PWD):/project/simcore jeffmm/u_simcore:latest bash

===OPENGL GRAPHICS FOR DEBUGGING AND MAKING MOVIES===

First make sure XQuartz is installed and running. You can install it with brew

$ brew install caskroom/cask/XQuartz

Then in XQuartz go to Preferences > Security and select "Allow connections from network clients"

Now install socat

$ brew install socat 

and in a separate terminal run

$ socat TCP-LISTEN:6000,reuseaddr,fork UNIX-CLIENT:\"$DISPLAY\"

Now set the following environment variables

$ IP=$(ifconfig en0 | grep inet | awk '$1=="inet" {print $2}')

==== MAYBE NOT NECESSARY?? ====
Then enter the command
$ xhost + $IP
===============================

Now we can run docker with X11 forwarding by using some extra flags:

==== NO NEED TO SET ENV ====
$ docker run --rm -it -e $LD_LIBRARY_PATH -v /tmp/.X11-unix:/tmp/.X11-unix -e DISPLAY=$IP:0 -v $(PWD):/project/simcore jeffmm/u_simcore_gl ./simcore params.yaml
============================

$ docker run --rm -it -v /tmp/.X11-unix:/tmp/.X11-unix -e DISPLAY=$IP:0 -v $(PWD):/project/simcore jeffmm/u_simcore_gl ./simcore params.yaml

The openGL window should pop up in XQuartz

