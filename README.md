offscreen image blurring tool
===

Based on the [idea](http://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/)

build on debian/ubuntu
===
requirements:
+ libglm-dev
+ libglfw3-dev (only needed if build demo `blur-exp`)
+ libglew-dev
+ libgbm-dev
+ libegl1-mesa-dev
+ libgles2-mesa-dev
+ libgdk-pixbuf2.0-dev

`mkdir build && cd build && cmake .. && make`

after build has done, 
use the binary `blur_image` to blur a image and save as file. show usage by `./blur_image -h` 
  example: `./blur_image  -p 10 -r 11 /usr/share/wallpapers/deepin/Garden\ In\ The\ Autumn.jpg -o out.jpg `

note: if you run it with a X Server running (which is  the most probable case), add `sudo` at the front.
or you can use render node to do blurring without privilege like 
```
./blur_image -d /dev/dri/renderD128 -p 6 -r 7 /usr/share/wallpapers/deepin/Garden\ In\ The\ Autumn.jpg -o out.jpg 
```

in case if you want to build demo
use `cmake -DBUILD_DEMO=on ..` instead and after build finished, 
use `blur-exps` to test blurring with windowing system. 
