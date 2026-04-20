const mqtt = require('mqtt');

// Connettiti al broker locale (Mosquitto sul tuo stesso PC)
const client = mqtt.connect('mqtt://127.0.0.1'); // Usa localhost

client.on('connect', () => {
    console.log('✅ Edge Server connesso al Broker MQTT locale');
    client.subscribe('esp32/average'); // Ascolta i dati dall'ESP32
});

client.on('message', (topic, message) => {
    if (topic === 'esp32/average') {
        const payload = message.toString();
        console.log(`[${new Date().toISOString()}] Ricevuto dato: ${payload}`);
        
        // RISPOSTA ISTANTANEA (PING-PONG)
        // Mandiamo un segnale di "ricevuto" sull'altro topic
        client.publish('server/ack', 'ACK');
        console.log(` ↳ Inviato ACK di conferma all'ESP32`);
    }
});