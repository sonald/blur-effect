blur testing
===

Based on the [idea](http://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/)

build on debian/ubuntu
===
+ libglm-dev
+ libglfw3-dev
+ libglew-dev
+ libgbm-dev
+ libegl1-mesa-dev
+ libgles2-mesa-dev
+ libgdk-pixbuf2.0-dev

`mkdir build && cd build && cmake .. && make`

after build done, there are two executables. 
- use `blur-exps` to test blurring with windowing system. 
- use `blur_image` to blur a image and save as file. show usage by `./blur_image -h` 
  example: `./blur_image  -p 10 -r 11 /usr/share/wallpapers/deepin/Garden\ In\ The\ Autumn.jpg -o out.jpg `
