
export async function get({request}) {
    return {
        status: 200,
        body: [
            {
                "id": 1,
                "apName": "Stored WiFi #1" + Math.round(Math.random() * 10),
                "apPass": true
            },
            {
                "id": 2,
                "apName": "Stored WiFi #" + Math.round(Math.random() * 10),
                "apPass": false
            }
        ]
    }
}
