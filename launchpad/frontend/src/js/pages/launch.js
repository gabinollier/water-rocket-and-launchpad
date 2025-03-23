import { webSocket } from "../websocket.js";

// State variables
let currentState = 'setup'; // setup, filling, launch, post-launch
let currentWaterVolume = 0;
let targetWaterVolume = 250;
let currentPressure = 0;
let targetPressure = 40;
let fillCompleted = false;
let pressurizeCompleted = false;
let countdownEnabled = false;
let countdownInterval = null;
let countdownValue = 5;
let isRocketLocked = false;
let isRocketReady = false;

// DOM Elements
const setupPhase = document.getElementById('setup-phase');
const fillingPhase = document.getElementById('filling-phase');
const launchPhase = document.getElementById('launch-phase');
const postLaunchPhase = document.getElementById('post-launch-phase');
const abortButton = document.getElementById('abort-button');
const fillButton = document.getElementById('fill-button');
const confirmFillButton = document.getElementById('confirm-fill');
const cancelFillButton = document.getElementById('cancel-fill');
const confirmLockedCheckbox = document.getElementById('confirm-locked');
const confirmClearedCheckbox = document.getElementById('confirm-cleared');
const fillConfirmationModal = document.getElementById('fill-confirmation-modal');
const waterVolumeInput = document.getElementById('water-volume');
const pressureInput = document.getElementById('pressure');
const launchButton = document.getElementById('launch-button');
const enableCountdownCheckbox = document.getElementById('enable-countdown');
const countdownDisplay = document.getElementById('countdown-display');
const lockStatusElement = document.getElementById('lock-status');
const rocketReadinessElement = document.getElementById('rocket-readiness');
const waterVolumeCurrentElement = document.getElementById('water-volume-current');
const waterVolumeTargetElement = document.getElementById('water-volume-target');
const pressureCurrentElement = document.getElementById('pressure-current');
const pressureTargetElement = document.getElementById('pressure-target');
const waterProgressBar = document.getElementById('water-progress-bar');
const pressureProgressBar = document.getElementById('pressure-progress-bar');
const fillingStatusElement = document.getElementById('filling-status');
const currentPressureDisplay = document.getElementById('current-pressure-display');
const waitingReconnection = document.getElementById('waiting-reconnection');
const dataTransmission = document.getElementById('data-transmission');
const dataComplete = document.getElementById('data-complete');
const dataProgress = document.getElementById('data-progress');

export const onPageLoad = () => {
    // Initialize UI
    initializeUI();
    
    // Setup event listeners
    setupEventListeners();
    
    // Start WebSocket listener
    setupWebSocketListener();
    
    // Check initial rocket status
    checkRocketStatus();
};

function initializeUI() {
    // Set initial values
    waterVolumeTargetElement.textContent = `${targetWaterVolume} ml`;
    pressureTargetElement.textContent = `${targetPressure} PSI`;
    
    // Show the setup phase initially
    showPhase('setup');
    
    // Initialize abort button (hidden initially)
    abortButton.classList.add('hidden');
}

function setupEventListeners() {
    // Fill button click
    fillButton.addEventListener('click', () => {
        targetWaterVolume = parseInt(waterVolumeInput.value);
        targetPressure = parseInt(pressureInput.value);
        
        // Update target displays
        waterVolumeTargetElement.textContent = `${targetWaterVolume} ml`;
        pressureTargetElement.textContent = `${targetPressure} PSI`;
        
        // Show confirmation modal
        fillConfirmationModal.classList.remove('hidden');
    });
    
    // Confirmation checkboxes
    confirmLockedCheckbox.addEventListener('change', updateConfirmButtonState);
    confirmClearedCheckbox.addEventListener('change', updateConfirmButtonState);
    
    // Cancel fill button
    cancelFillButton.addEventListener('click', () => {
        fillConfirmationModal.classList.add('hidden');
        confirmLockedCheckbox.checked = false;
        confirmClearedCheckbox.checked = false;
        updateConfirmButtonState();
    });
    
    // Confirm fill button
    confirmFillButton.addEventListener('click', () => {
        fillConfirmationModal.classList.add('hidden');
        startFillingProcess();
    });
    
    // Launch button
    launchButton.addEventListener('click', () => {
        if (countdownEnabled) {
            startCountdown();
        } else {
            launchRocket();
        }
    });
    
    // Enable countdown checkbox
    enableCountdownCheckbox.addEventListener('change', () => {
        countdownEnabled = enableCountdownCheckbox.checked;
        countdownDisplay.classList.toggle('hidden', !countdownEnabled);
    });
    
    // Abort button
    abortButton.addEventListener('click', abortProcess);
}

