/**
 * @file PresetManager.cpp
 * @brief Implementation of height preset management with NVS persistence
 */

#include "PresetManager.h"
#include "Config.h"
#include "utils/Logger.h"

static const char* TAG = "PresetManager";

PresetManager::PresetManager()
    : initialized_(false)
{
    // Initialize all presets to default values
    for (uint8_t i = 0; i < MAX_PRESETS; i++) {
        presets_[i].slot = i + 1;  // Slots are 1-based
        presets_[i].reset();
    }
}

bool PresetManager::init() {
    Logger::info(TAG, "Initializing PresetManager...");
    
    // Open NVS namespace
    if (!prefs_.begin(NVS_NAMESPACE, false)) {  // false = read-write mode
        Logger::error(TAG, "Failed to open NVS namespace '%s'", NVS_NAMESPACE);
        return false;
    }
    
    // Load all presets from NVS
    for (uint8_t slot = 1; slot <= MAX_PRESETS; slot++) {
        loadPreset(slot);
    }
    
    uint8_t enabledCount = getEnabledCount();
    Logger::info(TAG, "Loaded %d presets (%d enabled)", MAX_PRESETS, enabledCount);
    
    initialized_ = true;
    return true;
}

bool PresetManager::savePreset(uint8_t slot, const char* name, float height_cm) {
    // Validate slot
    if (!isValidSlot(slot)) {
        Logger::warn(TAG, "Invalid slot %d (must be 1-%d)", slot, MAX_PRESETS);
        return false;
    }
    
    // Validate height
    if (!isValidHeight(height_cm)) {
        Logger::warn(TAG, "Invalid height %.1f (must be %.0f-%.0f cm)", 
                     height_cm, DEFAULT_MIN_HEIGHT_CM, DEFAULT_MAX_HEIGHT_CM);
        return false;
    }
    
    // Get preset reference (0-indexed)
    Preset& preset = presets_[slot - 1];
    
    // Update preset data
    preset.height_cm = height_cm;
    preset.last_modified_ms = millis();
    
    // Copy name with truncation if needed
    if (name != nullptr) {
        strncpy(preset.name, name, MAX_PRESET_NAME_LENGTH);
        preset.name[MAX_PRESET_NAME_LENGTH] = '\0';  // Ensure null termination
    } else {
        preset.name[0] = '\0';
    }
    
    // Write to NVS
    if (!writePreset(slot)) {
        Logger::error(TAG, "Failed to write preset %d to NVS", slot);
        return false;
    }
    
    Logger::info(TAG, "Saved preset %d: '%s' = %.1f cm", 
                 slot, preset.name, preset.height_cm);
    return true;
}

bool PresetManager::deletePreset(uint8_t slot) {
    if (!isValidSlot(slot)) {
        Logger::warn(TAG, "Invalid slot %d for delete", slot);
        return false;
    }
    
    // Reset preset to defaults
    Preset& preset = presets_[slot - 1];
    preset.reset();
    
    // Write empty values to NVS
    if (!writePreset(slot)) {
        Logger::error(TAG, "Failed to delete preset %d from NVS", slot);
        return false;
    }
    
    Logger::info(TAG, "Deleted preset %d", slot);
    return true;
}

const Preset* PresetManager::getPreset(uint8_t slot) const {
    if (!isValidSlot(slot)) {
        return nullptr;
    }
    return &presets_[slot - 1];
}

const Preset* PresetManager::getAllPresets() const {
    return presets_;
}

bool PresetManager::isValidSlot(uint8_t slot) {
    return slot >= 1 && slot <= MAX_PRESETS;
}

bool PresetManager::isValidHeight(float height_cm) {
    return height_cm >= DEFAULT_MIN_HEIGHT_CM && height_cm <= DEFAULT_MAX_HEIGHT_CM;
}

uint8_t PresetManager::getEnabledCount() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_PRESETS; i++) {
        if (presets_[i].isEnabled()) {
            count++;
        }
    }
    return count;
}

void PresetManager::loadPreset(uint8_t slot) {
    if (!isValidSlot(slot)) return;
    
    char heightKey[4], nameKey[4];
    getHeightKey(slot, heightKey);
    getNameKey(slot, nameKey);
    
    Preset& preset = presets_[slot - 1];
    
    // Load height (default to 0 if not found)
    preset.height_cm = prefs_.getFloat(heightKey, 0.0f);
    
    // Load name (default to empty string)
    String nameStr = prefs_.getString(nameKey, "");
    strncpy(preset.name, nameStr.c_str(), MAX_PRESET_NAME_LENGTH);
    preset.name[MAX_PRESET_NAME_LENGTH] = '\0';
    
    // Validate loaded height
    if (preset.height_cm != 0.0f && !isValidHeight(preset.height_cm)) {
        Logger::warn(TAG, "Preset %d has invalid height %.1f, resetting", 
                     slot, preset.height_cm);
        preset.reset();
    }
    
    if (preset.isEnabled()) {
        Logger::debug(TAG, "Loaded preset %d: '%s' = %.1f cm", 
                      slot, preset.name, preset.height_cm);
    }
}

bool PresetManager::writePreset(uint8_t slot) {
    if (!isValidSlot(slot)) return false;
    
    char heightKey[4], nameKey[4];
    getHeightKey(slot, heightKey);
    getNameKey(slot, nameKey);
    
    const Preset& preset = presets_[slot - 1];
    
    // Write height
    if (prefs_.putFloat(heightKey, preset.height_cm) == 0) {
        Logger::error(TAG, "Failed to write height for preset %d", slot);
        return false;
    }
    
    // Write name
    if (prefs_.putString(nameKey, preset.name) == 0) {
        Logger::error(TAG, "Failed to write name for preset %d", slot);
        return false;
    }
    
    return true;
}

void PresetManager::getHeightKey(uint8_t slot, char* key) {
    snprintf(key, 4, "h%d", slot);
}

void PresetManager::getNameKey(uint8_t slot, char* key) {
    snprintf(key, 4, "n%d", slot);
}
