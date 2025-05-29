import { api } from "../modules/api.js";

export const onPageLoad = (args) => {
    if (args && args.timestamp) {
        showFlightData(args.timestamp);
    }
    else {
        showTimestampRequired();
    }
};

async function showFlightData(timestamp) {
    const flightDataContainer = document.querySelector('.bg-white.p-6.rounded-lg.shadow-md');
    if (!flightDataContainer) return;

    // Show loading message
    const date = new Date(parseInt(timestamp));
    const formattedDate = date.toLocaleString();
    flightDataContainer.innerHTML = `
        <h2>Flight Data for ${formattedDate}</h2>
        <p>Loading flight data...</p>
    `;

    try {
        // Fetch flight data using the API
        const flightData = await api.fetchFlightData(timestamp);
        console.log("First data point timestamp from API:", flightData.length > 0 ? flightData[0].timestamp : "No data");

        if (!flightData || !Array.isArray(flightData) || flightData.length < 2) { // Need at least 2 points to calculate velocity
            flightDataContainer.innerHTML = `
                <h2>Flight Data for ${formattedDate}</h2>
                <p>Not enough flight data to calculate velocities for this timestamp.</p>
            `;
            return;
        }

        // Calculate velocities
        const velocities = [];
        let maxAltitude = -Infinity;

        if (flightData.length > 0) {
            maxAltitude = flightData[0].relativeAltitude;
        }

        for (let i = 0; i < flightData.length - 1; i++) {
            const dataPoint1 = flightData[i];
            const dataPoint2 = flightData[i+1];

            if (dataPoint1.relativeAltitude > maxAltitude) {
                maxAltitude = dataPoint1.relativeAltitude;
            }
            if (dataPoint2.relativeAltitude > maxAltitude) {
                maxAltitude = dataPoint2.relativeAltitude;
            }

            const dt = Number(dataPoint2.timestamp) - Number(dataPoint1.timestamp);
            
            if (dt === 0) { // Avoid division by zero
                velocities.push(0); 
            } else {
                const timeDiffSeconds = dt / 1000;
                const velocity = (dataPoint2.relativeAltitude - dataPoint1.relativeAltitude) / timeDiffSeconds;
                velocities.push(velocity);
            }
        }
        // Ensure the last point's altitude is considered for maxAltitude if it's the highest
        if (flightData.length > 0 && flightData[flightData.length -1].relativeAltitude > maxAltitude){
            maxAltitude = flightData[flightData.length -1].relativeAltitude;
        }

        let maxVelocity = 0;
        if (velocities.length > 0) {
            maxVelocity = Math.max(...velocities.map(v => Math.abs(v))); // Consider absolute velocity for max
        }

        // Display max altitude and max velocity
        let flightSummaryHtml = `
            <h2>Flight Data for ${formattedDate}</h2>
            <p><strong>Total data points:</strong> ${flightData.length}</p>
            <h3>Flight Summary:</h3>
            <p><strong>Maximum Altitude:</strong> ${maxAltitude.toFixed(2)} m</p>
            <p><strong>Maximum Velocity:</strong> ${maxVelocity.toFixed(2)} m/s</p>
            <p>${velocities}</p>
        `;

        flightDataContainer.innerHTML = flightSummaryHtml;

    } catch (error) {
        console.error('Error loading or processing flight data:', error);
        flightDataContainer.innerHTML = `
            <h2>Flight Data for ${formattedDate}</h2>
            <p style="color: red;">Error loading or processing flight data: ${error.message}</p>
        `;
    }
}

function showTimestampRequired() {
    const flightDataContainer = document.querySelector('.bg-white.p-6.rounded-lg.shadow-md');
    if (flightDataContainer) {
        flightDataContainer.textContent = 'Timestamp is required to view flight data.';
    }
}