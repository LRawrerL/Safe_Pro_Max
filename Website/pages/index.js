import { use, useEffect, useState } from 'react';
import { Line } from 'react-chartjs-2';
import { useRouter } from 'next/router';
import {
    Chart as ChartJS,
    CategoryScale,
    LinearScale,
    PointElement,
    LineElement,
    Title,
    Tooltip,
    Legend,
} from 'chart.js';


// Register Chart.js components
ChartJS.register(
    CategoryScale,
    LinearScale,
    PointElement,
    LineElement,
    Title,
    Tooltip,
    Legend
);

export default function Home() {
    const [data, setData] = useState([]);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState(null);
    const [mqttMessages, setMqttMessages] = useState([]); // State for MQTT messages

    // Function to format time to a readable format (e.g., "10:17:46 AM")
    const formatTime = (isoString) => {
        const date = new Date(isoString);
        return date.toLocaleTimeString('en-US', {
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit',
            hour12: true,
        });
    };

    const router = useRouter();

    useEffect(() => {
        // Check if the user is logged in
        const token = localStorage.getItem('token');
        if (!token) {
            // Redirect to the login page if not logged in
            router.push('/login');
        }
    }, [router]);

  const handleLogout = () => {
    // Remove the token from local storage
    localStorage.removeItem('token');

    // Redirect to the login page
    router.push('/login');
  };

    // Fetch data from DynamoDB
    const fetchData = async () => {
      try {
          const response = await fetch('/api/data');
          const result = await response.json();
          // console.log('API Response:', result); // Log the API response
  
          if (Array.isArray(result)) {
              // Filter out data older than 1 hour
              const now = new Date();
              const oneHourAgo = new Date(now.getTime() - 60 * 60 * 2000);
              const freshData = result.filter((item) => new Date(item.time) > oneHourAgo);
  
              // Sort the data by the original `time` values (ISO strings or timestamps)
              freshData.sort((a, b) => new Date(a.time) - new Date(b.time));
  
              // Format the time for each item after sorting
              const formattedData = freshData.map((item) => ({
                  ...item,
                  time: formatTime(item.time), // Format the time
              }));
  
              console.log('Formatted Data:', formattedData); // Log the formatted data
  
              setData(formattedData);
          } else {
              setError(result.error || 'Invalid data format');
          }
      } catch (err) {
          console.error('Error fetching data:', err);
          setError('Failed to fetch data');
      } finally {
          setLoading(false);
      }
  };

    const fetchMessages = async () => {
      try {
          const response = await fetch('/api/mqtt');
          const data_mqtt = await response.json();
          console.log('API Response:', data_mqtt);

          // Assuming data_mqtt is an array of objects like [{"Door Status":"closed"},{"Door Status":"open"}]
          if (Array.isArray(data_mqtt)) {
              // Extract the last message in the array
              const newestMessage = data_mqtt[data_mqtt.length - 1];
              // Convert the newest message to a string for display
              const messageString = JSON.stringify(newestMessage);
              setMqttMessages([messageString]); // Update state with the MQTT message
          } else {
              setMqttMessages([]); // Clear messages if the data is invalid
          }
      } catch (err) {
          console.error('Error fetching MQTT messages:', err);
      }
  };

    useEffect(() => {
      console.log('Fetching data from mqtt');
      fetchMessages();
      console.log('Fetching data from dynamodb');
      fetchData();
    }, []);

    // Poll for new data every 5 seconds
    useEffect(() => {
        const interval = setInterval(fetchData, 60000);
        // Clean up on component unmount
        return () => clearInterval(interval);
    }, []);

    useEffect(() => {
      const interval = setInterval(fetchMessages, 1000);
      return () => clearInterval(interval);
    }, []);

    console.log("Data before fullness:", data);
    const latestFullness = data.length > 0 && data[data.length - 1].fullness !== undefined 
      ? data[data.length - 1].fullness 
      : 'N/A';
    console.log("Latest Fullness:", latestFullness);
  // Prepare data for the graph
  const chartData = {
    labels: data.map((item) => item.time), // X-axis labels (formatted time)
    datasets: [
      {
        label: 'Temperature (°C)',
        data: data.map((item) => item.temperature), // Y-axis data
        borderColor: 'rgba(75, 192, 192, 1)',
        fill: false,
      },
      {
        label: 'Humidity (%)',
        data: data.map((item) => item.humidity), // Y-axis data
        borderColor: 'rgba(153, 102, 255, 1)',
        fill: false,
      },
    ],
  };

  const options = {
    responsive: true,
    plugins: {
      legend: {
        position: 'top',
      },
      title: {
        display: true,
        text: 'Sensor Data Over Time',
      },
    },
  };

  if (loading) {
    return <div>Loading...</div>;
  }

  if (error) {
    return <div>Error: {error}</div>;
  }

  return (
    <div>
      <h1>Welcome to the Dashboard</h1>
      <p>You are logged in!</p>
      <button onClick={handleLogout}>Logout</button>
      <h1>Sensor Data</h1>
      <Line data={chartData} options={options} /> {/* Render the graph */}
      <h1>Fullness</h1>
      <h2>{latestFullness}</h2>
      <h1>Door Status</h1>
      <h2>{mqttMessages.length > 0 ? mqttMessages[0] : 'No data'}</h2>
    </div>
  );
}