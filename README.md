Rytra
====

Rytra is a work in progress raytracing utility for rendering real time images.

It's progress is in the most early stage you can think off and comes without any optimizations. (for now)

Rendered on a Georce gtx 780 ti ~13 fps, ~0.076s to draw an image. 100 spp (samples per pixel) and a rrd (ray reflection depth) of 4. Rendered at 1200x600

![](scr.png?raw=true)


Building
-----

install the following libraries:
SDL2 + Cuda/OpenCL + make + cmake

On Windows and MacOS:
Install make and cmake, and make sure you have atleast the C++11 compiler installed.
  
Use those command to build:

- mkdir build
- cd build
- cmake ..
- make

  Run
  -----
Then you can run the demo:

	./demo


  Documentation
  -----
  None (for now)

Libraries
---------

- SDL2: <https://www.libsdl.org/download-2.0.php>
- OPENCL: <https://developer.nvidia.com/opencl/>


References
-------
- Raytracing in one weekend: <https://www.amazon.com/Ray-Tracing-Weekend-Minibooks-Book-ebook/dp/B01B5AODD8/>

License
-------

Copyright 2018 Rutger Klamer <rutger.klamer@gmail.com>

This project is provided as opensource. Do with it whatever you want.
