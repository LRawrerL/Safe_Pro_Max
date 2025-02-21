import { getDynamoDBData } from '../../lib/dynamodb';

export default async function handler(req, res) {
    console.log('API Route Called for dyanmodb'); // Log when the API route is called

    try {
        const data = await getDynamoDBData(); // Fetch data from DynamoDB
        // console.log('Raw DynamoDB Data:', data); // Log the raw data

        // Process the data to extract fields from the payload
        const processedData = data.map(item => ({
            time: item.time || 'N/A',
            temperature: item.payload?.temperature !== undefined ? item.payload.temperature : 'N/A',
            humidity: item.payload?.humidity !== undefined ? item.payload.humidity : 'N/A',
            fullness: item.payload?.fullness || 'N/A', // Directly map the string value
        }));

        //console.log('Processed Data:', processedData); // Log the processed data
        res.status(200).json(processedData); // Send the processed data
    } catch (err) {
        console.error("Error fetching data from DynamoDB:", err);
        res.status(500).json({ error: 'Failed to fetch data', details: err.message });
    }
}