import awsIot from 'aws-iot-device-sdk';
import path from 'path';

const keyPath = path.resolve(process.cwd(), 'certs/d62da59f8ffaa4207ed0e418504b8f0c7fdce38705c6321221d0e92c2d3f5616-private.pem.key');
const certPath = path.resolve(process.cwd(), 'certs/d62da59f8ffaa4207ed0e418504b8f0c7fdce38705c6321221d0e92c2d3f5616-certificate.pem.crt');
const caPath = path.resolve(process.cwd(), 'certs/AmazonRootCA1.pem');

const device = awsIot.device({
  keyPath,
  certPath,
  caPath,
  clientId: 'WebServerThing',
  host: '',
});

// Store messages in memory (or use a database for persistence)
let messages = [];

// Connect to AWS IoT Core
device.on('connect', () => {
    console.log('Connected to AWS IoT Core');

    // Subscribe to the desired topic
    const topic = 'door_status/pub'; // Replace with your MQTT topic
    device.subscribe(topic, (err) => {
        if (err) {
            console.error('Failed to subscribe to topic:', err);
        } else {
            console.log(`Subscribed to topic: ${topic}`);
            console.log("Fetching data from mqtt");
        }
    });
});

// Handle incoming messages
device.on('message', (topic, payload) => {
    console.log(`Received message on topic: ${topic}`);
    console.log('Message:', payload.toString());
    const message = JSON.parse(payload.toString());
    messages.push(message); // Store the message
});

// API route to fetch messages
export default function handler(req, res) {
    if (req.method === 'GET') {
        res.status(200).json(messages); // Return the stored messages
    } else {
        res.status(405).json({ error: 'Method not allowed' });
    }
}