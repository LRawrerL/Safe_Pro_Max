import { useState, useEffect  } from 'react';
import { useRouter } from 'next/router';

export default function Login() {
    const router = useRouter();
    const [isLogin, setIsLogin] = useState(true); // Toggle between login and registration
    const [username, setUsername] = useState('');
    const [password, setPassword] = useState('');
    const [error, setError] = useState('');
    
    // const fetchCredentials = async () => {
    //     const username = 'c7ad44cbad762a5da0a452f9e854fdc1e0e7a52a38015f23f3eab1d80b931dd472634dfac71cd34ebc35d16ab7fb8a90c81f975113d6c7538dc69dd8de9077ec'; // Replace with actual username
    //     const response = await fetch('/api/credentials', {
    //         method: 'POST',
    //         headers: {
    //             'Content-Type': 'application/json',
    //         },
    //         body: JSON.stringify({ username }),
    //     });
    
    //     const result = await response.json();
    //     console.log('Fetched credentials:', result);
    //     return result;
    // };
    
    // useEffect(() => {
    //       console.log('testing login');
    //       fetchCredentials();
    //     }, []);

    const handleSubmit = async (e) => {
        e.preventDefault();

        // Basic validation
        if (!username || !password) {
            setError('Please fill in all fields');
            return;
        }

        console.log('Submitting form:', { username, password }); // Log form submission

        try {
            const endpoint = isLogin ? '/api/login' : '/api/register';
            console.log('Calling endpoint:', endpoint); // Log the endpoint

            const response = await fetch(endpoint, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ username, password }),
            });

            console.log('Response status:', response.status); // Log the response status

            const result = await response.json();
            console.log('Response data:', result); // Log the response data

            if (response.ok) {
                // Save the token in local storage (or cookies)
                localStorage.setItem('token', result.token);
                console.log('Token saved:', result.token); // Log the token

                // Redirect to the index page
                console.log('Redirecting to index page');
                router.push('/');
            } else {
                setError(result.message || 'An error occurred');
                console.log('Error:', result.message); // Log the error
            }
        } catch (err) {
            setError('An error occurred. Please try again.');
            console.error('Error:', err); // Log the error
        }
    };

    return (
        <div style={{ maxWidth: '400px', margin: '0 auto', padding: '20px' }}>
            <h1>{isLogin ? 'Login' : 'Register'}</h1>
            <form onSubmit={handleSubmit}>
                <div>
                    <label>Username:</label>
                    <input
                        type="text"
                        value={username}
                        onChange={(e) => setUsername(e.target.value)}
                        required
                    />
                </div>
                <div>
                    <label>Password:</label>
                    <input
                        type="password"
                        value={password}
                        onChange={(e) => setPassword(e.target.value)}
                        required
                    />
                </div>
                {error && <p style={{ color: 'red' }}>{error}</p>}
                <button type="submit">{isLogin ? 'Login' : 'Register'}</button>
            </form>
            {/* <p>
                {isLogin ? "Don't have an account? " : 'Already have an account? '}
                <button onClick={() => setIsLogin(!isLogin)}>
                    {isLogin ? 'Register' : 'Login'}
                </button>
            </p> */}
        </div>
    );
}