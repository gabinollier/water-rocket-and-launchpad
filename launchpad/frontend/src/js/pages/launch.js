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
            "IDLING": "font-semibold text-green-600"
        };
        return stateClasses[state] || "font-semibold text-gray-700";
    },

    getStateDisplayName(state) {
        if (state === "ERROR") 
            return "Rocket error";
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
    
            if (statesController.rocketState !== 'IDLING') {
                if (!confirm("La fusée n'est pas connectée ou bien n'est pas prête. En cas de lancement, elle ne pourra pas déclencher son parachute. Êtes vous vraiment sûrs de vouloir continuer ?")) {
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
    
    setupSlider(id, initialValue) {
        const slider = document.getElementById(`${id}-slider`);
        const input = document.getElementById(id);
        if (slider && input) {
            let value = initialValue;
            if (value === null || isNaN(value)) {
                value = parseFloat(slider.value); // fallback to default slider default
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
        const waterProgressBar = document.getElementById('water-progress-bar');
        const waterVolumeSlider = document.getElementById('water-volume-slider');
        const pressureProgressBar = document.getElementById('pressure-progress-bar');
        const pressureSlider = document.getElementById('pressure-slider');
        const startFillingButton = document.getElementById('start-filling-button');

        let fillingButtonEnabled = statesController.launchpadState === 'IDLING';

        [waterProgressBar, waterVolumeSlider, pressureProgressBar, pressureSlider].forEach(element => {
            if (element) {
                element.disabled = !fillingButtonEnabled;
                if (fillingButtonEnabled) {
                    element.classList.remove('animate-pulse');
                } else {
                    element.classList.add('animate-pulse');
                }
            }
        });

        if (startFillingButton) {
            // Update button text based on launchpad state
            if (statesController.launchpadState === 'WATER_FILLING')
            {
                startFillingButton.textContent = "Remplissage de l'eau...";
            }
            else if (statesController.launchpadState === 'PRESSURIZING') 
            {
                startFillingButton.textContent = "Pressurisation...";
            }
            else if (statesController.launchpadState === 'READY_FOR_LAUNCH') 
            {
                startFillingButton.textContent = "Remplissage terminé";
            } 
            else 
            {
                startFillingButton.textContent = "Commencer le remplissage";
            }

            startFillingButton.disabled = !fillingButtonEnabled;
            if (fillingButtonEnabled) {
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
    },

    setupLaunchButton() {
        const launchButton = document.getElementById('launch-button');
        if (!launchButton) return;
    
        launchButton.addEventListener('click', () => {
            if (statesController.launchpadState !== 'READY_FOR_LAUNCH') {
                alert("Le pas de tir n'est pas prêt pour le lancement. Merci de vérifier l'état du pas de tir.");
                return;
            }
    
            if (statesController.rocketState !== 'WAITING_FOR_LAUNCH') {
                if (!confirm("ATTENTION : La fusée n'est pas connectée ou bien n'est pas prête. En cas de lancement, elle ne pourra pas déclencher son parachute. Êtes vous vraiment sûrs de vouloir continuer ?")) {
                    return;
                }
            }
    
            api.launch().then((response) => {
                if (response.ok) {
                    console.log("Launched.");
                } else {
                    console.error("Failed to launch:", response.statusText);
                }
            }).catch((error) => {
                console.error("Error launching:", error);
            })
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
    stepOneController.setupSlider('water-volume', waterVolume);
    stepOneController.setupSlider('pressure', pressure);
    stepOneController.setupStartFillingButton();
    stepOneController.setupLaunchButton();
    stepOneController.setupAbortButton();

    setupWebSocketListeners();
};