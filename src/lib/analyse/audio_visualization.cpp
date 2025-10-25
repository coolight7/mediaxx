extern "C" {
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libswresample/swresample.h"
}

#include "audio_visualization.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdint.h>
#include <util/ffmpeg_ext.h>
#include <vector>

AudioVisualization_c AudioVisualization_c::instance = AudioVisualization_c();

bool AudioVisualization_c::analyse(
    const char* input_filename,
    const char* output_filename
) {
    return true;
}
