#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/span.h"

namespace fl {

// Minimal ISO BMFF / MP4 container parser for H.264 video.
// Navigates ftyp→moov→trak→mdia→minf→stbl→stsd→avc1→avcC
// to extract SPS/PPS NAL units and video dimensions.

struct Mp4TrackInfo {
    fl::u16 width = 0;
    fl::u16 height = 0;
    fl::u8 profile = 0;       // H.264 profile_idc from avcC
    fl::u8 level = 0;         // H.264 level_idc from avcC
    fl::u8 lengthSizeMinusOne = 0; // NALU length field size minus 1 (typically 3 → 4 bytes)
    fl::vector<fl::vector<fl::u8>> sps; // Sequence Parameter Sets
    fl::vector<fl::vector<fl::u8>> pps; // Picture Parameter Sets
    bool isValid = false;
};

// Parse an MP4 container and extract H.264 track info.
// Only handles single-track H.264 video (avc1 codec).
Mp4TrackInfo parseMp4(fl::span<const fl::u8> data, fl::string* error = nullptr);

// Extract H.264 NAL units from MP4 mdat section, converting AVCC→Annex B format.
// Prepends 0x00000001 start codes to each NAL unit.
fl::vector<fl::u8> extractH264NalUnits(fl::span<const fl::u8> data,
                                        const Mp4TrackInfo& track,
                                        fl::string* error = nullptr);

} // namespace fl
