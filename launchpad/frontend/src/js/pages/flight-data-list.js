import { api } from '../modules/api.js';
import { route } from '../framework/router.js';

let timestamps = [];

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

        timestamps = await api.fetchAllFlightTimestamps();

        if (timestamps.length === 0) {
            // No flights found
            loadingElement.classList.add('hidden');
            noFlightsElement.classList.remove('hidden');
            return;
        }
        
        timestamps.sort((a, b) => b.timestamp - a.timestamp);
        
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
    
    timestamps.forEach((timestamp, index) => {
        const flightElement = createFlightElement(timestamp, index);
        flightsListElement.appendChild(flightElement);
    });
}

function createFlightElement(timestamp, index) {
    const a = document.createElement('a');
    a.href = `/flight-data?timestamp=${timestamp}`;
    a.addEventListener('click', (event) => {
        event.preventDefault();
        event.stopPropagation();
        
        window.history.pushState({}, "", `/flight-data?timestamp=${timestamp}`);
        
        // Trigger the router's updateContent function
        window.dispatchEvent(new PopStateEvent('popstate'));
    });
    
    const formattedDate = formatTimestamp(timestamp);
    
    a.className = 'bg-white border border-gray-200 rounded-lg p-4 hover:bg-sky-100 cursor-pointer transition-colors block';
    a.innerHTML = `
        <div class="flex justify-between items-center">
            <div>
                <h3 class="text-lg font-semibold text-gray-800">Vol #${index + 1}</h3>
                <p class="text-gray-600">${formattedDate}</p>
            </div>
            <div class="text-right">
                <div class="text-gray-800">
                    <svg class="w-6 h-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 5l7 7-7 7"></path>
                    </svg>
                </div>
            </div>
        </div>
    `;
    
    return a;
}

function formatTimestamp(timestamp) {
    const date = new Date(parseInt(timestamp));
    return date.toLocaleString();
}