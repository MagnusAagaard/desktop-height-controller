/**
 * @file MovementController.h
 * @brief Desk movement control with state machine and MOSFET control
 * 
 * Implements the movement state machine per data-model.md Section 4:
 * - IDLE: No movement, desk stationary
 * - MOVING_UP: Desk rising toward target
 * - MOVING_DOWN: Desk lowering toward target
 * - STABILIZING: Within tolerance, waiting for stability confirmation
 * - ERROR: Movement stopped due to fault
 * 
 * Safety features:
 * - Mutual exclusion (never both MOSFETs on)
 * - Timeout protection
 * - Sensor failure detection
 * - Emergency stop
 */

#ifndef MOVEMENT_CONTROLLER_H
#define MOVEMENT_CONTROLLER_H

#include <Arduino.h>
#include "Config.h"
#include "SystemConfiguration.h"
#include "HeightController.h"

/**
 * @enum MovementState
 * @brief Movement state machine states per data-model.md
 */
enum class MovementState : uint8_t {
    IDLE,           ///< No movement, desk stationary
    MOVING_UP,      ///< Desk rising toward target
    MOVING_DOWN,    ///< Desk lowering toward target
    STABILIZING,    ///< Within tolerance, confirming stability
    ERROR           ///< Movement stopped due to fault
};

/**
 * @enum TargetSource
 * @brief How the target height was set per data-model.md Section 2
 */
enum class TargetSource : uint8_t {
    MANUAL,     ///< Direct user input
    PRESET      ///< From preset activation
};

/**
 * @struct TargetHeight
 * @brief Target height configuration per data-model.md Section 2
 */
struct TargetHeight {
    uint16_t target_height_cm;          ///< Desired height
    uint16_t tolerance_mm;              ///< Acceptable deviation
    unsigned long activation_timestamp; ///< When target was set
    TargetSource source;                ///< How target was triggered
    uint8_t source_id;                  ///< Preset slot (if from preset)
    bool active;                        ///< Is target currently active
};

/**
 * @typedef MovementStatusCallback
 * @brief Callback for movement status changes
 */
typedef void (*MovementStatusCallback)(MovementState state, const String& message);

/**
 * @class MovementController
 * @brief Controls desk movement with state machine
 * 
 * Usage:
 *   MovementController movement(heightController);
 *   movement.init();
 *   movement.setTargetHeight(75);
 *   // In loop:
 *   movement.update();
 */
class MovementController {
public:
    /**
     * @brief Construct MovementController
     * @param heightController Reference to height sensor controller
     */
    explicit MovementController(HeightController& heightController);
    
    /**
     * @brief Initialize GPIO pins
     */
    void init();
    
    /**
     * @brief Update state machine (call from main loop at 5Hz)
     * 
     * Checks current height, manages state transitions,
     * controls motor pins.
     */
    void update();
    
    /**
     * @brief Set new target height (manual input)
     * @param height_cm Target height in cm
     * @return true if target accepted, false if invalid
     */
    bool setTargetHeight(uint16_t height_cm);
    
    /**
     * @brief Set target height from preset
     * @param height_cm Target height in cm
     * @param preset_slot Preset slot number (1-5)
     * @return true if target accepted
     */
    bool setTargetFromPreset(uint16_t height_cm, uint8_t preset_slot);
    
    /**
     * @brief Emergency stop - immediately stop movement
     * 
     * Stops both motors, clears target, enters IDLE state.
     */
    void emergencyStop();
    
    /**
     * @brief Clear any error state and return to IDLE
     */
    void clearError();
    
    /**
     * @brief Get current state
     * @return MovementState Current state
     */
    MovementState getState() const;
    
    /**
     * @brief Get state as human-readable string
     * @return const char* State name
     */
    const char* getStateString() const;
    
    /**
     * @brief Check if movement is currently active
     * @return true if in MOVING_UP or MOVING_DOWN state
     */
    bool isMoving() const;
    
    /**
     * @brief Check if in error state
     * @return true if in ERROR state
     */
    bool hasError() const;
    
    /**
     * @brief Get current target (if any)
     * @return const TargetHeight& Target information
     */
    const TargetHeight& getTarget() const;
    
    /**
     * @brief Get last error message
     * @return const String& Error message
     */
    const String& getLastError() const;
    
    /**
     * @brief Set callback for status changes
     * @param callback Function to call on state change
     */
    void setStatusCallback(MovementStatusCallback callback);
    
    /**
     * @brief Get status as JSON string (for API/SSE)
     * @return String JSON representation
     */
    String toJson() const;

private:
    HeightController& heightController_;
    MovementState state_;
    TargetHeight target_;
    String lastError_;
    MovementStatusCallback statusCallback_;
    
    unsigned long movementStartTime_;
    unsigned long stabilizationStartTime_;
    
    /**
     * @brief Set motor pins based on state
     * @param state Target state for pin configuration
     */
    void setMotorPins(MovementState state);
    
    /**
     * @brief Transition to new state
     * @param newState State to transition to
     * @param message Status message
     */
    void setState(MovementState newState, const String& message);
    
    /**
     * @brief Check if current height is within tolerance of target
     * @return true if within tolerance
     */
    bool isWithinTolerance() const;
    
    /**
     * @brief Determine direction to move based on current vs target
     * @return MovementState MOVING_UP, MOVING_DOWN, or IDLE (at target)
     */
    MovementState determineDirection() const;
    
    /**
     * @brief Check for timeout condition
     * @return true if movement has timed out
     */
    bool checkTimeout() const;
    
    /**
     * @brief Check if sensor reading is valid
     * @return true if sensor is providing valid data
     */
    bool checkSensorValidity() const;
    
    // State handlers
    void handleIdleState();
    void handleMovingState();
    void handleStabilizingState();
    void handleErrorState();
};

#endif // MOVEMENT_CONTROLLER_H
