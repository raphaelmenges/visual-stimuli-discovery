# WebVSD: Visual Stimuli Discovery for Websites
A implementation of the visual stimuli discovery for Websites in C++, using OpenCV, Tesseract, and Shogun.

## Requirements
Generation and compilation have been tested on Windows 10 with Visual Studio 2015 or 2017 and Debian 8.8 with the software listed below.

### Windows
Install the following software:
- *Git*
- *Python 3*
- *cmake 2.8* or higher
- *Visual Studio 2015* or *2017*
There should be **no Anaconda** distribution installed on the system.
Third party frameworks like Shogun or Leptonica link into Anaconda-provided dependencies by accident, yet, Anaconda does not provide a complete distribution of the dependency why the build process fails.
For example, if Anaconda provides a MKL / BLAS / LAPACK library, Shogun will fail building due to missing headers.

### Debian 8.8 (similar on Ubuntu)
Install the following packages through the package manager:
- *git*
- *build-essential*
- *cmake*
- optional: *libgtk2.0-dev* or *libgtk3.0-dev* (for non-headless mode)

## Procedure
Open _cmd_/_terminal_ in the directory where the project should be placed and execute the following commands:
```sh
git clone --recursive https://github.com/raphaelmenges/VisualStimuliDiscovery.git
cd VisualStimuliDiscovery
python .
```
The procedure assumes Python 3 to be the standard Python environment on your system. There are arguments available for the Python call:
* `-c` (`--configuration`): build configuration, either 'release' or 'debug'
* `-g` (`--generator`): generator, either 'MSVC2015' or 'MSVC2017' on Windows or 'Make' on Linux
* `-v` (`--visualdebug`): visual debugging mode. aka non-headless
* `-d` (`--deploy`): deploy build binaries and corresponding resources
* `-s` (`--singlethreaded`): execute tasks single- instead of multi-threaded
* `-b` (`--build`): compiles the framework after generation

## Dataset
https://zenodo.org/record/3908124

## Tools
The folder _tools_ contains further software for the evaluation of the Visual Stimuli Discovery.

## License
>Copyright 2020 Raphael Menges and Christoph Schaefer
>
>Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
>
>The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
>
>THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Acknowledgment
Developed as part of the [GazeMining](https://gazemining.de/index_en.html) research project. We acknowledge the financial support by the Federal Ministry of Education and Research of Germany under the project number 01IS17095B.

## TODO
- Better understanding of dynamic linking of Tesseract and Leptonica under Linux
- Fix deployment of the framework under Linux in regard to dynamic libraries of Tesseract and Leptonica (rpath / ld_library_path)
- Framework issue: Trainer and other applications can only be built when Visual Debug is enabled...
- Change static linking of OpenCV and internal libraries to dynamic linking?
- Fix linking of Shogun in Debug mode (both Windows and Linux)
