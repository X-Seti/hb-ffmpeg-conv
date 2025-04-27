#X-Seti Aka Mooheda 17 April 2025
#kb-ffmpeg.conv tool
#See --help in tool for more details.

#Dependancies; nlohmann-json3-dev

#Debian / Ubuntu / Mint
sudo apt-get install nlohmann-json3-dev

#Pacman - Arch / Manjaro
sudo pacman -S nlohmann-json3-dev

# Compile with C++17 support.
g++ -std=c++17 kb-ffmpeg-conv.cpp -o kb-ffmpeg-conv

# Build with Cmake.
mkdir build && cd build
cmake ..
cmake --build .

# optional.
sudo cmake --install .

# Run the converter.
./kb-ffmpeg-conv your_preset.json [options]

# Example with the handbrake saved preset provided.
./kb-ffmpeg-conv hbpreset.json -d
