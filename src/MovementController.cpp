/**
 * @file MovementController.cpp
 * @brief Implementation of desk movement control state machine
 */

#include "MovementController.h"
#include "utils/Logger.h"

static const char* TAG = "MovementController";

MovementController::MovementController(HeightController& heightController)
    : heightController_(heightController)
    , state_(MovementState::IDLE)
    , statusCallback_(nullptr)
    , movementStartTime_(0)
    , stabilizationStartTime_(0)
{
    // Initialize target as inactive - tolerance will be set in init()
    target_.active = false;
    target_.target_height_cm = 0;
    target_.tolerance_mm = DEFAULT_TOLERANCE_MM;  // Use default, init() will update
    target_.source = TargetSource::MANUAL;
    target_.source_id = 0;
    target_.activation_timestamp = 0;
}

void MovementController::init() {
    // Configure motor control pins as outputs
    pinMode(PIN_MOTOR_UP, OUTPUT);
    pinMode(PIN_MOTOR_DOWN, OUTPUT);
    
    // Ensure motors are off at startup
    setMotorPins(MovementState::IDLE);
    
    // Update tolerance from config (SystemConfig now initialized)
    target_.tolerance_mm = SystemConfig.getTolerance();
    
    Logger::info(TAG, "Initialized - UP pin: %d, DOWN pin: %d, Tolerance: %dmm", 
                 PIN_MOTOR_UP, PIN_MOTOR_DOWN, target_.tolerance_mm);
}

void MovementController::update() {
    // Safety check: if sensor is not valid and we're moving, stop!
    if (!checkSensorValidity() && isMoving()) {
        setState(MovementState::ERROR, "Sensor reading invalid during movement");
        return;
    }
    
    // Check for timeout during movement
    if (isMoving() && checkTimeout()) {
        setState(MovementState::ERROR, "Movement timeout - target not reached");
        return;
    }
    
    // Run state-specific logic
    switch (state_) {
        case MovementState::IDLE:
            handleIdleState();
            break;
            
        case MovementState::MOVING_UP:
        case MovementState::MOVING_DOWN:
            handleMovingState();
            break;
            
        case MovementState::STABILIZING:
            handleStabilizingState();
            break;
            
        case MovementState::ERROR:
            handleErrorState();
            break;
    }
}

bool MovementController::setTargetHeight(uint16_t height_cm) {
    // Validate height against configured limits
    if (!SystemConfig.isValidHeight(height_cm)) {
        Logger::warn(TAG, "Invalid target height: %d cm (valid range: %d-%d)",
                     height_cm, 
                     SystemConfig.getMinHeight(), 
                     SystemConfig.getMaxHeight());
        return false;
    }
    
    // Check if system is calibrated
    if (!SystemConfig.isCalibrated()) {
        Logger::error(TAG, "Cannot set target: system not calibrated");
        lastError_ = "System not calibrated";
        return false;
    }
    
    // If in error state, need to clear first
    if (state_ == MovementState::ERROR) {
        Logger::warn(TAG, "Cannot set target while in ERROR state");
        return false;
    }
    
    // Set target
    target_.target_height_cm = height_cm;
    target_.tolerance_mm = SystemConfig.getTolerance();
    target_.activation_timestamp = millis();
    target_.source = TargetSource::MANUAL;
    target_.source_id = 0;
    target_.active = true;
    
    Logger::info(TAG, "Target set: %d cm (tolerance: ±%d mm)", 
                 height_cm, target_.tolerance_mm);
    
    // Determine initial direction and start moving
    MovementState direction = determineDirection();
    
    if (direction == MovementState::IDLE) {
        // Already at target
        target_.active = false;
        Logger::info(TAG, "Already at target height");
        return true;
    }
    
    movementStartTime_ = millis();
    setState(direction, direction == MovementState::MOVING_UP ? 
             "Moving up to target" : "Moving down to target");
    
    return true;
}

