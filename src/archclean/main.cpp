//
// Created by user2378 on 12.10.2026.
//

#include "jpeg.hpp"
#include "png.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>

using namespace std;
namespace fs = std::filesystem;

enum class ImageFormat {
    JPEG,
    PNG,
    UNKNOWN
};

ImageFormat detectFormat(const Bytes& data) {
    if (data.size() >= 3 &&
        data[0] == 0xFF &&
        data[1] == 0xD8 &&
        data[2] == 0xFF) {
        return ImageFormat::JPEG;
    }

    constexpr uint8_t signature[] = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A
    };

    if (data.size() >= 8) {
        bool matches = true;

        for (size_t i = 0; i < 8; ++i) {
            if (data[i] != signature[i]) {
                matches = false;
                break;
            }
        }

        if (matches) {
            return ImageFormat::PNG;
        }
    }

    return ImageFormat::UNKNOWN;
}

bool readFile(const fs::path& path, Bytes& data) {
    ifstream file(path, ios::binary);

    if (!file) {
        return false;
    }

    data.assign(
        istreambuf_iterator<char>(file),
        istreambuf_iterator<char>()
    );

    return !file.bad();
}

bool writeFile(const fs::path& path, const Bytes& data) {
    // Refuse to overwrite an existing output file.
    error_code ec;

    if (fs::exists(path, ec) || ec) {
        return false;
    }

    ofstream file(path, ios::binary);

    if (!file) {
        return false;
    }

    if (!data.empty()) {
        file.write(
            reinterpret_cast<const char*>(data.data()),
            static_cast<streamsize>(data.size())
        );
    }

    file.close();
    return static_cast<bool>(file);
}

int main(int argc, char* argv[]) {
    // ArchClean ASCII logo
    cout << R"(
     _                    _     ____ _
    / \   _ __ ___   ___| |__ / ___| | ___  __ _ _ __
   / _ \ | '__/ __| / __| '_ \ |   | |/ _ \/ _` | '_ \
  / ___ \| | | (__ | (__| | | | |___| |  __/ (_| | | | |
 /_/   \_\_|  \___| \___|_| |_|\____|_|\___|\__,_|_| |_|

                 METADATA CLEANER
                   C++20 EDITION
)" << '\n';

    cout
        << "====================================================\n"
        << "  Strip the traces from the image.\n"
        << "====================================================\n\n";

    string inputName;
    string outputName;

    // Accept paths as command-line arguments or prompt for them.
    if (argc == 3) {
        inputName = argv[1];
        outputName = argv[2];
    } else if (argc == 1) {
        cout << "[?] Input image path: ";
        getline(cin, inputName);

        cout << "[?] Output image path: ";
        getline(cin, outputName);
    } else {
        cerr << "[-] Usage: archclean INPUT OUTPUT\n";
        return 1;
    }

    if (inputName.empty() || outputName.empty()) {
        cerr << "[-] Paths cannot be empty.\n";
        return 1;
    }

    const fs::path inputPath(inputName);
    const fs::path outputPath(outputName);

    error_code ec;

    if (!fs::is_regular_file(inputPath, ec) || ec) {
        cerr << "[-] Input file does not exist or is not a regular file.\n";
        return 1;
    }

    // Resolve paths so the input cannot accidentally be its own output.
    const fs::path absoluteInput =
        fs::absolute(inputPath, ec).lexically_normal();

    if (ec) {
        cerr << "[-] Could not resolve input path.\n";
        return 1;
    }

    const fs::path absoluteOutput =
        fs::absolute(outputPath, ec).lexically_normal();

    if (ec) {
        cerr << "[-] Could not resolve output path.\n";
        return 1;
    }

    if (absoluteInput == absoluteOutput) {
        cerr << "[-] Input and output paths must differ.\n";
        return 1;
    }

    if (fs::exists(outputPath, ec) || ec) {
        cerr << "[-] Output already exists or cannot be checked.\n";
        return 1;
    }

    Bytes input;

    cout << "\n[+] Reading input file...\n";

    if (!readFile(inputPath, input)) {
        cerr << "[-] Failed to read input file.\n";
        return 1;
    }

    cout << "[+] File size: " << input.size() << " bytes\n";

    const ImageFormat format = detectFormat(input);
    CleanResult result;

    switch (format) {
        case ImageFormat::JPEG:
            cout << "[+] Detected format: JPEG\n";
            cout << "[+] Scanning JPEG segments...\n\n";
            result = cleanJpeg(input);
            break;

        case ImageFormat::PNG:
            cout << "[+] Detected format: PNG\n";
            cout << "[+] Scanning PNG chunks...\n\n";
            result = cleanPng(input);
            break;

        default:
            cerr << "[-] Unsupported image format. Use JPEG or PNG.\n";
            return 1;
    }

    if (!result.valid) {
        cerr << "\n[-] Cleaning failed: "
                  << result.error << '\n';
        cerr << "[-] No cleaned output was saved.\n";
        return 1;
    }

    // Print each metadata segment or chunk removed by the cleaner.
    cout << "---------------- REMOVAL LOG ----------------\n";

    if (result.removedItems.empty()) {
        cout << "[+] No targeted chunks or segments were removed.\n";
    } else {
        for (const string& item : result.removedItems) {
            cout << "[YOINKED] " << item << '\n';
        }
    }

    cout << "----------------------------------------------\n";
    cout << "[+] Total items removed: "
              << result.removedItems.size() << '\n';

    cout << "\n[+] Writing cleaned image...\n";

    if (!writeFile(outputPath, result.data)) {
        cerr << "[-] Could not create output file. "
                     "It may already exist or the directory may be inaccessible.\n";
        return 1;
    }

    cout
        << "\n====================================================\n"
        << "                 CLEANING COMPLETE\n"
        << "====================================================\n"
        << "[+] Original size: " << input.size() << " bytes\n"
        << "[+] Cleaned size:  " << result.data.size() << " bytes\n"
        << "[+] Items yoinked: " << result.removedItems.size() << '\n'
        << "[+] Output: " << outputPath.string() << '\n'
        << "\n[!] Some metadata may remain in preserved segments/chunks.\n"
        << "====================================================\n";

    return 0;
}