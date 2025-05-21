const ws = new WebSocket(`ws://${window.location.hostname}:81`);

ws.onopen = () => {
    console.log('WebSocket connection established');
};

ws.onerror = (error) => {
    console.error('WebSocket error:', error);
};

ws.onclose = () => {
    console.log('WebSocket connection closed');
};

export const webSocket = ws;