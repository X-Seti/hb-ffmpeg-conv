// hb-ffmpeg-conv.cpp
// X-Seti aka Mooheda - 27 April 2025
// Handbrake saved json presets, Converted to run as ffmpeg syntax
// Movie file convertion tool.

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <cstdlib>
#include <algorithm>
#include <memory>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

// Script version
const std::string SCRIPT_VERSION = "0.9";

// Default media extensions
const std::vector<std::string> MEDIA_EXTENSIONS = {
    "mp4", "mkv", "avi", "mov", "wmv", "flv", "webm", "m4v", "mpg", "mpeg", "ts"
};

struct Settings {
    std::string preset_name;
    std::string video_encoder;
    std::string video_bitrate;
    std::string video_preset;
    std::string video_profile;
    std::string video_framerate;
    std::string video_quality;
    std::string video_quality_type;
    bool video_multipass;
    std::string picture_width;
    std::string picture_height;
    std::string audio_encoder;
    std::string audio_bitrate;
    std::string audio_mixdown;
    std::string container;
};

struct FFmpegParams {
    std::string vcodec;
    std::string acodec;
    std::string audio_channels;
    std::string quality;
    std::string format;
    std::string preset;
    std::string profile;
    std::string framerate;
    std::string resolution;
    bool multipass;
    std::string preset_name;
};

// Function prototypes
void show_usage(const char* progname);
Settings extract_preset_settings(const json& preset_data);
FFmpegParams convert_to_ffmpeg_params(const Settings& settings);
void show_preset(const FFmpegParams& ffmpeg_params, const std::string& output_format,
                int analyze_duration, int probe_size);
bool should_ignore_file(const std::string& file_path, const std::string& ignore_flag);
std::string format_filename(const std::string& basename, bool replace_underscores);
bool check_file_access(const std::string& file_path);
std::string get_null_device();
std::vector<std::string> build_ffmpeg_command(const std::string& input_file,
                                             const std::string& output_file,
                                             const FFmpegParams& ffmpeg_params,
                                             int analyze_duration,
                                             int probe_size,
                                             bool verbose);
std::vector<std::vector<std::string>> build_multipass_commands(const std::string& input_file,
                                                              const std::string& output_file,
                                                              const FFmpegParams& ffmpeg_params,
                                                              int analyze_duration,
                                                              int probe_size,
                                                              bool verbose);
bool rename_to_m4v(const std::string& file_path, bool dry_run);
void get_file_info(const std::string& file_path);
std::vector<std::string> find_media_files(const std::string& directory,
                                         bool recursive,
                                         const std::vector<std::string>& extensions);
int process_file(const std::string& input_file,
                const std::string& media_dir,
                const std::string& output_dir,
                const FFmpegParams& ffmpeg_params,
                const std::string& original_format,
                const std::string& output_format,
                bool force_m4v,
                bool execute,
                bool dry_run,
                bool replace_underscores,
                int analyze_duration,
                int probe_size,
                bool verbose);
std::vector<std::string> split_string(const std::string& s, char delimiter);
std::string join_string(const std::vector<std::string>& elements, const std::string& delimiter);
std::string escape_string(const std::string& s);
int execute_command(const std::vector<std::string>& cmd, bool verbose);

void show_usage(const char* progname) {
    std::cout << "Usage: " << progname << " [input_json_file] [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -r, --recursive    Process media files recursively in subdirectories" << std::endl;
    std::cout << "  -e, --execute      Execute the generated ffmpeg commands" << std::endl;
    std::cout << "  -d, --dry-run      Show what would be done without actually doing it" << std::endl;
    std::cout << "  -p, --show-preset  Show only the ffmpeg equivalent of the preset" << std::endl;
    std::cout << "  -i, --input-dir    Specify input directory (default: same as JSON file)" << std::endl;
    std::cout << "  -o, --output-dir   Specify output directory (default: input_dir/converted)" << std::endl;
    std::cout << "  -m, --force-m4v    Force output extension to .m4v regardless of container" << std::endl;
    std::cout << "  -u, --no-underscore-replace  Don't replace underscores with spaces in output filenames" << std::endl;
    std::cout << "  --ignore-flag=X    Set custom ignore flag file (default: .noconvert)" << std::endl;
    std::cout << "  --verbose          Show verbose output and ffmpeg logs" << std::endl;
    std::cout << "  -v, --version      Show version: " << SCRIPT_VERSION << std::endl;
    std::cout << "  -h, --help         Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Special Features:" << std::endl;
    std::cout << "  - Files will be skipped if a '.noconvert' file exists in the same directory" << std::endl;
    std::cout << "  - Use -m/--force-m4v to output all files with .m4v extension" << std::endl;
    std::cout << "  - By default, underscores in filenames are replaced with spaces" << std::endl;
    exit(1);
}

