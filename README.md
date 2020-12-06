## Building
Prerequisites:
- Download and unpack the **Desktop Video** (library and kernel module) and the **Desktop Video SDK** (development files) packages from [https://www.blackmagicdesign.com/support/family/capture-and-playback](https://www.blackmagicdesign.com/support/family/capture-and-playback)
- Rename the unpacked SDK directory to something that has no whitespace in it.
- OpenCV

```
# install Blackmagic libraries
sudo dpkg -i $HOME/VOC/Blackmagic_Desktop_Video_Linux_10.9.9/deb/x86_64/desktopvideo_10.9.9a4_amd64.deb
# install build dependencies
sudo apt-get install libmicrohttpd-dev libpng-dev cmake make gcc g++
# clone Git repository
git clone http://c3voc.de/git/decklink-debugger.git
cd decklink-debugger
# create build directory
# you can then start over by removing and recreating this directory
mkdir build
cd build
# specify path to Blackmagic SDK (optional parameter)
cmake .. -DBLACKMAGIC_SDK_DIR=~/VOC/Blackmagic-DeckLink-SDK-10.9.9/Linux/include/
# compile application
make -j$(nproc)
```
