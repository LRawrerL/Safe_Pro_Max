import '../styles/globals.css'
import { useRouter } from 'next/router';
import { useEffect } from 'react';

function MyApp({ Component, pageProps }) {
    const router = useRouter();

    useEffect(() => {
        // Check if the user is logged in
        const token = localStorage.getItem('token');
        if (!token && router.pathname !== '/login') {
            // Redirect to the login page if not logged in
            router.push('/login');
        }
    }, [router]);

    return <Component {...pageProps} />;
}

export default MyApp;
