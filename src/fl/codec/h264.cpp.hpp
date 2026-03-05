#include "fl/codec/h264.h"
#include "fl/codec/mp4_parser.h"
#include "fl/stl/utility.h"

namespace fl {

namespace {

// Exp-Golomb bitstream reader for H.264 SPS parsing.
struct BitReader {
    fl::span<const fl::u8> data;
    fl::size bytePos = 0;
    fl::u8 bitPos = 0; // 0-7, MSB first

    BitReader(fl::span<const fl::u8> d) : data(d) {}

    bool hasBits(fl::size n) const {
        fl::size totalBits = data.size() * 8;
        fl::size currentBit = bytePos * 8 + bitPos;
        return currentBit + n <= totalBits;
    }

    fl::u32 readBits(fl::u8 n) {
        fl::u32 val = 0;
        for (fl::u8 i = 0; i < n; i++) {
            if (bytePos >= data.size()) return val;
            val = (val << 1) | ((data[bytePos] >> (7 - bitPos)) & 1);
            bitPos++;
            if (bitPos == 8) {
                bitPos = 0;
                bytePos++;
            }
        }
        return val;
    }

    // Read unsigned Exp-Golomb coded value (ue(v))
    fl::u32 readUE() {
        int leadingZeros = 0;
        while (hasBits(1) && readBits(1) == 0) {
            leadingZeros++;
            if (leadingZeros > 32) return 0; // prevent infinite loop
        }
        if (leadingZeros == 0) return 0;
        fl::u32 suffix = readBits(leadingZeros);
        return (1u << leadingZeros) - 1 + suffix;
    }

    // Read signed Exp-Golomb coded value (se(v))
    fl::i32 readSE() {
        fl::u32 val = readUE();
        if (val & 1) {
            return fl::i32((val + 1) / 2);
        } else {
            return -fl::i32(val / 2);
        }
    }

    void skipBits(fl::size n) {
        for (fl::size i = 0; i < n; i++) {
            bitPos++;
            if (bitPos == 8) {
                bitPos = 0;
                bytePos++;
            }
        }
    }
};

// Parse scaling list (skip it — we only need dimensions).
void skipScalingList(BitReader& br, int size) {
    int lastScale = 8;
    int nextScale = 8;
    for (int j = 0; j < size; j++) {
        if (nextScale != 0) {
            fl::i32 delta = br.readSE();
            nextScale = (lastScale + delta + 256) % 256;
        }
        lastScale = (nextScale == 0) ? lastScale : nextScale;
    }
}

} // namespace

H264Info H264::parseSPS(fl::span<const fl::u8> spsData, fl::string* error_message) {
    H264Info info;

    if (spsData.size() < 4) {
        if (error_message) *error_message = "SPS data too small";
        return info;
    }

    BitReader br(spsData);

    // NAL unit header: forbidden_zero_bit(1) + nal_ref_idc(2) + nal_unit_type(5)
    fl::u8 nalHeader = (fl::u8)br.readBits(8);
    fl::u8 nalType = nalHeader & 0x1F;
    FL_UNUSED(nalType); // should be 7 for SPS

    // SPS header
    fl::u8 profile_idc = (fl::u8)br.readBits(8);
    br.readBits(8); // constraint_set flags + reserved
    fl::u8 level_idc = (fl::u8)br.readBits(8);
    br.readUE(); // seq_parameter_set_id

    info.profile = profile_idc;
    info.level = level_idc;

    // High profile extensions
    fl::u32 chromaFormatIdc = 1; // default
    if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 ||
        profile_idc == 244 || profile_idc == 44  || profile_idc == 83  ||
        profile_idc == 86  || profile_idc == 118 || profile_idc == 128 ||
        profile_idc == 138 || profile_idc == 139 || profile_idc == 134 ||
        profile_idc == 135) {
        chromaFormatIdc = br.readUE();
        if (chromaFormatIdc == 3) {
            br.readBits(1); // separate_colour_plane_flag
        }
        br.readUE(); // bit_depth_luma_minus8
        br.readUE(); // bit_depth_chroma_minus8
        br.readBits(1); // qpprime_y_zero_transform_bypass_flag

        fl::u32 seqScalingMatrixPresent = br.readBits(1);
        if (seqScalingMatrixPresent) {
            int limit = (chromaFormatIdc != 3) ? 8 : 12;
            for (int i = 0; i < limit; i++) {
                if (br.readBits(1)) { // seq_scaling_list_present_flag
                    skipScalingList(br, (i < 6) ? 16 : 64);
                }
            }
        }
    }