function updateConfirmButtonState() {
    confirmFillButton.disabled = !(confirmLockedCheckbox.checked && confirmClearedCheckbox.checked);
}

function setupWebSocketListener() {
    webSocket.onmessage = (event) => {
        const wsMessage = JSON.parse(event.data);
        
        switch (wsMessage.type) {
            case 'pressure':
                updatePressureDisplay(wsMessage.pressure);
                break;
                
            case 'water-volume':
                updateWaterVolumeDisplay(wsMessage.volume);
                break;
                
            case 'reconnected-with-rocket':
                handleRocketReconnected();
                break;
                
            case 'done-data-upload':
                handleDataUploadComplete();
                break;
        }
    };
    
    webSocket.onclose = () => {
        console.error("WebSocket connection closed");
        showError("Lost connection to the launchpad");
    };
}

function checkRocketStatus() {
    // Check rocket lock state
    fetch('/api/get-lock-state')
        .then(response => response.json())
        .then(data => {
            isRocketLocked = data.locked;
            updateLockStatusDisplay();
        })
        .catch(error => {
            console.error("Error checking lock state:", error);
            showError("Failed to check rocket lock state");
        });
    
    // Check rocket readiness
    fetch('/api/get-rocket-readiness')
        .then(response => response.json())
        .then(data => {
            isRocketReady = data.ready;
            updateReadinessDisplay();
        })
        .catch(error => {
            console.error("Error checking rocket readiness:", error);
            showError("Failed to check rocket readiness");
        });
}

function updateLockStatusDisplay() {
    if (isRocketLocked) {
        lockStatusElement.textContent = "Locked";
        lockStatusElement.classList.add('text-green-600');
        lockStatusElement.classList.remove('text-red-600');
    } else {
        lockStatusElement.textContent = "Unlocked";
        lockStatusElement.classList.add('text-red-600');
        lockStatusElement.classList.remove('text-green-600');
    }
}

function updateReadinessDisplay() {
    if (isRocketReady) {
        rocketReadinessElement.textContent = "Ready";
        rocketReadinessElement.classList.add('text-green-600');
        rocketReadinessElement.classList.remove('text-red-600');
    } else {
        rocketReadinessElement.textContent = "Not Ready";
        rocketReadinessElement.classList.add('text-red-600');
        rocketReadinessElement.classList.remove('text-green-600');
    }
}

function showPhase(phase) {
    // Hide all phases
    setupPhase.classList.add('hidden');
    fillingPhase.classList.add('hidden');
    launchPhase.classList.add('hidden');
    postLaunchPhase.classList.add('hidden');
    
    // Show the selected phase
    currentState = phase;
    
    switch (phase) {
        case 'setup':
            setupPhase.classList.remove('hidden');
            abortButton.classList.add('hidden');
            break;
        case 'filling':
            fillingPhase.classList.remove('hidden');
            abortButton.classList.remove('hidden');
            break;
        case 'launch':
            launchPhase.classList.remove('hidden');
            abortButton.classList.remove('hidden');
            break;
        case 'post-launch':
            postLaunchPhase.classList.remove('hidden');
            abortButton.classList.add('hidden');
            break;
    }
}

function startFillingProcess() {
    // Show the filling phase
    showPhase('filling');
    
    // Reset progress indicators
    fillCompleted = false;
    pressurizeCompleted = false;
    currentWaterVolume = 0;
    currentPressure = 0;
    updateProgressBars();
    
    // Lock the rocket
    fetch('/api/lock-rocket')
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                isRocketLocked = true;
                updateLockStatusDisplay();
                
                // Start water filling
                fillingStatusElement.textContent = "Starting water filling process...";
                return fetch(`/api/start-water-filling?volume=${targetWaterVolume}`);
            } else {
                throw new Error("Failed to lock rocket");
            }
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                fillingStatusElement.textContent = "Water filling in progress...";
            } else {
                throw new Error("Failed to start water filling");
            }
        })
        .catch(error => {
            console.error("Error in filling process:", error);
            showError("Error: " + error.message);
            abortProcess();
        });
}

