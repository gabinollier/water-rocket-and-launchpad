import { webSocket } from "../modules/websocket.js";
import { api } from "../modules/api.js";

const statesController = {
    launchpadState: "UNKNOWN",
    rocketState: "UNKNOWN",

    async setupStates() {
        statesController.rocketState = await api.fetchRocketState();
        statesController.launchpadState = await api.fetchLaunchpadState();
        statesController.updateStateElements();


    },

    updateStateElements() {
        const rocketStateElement = document.getElementById("rocket-state");
        if (rocketStateElement) {
            rocketStateElement.textContent = statesController.getStateDisplayName(this.rocketState);
            rocketStateElement.className = statesController.getStateCSSClass(this.rocketState);
        }
    
        const launchpadStateElement = document.getElementById("launchpad-state");
        if (launchpadStateElement) {
            launchpadStateElement.textContent = statesController.getStateDisplayName(this.launchpadState);
            launchpadStateElement.className = statesController.getStateCSSClass(this.launchpadState);
        }

        stepOneController.updateButtonsStates();
    },

    getStateCSSClass(state) {
        const stateClasses = {
            "DISCONNECTED": "font-semibold text-red-600",
            "UNKNOWN": "font-semibold text-red-600",
            "ERROR": "font-semibold text-red-600",
            "IDLING": "font-semibold text-green-600",
            "IDLING_CLOSED": "font-semibold text-green-600",
            "IDLING_OPEN": "font-semibold text-orange-600",
        };
        return stateClasses[state] || "font-semibold text-gray-700";
    },

    getStateDisplayName(state) {
        if (state === "WAITING_FOR_LAUNCH")
            return "Waiting For Launch (Disconnected)";
        return state.charAt(0).toUpperCase() + state.slice(1).toLowerCase().replace(/_/g, ' ');
    },
};