    br.readUE(); // log2_max_frame_num_minus4
    fl::u32 picOrderCntType = br.readUE();
    if (picOrderCntType == 0) {
        br.readUE(); // log2_max_pic_order_cnt_lsb_minus4
    } else if (picOrderCntType == 1) {
        br.readBits(1); // delta_pic_order_always_zero_flag
        br.readSE(); // offset_for_non_ref_pic
        br.readSE(); // offset_for_top_to_bottom_field
        fl::u32 numRefFramesInPoc = br.readUE();
        for (fl::u32 i = 0; i < numRefFramesInPoc; i++) {
            br.readSE(); // offset_for_ref_frame
        }
    }

    info.numRefFrames = (fl::u8)br.readUE(); // max_num_ref_frames
    br.readBits(1); // gaps_in_frame_num_value_allowed_flag

    fl::u32 picWidthInMbs = br.readUE() + 1;
    fl::u32 picHeightInMapUnits = br.readUE() + 1;

    fl::u32 frameMbsOnly = br.readBits(1);
    if (!frameMbsOnly) {
        br.readBits(1); // mb_adaptive_frame_field_flag
    }

    br.readBits(1); // direct_8x8_inference_flag

    // Frame cropping
    fl::u32 cropLeft = 0, cropRight = 0, cropTop = 0, cropBottom = 0;
    fl::u32 frameCropping = br.readBits(1);
    if (frameCropping) {
        cropLeft = br.readUE();
        cropRight = br.readUE();
        cropTop = br.readUE();
        cropBottom = br.readUE();
    }

    // Calculate dimensions
    fl::u32 width = picWidthInMbs * 16;
    fl::u32 height = (2 - frameMbsOnly) * picHeightInMapUnits * 16;

    // Apply cropping
    fl::u32 cropUnitX = 1, cropUnitY = 2 - frameMbsOnly;
    if (chromaFormatIdc == 1) {
        cropUnitX = 2;
        cropUnitY *= 2;
    } else if (chromaFormatIdc == 2) {
        cropUnitX = 2;
        cropUnitY *= 1;
    }

    width -= (cropLeft + cropRight) * cropUnitX;
    height -= (cropTop + cropBottom) * cropUnitY;

    info.width = (fl::u16)width;
    info.height = (fl::u16)height;
    info.isValid = true;

    return info;
}

H264Info H264::parseH264Info(fl::span<const fl::u8> mp4Data, fl::string* error_message) {
    H264Info info;

    Mp4TrackInfo track = parseMp4(mp4Data, error_message);
    if (!track.isValid) return info;

    // Use dimensions from avc1 box
    info.width = track.width;
    info.height = track.height;
    info.profile = track.profile;
    info.level = track.level;

    // Parse SPS for more detailed info if available
    if (!track.sps.empty()) {
        fl::string spsError;
        H264Info spsInfo = parseSPS(track.sps[0], &spsError);
        if (spsInfo.isValid) {
            // SPS-parsed dimensions are more authoritative
            info.width = spsInfo.width;
            info.height = spsInfo.height;
            info.profile = spsInfo.profile;
            info.level = spsInfo.level;
            info.numRefFrames = spsInfo.numRefFrames;
        }
    }

    info.isValid = true;
    return info;
}

} // namespace fl

// allow-include-after-namespace
// Platform-specific factory: createDecoder() and isSupported()
// IWYU pragma: begin_keep
#include "platforms/esp/is_esp.h" // ok platform headers
#if defined(FL_IS_ESP_32P4)
#include "platforms/esp/32/codec/h264_hw_decoder.hpp" // ok platform headers
#else
#include "platforms/shared/codec/h264_noop.hpp" // ok platform headers
#endif
// IWYU pragma: end_keep