json load_json_preset(const std::string& json_file) {
    try {
        std::ifstream f(json_file);
        if (!f.is_open()) {
            std::cerr << "Error: JSON file '" << json_file << "' does not exist." << std::endl;
            exit(1);
        }
        json data = json::parse(f);
        return data;
    } catch (json::parse_error& e) {
        std::cerr << "Error: Failed to parse '" << json_file << "' as valid JSON: " << e.what() << std::endl;
        exit(1);
    }
}

Settings extract_preset_settings(const json& preset_data) {
    Settings settings;

    // Extract preset from PresetList (first preset)
    json preset = preset_data.value("PresetList", json::array())[0];

    // Extract audio settings from first audio track (if available)
    json audio_list = preset.value("AudioList", json::array());
    json audio_settings = audio_list.size() > 0 ? audio_list[0] : json::object();

    // Fill the settings struct with type-safe accessors
    settings.preset_name = preset.value("PresetName", "");
    settings.video_encoder = preset.value("VideoEncoder", "");

    // Handle values that could be numbers or strings safely
    if (preset.contains("VideoAvgBitrate")) {
        settings.video_bitrate = std::to_string(preset["VideoAvgBitrate"].get<int>());
    } else {
        settings.video_bitrate = "0";
    }

    settings.video_preset = preset.value("VideoPreset", "");
    settings.video_profile = preset.value("VideoProfile", "");

    if (preset.contains("VideoFramerate")) {
        if (preset["VideoFramerate"].is_string()) {
            settings.video_framerate = preset["VideoFramerate"].get<std::string>();
        } else {
            settings.video_framerate = std::to_string(preset["VideoFramerate"].get<int>());
        }
    } else {
        settings.video_framerate = "";
    }

    if (preset.contains("VideoQualitySlider")) {
        settings.video_quality = std::to_string(preset["VideoQualitySlider"].get<double>());
    } else {
        settings.video_quality = "0";
    }

    if (preset.contains("VideoQualityType")) {
        if (preset["VideoQualityType"].is_string()) {
            settings.video_quality_type = preset["VideoQualityType"].get<std::string>();
        } else {
            settings.video_quality_type = std::to_string(preset["VideoQualityType"].get<int>());
        }
    } else {
        settings.video_quality_type = "";
    }

    settings.video_multipass = preset.value("VideoMultiPass", false);

    if (preset.contains("PictureWidth")) {
        settings.picture_width = std::to_string(preset["PictureWidth"].get<int>());
    } else {
        settings.picture_width = "0";
    }

    if (preset.contains("PictureHeight")) {
        settings.picture_height = std::to_string(preset["PictureHeight"].get<int>());
    } else {
        settings.picture_height = "0";
    }

    settings.audio_encoder = audio_settings.value("AudioEncoder", "");

    if (audio_settings.contains("AudioBitrate")) {
        settings.audio_bitrate = std::to_string(audio_settings["AudioBitrate"].get<int>());
    } else {
        settings.audio_bitrate = "0";
    }

    settings.audio_mixdown = audio_settings.value("AudioMixdown", "");
    settings.container = preset.value("FileFormat", "");

    return settings;
}