const stepOneController = {    
    setupStartFillingButton() {
        const startFillingButton = document.getElementById('start-filling-button');
        if (!startFillingButton) return;
    
        startFillingButton.addEventListener('click', async () => {
            const waterVolume = document.getElementById('water-volume').value;
            const pressure = document.getElementById('pressure').value;
    
            if (statesController.launchpadState !== 'IDLING') {
                alert("Le pas de tir n'est pas prêt. Merci de vérifier l'état du pas de tir.");
                return;
            }
    
            // Check if rocket fairing is closed
            if (statesController.rocketState === 'IDLING_OPEN') {
                if (!confirm("ATTENTION : La coiffe de la fusée n'est pas fermée. En cas de lancement, elle ne pourra pas déclencher son parachute. Êtes vous vraiment sûrs de vouloir continuer ?")) {
                    return;
                }
            }

            else if (statesController.rocketState !== 'IDLING_CLOSED') {
                if (!confirm("ATTENTION : La fusée semble déconnectée. En cas de lancement, elle ne pourra pas déclencher son parachute. Êtes vous vraiment sûrs de vouloir continuer ?")) {
                    return;
                }
            }
    
            api.startFilling(waterVolume, pressure).then((response) => {
                if (response.ok) {
                    console.log("Filling started successfully.");
                } else {
                    console.error("Failed to start filling:", response.statusText);
                }
            }).catch((error) => {
                console.error("Error starting filling:", error);
            });
        });
    },
    
    updateSliderValue(id, value) {
        const slider = document.getElementById(`${id}-slider`);
        const input = document.getElementById(id);
        if (slider && input) {
            slider.value = value;
            input.value = parseFloat(value.toFixed(2));
            this.updateSliderProgress(slider);
        }
    },
    
    updateSliderProgress(slider) {
        const progress = slider.parentElement?.querySelector('.bg-gradient-to-r');
        if (progress) {
            const percentage = (slider.value - slider.min) / (slider.max - slider.min) * 100;
            progress.style.width = `${percentage}%`;
        }
    },
    
    async setupSlider(id, initialValue) { // Made this async to await initial values
        const slider = document.getElementById(`${id}-slider`);
        const input = document.getElementById(id);
        if (slider && input) {
            let value = initialValue;
            if (value === null || isNaN(value)) {
                // Fetch initial values from the server if not provided
                if (id === 'water-volume') {
                    const settings = await api.fetchLaunchpadSettings();
                    value = settings && settings.water_volume !== undefined ? settings.water_volume : parseFloat(slider.value);
                } else if (id === 'pressure') {
                    const settings = await api.fetchLaunchpadSettings();
                    value = settings && settings.pressure !== undefined ? settings.pressure : parseFloat(slider.value);
                }
                else {
                    value = parseFloat(slider.value); // fallback to default slider default
                }
            }
            slider.value = value;
            input.value = parseFloat(value.toFixed(2));
            this.updateSliderProgress(slider);

            slider.addEventListener('input', (e) => {
                const value = parseFloat(e.target.value);
                input.value = value;
                this.updateSliderProgress(slider);
            });

            input.addEventListener('change', (e) => {
                let value = parseFloat(e.target.value);
                value = Math.min(Math.max(value, parseFloat(slider.min)), parseFloat(slider.max));
                input.value = value;
                slider.value = value;
                this.updateSliderProgress(slider);
            });
        }
    },

    updateButtonsStates() {
        const waterVolumeInput = document.getElementById('water-volume');
        const waterProgressBar = document.getElementById('water-progress-bar');
        const waterVolumeSlider = document.getElementById('water-volume-slider');
        
        const pressureInput = document.getElementById('pressure');
        const pressureProgressBar = document.getElementById('pressure-progress-bar');
        const pressureSlider = document.getElementById('pressure-slider');
        
        const startFillingButton = document.getElementById('start-filling-button');

        const isLaunchpadIdling = statesController.launchpadState === 'IDLING';

        // Gérer l'activation/désactivation des champs de formulaire et des sliders
        const formControls = [waterVolumeInput, waterVolumeSlider, pressureInput, pressureSlider];
        formControls.forEach(element => {
            if (element) {
                element.disabled = !isLaunchpadIdling;
                if (!isLaunchpadIdling) { // When disabled
                    element.classList.add('cursor-not-allowed');
                    if (element.type === 'range') { // Sliders
                        element.classList.add('opacity-30'); // Opacity for sliders
                        element.classList.remove('cursor-pointer');
                    } else { // Input fields (water-volume, pressure)
                        element.classList.remove('opacity-30'); // No general opacity for input fields
                        if (element.id === 'water-volume') {
                            element.classList.remove('focus:border-blue-700', 'border-blue-300');
                            element.classList.add('border-gray-300'); // Use gray border for disabled
                        } else if (element.id === 'pressure') {
                            element.classList.remove('focus:border-red-700', 'border-red-300');
                            element.classList.add('border-gray-300'); // Use gray border for disabled
                        }
                    }
                } else { // When enabled (isLaunchpadIdling is true)
                    element.classList.remove('cursor-not-allowed');
                    if (element.type === 'range') { // Sliders
                        element.classList.remove('opacity-30');
                        element.classList.add('cursor-pointer');
                    } else { // Input fields
                        element.classList.remove('opacity-30'); 
                        if (element.id === 'water-volume') {
                            element.classList.remove('border-gray-300');
                            element.classList.add('focus:border-blue-700', 'border-blue-300');
                        } else if (element.id === 'pressure') {
                            element.classList.remove('border-gray-300');
                            element.classList.add('focus:border-red-700', 'border-red-300');
                        }
                    }
                }
            }
        });

        // Gérer l'effet de pulsation pour les barres de progression et les sliders
        const elementsForPulseEffect = [waterProgressBar, waterVolumeSlider, pressureProgressBar, pressureSlider];
        elementsForPulseEffect.forEach(element => {
            if (element) {
                if (isLaunchpadIdling) {
                    element.classList.remove('animate-pulse');
                } else {
                    element.classList.add('animate-pulse');
                }
            }
        });

        // Logique pour le bouton de remplissage (startFillingButton)
        if (startFillingButton) {
            // Mettre à jour le texte du bouton en fonction de l'état du pas de tir
            if (statesController.launchpadState === 'WATER_FILLING') {
                startFillingButton.textContent = "Remplissage de l'eau...";
            } else if (statesController.launchpadState === 'PRESSURIZING') {
                startFillingButton.textContent = "Pressurisation...";
            } else if (statesController.launchpadState === 'READY_FOR_LAUNCH') {
                startFillingButton.textContent = "Remplissage terminé";
            } else {
                startFillingButton.textContent = "Commencer le remplissage";
            }

            startFillingButton.disabled = !isLaunchpadIdling; // Utilisation de la variable renommée
            if (isLaunchpadIdling) {
                startFillingButton.classList.remove('opacity-30', 'cursor-not-allowed');
                startFillingButton.classList.add('hover:bg-blue-700');
            } else {
                startFillingButton.classList.add('opacity-30', 'cursor-not-allowed');
                startFillingButton.classList.remove('hover:bg-blue-700');
            }
        }
        
        const launchButton = document.getElementById('launch-button');

        if (launchButton) {
            const canLaunch = statesController.launchpadState === 'READY_FOR_LAUNCH';
            launchButton.disabled = !canLaunch;
            if (canLaunch) {
                launchButton.classList.remove('opacity-30', 'cursor-not-allowed');
                launchButton.classList.add('hover:bg-red-700');
            } else {
                launchButton.classList.add('opacity-30', 'cursor-not-allowed');
                launchButton.classList.remove('hover:bg-red-700');
            }
        }

        // Handle fairing buttons visibility and state
        const openFairingButton = document.getElementById('open-fairing-button');
        const closeFairingButton = document.getElementById('close-fairing-button');

        if (openFairingButton) {
            // Show open fairing button only when rocket is IDLING_CLOSED and launchpad is IDLING
            const shouldShowOpen = statesController.rocketState === 'IDLING_CLOSED' && statesController.launchpadState === 'IDLING';
            openFairingButton.style.display = shouldShowOpen ? 'block' : 'none';
            openFairingButton.disabled = statesController.launchpadState !== 'IDLING';
            
            if (shouldShowOpen) {
                openFairingButton.disabled = false;
                openFairingButton.classList.remove('opacity-30', 'cursor-not-allowed');
            }
        }

        if (closeFairingButton) {
            // Show close fairing button only when rocket is IDLING_OPEN and launchpad is IDLING
            const shouldShowClose = statesController.rocketState === 'IDLING_OPEN';
            closeFairingButton.style.display = shouldShowClose ? 'block' : 'none';
            closeFairingButton.disabled = statesController.launchpadState !== 'IDLING';
            
            if (shouldShowClose) {
                closeFairingButton.disabled = false;
                closeFairingButton.classList.remove('opacity-30', 'cursor-not-allowed');
            }
        }

        // Handle skip buttons visibility
        const skipWaterButton = document.getElementById('skip-water-button');
        const skipPressureButton = document.getElementById('skip-pressure-button');

        if (skipWaterButton) {
            skipWaterButton.style.display = statesController.launchpadState === 'WATER_FILLING' ? 'block' : 'none';
        }

        if (skipPressureButton) {
            skipPressureButton.style.display = statesController.launchpadState === 'PRESSURIZING' ? 'block' : 'none';
        }
    },
    setupLaunchButton() {
        const launchButton = document.getElementById('launch-button');
        if (!launchButton)
            return;
    
        launchButton.addEventListener('click', () => {
            api.launch().then((response) => {
                if (response.ok) {
                    statesController.rocketState = "DISCONNECTED"; 
                    statesController.updateStateElements();
                } else {
                    console.error("Failed to send launch command:", response.statusText);
                }
            }).catch((error) => {
                console.error("Error sending launch command:", error);
            });
        });
    },    
    setupAbortButton() {
        const abortButton = document.getElementById('abort-button');
        if (!abortButton) return;
    
        abortButton.addEventListener('click', () => {
            api.abort().then((response) => {
                if (response.ok) {
                    console.log("Aborted.");
                } else {
                    console.error("Failed to abort:", response.statusText);
                    alert("Failed to abort: " + response.statusText);
                }
            }).catch((error) => {
                console.error("Error aborting:", error);
                alert("Error aborting: " + error);
            })
        });
    },

    setupFairingButtons() {
        const openFairingButton = document.getElementById('open-fairing-button');
        const closeFairingButton = document.getElementById('close-fairing-button');

        if (openFairingButton) {
            openFairingButton.addEventListener('click', async () => {
                try {
                    const response = await api.openFairing();
                    if (!response.ok) {
                        const errorData = await response.json().catch(() => ({ message: response.statusText }));
                        console.error("Failed to open fairing:", errorData.message);
                        alert(`Error opening fairing: ${errorData.message}`);
                    } else {
                        console.log("Fairing opened successfully.");
                        statesController.rocketState = await api.fetchRocketState();
                        statesController.updateStateElements();
                    }
                } catch (error) {
                    console.error("Error calling open-fairing API:", error);
                    alert("An error occurred while trying to open the fairing.");
                }
            });
        }

        if (closeFairingButton) {
            closeFairingButton.addEventListener('click', async () => {
                try {
                    const response = await api.closeFairing();
                    if (!response.ok) {
                        const errorData = await response.json().catch(() => ({ message: response.statusText }));
                        console.error("Failed to close fairing:", errorData.message);
                        alert(`Error closing fairing: ${errorData.message}`);
                    } else {
                        console.log("Fairing closed successfully.");
                        statesController.rocketState = await api.fetchRocketState();
                        statesController.updateStateElements();
                    }
                } catch (error) {
                    console.error("Error calling close-fairing API:", error);
                    alert("An error occurred while trying to close the fairing.");
                }
            });
        }
    },

    setupSkipButtons() {
        const skipWaterButton = document.getElementById('skip-water-button');
        if (skipWaterButton) {
            skipWaterButton.addEventListener('click', async () => {
                await api.skipWaterFilling();
            });
        }

        const skipPressureButton = document.getElementById('skip-pressure-button');
        if (skipPressureButton) {
            skipPressureButton.addEventListener('click', async () => {
                await api.skipPressurizing();
            });
        }
    }
};