bool MovementController::setTargetFromPreset(uint16_t height_cm, uint8_t preset_slot) {
    if (!setTargetHeight(height_cm)) {
        return false;
    }
    
    target_.source = TargetSource::PRESET;
    target_.source_id = preset_slot;
    
    Logger::info(TAG, "Target from preset %d: %d cm", preset_slot, height_cm);
    return true;
}

void MovementController::emergencyStop() {
    Logger::warn(TAG, "EMERGENCY STOP triggered");
    
    // Immediately stop motors
    setMotorPins(MovementState::IDLE);
    
    // Clear target
    target_.active = false;
    
    // Return to idle (not error - this was intentional stop)
    setState(MovementState::IDLE, "Emergency stop activated");
}

void MovementController::clearError() {
    if (state_ != MovementState::ERROR) {
        return;
    }
    
    Logger::info(TAG, "Clearing error state");
    target_.active = false;
    lastError_ = "";
    setState(MovementState::IDLE, "Error cleared");
}

MovementState MovementController::getState() const {
    return state_;
}

const char* MovementController::getStateString() const {
    switch (state_) {
        case MovementState::IDLE:        return "Idle";
        case MovementState::MOVING_UP:   return "Moving Up";
        case MovementState::MOVING_DOWN: return "Moving Down";
        case MovementState::STABILIZING: return "Stabilizing";
        case MovementState::ERROR:       return "Error";
        default:                         return "Unknown";
    }
}

bool MovementController::isMoving() const {
    return state_ == MovementState::MOVING_UP || 
           state_ == MovementState::MOVING_DOWN;
}

bool MovementController::hasError() const {
    return state_ == MovementState::ERROR;
}

const TargetHeight& MovementController::getTarget() const {
    return target_;
}

const String& MovementController::getLastError() const {
    return lastError_;
}

void MovementController::setStatusCallback(MovementStatusCallback callback) {
    statusCallback_ = callback;
}

void MovementController::setMotorPins(MovementState state) {
    // CRITICAL: Always ensure mutual exclusion
    // Never have both pins HIGH at the same time
    
    switch (state) {
        case MovementState::MOVING_UP:
            digitalWrite(PIN_MOTOR_DOWN, LOW);  // Ensure DOWN is off first
            delayMicroseconds(100);              // Brief delay for safety
            digitalWrite(PIN_MOTOR_UP, HIGH);
            Logger::debug(TAG, "Motors: UP=HIGH, DOWN=LOW");
            break;
            
        case MovementState::MOVING_DOWN:
            digitalWrite(PIN_MOTOR_UP, LOW);    // Ensure UP is off first
            delayMicroseconds(100);
            digitalWrite(PIN_MOTOR_DOWN, HIGH);
            Logger::debug(TAG, "Motors: UP=LOW, DOWN=HIGH");
            break;
            
        case MovementState::IDLE:
        case MovementState::STABILIZING:
        case MovementState::ERROR:
        default:
            digitalWrite(PIN_MOTOR_UP, LOW);
            digitalWrite(PIN_MOTOR_DOWN, LOW);
            Logger::debug(TAG, "Motors: UP=LOW, DOWN=LOW");
            break;
    }
}

void MovementController::setState(MovementState newState, const String& message) {
    if (state_ != newState) {
        Logger::info(TAG, "State: %s -> %s (%s)", 
                     getStateString(), 
                     (newState == MovementState::IDLE) ? "Idle" :
                     (newState == MovementState::MOVING_UP) ? "Moving Up" :
                     (newState == MovementState::MOVING_DOWN) ? "Moving Down" :
                     (newState == MovementState::STABILIZING) ? "Stabilizing" : "Error",
                     message.c_str());
        
        MovementState oldState = state_;
        state_ = newState;
        
        // Update motor pins for new state
        setMotorPins(newState);
        
        // If entering error state, record error message
        if (newState == MovementState::ERROR) {
            lastError_ = message;
        }
        
        // If entering stabilizing, start timer
        if (newState == MovementState::STABILIZING) {
            stabilizationStartTime_ = millis();
        }
        
        // Notify callback
        if (statusCallback_ != nullptr) {
            statusCallback_(newState, message);
        }
    }
}

