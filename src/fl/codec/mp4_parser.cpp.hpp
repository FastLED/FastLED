#include "fl/codec/mp4_parser.h"

namespace fl {

namespace {

// Read a big-endian uint32 from a byte span at offset.
// Returns 0 and sets ok=false if out of bounds.
inline fl::u32 readU32BE(fl::span<const fl::u8> data, fl::size offset, bool& ok) {
    if (offset + 4 > data.size()) { ok = false; return 0; }
    return (fl::u32(data[offset]) << 24) |
           (fl::u32(data[offset + 1]) << 16) |
           (fl::u32(data[offset + 2]) << 8) |
            fl::u32(data[offset + 3]);
}

inline fl::u16 readU16BE(fl::span<const fl::u8> data, fl::size offset, bool& ok) {
    if (offset + 2 > data.size()) { ok = false; return 0; }
    return (fl::u16(data[offset]) << 8) | fl::u16(data[offset + 1]);
}

// 4-char box type comparison
inline bool boxIs(fl::span<const fl::u8> data, fl::size offset, const char* type) {
    if (offset + 4 > data.size()) return false;
    return data[offset] == fl::u8(type[0]) &&
           data[offset + 1] == fl::u8(type[1]) &&
           data[offset + 2] == fl::u8(type[2]) &&
           data[offset + 3] == fl::u8(type[3]);
}

// Find a top-level box by type within a range. Returns offset of box start, or ~0 if not found.
fl::size findBox(fl::span<const fl::u8> data, fl::size start, fl::size end, const char* type) {
    fl::size pos = start;
    while (pos + 8 <= end) {
        bool ok = true;
        fl::u32 boxSize = readU32BE(data, pos, ok);
        if (!ok || boxSize < 8) break;
        if (boxIs(data, pos + 4, type)) return pos;
        pos += boxSize;
    }
    return fl::size(-1); // not found
}

// Parse avcC (AVC Decoder Configuration Record) at given offset.
// ISO 14496-15 section 5.2.4.1
bool parseAvcC(fl::span<const fl::u8> data, fl::size offset, fl::size boxEnd,
               Mp4TrackInfo& info) {
    // avcC box: 8 bytes header, then:
    //   u8 configVersion (1)
    //   u8 profile_idc
    //   u8 profile_compat
    //   u8 level_idc
    //   u8 lengthSizeMinusOne (& 0x03)
    //   u8 numSPS (& 0x1F)
    //   for each SPS: u16 spsLength, u8[] spsData
    //   u8 numPPS
    //   for each PPS: u16 ppsLength, u8[] ppsData

    fl::size pos = offset + 8; // skip box header
    if (pos + 6 > boxEnd) return false;

    fl::u8 configVersion = data[pos++];
    if (configVersion != 1) return false;

    info.profile = data[pos++];
    pos++; // profile_compat
    info.level = data[pos++];
    info.lengthSizeMinusOne = data[pos++] & 0x03;

    fl::u8 numSPS = data[pos++] & 0x1F;
    for (fl::u8 i = 0; i < numSPS; i++) {
        bool ok = true;
        fl::u16 spsLen = readU16BE(data, pos, ok);
        if (!ok) return false;
        pos += 2;
        if (pos + spsLen > boxEnd) return false;
        fl::vector<fl::u8> sps(spsLen);
        for (fl::u16 j = 0; j < spsLen; j++) {
            sps[j] = data[pos + j];
        }
        info.sps.push_back(sps);
        pos += spsLen;
    }

    if (pos >= boxEnd) return false;
    fl::u8 numPPS = data[pos++];
    for (fl::u8 i = 0; i < numPPS; i++) {
        bool ok = true;
        fl::u16 ppsLen = readU16BE(data, pos, ok);
        if (!ok) return false;
        pos += 2;
        if (pos + ppsLen > boxEnd) return false;
        fl::vector<fl::u8> pps(ppsLen);
        for (fl::u16 j = 0; j < ppsLen; j++) {
            pps[j] = data[pos + j];
        }
        info.pps.push_back(pps);
        pos += ppsLen;
    }

    return true;
}

} // namespace

Mp4TrackInfo parseMp4(fl::span<const fl::u8> data, fl::string* error) {
    Mp4TrackInfo info;

    if (data.size() < 8) {
        if (error) *error = "Data too small for MP4";
        return info;
    }

    // Find moov box at top level
    fl::size moovPos = findBox(data, 0, data.size(), "moov");
    if (moovPos == fl::size(-1)) {
        if (error) *error = "No moov box found";
        return info;
    }

    bool ok = true;
    fl::u32 moovSize = readU32BE(data, moovPos, ok);
    if (!ok) {
        if (error) *error = "Invalid moov box size";
        return info;
    }
    fl::size moovEnd = moovPos + moovSize;
    if (moovEnd > data.size()) moovEnd = data.size();
    fl::size moovBody = moovPos + 8;

    // Find trak box inside moov
    fl::size trakPos = findBox(data, moovBody, moovEnd, "trak");
    if (trakPos == fl::size(-1)) {
        if (error) *error = "No trak box found";
        return info;
    }

    fl::u32 trakSize = readU32BE(data, trakPos, ok);
    if (!ok) {
        if (error) *error = "Invalid trak box size";
        return info;
    }
    fl::size trakEnd = trakPos + trakSize;
    if (trakEnd > moovEnd) trakEnd = moovEnd;
    fl::size trakBody = trakPos + 8;

    // Find mdia box inside trak
    fl::size mdiaPos = findBox(data, trakBody, trakEnd, "mdia");
    if (mdiaPos == fl::size(-1)) {
        if (error) *error = "No mdia box found";
        return info;
    }

    fl::u32 mdiaSize = readU32BE(data, mdiaPos, ok);
    if (!ok) {
        if (error) *error = "Invalid mdia box size";
        return info;
    }
    fl::size mdiaEnd = mdiaPos + mdiaSize;
    if (mdiaEnd > trakEnd) mdiaEnd = trakEnd;
    fl::size mdiaBody = mdiaPos + 8;

    // Find minf box inside mdia
    fl::size minfPos = findBox(data, mdiaBody, mdiaEnd, "minf");
    if (minfPos == fl::size(-1)) {
        if (error) *error = "No minf box found";
        return info;
    }

    fl::u32 minfSize = readU32BE(data, minfPos, ok);
    if (!ok) {
        if (error) *error = "Invalid minf box size";
        return info;
    }
    fl::size minfEnd = minfPos + minfSize;
    if (minfEnd > mdiaEnd) minfEnd = mdiaEnd;
    fl::size minfBody = minfPos + 8;

    // Find stbl box inside minf
    fl::size stblPos = findBox(data, minfBody, minfEnd, "stbl");
    if (stblPos == fl::size(-1)) {
        if (error) *error = "No stbl box found";
        return info;
    }

    fl::u32 stblSize = readU32BE(data, stblPos, ok);
    if (!ok) {
        if (error) *error = "Invalid stbl box size";
        return info;
    }
    fl::size stblEnd = stblPos + stblSize;
    if (stblEnd > minfEnd) stblEnd = minfEnd;
    fl::size stblBody = stblPos + 8;

    // Find stsd box inside stbl
    fl::size stsdPos = findBox(data, stblBody, stblEnd, "stsd");
    if (stsdPos == fl::size(-1)) {
        if (error) *error = "No stsd box found";
        return info;
    }

    fl::u32 stsdSize = readU32BE(data, stsdPos, ok);
    if (!ok) {
        if (error) *error = "Invalid stsd box size";
        return info;
    }
    fl::size stsdEnd = stsdPos + stsdSize;
    if (stsdEnd > stblEnd) stsdEnd = stblEnd;

    // stsd is a full box: 8 byte header + 4 byte version/flags + 4 byte entry_count
    fl::size stsdBody = stsdPos + 8 + 4 + 4;
    if (stsdBody > stsdEnd) {
        if (error) *error = "stsd box too small";
        return info;
    }

    // Look for avc1 sample entry inside stsd
    fl::size avc1Pos = findBox(data, stsdBody, stsdEnd, "avc1");
    if (avc1Pos == fl::size(-1)) {
        if (error) *error = "No avc1 sample entry found (not H.264)";
        return info;
    }

    fl::u32 avc1Size = readU32BE(data, avc1Pos, ok);
    if (!ok) {
        if (error) *error = "Invalid avc1 box size";
        return info;
    }
    fl::size avc1End = avc1Pos + avc1Size;
    if (avc1End > stsdEnd) avc1End = stsdEnd;

    // avc1 sample entry layout:
    //   8 bytes: box header (size + "avc1")
    //   6 bytes: reserved
    //   2 bytes: data_reference_index
    //   2 bytes: pre_defined
    //   2 bytes: reserved
    //   12 bytes: pre_defined[3]
    //   2 bytes: width
    //   2 bytes: height
    //   ... then sub-boxes including avcC
    fl::size avc1Body = avc1Pos + 8 + 6 + 2 + 2 + 2 + 12;
    if (avc1Body + 4 > avc1End) {
        if (error) *error = "avc1 box too small for dimensions";
        return info;
    }

    info.width = readU16BE(data, avc1Body, ok);
    info.height = readU16BE(data, avc1Body + 2, ok);
    if (!ok) {
        if (error) *error = "Failed to read dimensions from avc1";
        return info;
    }

    // Skip to sub-boxes: avc1Body + 4 (width/height) + 4 (horiz res) + 4 (vert res) +
    //   4 (reserved) + 2 (frame_count) + 32 (compressor_name) + 2 (depth) + 2 (pre_defined)
    fl::size subBoxStart = avc1Body + 4 + 4 + 4 + 4 + 2 + 32 + 2 + 2;

    // Find avcC box inside avc1
    fl::size avcCPos = findBox(data, subBoxStart, avc1End, "avcC");
    if (avcCPos == fl::size(-1)) {
        if (error) *error = "No avcC box found inside avc1";
        return info;
    }

    fl::u32 avcCSize = readU32BE(data, avcCPos, ok);
    if (!ok) {
        if (error) *error = "Invalid avcC box size";
        return info;
    }
    fl::size avcCEnd = avcCPos + avcCSize;
    if (avcCEnd > avc1End) avcCEnd = avc1End;

    if (!parseAvcC(data, avcCPos, avcCEnd, info)) {
        if (error) *error = "Failed to parse avcC configuration record";
        return info;
    }

    info.isValid = true;
    return info;
}

fl::vector<fl::u8> extractH264NalUnits(fl::span<const fl::u8> data,
                                        const Mp4TrackInfo& track,
                                        fl::string* error) {
    fl::vector<fl::u8> annexB;

    if (!track.isValid) {
        if (error) *error = "Invalid track info";
        return annexB;
    }

    fl::u8 nalLenSize = track.lengthSizeMinusOne + 1; // typically 4

    // Emit SPS NAL units with Annex B start codes
    for (const auto& sps : track.sps) {
        annexB.push_back(0x00);
        annexB.push_back(0x00);
        annexB.push_back(0x00);
        annexB.push_back(0x01);
        for (fl::size j = 0; j < sps.size(); j++) {
            annexB.push_back(sps[j]);
        }
    }

    // Emit PPS NAL units
    for (const auto& pps : track.pps) {
        annexB.push_back(0x00);
        annexB.push_back(0x00);
        annexB.push_back(0x00);
        annexB.push_back(0x01);
        for (fl::size j = 0; j < pps.size(); j++) {
            annexB.push_back(pps[j]);
        }
    }

    // Find mdat box and convert AVCC NAL units to Annex B
    fl::size mdatPos = fl::size(-1);
    {
        fl::size pos = 0;
        while (pos + 8 <= data.size()) {
            bool ok = true;
            fl::u32 boxSize = readU32BE(data, pos, ok);
            if (!ok || boxSize < 8) break;
            if (boxIs(data, pos + 4, "mdat")) {
                mdatPos = pos;
                break;
            }
            pos += boxSize;
        }
    }

    if (mdatPos == fl::size(-1)) {
        if (error) *error = "No mdat box found";
        return annexB;
    }

    bool ok = true;
    fl::u32 mdatSize = readU32BE(data, mdatPos, ok);
    if (!ok) {
        if (error) *error = "Invalid mdat box size";
        return annexB;
    }

    fl::size mdatBody = mdatPos + 8;
    fl::size mdatEnd = mdatPos + mdatSize;
    if (mdatEnd > data.size()) mdatEnd = data.size();

    fl::size pos = mdatBody;
    while (pos + nalLenSize <= mdatEnd) {
        fl::u32 nalLen = 0;
        for (fl::u8 k = 0; k < nalLenSize; k++) {
            nalLen = (nalLen << 8) | data[pos + k];
        }
        pos += nalLenSize;

        if (pos + nalLen > mdatEnd) break;

        // Emit Annex B start code + NAL data
        annexB.push_back(0x00);
        annexB.push_back(0x00);
        annexB.push_back(0x00);
        annexB.push_back(0x01);
        for (fl::u32 j = 0; j < nalLen; j++) {
            annexB.push_back(data[pos + j]);
        }
        pos += nalLen;
    }

    return annexB;
}

} // namespace fl
