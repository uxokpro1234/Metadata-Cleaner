//
// Created by user2378 on 10.10.2026.
//

#include "jpeg.hpp"

#include <iomanip>
#include <sstream>

using namespace std;
namespace {

string markerName(uint8_t marker) {
    if (marker >= 0xE0 && marker <= 0xEF) {
        const int appNumber = marker - 0xE0;
        ostringstream out;
        out << "APP" << appNumber;

        switch (marker) {
            case 0xE1: return "APP1 - EXIF/XMP metadata";
            case 0xE3: return "APP3 - application metadata";
            case 0xE4: return "APP4 - application metadata";
            case 0xE5: return "APP5 - application metadata";
            case 0xE6: return "APP6 - application metadata";
            case 0xE7: return "APP7 - application metadata";
            case 0xE8: return "APP8 - application metadata";
            case 0xE9: return "APP9 - application metadata";
            case 0xEA: return "APP10 - application metadata";
            case 0xEB: return "APP11 - application metadata";
            case 0xEC: return "APP12 - application metadata";
            case 0xED: return "APP13 - Photoshop/IPTC metadata";
            case 0xEE: return "APP14 - Adobe data (preserved)";
            case 0xEF: return "APP15 - application metadata";
            default: break;
        }

        return out.str();
    }

    if (marker == 0xFE) return "COM - JPEG comment";
    return "Unknown JPEG marker";
}

bool isStandalone(uint8_t marker) {
    return marker == 0x01 ||
           marker == 0xD8 ||
           marker == 0xD9 ||
           (marker >= 0xD0 && marker <= 0xD7);
}

bool shouldRemove(uint8_t marker) {
    // Remove COM and APP1, APP3 through APP13, and APP15.
    // Keep APP0 (JFIF), APP2 (ICC/MPF), and APP14 (Adobe).
    if (marker == 0xFE) return true;

    if (marker >= 0xE0 && marker <= 0xEF) {
        return marker != 0xE0 &&
               marker != 0xE2 &&
               marker != 0xEE;
    }

    return false;
}

} // namespace

CleanResult cleanJpeg(const Bytes& input) {
    CleanResult result;

    if (input.size() < 4 ||
        input[0] != 0xFF ||
        input[1] != 0xD8) {
        result.error = "Not a valid JPEG: missing SOI marker.";
        return result;
    }

    result.data.push_back(0xFF);
    result.data.push_back(0xD8);

    size_t pos = 2;
    bool sawEoi = false;

    while (pos < input.size()) {
        if (input[pos] != 0xFF) {
            result.error = "Invalid JPEG marker structure.";
            return result;
        }

        const size_t markerStart = pos;

        while (pos < input.size() && input[pos] == 0xFF) {
            ++pos;
        }

        if (pos >= input.size()) {
            result.error = "Unexpected end of file after marker prefix.";
            return result;
        }

        const uint8_t marker = input[pos++];

        // Stuffed bytes are only valid inside entropy-coded scan data.
        if (marker == 0x00) {
            result.error = "Unexpected stuffed byte outside image scan.";
            return result;
        }

        if (marker == 0xD9) {
            result.data.insert(
                result.data.end(),
                input.begin() + static_cast<ptrdiff_t>(markerStart),
                input.begin() + static_cast<ptrdiff_t>(pos)
            );
            sawEoi = true;

            if (pos != input.size()) {
                result.error = "Unexpected trailing data after JPEG EOI.";
                return result;
            }

            break;
        }

        if (isStandalone(marker)) {
            result.data.insert(
                result.data.end(),
                input.begin() + static_cast<ptrdiff_t>(markerStart),
                input.begin() + static_cast<ptrdiff_t>(pos)
            );
            continue;
        }

        if (pos + 2 > input.size()) {
            result.error = "Truncated JPEG segment length.";
            return result;
        }

        const size_t segmentLength =
            (static_cast<size_t>(input[pos]) << 8) |
            static_cast<size_t>(input[pos + 1]);

        if (segmentLength < 2 ||
            segmentLength > input.size() - pos) {
            result.error = "Invalid or truncated JPEG segment.";
            return result;
        }

        const size_t segmentEnd = pos + segmentLength;
        const bool remove = shouldRemove(marker);

        if (remove) {
            result.removedItems.push_back(markerName(marker));
            ++result.removed;
        } else {
            result.data.insert(
                result.data.end(),
                input.begin() + static_cast<ptrdiff_t>(markerStart),
                input.begin() + static_cast<ptrdiff_t>(segmentEnd)
            );
        }

        pos = segmentEnd;

        // After SOS, copy compressed image data as-is until the next
        // actual marker. FF00 and restart markers are part of the scan.
        if (marker == 0xDA) {
            const size_t scanStart = pos;

            while (pos < input.size()) {
                if (input[pos] != 0xFF) {
                    ++pos;
                    continue;
                }

                if (pos + 1 >= input.size()) {
                    result.error = "Truncated JPEG scan data.";
                    return result;
                }

                const uint8_t next = input[pos + 1];

                if (next == 0x00 ||
                    (next >= 0xD0 && next <= 0xD7)) {
                    pos += 2;
                    continue;
                }

                if (next == 0xFF) {
                    ++pos;
                    continue;
                }

                break;
            }

            result.data.insert(
                result.data.end(),
                input.begin() + static_cast<ptrdiff_t>(scanStart),
                input.begin() + static_cast<ptrdiff_t>(pos)
            );
        }
    }

    if (!sawEoi) {
        result.error = "JPEG is missing its EOI end marker.";
        result.data.clear();
        return result;
    }

    result.valid = true;
    return result;
}