
To build the image, do

docker build -t "jeffmm/u_simcore:latest" .

To run the image (which currently defaults to using Make to build simcore for release), do

docker run --rm -v $(PWD):/project/simcore jeffmm/u_simcore:latest

To run simcore, 

docker run --rm -v $(PWD):/project/simcore jeffmm/u_simcore:latest ./simcore params.yaml

To shell into the container:

docker run -it -v $(PWD):/project/simcore jeffmm/u_simcore:latest bash
