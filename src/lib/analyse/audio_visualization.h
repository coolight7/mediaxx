#pragma once

class AudioVisualization_c {
public:

    static AudioVisualization_c instance;

    AudioVisualization_c() {}

    ~AudioVisualization_c() {}

    bool analyse(const char* input_filename, const char* output_filename);
};
