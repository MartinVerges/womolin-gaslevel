
export async function get({request}) {
    return {
        status: 200,
        body: [
            { 
                id: 0,
                level: Math.floor(Math.random() * 101),
                sensorValue: Math.floor(Math.random() * 101) * 1000,
                gasWeight:  Math.floor(Math.random() * 101) * 11000
            },
            { 
                id: 1,
                level: Math.floor(Math.random() * 101),
                sensorValue: Math.floor(Math.random() * 101) * 1000,
                gasWeight:  Math.floor(Math.random() * 101) * 11000
            }
        ]
    }
}