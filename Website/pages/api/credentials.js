import { getCredentials } from '../../lib/dynamodb';

export default async function handler(req, res) {
    console.log('API Route Called for DynamoDB, fetching credentials');

    // Extract username from request body
    const { username } = req.body;

    // Check if username is provided
    if (!username) {
        return res.status(400).json({ error: "Username is required" });
    }

    console.log('Fetching credentials for username:', username);

    try {
        const data = await getCredentials(username); // Fetch data from DynamoDB using the username
        ///console.log('Credentials:', data); // Log the raw data

        res.status(200).json(data); // Send the retrieved credentials
    } catch (err) {
        console.error("Error fetching data from DynamoDB API:", err);
        res.status(500).json({ error: 'Failed to fetch data', details: err.message });
    }
}
