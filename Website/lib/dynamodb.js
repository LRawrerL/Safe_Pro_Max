const AWS = require('aws-sdk');
AWS.config.update({ region: 'ap-southeast-1' });
const dynamoDB = new AWS.DynamoDB.DocumentClient();

export async function getDynamoDBData() {
    const params = {
        TableName: '',
    };

    try {
        const data = await dynamoDB.scan(params).promise();
        //console.log("DynamoDB Response:", data.Items); // Debugging log
        return data.Items || []; // Return an empty array if Items is undefined
    } catch (err) {
        console.error("Error fetching data from DynamoDB:", err);
        throw err;
    }
}

export async function getCredentials(usernameHash) {
    const params = {
        TableName: 'credentials',
        FilterExpression: '#usr = :usernameHash', // Use a placeholder for the reserved keyword
        ExpressionAttributeNames: {
            '#usr': 'User', // Map the placeholder to the actual attribute name
        },
        ExpressionAttributeValues: {
            ':usernameHash': usernameHash, // Use the provided hash
        },
    };

    try {
        const data = await dynamoDB.scan(params).promise();
        //console.log("DynamoDB Response:", data); // Debugging log
        return data.Items || []; // Return an empty array if Items is undefined
    } catch (err) {
        console.error("Error fetching data from DynamoDB in lib:", err);
        throw err;
    }
}



