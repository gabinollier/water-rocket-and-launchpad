import { webSocket } from "../websocket.js";

let logs = [];

function updateUI() {
    const logsElement = document.getElementById("logs");
    logsElement.innerHTML = logs
        .map(log => {
            const newlineCount = (log.message.match(/\n/g) || []).length;
            
            const brTags = newlineCount > 0 ? '<br>'.repeat(newlineCount) : '';
            
            return `<div>${brTags}<strong>[${formatTimestamp(log.timestamp)}]</strong> ${log.message}</div>`;
        })
        .join("");
}

function formatTimestamp(ms) {
    let minutes = Math.floor(ms / 60000);
    let seconds = Math.floor((ms % 60000) / 1000);
    let milliseconds = ms % 1000;
    return `${minutes}m ${seconds}s ${milliseconds}ms`;
}

async function fetchLogs()
{
    if (!document.getElementById("logs")) return; 

    const response = await fetch("/api/getlogs");
    logs = await response.json(); 

    updateUI();
}

function listenWebSocket()
{
    webSocket.onmessage = (event) => {
        const wsMessage = JSON.parse(event.data);
    
        if (wsMessage.type == 'log') {
            const { timestamp, message } = wsMessage;
            logs.push({ timestamp, message }); 
            updateUI(); 
        }
    };
}

fetchLogs();
listenWebSocket();

export const onPageLoad = () => {
    updateUI();
};