FFmpegParams convert_to_ffmpeg_params(const Settings& settings) {
    FFmpegParams result;

    // Convert video encoder
    if (settings.video_encoder == "x265") {
        result.vcodec = "libx265";
    } else if (settings.video_encoder == "x264") {
        result.vcodec = "libx264";
    } else {
        result.vcodec = settings.video_encoder;
    }

    // Convert audio encoder
    if (settings.audio_encoder.rfind("copy:", 0) == 0) {
        std::string audio_codec = settings.audio_encoder.substr(5); // remove 'copy:'
        result.acodec = "-c:a copy";
    } else {
        result.acodec = "-c:a aac -b:a " + settings.audio_bitrate + "k";
    }

    // Handle audio mixdown
    if (settings.audio_mixdown == "5point1") {
        result.audio_channels = "-ac 6";
    } else if (settings.audio_mixdown == "stereo") {
        result.audio_channels = "-ac 2";
    } else if (settings.audio_mixdown == "mono") {
        result.audio_channels = "-ac 1";
    } else {
        result.audio_channels = "";
    }

    // Video quality settings
    if (settings.video_quality_type == "2") {
        // CRF mode
        result.quality = "-crf " + settings.video_quality;
    } else {
        // Bitrate mode
        result.quality = "-b:v " + settings.video_bitrate + "k";
    }

    // Convert container format
    if (settings.container == "av_mkv") {
        result.format = "mkv";
    } else if (settings.container == "av_mp4") {
        result.format = "mp4";
    } else {
        result.format = "mkv";  // Default to MKV
    }

    result.preset = settings.video_preset;
    result.profile = settings.video_profile;
    result.framerate = settings.video_framerate;
    result.resolution = settings.picture_width + "x" + settings.picture_height;
    result.multipass = settings.video_multipass;
    result.preset_name = settings.preset_name;

    return result;
}

void show_preset(const FFmpegParams& ffmpeg_params, const std::string& output_format,
                int analyze_duration, int probe_size) {
    std::cout << "============================================" << std::endl;
    std::cout << "Handbrake Preset: " << ffmpeg_params.preset_name << std::endl;
    std::cout << "FFmpeg Equivalent Parameters:" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "Video codec:      -c:v " << ffmpeg_params.vcodec << std::endl;
    std::cout << "Quality:          " << ffmpeg_params.quality << std::endl;
    std::cout << "Preset:           -preset " << ffmpeg_params.preset << std::endl;

    if (ffmpeg_params.framerate != "auto" && !ffmpeg_params.framerate.empty()) {
        std::cout << "Framerate:        -r " << ffmpeg_params.framerate << std::endl;
    }

    std::cout << "Resolution:       -s " << ffmpeg_params.resolution << std::endl;
    std::cout << "Audio:            " << ffmpeg_params.acodec << " " << ffmpeg_params.audio_channels << std::endl;

    if (ffmpeg_params.profile != "auto" && !ffmpeg_params.profile.empty()) {
        std::cout << "Profile:          -profile:v " << ffmpeg_params.profile << std::endl;
    }

    std::cout << "Output format:    " << output_format << std::endl;

    if (ffmpeg_params.multipass && ffmpeg_params.quality.find("-crf") == std::string::npos) {
        std::cout << "Multipass:        Enabled (two-pass encoding)" << std::endl;
    } else {
        std::cout << "Multipass:        Disabled (single-pass encoding)" << std::endl;
    }

    std::cout << "Analyze duration: " << analyze_duration << std::endl;
    std::cout << "Probe size:       " << probe_size << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "Example usage:" << std::endl;
    std::cout << "ffmpeg -analyzeduration " << analyze_duration << " -probesize " << probe_size
              << " -i input.mp4 -c:v " << ffmpeg_params.vcodec << " " << ffmpeg_params.quality
              << " -preset " << ffmpeg_params.preset << " -s " << ffmpeg_params.resolution
              << " " << ffmpeg_params.acodec << " " << ffmpeg_params.audio_channels
              << " output." << output_format << std::endl;
    std::cout << "============================================" << std::endl;
}

bool should_ignore_file(const std::string& file_path, const std::string& ignore_flag) {
    fs::path dir_path = fs::path(file_path).parent_path();
    if (fs::exists(dir_path / ignore_flag)) {
        return true;
    }
    return false;
}

std::string format_filename(const std::string& basename, bool replace_underscores) {
    if (replace_underscores) {
        std::string result = basename;
        std::replace(result.begin(), result.end(), '_', ' ');
        return result;
    }
    return basename;
}

