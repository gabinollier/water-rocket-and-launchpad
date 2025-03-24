export const webSocket = new WebSocket(`ws://${window.location.hostname}:81`);

webSocket.onerror = (error) => {
    console.error('Erreur WebSocket:', error);
};