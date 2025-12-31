#define LOG_TAG "mploader"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <android-base/properties.h>
#include <cctype>
#include <algorithm>
#include <string>
#include <vector>
#include <fstream>

#define DEFAULT_ID "0X00000000"
#define SYS_PROP_READY "sys.rilprops_ready"

#define CMDLINE "/proc/cmdline"
#define PHONE_PROP "/vendor/phone.prop"

// Helper: convert string to uppercase
std::string ToUpper(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::toupper(c); });
    return result;
}

std::string ReadProductId() {
    std::string prid = DEFAULT_ID;
    std::string cmdline;

    if (!android::base::ReadFileToString(CMDLINE, &cmdline)) {
        LOG(ERROR) << "Failed to read " << CMDLINE;
        return prid;
    }

    for (const auto& token : android::base::Split(android::base::Trim(cmdline), " ")) {
        std::vector<std::string> parts = android::base::Split(token, "=", 1);
        if (parts.size() == 2 && parts[0] == "productid") {
            prid = ToUpper(parts[1]);
            // Normalize "0x..." to "0X..." for consistency
            if (prid.length() >= 2 && prid[0] == '0' && prid[1] == 'X') {
                // already good
            } else if (prid.length() >= 2 && prid[0] == '0' && prid[1] == 'X') {
                // handled above
            }
            // If input was "0x1234", ToUpper makes it "0X1234"
            LOG(INFO) << "Found productid: " << prid;
            break;
        }
    }

    return prid;
}

int LoadPhoneProperties(const std::string& prid) {
    std::ifstream file(PHONE_PROP);
    if (!file.is_open()) {
        LOG(ERROR) << "Failed to open " << PHONE_PROP;
        return -1;
    }

    std::string line;
    bool in_section = false;

    while (std::getline(file, line)) {
        std::string trimmed = android::base::Trim(line);

        // Skip comments and blank lines when not in section
        if (trimmed.empty() || trimmed[0] == '#') {
            if (in_section) {
                // End of section on blank/comment line
                break;
            }
            continue;
        }

        // Check for exact productid match (case-insensitive handled by ToUpper)
        if (!in_section && trimmed == prid) {
            LOG(INFO) << "Matched product section: " << prid;
            in_section = true;
            continue;
        }

        if (in_section) {
            std::vector<std::string> parts = android::base::Split(trimmed, "=", 1);
            if (parts.size() == 2) {
                const std::string& key = android::base::Trim(parts[0]);
                const std::string& value = android::base::Trim(parts[1]);
                LOG(INFO) << "Setting property: " << key << " = " << value;
                if (!android::base::SetProperty(key, value)) {
                    LOG(WARNING) << "Failed to set property: " << key;
                }
            } else {
                LOG(WARNING) << "Invalid property line: " << trimmed;
            }
        }
    }

    if (!in_section) {
        LOG(ERROR) << "No section found for productid: " << prid;
        return -1;
    }

    return 0;
}

int main() {
    std::string productId = ReadProductId();

    if (productId == DEFAULT_ID) {
        LOG(INFO) << "No valid productid found, skipping property loading";
        return 0; // Not an error; just no properties to load
    }

    int ret = LoadPhoneProperties(productId);
    if (ret == 0) {
        LOG(INFO) << "Successfully loaded phone properties for " << productId;
        android::base::SetProperty(SYS_PROP_READY, "1");
    } else {
        LOG(ERROR) << "Failed to load properties for " << productId;
    }

    return ret;
}