bool MovementController::isWithinTolerance() const {
    if (!target_.active) return false;
    
    uint16_t currentHeight = heightController_.getCurrentHeight();
    int16_t diff_mm = ((int16_t)target_.target_height_cm - (int16_t)currentHeight) * 10;
    
    return abs(diff_mm) <= (int16_t)target_.tolerance_mm;
}

MovementState MovementController::determineDirection() const {
    if (!target_.active) return MovementState::IDLE;
    
    uint16_t currentHeight = heightController_.getCurrentHeight();
    
    // Check if already within tolerance
    int16_t diff_mm = ((int16_t)target_.target_height_cm - (int16_t)currentHeight) * 10;
    
    if (abs(diff_mm) <= (int16_t)target_.tolerance_mm) {
        return MovementState::IDLE;  // Already at target
    }
    
    if (target_.target_height_cm > currentHeight) {
        return MovementState::MOVING_UP;
    } else {
        return MovementState::MOVING_DOWN;
    }
}

bool MovementController::checkTimeout() const {
    if (!isMoving()) return false;
    
    unsigned long elapsed = millis() - movementStartTime_;
    return elapsed > SystemConfig.getMovementTimeout();
}

bool MovementController::checkSensorValidity() const {
    return heightController_.isValid() || 
           heightController_.getReadingAge() < READING_STALE_TIMEOUT_MS;
}

void MovementController::handleIdleState() {
    // In IDLE, we wait for a new target to be set
    // Target setting is handled by setTargetHeight()
    
    // If somehow we have an active target while in IDLE, start moving
    if (target_.active) {
        MovementState direction = determineDirection();
        if (direction != MovementState::IDLE) {
            movementStartTime_ = millis();
            setState(direction, "Starting movement to target");
        }
    }
}

void MovementController::handleMovingState() {
    // Check if we've reached the target (within tolerance)
    if (isWithinTolerance()) {
        setState(MovementState::STABILIZING, "Target reached, stabilizing");
        return;
    }
    
    // Check if direction needs to change (shouldn't happen normally)
    MovementState desiredDirection = determineDirection();
    if (desiredDirection != state_ && desiredDirection != MovementState::IDLE) {
        setState(desiredDirection, "Direction changed");
    }
}

void MovementController::handleStabilizingState() {
    // Check if we're still within tolerance
    if (!isWithinTolerance()) {
        // Drifted outside tolerance, resume movement
        MovementState direction = determineDirection();
        if (direction != MovementState::IDLE) {
            // Don't reset movement start time - keep original timeout
            setState(direction, "Drifted outside tolerance, resuming movement");
        }
        return;
    }
    
    // Check if stabilization time has elapsed
    unsigned long elapsed = millis() - stabilizationStartTime_;
    if (elapsed >= SystemConfig.getStabilizationDuration()) {
        // Stable for required duration - movement complete!
        target_.active = false;
        setState(MovementState::IDLE, "Target reached and stable");
        Logger::info(TAG, "Movement complete at %d cm", 
                     heightController_.getCurrentHeight());
    }
}

void MovementController::handleErrorState() {
    // In ERROR state, motors should be off
    // Wait for clearError() to be called
    
    // Ensure motors are definitely off
    setMotorPins(MovementState::ERROR);
}

String MovementController::toJson() const {
    String json = "{";
    json += "\"state\":\"" + String(getStateString()) + "\",";
    json += "\"isMoving\":" + String(isMoving() ? "true" : "false") + ",";
    json += "\"hasError\":" + String(hasError() ? "true" : "false") + ",";
    
    if (target_.active) {
        json += "\"target\":" + String(target_.target_height_cm) + ",";
        json += "\"targetSource\":\"" + String(target_.source == TargetSource::PRESET ? "preset" : "manual") + "\"";
    } else {
        json += "\"target\":null,";
        json += "\"targetSource\":null";
    }
    
    if (hasError()) {
        json += ",\"error\":\"" + lastError_ + "\"";
    }
    
    json += "}";
    return json;
}
