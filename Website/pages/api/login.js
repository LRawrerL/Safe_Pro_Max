import crypto from 'crypto';

// Precomputed SHA-512 hashes for 'admin' and 'password'
const storedHashedUsername = 'c7ad44cbad762a5da0a452f9e854fdc1e0e7a52a38015f23f3eab1d80b931dd472634dfac71cd34ebc35d16ab7fb8a90c81f975113d6c7538dc69dd8de9077ec';
const storedHashedPassword = 'b109f3bbbc244eb82441917ed06d618b9008dd09b3befd1b5e07394c706a8bb980b1d7785e5976ec049b46df5f1326af5a2ea6d103fd07c95385ffab0cacbc86';

// Helper function to hash a string using SHA-512
function hashSHA512(input) {
    return crypto.createHash('sha512').update(input).digest('hex');
}

export default function handler(req, res) {
    if (req.method === 'POST') {
        const { username, password } = req.body;

        try {
            // Hash the incoming username and password using SHA-512
            const incomingHashedUsername = hashSHA512(username);
            const incomingHashedPassword = hashSHA512(password);

            // Compare the hashed values with the stored hashes
            if (incomingHashedUsername === storedHashedUsername && incomingHashedPassword === storedHashedPassword) {
                // Generate a token (for simplicity, use a dummy token)
                const token = 'dummy-token';
                res.status(200).json({ token });
            } else {
                res.status(401).json({ message: 'Invalid credentials' });
            }
        } catch (error) {
            res.status(500).json({ message: 'Internal server error' });
        }
    } else {
        res.status(405).json({ message: 'Method not allowed' });
    }
}   