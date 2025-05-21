import { webSocket } from "../modules/websocket.js";

let logs = [];

fetchLogs();
listenWebSocket();

export const onPageLoad = () => {
    updateUI();
    setupServoControls();
};

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

    const response = await fetch("/api/get-logs");
    logs = await response.json(); 

    updateUI();
}

function listenWebSocket()
{
    webSocket.onmessage = (event) => {
        const wsMessage = JSON.parse(event.data);
    
        if (wsMessage.type == 'new-log') {
            const { timestamp, message } = wsMessage;
            logs.push({ timestamp, message }); 
            updateUI(); 
        }
    };
}

function setupServoControls() {
    document.getElementById("rotate-servo-plus-2")?.addEventListener("click", () => rotateServo(2));
    document.getElementById("rotate-servo-minus-2")?.addEventListener("click", () => rotateServo(-2));

    document.getElementById("rotate-servo-plus-120")?.addEventListener("click", () => {
        if (confirm("ATTENTION : Êtes-vous sûr de vouloir faire tourner le servo de 120° ? Si la fusée est pressurisée, elle va décoller"))
        {
            rotateServo(120);
        }
    });
    document.getElementById("rotate-servo-minus-120")?.addEventListener("click", () => {
        if (confirm("ATTENTION : Êtes-vous sûr de vouloir faire tourner le servo de -120° ? Si la fusée est pressurisée, elle va décoller"))
        {
            rotateServo(-120);
        }
    });

}

async function rotateServo(degrees) {
    const turns = degrees / 360;
    try {
        const response = await fetch("/api/rotate-servo", {
            method: "POST",
            headers: {
                "Content-Type": "application/x-www-form-urlencoded", 
            },
            body: `turns=${turns}`, 
        });

        if (!response.ok) {
            const errorData = await response.json();
            console.error("Failed to rotate servo:", errorData.message);
            alert(`Error rotating servo: ${errorData.message}`);
        } else {
            console.log(`Servo rotated by ${turns} turns.`);
        }
    } catch (error) {
        console.error("Error calling rotate-servo API:", error);
        alert("An error occurred while trying to rotate the servo.");
    }
}