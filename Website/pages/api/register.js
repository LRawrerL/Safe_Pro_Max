export default function handler(req, res) {
    if (req.method === 'POST') {
        const { username, password } = req.body;

        console.log('Registration request:', { username, password }); // Log the request

        // Replace this with your actual registration logic
        if (username && password) {
            // Generate a token (for simplicity, use a dummy token)
            const token = 'dummy-token';

            console.log('Registration successful:', { token }); // Log the token
            res.status(200).json({ token });
        } else {
            console.log('Invalid input:', { username, password }); // Log invalid input
            res.status(400).json({ message: 'Invalid input' });
        }
    } else {
        console.log('Method not allowed:', req.method); // Log invalid method
        res.status(405).json({ message: 'Method not allowed' });
    }
}