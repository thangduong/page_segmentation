# page_segmentation
code to segment a page of a paper into logical sections

<img src="https://github.com/thangduong/page_segmentation/blob/master/images/evorus-0000.png?raw=true" width="200">
<img src="https://github.com/thangduong/page_segmentation/blob/master/images/evorus-0000.green.png?raw=true" width="200">


When converted with -density 196:
segment -input_file <filename> -num_stages 3 -vp1_min_break_size 0 -hp0_min_break_size 20

When converted with -density 128:
segment -input_file <filename> -num_stages 3 -vp1_min_break_size 0 -hp0_min_break_size 15



Getting Started


git@github.com:thangduong/page_segmentation.git
cd page_segmentation
git submodule update --init --recursive
cd gflags
cmake .
make
