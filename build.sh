sudo apt-get install -y libopencv-dev python-opencv
git submodule update --init --recursive 
cd gflags && cmake . && make && cd ..
cd segment && cmake . && make && cd ..