function setupWebSocketListeners() {
    webSocket.onmessage = (event) => {
        const data = JSON.parse(event.data);

        if (data.type === "new-rocket-state") {
            statesController.rocketState = data["rocket-state"];
            statesController.updateStateElements();
        }
        else if (data.type === "new-launchpad-state") {
            statesController.launchpadState = data["launchpad-state"];
            statesController.updateStateElements();
        }
        else if (data.type === "filling") {
            stepOneController.updateSliderValue('pressure', data["pressure"]);
            stepOneController.updateSliderValue('water-volume', data["water-volume"]);
        }
        else if (data.type === "new-data-available") {
            const date = new Date(data.launchtime);
            const hours = date.getHours().toString().padStart(2, '0');
            const minutes = date.getMinutes().toString().padStart(2, '0');
            
            document.getElementById('flight-time').textContent = `${hours}:${minutes}`;
            document.getElementById('max-altitude').textContent = data.maxRelativeAltitude.toFixed(1);
            document.getElementById('new-data-notification').classList.remove('hidden');
        }
    };
}

export const onPageLoad = async () => {
    await statesController.setupStates();

    // Fetch initial pressure and water volume for UI initialization
    const pressure = await api.fetchPressure();
    const waterVolume = await api.fetchWaterVolume();

    // Show the launch procedure section and hide loading indicator
    const loadingDiv = document.getElementById('loading-launch-procedure');
    const launchProcedureSection = document.getElementById('launch-procedure-section');
    if (loadingDiv && launchProcedureSection) {
        loadingDiv.style.display = 'none';
        launchProcedureSection.style.display = 'block';
    }

    stepOneController.setupSlider('water-volume', waterVolume);
    stepOneController.setupSlider('pressure', pressure);
    stepOneController.setupStartFillingButton();
    stepOneController.setupLaunchButton();
    stepOneController.setupAbortButton();
    stepOneController.setupFairingButtons();
    stepOneController.setupSkipButtons();

    setupWebSocketListeners();
};