function updateWaterVolumeDisplay(volume) {
    currentWaterVolume = volume;
    waterVolumeCurrentElement.textContent = `${currentWaterVolume} ml`;
    updateProgressBars();
    
    // Check if water filling is complete
    if (currentWaterVolume >= targetWaterVolume && !fillCompleted) {
        fillCompleted = true;
        fillingStatusElement.textContent = "Water filling complete. Starting pressurization...";
        
        // Start pressurization
        fetch(`/api/start-pressurising?pressure=${targetPressure}`)
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    fillingStatusElement.textContent = "Pressurization in progress...";
                } else {
                    throw new Error("Failed to start pressurization");
                }
            })
            .catch(error => {
                console.error("Error in pressurization:", error);
                showError("Error: " + error.message);
                abortProcess();
            });
    }
}

function updatePressureDisplay(pressure) {
    currentPressure = pressure;
    pressureCurrentElement.textContent = `${currentPressure} PSI`;
    currentPressureDisplay.textContent = currentPressure;
    updateProgressBars();
    
    // Check if pressurization is complete
    if (currentPressure >= targetPressure && !pressurizeCompleted && fillCompleted) {
        pressurizeCompleted = true;
        fillingStatusElement.textContent = "Pressurization complete. Ready for launch!";
        
        // Show launch phase after a short delay
        setTimeout(() => {
            showPhase('launch');
        }, 2000);
    }
}

function updateProgressBars() {
    // Update water progress bar
    const waterPercentage = Math.min((currentWaterVolume / targetWaterVolume) * 100, 100);
    waterProgressBar.style.width = `${waterPercentage}%`;
    
    // Update pressure progress bar
    const pressurePercentage = Math.min((currentPressure / targetPressure) * 100, 100);
    pressureProgressBar.style.width = `${pressurePercentage}%`;
}

function startCountdown() {
    countdownValue = 5;
    countdownDisplay.textContent = countdownValue;
    countdownDisplay.classList.remove('hidden');
    launchButton.disabled = true;
    
    countdownInterval = setInterval(() => {
        countdownValue--;
        countdownDisplay.textContent = countdownValue;
        
        if (countdownValue <= 0) {
            clearInterval(countdownInterval);
            launchRocket();
        }
    }, 1000);
}

function launchRocket() {
    // Clear any existing countdown
    if (countdownInterval) {
        clearInterval(countdownInterval);
    }
    
    // Disable launch button
    launchButton.disabled = true;
    
    // Call the launch API
    fetch('/api/release-rocket')
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                isRocketLocked = false;
                updateLockStatusDisplay();
                
                // Show post-launch phase
                showPhase('post-launch');
            } else {
                throw new Error("Failed to launch rocket");
            }
        })
        .catch(error => {
            console.error("Error launching rocket:", error);
            showError("Error: " + error.message);
            // Re-enable launch button
            launchButton.disabled = false;
        });
}

function handleRocketReconnected() {
    waitingReconnection.classList.add('hidden');
    dataTransmission.classList.remove('hidden');
    dataProgress.style.width = "0%";
    
    // Simulate progress (in a real implementation, this would be based on actual transfer progress)
    let progress = 0;
    const progressInterval = setInterval(() => {
        progress += 5;
        dataProgress.style.width = `${progress}%`;
        
        if (progress >= 100) {
            clearInterval(progressInterval);
        }
    }, 300);
}

function handleDataUploadComplete() {
    dataTransmission.classList.add('hidden');
    dataComplete.classList.remove('hidden');
}

function abortProcess() {
    // Call the abort API
    fetch('/api/abort')
        .then(response => response.json())
        .then(data => {
            showPhase('setup');
            
            // Clear any countdown
            if (countdownInterval) {
                clearInterval(countdownInterval);
            }
            
            // Reset checkboxes
            confirmLockedCheckbox.checked = false;
            confirmClearedCheckbox.checked = false;
            enableCountdownCheckbox.checked = false;
            
            // Check rocket status again
            checkRocketStatus();
        })
        .catch(error => {
            console.error("Error aborting process:", error);
            showError("Error during abort: " + error.message);
        });
}

function showError(message) {
    // Simple error handling - in a real app, you'd want a more sophisticated approach
    alert(message);
}