bool check_file_access(const std::string& file_path) {
    fs::path path(file_path);
    fs::path dir_path = path.parent_path();

    if (!fs::is_directory(dir_path)) {
        std::cout << "Error: Output directory '" << dir_path.string() << "' does not exist." << std::endl;
        return false;
    }

    if (fs::exists(path) && fs::status(path).permissions() == fs::perms::none) {
        std::cout << "Error: Output file '" << file_path << "' exists but is not writable." << std::endl;
        return false;
    }

    try {
        // Quick test if we can write to the directory
        fs::path test_file = dir_path / ".write_test_temp";
        std::ofstream test(test_file);
        if (!test.is_open()) {
            std::cout << "Error: Output directory '" << dir_path.string() << "' is not writable." << std::endl;
            return false;
        }
        test.close();
        fs::remove(test_file);
    } catch (...) {
        std::cout << "Error: Output directory '" << dir_path.string() << "' is not writable." << std::endl;
        return false;
    }

    return true;
}

std::string get_null_device() {
#ifdef _WIN32
    return "NUL";
#else
    return "/dev/null";
#endif
}

std::vector<std::string> split_string(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream token_stream(s);
    while (std::getline(token_stream, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

std::string join_string(const std::vector<std::string>& elements, const std::string& delimiter) {
    std::ostringstream result;
    for (size_t i = 0; i < elements.size(); ++i) {
        result << elements[i];
        if (i < elements.size() - 1) {
            result << delimiter;
        }
    }
    return result.str();
}

std::string escape_string(const std::string& s) {
    if (s.find(' ') != std::string::npos) {
        return "\"" + s + "\"";
    }
    return s;
}

std::vector<std::string> build_ffmpeg_command(const std::string& input_file,
                                             const std::string& output_file,
                                             const FFmpegParams& ffmpeg_params,
                                             int analyze_duration,
                                             int probe_size,
                                             bool verbose) {
    std::vector<std::string> cmd;

    // Base command with proper escaping and extended analysis parameters
    cmd.push_back("ffmpeg");
    cmd.push_back("-analyzeduration");
    cmd.push_back(std::to_string(analyze_duration));
    cmd.push_back("-probesize");
    cmd.push_back(std::to_string(probe_size));
    cmd.push_back("-i");
    cmd.push_back(input_file);
    cmd.push_back("-c:v");
    cmd.push_back(ffmpeg_params.vcodec);

    // Add quality parameters
    std::vector<std::string> quality_parts = split_string(ffmpeg_params.quality, ' ');
    cmd.insert(cmd.end(), quality_parts.begin(), quality_parts.end());

    // Add preset
    cmd.push_back("-preset");
    cmd.push_back(ffmpeg_params.preset);

    // Add framerate if specified
    if (ffmpeg_params.framerate != "auto" && !ffmpeg_params.framerate.empty()) {
        cmd.push_back("-r");
        cmd.push_back(ffmpeg_params.framerate);
    }

    // Add resolution
    cmd.push_back("-s");
    cmd.push_back(ffmpeg_params.resolution);

    // Add audio settings
    std::vector<std::string> audio_parts = split_string(ffmpeg_params.acodec, ' ');
    cmd.insert(cmd.end(), audio_parts.begin(), audio_parts.end());

    if (!ffmpeg_params.audio_channels.empty()) {
        std::vector<std::string> audio_channel_parts = split_string(ffmpeg_params.audio_channels, ' ');
        cmd.insert(cmd.end(), audio_channel_parts.begin(), audio_channel_parts.end());
    }

    // Add profile if specified
    if (ffmpeg_params.profile != "auto" && !ffmpeg_params.profile.empty()) {
        cmd.push_back("-profile:v");
        cmd.push_back(ffmpeg_params.profile);
    }

    // Add verbosity level
    if (!verbose) {
        cmd.push_back("-v");
        cmd.push_back("error");
        cmd.push_back("-stats");
    }

    // Always copy all streams from input
    cmd.push_back("-map");
    cmd.push_back("0");

    // Add output file
    cmd.push_back(output_file);

    return cmd;
}

std::vector<std::vector<std::string>> build_multipass_commands(const std::string& input_file,
                                                              const std::string& output_file,
                                                              const FFmpegParams& ffmpeg_params,
                                                              int analyze_duration,
                                                              int probe_size,
                                                              bool verbose) {
    std::vector<std::vector<std::string>> commands;

    // Get appropriate null device
    std::string null_device = get_null_device();

    // First pass command
    std::vector<std::string> pass1_cmd = build_ffmpeg_command(input_file, null_device, ffmpeg_params,
                                                           analyze_duration, probe_size, verbose);

    // Remove the output file (last element)
    pass1_cmd.pop_back();

    // Add pass and format parameters
    pass1_cmd.push_back("-pass");
    pass1_cmd.push_back("1");
    pass1_cmd.push_back("-f");
    pass1_cmd.push_back("null");
    pass1_cmd.push_back(null_device);

    // Second pass command
    std::vector<std::string> pass2_cmd = build_ffmpeg_command(input_file, output_file, ffmpeg_params,
                                                           analyze_duration, probe_size, verbose);
    pass2_cmd.push_back("-pass");
    pass2_cmd.push_back("2");

    commands.push_back(pass1_cmd);
    commands.push_back(pass2_cmd);

    return commands;
}

int execute_command(const std::vector<std::string>& cmd, bool verbose) {
    std::string command = join_string(cmd, " ");

    if (verbose) {
        std::cout << "Executing: " << command << std::endl;
    }

    return system(command.c_str());
}

bool rename_to_m4v(const std::string& file_path, bool dry_run) {
    fs::path path(file_path);
    fs::path m4v_path = path.parent_path() / (path.stem().string() + ".m4v");

    if (dry_run) {
        std::cout << "[DRY RUN] Would rename " << file_path << " to " << m4v_path << std::endl;
        return true;
    }

    std::cout << "Renaming " << file_path << " to " << m4v_path << std::endl;
    if (fs::exists(path)) {
        try {
            fs::rename(path, m4v_path);
            return true;
        } catch (const fs::filesystem_error& e) {
            std::cout << "Error renaming file to .m4v: " << e.what() << std::endl;
            return false;
        }
    } else {
        std::cout << "Error: File " << file_path << " not found for renaming" << std::endl;
        return false;
    }
}

void get_file_info(const std::string& file_path) {
    std::cout << "File information for " << file_path << ":" << std::endl;
    std::string cmd = "ffprobe -hide_banner -v error -show_format -show_streams \"" + file_path + "\"";
    system(cmd.c_str());
}

std::vector<std::string> find_media_files(const std::string& directory,
                                         bool recursive,
                                         const std::vector<std::string>& extensions) {
    std::vector<std::string> result;

    try {
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(directory)) {
                if (fs::is_regular_file(entry)) {
                    std::string extension = entry.path().extension().string();
                    if (!extension.empty()) {
                        // Remove the dot from extension
                        extension = extension.substr(1);
                        // Convert to lowercase for comparison
                        std::transform(extension.begin(), extension.end(), extension.begin(),
                                     [](unsigned char c){ return std::tolower(c); });

                        if (std::find(extensions.begin(), extensions.end(), extension) != extensions.end()) {
                            result.push_back(entry.path().string());
                        }
                    }
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(directory)) {
                if (fs::is_regular_file(entry)) {
                    std::string extension = entry.path().extension().string();
                    if (!extension.empty()) {
                        // Remove the dot from extension
                        extension = extension.substr(1);
                        // Convert to lowercase for comparison
                        std::transform(extension.begin(), extension.end(), extension.begin(),
                                     [](unsigned char c){ return std::tolower(c); });

                        if (std::find(extensions.begin(), extensions.end(), extension) != extensions.end()) {
                            result.push_back(entry.path().string());
                        }
                    }
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error accessing directory: " << e.what() << std::endl;
    }

    return result;
}

int process_file(const std::string& input_file,
                const std::string& media_dir,
                const std::string& output_dir,
                const FFmpegParams& ffmpeg_params,
                const std::string& original_format,
                const std::string& output_format,
                bool force_m4v,
                bool execute,
                bool dry_run,
                bool replace_underscores,
                int analyze_duration,
                int probe_size,
                bool verbose) {
    // Calculate relative path to preserve directory structure
    fs::path input_path(input_file);
    fs::path media_path(media_dir);
    fs::path rel_path = fs::relative(input_path, media_path);

    // Ensure output subdirectory path is properly constructed
    fs::path dir_part = rel_path.parent_path();
    fs::path output_subdir = output_dir;

    if (!dir_part.empty()) {
        output_subdir = fs::path(output_dir) / dir_part;
    }

    std::string filename = input_path.filename().string();
    std::string basename = input_path.stem().string();

    // Format basename (replace underscores with spaces if enabled)
    std::string formatted_basename = format_filename(basename, replace_underscores);

    // Determine the correct output format for initial conversion
    std::string actual_format = output_format;
    if (force_m4v && execute) {
        // When executing with force_m4v, use original format first
        actual_format = original_format;
    }

    fs::path output_file = output_subdir / (formatted_basename + "." + actual_format);

    // Create output subdirectory if needed
    if (!fs::exists(output_subdir)) {
        if (!dry_run) {
            std::cout << "Creating output directory: " << output_subdir << std::endl;
            try {
                fs::create_directories(output_subdir);
            } catch (const fs::filesystem_error& e) {
                std::cout << "Error creating directory: " << output_subdir << ": " << e.what() << std::endl;
                return 1;
            }
        } else {
            std::cout << "[DRY RUN] Would create directory: " << output_subdir << std::endl;
        }
    }

    // Check if output file location is valid and writable
    if (!dry_run && execute) {
        if (!check_file_access(output_file.string())) {
            std::cout << "Skipping " << input_file << " due to output file access issues." << std::endl;
            return 1;
        }
    }

    // Determine if multipass is needed
    bool is_multipass = ffmpeg_params.multipass && ffmpeg_params.quality.find("-crf") == std::string::npos;

    // Build ffmpeg command(s)
    std::string ffmpeg_cmd_str;

    if (is_multipass) {
        std::vector<std::vector<std::string>> ffmpeg_cmds = build_multipass_commands(
            input_file, output_file.string(), ffmpeg_params, analyze_duration, probe_size, verbose
        );

        std::vector<std::string> cmd_strs;
        for (const auto& cmd : ffmpeg_cmds) {
            std::vector<std::string> escaped_cmd;
            for (const auto& arg : cmd) {
                escaped_cmd.push_back(escape_string(arg));
            }
            cmd_strs.push_back(join_string(escaped_cmd, " "));
        }

        ffmpeg_cmd_str = join_string(cmd_strs, " && ");
    } else {
        std::vector<std::string> ffmpeg_cmd = build_ffmpeg_command(
            input_file, output_file.string(), ffmpeg_params, analyze_duration, probe_size, verbose
        );

        std::vector<std::string> escaped_cmd;
        for (const auto& arg : ffmpeg_cmd) {
            escaped_cmd.push_back(escape_string(arg));
        }

        ffmpeg_cmd_str = join_string(escaped_cmd, " ");
    }

    if (dry_run) {
        std::cout << "[DRY RUN] Would execute:" << std::endl;
        std::cout << ffmpeg_cmd_str << std::endl;
        if (force_m4v) {
            fs::path m4v_output = output_file.parent_path() / (output_file.stem().string() + ".m4v");
            std::cout << "[DRY RUN] Would rename " << output_file << " to " << m4v_output << std::endl;
        }
    } else if (execute) {
        std::cout << "Processing: " << input_file << std::endl;
        std::cout << "Output: " << output_file << std::endl;
        std::cout << "Command: " << ffmpeg_cmd_str << std::endl;

        // Execute ffmpeg command(s)
        int result_code = 0;

        try {
            if (is_multipass) {
                // For multipass, run commands sequentially
                std::vector<std::vector<std::string>> ffmpeg_cmds = build_multipass_commands(
                    input_file, output_file.string(), ffmpeg_params, analyze_duration, probe_size, verbose
                );

                for (size_t i = 0; i < ffmpeg_cmds.size(); ++i) {
                    std::cout << "Running pass " << (i + 1) << " of " << ffmpeg_cmds.size() << "..." << std::endl;
                    result_code = execute_command(ffmpeg_cmds[i], verbose);

                    if (result_code != 0) {
                        break;
                    }
                }
            } else {
            // For single pass
            std::vector<std::string> ffmpeg_cmd = build_ffmpeg_command(
            input_file, output_file.string(), ffmpeg_params, analyze_duration, probe_size, verbose
            );
            result_code = execute_command(ffmpeg_cmd, verbose);
            }
            if (result_code == 0) {
                std::cout << "Conversion successful" << std::endl;

                // If successful and force_m4v is enabled, rename to .m4v
                if (force_m4v) {
                    if (!rename_to_m4v(output_file.string(), dry_run)) {
                        std::cout << "Warning: Failed to rename file to .m4v" << std::endl;
                    }
                }

                return 0;
            } else {
                std::cout << "Error: FFmpeg command failed with return code " << result_code << std::endl;
                std::cout << "Checking input file..." << std::endl;
                get_file_info(input_file);
                return 1;
            }
        } catch (const std::exception& e) {
            std::cout << "Error executing command: " << e.what() << std::endl;
            return 1;
        }
    } else {
        std::cout << "Generated command for " << input_file << ":" << std::endl;
        std::cout << ffmpeg_cmd_str << std::endl;
        if (force_m4v) {
            std::cout << "Note: If executed, the file will be converted to " << actual_format
                      << " then renamed to .m4v" << std::endl;
        }
    }

    return 0;
}

struct CmdOptions {
    std::string json_file;
    bool recursive = false;
    bool execute = false;
    bool dry_run = false;
    bool show_preset = false;
    bool force_m4v = false;
    bool no_underscore_replace = false;
    bool verbose = false;
    std::string input_dir;
    std::string output_dir;
    std::string ignore_flag = ".noconvert";
    std::string log_file;
};

CmdOptions parse_arguments(int argc, char* argv[]) {
    CmdOptions options;

    if (argc < 2) {
        show_usage(argv[0]);
    }

    // First argument is the JSON file
    options.json_file = argv[1];

    // Parse options
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-r" || arg == "--recursive") {
            options.recursive = true;
        } else if (arg == "-e" || arg == "--execute") {
            options.execute = true;
        } else if (arg == "-d" || arg == "--dry-run") {
            options.dry_run = true;
        } else if (arg == "-p" || arg == "--show-preset") {
            options.show_preset = true;
        } else if (arg == "-m" || arg == "--force-m4v") {
            options.force_m4v = true;
        } else if (arg == "-u" || arg == "--no-underscore-replace") {
            options.no_underscore_replace = true;
        } else if (arg == "--verbose") {
            options.verbose = true;
        } else if (arg == "-h" || arg == "--help") {
            show_usage(argv[0]);
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "Script Version=" << SCRIPT_VERSION << std::endl;
            exit(0);
        } else if (arg.substr(0, 14) == "--ignore-flag=") {
            options.ignore_flag = arg.substr(14);
        } else if (arg == "-i" || arg == "--input-dir") {
            if (i + 1 < argc) {
                options.input_dir = argv[++i];
            } else {
                show_usage(argv[0]);
            }
        } else if (arg == "-o" || arg == "--output-dir") {
            if (i + 1 < argc) {
                options.output_dir = argv[++i];
            } else {
                show_usage(argv[0]);
            }
        } else if (arg == "-l" || arg == "--log") {
            if (i + 1 < argc) {
                options.log_file = argv[++i];
            } else {
                show_usage(argv[0]);
            }
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            show_usage(argv[0]);
        }
    }

    return options;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    CmdOptions args = parse_arguments(argc, argv);

    // Check if required tools are installed
    if (system("ffmpeg -version > /dev/null 2>&1") != 0) {
        std::cerr << "Error: ffmpeg is required but not installed. Please install ffmpeg." << std::endl;
        return 1;
    }

    if (system("ffprobe -version > /dev/null 2>&1") != 0) {
        std::cerr << "Error: ffprobe is required but not installed. Please install ffprobe." << std::endl;
        return 1;
    }

    // Set up logging if requested
    std::ofstream log_file;
    std::streambuf* cout_buffer = nullptr;

    if (!args.log_file.empty()) {
        log_file.open(args.log_file);
        if (!log_file.is_open()) {
            std::cerr << "Error: Could not open log file: " << args.log_file << std::endl;
        } else {
            cout_buffer = std::cout.rdbuf();
            std::cout.rdbuf(log_file.rdbuf());
        }
    }

    // Load and parse JSON preset
    json preset_data = load_json_preset(args.json_file);

    // Extract settings from the preset
    Settings settings = extract_preset_settings(preset_data);

    // Convert to FFmpeg parameters
    FFmpegParams ffmpeg_params = convert_to_ffmpeg_params(settings);

    // Default FFmpeg extended settings
    int analyze_duration = 100000000;  // 100MB
    int probe_size = 100000000;        // 100MB

    // Determine output format
    std::string output_format = ffmpeg_params.format;
    std::string original_format = output_format;  // Store original before potentially overriding

    // Override output format if force m4v is enabled
    if (args.force_m4v && !args.execute) {
        output_format = "m4v";
        std::cout << "Forcing output extension to .m4v" << std::endl;
    } else if (args.force_m4v) {
        std::cout << "Force m4v is enabled. Files will be converted to " << original_format
                  << " first, then renamed to .m4v" << std::endl;
    }

    // Show preset only if requested
    if (args.show_preset) {
        show_preset(ffmpeg_params, output_format, analyze_duration, probe_size);
        return 0;
    }

    // Set media directory if not specified
    if (args.input_dir.empty()) {
        args.input_dir = fs::path(args.json_file).parent_path().string();
        std::cout << "Using media directory: " << args.input_dir << std::endl;
    }

    // Set output directory if not specified
    if (args.output_dir.empty()) {
        args.output_dir = fs::path(args.input_dir) / "converted";
        std::cout << "Using output directory: " << args.output_dir << std::endl;
    }

    // Make sure output directory doesn't contain duplicate "converted" subdirectories
    std::string converted_path = fs::path("converted").string();
    std::string double_converted = fs::path("converted") / "converted";

    if (args.output_dir.find(double_converted) != std::string::npos) {
        // Replace double converted path with single converted path
        size_t pos = args.output_dir.find(double_converted);
        args.output_dir.replace(pos, double_converted.length(), converted_path);
    }

    // Create output directory if it doesn't exist and not in dry run mode
    if (!fs::exists(args.output_dir) && !args.dry_run) {
        std::cout << "Creating output directory: " << args.output_dir << std::endl;
        try {
            fs::create_directories(args.output_dir);
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error creating output directory: " << e.what() << std::endl;
            return 1;
        }
    }

    // Find and process media files
    std::cout << "Searching for media files in " << args.input_dir << std::endl;
    std::cout << "Files with the '" << args.ignore_flag << "' file in their directory will be skipped" << std::endl;
    std::cout << "Output directory set to: " << args.output_dir << std::endl;
    std::cout << "Using analyzeduration: " << analyze_duration << ", probesize: " << probe_size << std::endl;

    if (!args.no_underscore_replace) {
        std::cout << "Underscores in filenames will be replaced with spaces in output files" << std::endl;
    } else {
        std::cout << "Output filenames will maintain the same format as input filenames" << std::endl;
    }

    if (args.recursive) {
        std::cout << "Recursive search enabled" << std::endl;
    } else {
        std::cout << "Non-recursive search" << std::endl;
    }

    if (args.verbose) {
        std::cout << "Verbose output enabled" << std::endl;
    }

    // Find media files
    std::vector<std::string> media_files = find_media_files(args.input_dir, args.recursive, MEDIA_EXTENSIONS);

    // Process each file
    int file_count = 0;
    int skipped_count = 0;
    int error_count = 0;

    for (const auto& file : media_files) {
        // Skip JSON file itself
        if (fs::equivalent(fs::path(file), fs::path(args.json_file))) {
            continue;
        }

        // Check if file should be ignored
        if (should_ignore_file(file, args.ignore_flag)) {
            std::cout << "Skipping: " << file << " (ignore flag found)" << std::endl;
            skipped_count++;
            continue;
        }

        // Process the file
        int result = process_file(
            file,
            args.input_dir,
            args.output_dir,
            ffmpeg_params,
            original_format,
            output_format,
            args.force_m4v,
            args.execute,
            args.dry_run,
            !args.no_underscore_replace,
            analyze_duration,
            probe_size,
            args.verbose
        );

        if (result == 0) {
            file_count++;
        } else {
            error_count++;
            std::cout << "Failed to process: " << file << std::endl;
        }
    }

    // Display summary
    std::cout << "Processing complete:" << std::endl;
    std::cout << "  - Successfully processed: " << file_count << " files" << std::endl;
    std::cout << "  - Skipped: " << skipped_count << " files" << std::endl;
    std::cout << "  - Failed: " << error_count << " files" << std::endl;

    if (file_count == 0 && skipped_count == 0 && error_count == 0) {
        std::cout << "No media files found in the specified directory." << std::endl;
    }

    // Restore cout buffer if logging was enabled
    if (cout_buffer != nullptr) {
        std::cout.rdbuf(cout_buffer);
        log_file.close();
    }

    // Return non-zero status if errors occurred
    return (error_count > 0) ? 1 : 0;
}
