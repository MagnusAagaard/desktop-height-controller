/**
 * @file PresetManager.h
 * @brief Height preset configuration management with NVS persistence
 * 
 * Manages up to 5 height presets that persist across reboots.
 * Each preset has a slot (1-5), name (max 16 chars), and height (50-125 cm).
 * 
 * Per FR-009: Store up to 5 height presets
 * Per FR-010: User-defined labels for presets
 * Per FR-011: Persist presets across reboots
 */

#ifndef PRESET_MANAGER_H
#define PRESET_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

// Constants
constexpr uint8_t MAX_PRESETS = 5;
constexpr uint8_t MAX_PRESET_NAME_LENGTH = 16;

/**
 * @struct Preset
 * @brief Represents a single height preset
 */
struct Preset {
    uint8_t slot;                           // Slot number (1-5)
    char name[MAX_PRESET_NAME_LENGTH + 1];  // User-defined label
    float height_cm;                        // Target height in cm (0 = disabled)
    unsigned long last_modified_ms;         // Timestamp of last modification
    
    /**
     * @brief Check if preset is enabled (has valid height)
     */
    bool isEnabled() const {
        return height_cm > 0;
    }
    
    /**
     * @brief Reset preset to default values
     */
    void reset() {
        name[0] = '\0';
        height_cm = 0.0f;
        last_modified_ms = 0;
    }
};

/**
 * @class PresetManager
 * @brief Manages height presets with NVS persistence
 * 
 * Usage:
 *   PresetManager presets;
 *   presets.init();  // Load from NVS
 *   presets.savePreset(1, "Standing", 110.0f);
 *   Preset* p = presets.getPreset(1);
 */
class PresetManager {
public:
    /**
     * @brief Constructor - initializes empty presets
     */
    PresetManager();
    
    /**
     * @brief Initialize and load presets from NVS
     * @return true if successful, false on NVS error
     */
    bool init();
    
    /**
     * @brief Save a preset to NVS
     * @param slot Slot number (1-5)
     * @param name User-defined label (max 16 chars)
     * @param height_cm Target height (50-125 cm)
     * @return true if successful, false if invalid params or NVS error
     */
    bool savePreset(uint8_t slot, const char* name, float height_cm);
    
    /**
     * @brief Delete a preset (set to default/disabled)
     * @param slot Slot number (1-5)
     * @return true if successful, false if invalid slot
     */
    bool deletePreset(uint8_t slot);
    
    /**
     * @brief Get preset by slot number
     * @param slot Slot number (1-5)
     * @return Pointer to preset, or nullptr if invalid slot
     */
    const Preset* getPreset(uint8_t slot) const;
    
    /**
     * @brief Get all presets array
     * @return Pointer to array of MAX_PRESETS presets
     */
    const Preset* getAllPresets() const;
    
    /**
     * @brief Check if slot number is valid
     * @param slot Slot number to check
     * @return true if slot is 1-5
     */
    static bool isValidSlot(uint8_t slot);
    
    /**
     * @brief Check if height is valid for preset
     * @param height_cm Height to check
     * @return true if height is within valid range (50-125)
     */
    static bool isValidHeight(float height_cm);
    
    /**
     * @brief Get count of enabled presets
     * @return Number of presets with height > 0
     */
    uint8_t getEnabledCount() const;

private:
    Preset presets_[MAX_PRESETS];
    Preferences prefs_;
    bool initialized_;
    
    static constexpr const char* NVS_NAMESPACE = "presets";
    
    /**
     * @brief Load a single preset from NVS
     * @param slot Slot number (1-5)
     */
    void loadPreset(uint8_t slot);
    
    /**
     * @brief Write a single preset to NVS
     * @param slot Slot number (1-5)
     * @return true if successful
     */
    bool writePreset(uint8_t slot);
    
    /**
     * @brief Generate NVS key for preset height
     * @param slot Slot number
     * @param key Output buffer (min 4 bytes)
     */
    static void getHeightKey(uint8_t slot, char* key);
    
    /**
     * @brief Generate NVS key for preset name
     * @param slot Slot number
     * @param key Output buffer (min 4 bytes)
     */
    static void getNameKey(uint8_t slot, char* key);
};

#endif // PRESET_MANAGER_H
