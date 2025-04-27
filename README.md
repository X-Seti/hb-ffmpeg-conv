# hb-ffmpeg-conv
Simple tool that converts handbrake saved presets, json files into usable ffmpeg syntax.
#See --help in tool for more details.

#Dependancies; nlohmann-json3-dev, ffmpeg

#Debian / Ubuntu / Mint
sudo apt-get install nlohmann-json3-dev ffmpeg

#Pacman - Arch / Manjaro
sudo pacman -S nlohmann-json3-dev ffmpeg

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
