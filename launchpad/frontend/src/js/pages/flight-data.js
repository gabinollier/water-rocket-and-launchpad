import { api } from '../modules/api.js';

let flights = [];

export const onPageLoad = async () => {
    await loadFlights();
};

async function loadFlights() {

    const loadingElement = document.getElementById('loading');
    const noFlightsElement = document.getElementById('no-flights');
    const flightsListElement = document.getElementById('flights-list');
    
    try {
        // Show loading state
        loadingElement.classList.remove('hidden');
        noFlightsElement.classList.add('hidden');
        flightsListElement.classList.add('hidden');
        
        // Fetch flight timestamps
        const timestamps = await api.fetchAllFlightTimestamps();

        if (timestamps.length === 0) {
            // No flights found
            loadingElement.classList.add('hidden');
            noFlightsElement.classList.remove('hidden');
            return;
        }
        
        // Fetch flight data for each timestamp
        flights = [];
        for (const timestamp of timestamps) {
            try {
                const flightData = await api.fetchFlightData(timestamp);
                if (flightData) {
                    flights.push({
                        timestamp: timestamp,
                        data: flightData
                    });
                }
            } catch (error) {
                console.error(`Error loading flight data for timestamp ${timestamp}:`, error);
            }
        }
        
        // Sort flights by timestamp (newest first)
        flights.sort((a, b) => b.timestamp - a.timestamp);
        
        // Display flights
        displayFlights();
        
        // Hide loading, show flights list
        loadingElement.classList.add('hidden');
        flightsListElement.classList.remove('hidden');
        
    } catch (error) {
        console.error('Error loading flights:', error);
        loadingElement.classList.add('hidden');
        noFlightsElement.classList.remove('hidden');
    }
}

function displayFlights() {
    const flightsListElement = document.getElementById('flights-list');
    flightsListElement.innerHTML = '';
    
    flights.forEach((flight, index) => {
        const flightElement = createFlightElement(flight, index);
        flightsListElement.appendChild(flightElement);
    });
}

function createFlightElement(flight, index) {
    const div = document.createElement('div');
    div.className = 'bg-white border border-gray-200 rounded-lg p-4 hover:bg-gray-100 cursor-pointer transition-colors';
    div.onclick = () => onFlightClick(flight, index);
    
    const formattedDate = formatTimestamp(flight.timestamp);
    
    div.innerHTML = `
        <div class="flex justify-between items-center">
            <div>
                <h3 class="text-lg font-semibold text-gray-800">Vol #${index + 1}</h3>
                <p class="text-gray-600">${formattedDate}</p>
            </div>
            <div class="text-right">
                <div class="text-blue-600">
                    <svg class="w-6 h-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 5l7 7-7 7"></path>
                    </svg>
                </div>
            </div>
        </div>
    `;
    
    return div;
}

function formatTimestamp(timestamp) {
    const date = new Date(timestamp);
    const options = {
        year: 'numeric',
        month: 'long',
        day: 'numeric',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
    };
    return date.toLocaleDateString('fr-FR', options);
}

function onFlightClick(flight, index) {
    console.log(`Flight #${index + 1} clicked:`, flight);
    // TODO: Implement detailed flight view with graphs
    // This will be implemented later as requested
    alert(`Vol #${index + 1} sélectionné. Affichage détaillé à venir...`);
}