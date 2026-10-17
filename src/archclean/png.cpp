//
// Created by user2378 on 10.10.2026.
//

#include "png.hpp"

#include <array>
#include <cstdint>
#include <string>

using namespace std;
namespace {

constexpr array<uint8_t, 8> PNG_SIGNATURE = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
};

uint32_t readU32(const Bytes& data, size_t pos) {
    return (static_cast<uint32_t>(data[pos]) << 24) |
           (static_cast<uint32_t>(data[pos + 1]) << 16) |
           (static_cast<uint32_t>(data[pos + 2]) << 8) |
           static_cast<uint32_t>(data[pos + 3]);
}

uint32_t crc32(const Bytes& data,
                    size_t start,
                    size_t length) {
    uint32_t crc = 0xFFFFFFFFU;

    for (size_t i = start; i < start + length; ++i) {
        crc ^= data[i];

        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 1U) {
                crc = (crc >> 1) ^ 0xEDB88320U;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFFU;
}

string chunkType(const Bytes& data, size_t pos) {
    return string(
        reinterpret_cast<const char*>(data.data() + pos), 4
    );
}

bool isKnownRenderingOrAnimationChunk(const string& type) {
    // Common color, transparency, and rendering-related chunks.
    // These are kept because removing them can alter image appearance.
    static const char* const kept[] = {
        "cHRM", "gAMA", "iCCP", "sBIT", "sRGB",
        "bKGD", "hIST", "tRNS", "sPLT",
        // APNG animation chunks:
        "acTL", "fcTL", "fdAT"
    };

    for (const char* item : kept) {
        if (type == item) return true;
    }

    return false;
}

bool isMetadataChunk(const string& type) {
    return type == "eXIf" ||
           type == "tEXt" ||
           type == "zTXt" ||
           type == "iTXt" ||
           type == "tIME" ||
           type == "pHYs";
}

bool isAncillaryChunk(const string& type) {
    // PNG defines an ancillary chunk by a lowercase first letter.
    return !type.empty() &&
           type[0] >= 'a' && type[0] <= 'z';
}

string chunkDescription(const string& type) {
    if (type == "eXIf") return "EXIF metadata";
    if (type == "tEXt") return "text metadata";
    if (type == "zTXt") return "compressed text metadata";
    if (type == "iTXt") return "international text metadata";
    if (type == "tIME") return "modification timestamp";
    if (type == "pHYs") return "pixel density information";

    return "unknown ancillary chunk";
}

} // namespace

CleanResult cleanPng(const Bytes& input) {
    CleanResult result;

    if (input.size() < PNG_SIGNATURE.size()) {
        result.error = "File is too small to be a PNG.";
        return result;
    }

    for (size_t i = 0; i < PNG_SIGNATURE.size(); ++i) {
        if (input[i] != PNG_SIGNATURE[i]) {
            result.error = "Invalid PNG signature.";
            return result;
        }
    }

    result.data.insert(
        result.data.end(),
        input.begin(),
        input.begin() + 8
    );

    size_t pos = 8;
    bool sawIHDR = false;
    bool sawIDAT = false;
    bool sawIEND = false;

    while (pos < input.size()) {
        // Length (4), type (4), CRC (4).
        if (input.size() - pos < 12) {
            result.error = "Truncated PNG chunk header or CRC.";
            return result;
        }

        const uint32_t length32 = readU32(input, pos);

        if (static_cast<uint64_t>(length32) >
            static_cast<uint64_t>(input.size() - pos - 12)) {
            result.error = "PNG chunk length exceeds file size.";
            return result;
        }

        const size_t length = static_cast<size_t>(length32);
        const size_t typePos = pos + 4;
        const size_t dataPos = pos + 8;
        const size_t crcPos = dataPos + length;
        const size_t chunkEnd = crcPos + 4;

        const std::string type = chunkType(input, typePos);

        // Verify CRC over the four-byte type and the chunk data.
        const uint32_t storedCrc = readU32(input, crcPos);
        const uint32_t calculatedCrc =
            crc32(input, typePos, 4 + length);

        if (storedCrc != calculatedCrc) {
            result.error = "CRC mismatch in PNG chunk: " + type;
            result.data.clear();
            return result;
        }

        if (!sawIHDR && type != "IHDR") {
            result.error = "IHDR must be the first PNG chunk.";
            result.data.clear();
            return result;
        }

        if (type == "IHDR") {
            if (sawIHDR || length != 13) {
                result.error = "Invalid or duplicate IHDR chunk.";
                result.data.clear();
                return result;
            }
            sawIHDR = true;
        }

        if (type == "IDAT") {
            sawIDAT = true;
        }

        const bool remove =
            isMetadataChunk(type) ||
            (isAncillaryChunk(type) &&
             !isKnownRenderingOrAnimationChunk(type));

        if (remove) {
            result.removedItems.push_back(
                type + " - " + chunkDescription(type)
            );
            ++result.removed;
        } else {
            result.data.insert(
                result.data.end(),
                input.begin() + static_cast<ptrdiff_t>(pos),
                input.begin() + static_cast<ptrdiff_t>(chunkEnd)
            );
        }

        pos = chunkEnd;

        if (type == "IEND") {
            if (length != 0) {
                result.error = "Invalid IEND chunk length.";
                result.data.clear();
                return result;
            }

            sawIEND = true;

            if (pos != input.size()) {
                result.error = "Unexpected trailing data after PNG IEND.";
                result.data.clear();
                return result;
            }

            break;
        }
    }

    if (!sawIHDR || !sawIDAT || !sawIEND) {
        result.error = "PNG is missing IHDR, IDAT, or IEND.";
        result.data.clear();
        return result;
    }

    result.valid = true;
    return result;
}