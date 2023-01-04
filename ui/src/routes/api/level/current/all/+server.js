/** @type {import('./$types').RequestHandler} */
export function GET() {
	let responseBody = [
		{
			id: 0,
			level: Math.floor(Math.random() * 101),
			sensorValue: Math.floor(Math.random() * 101) * 1000,
			gasWeight: Math.floor(Math.random() * 101) * 11000
		},
		{
			id: 1,
			level: Math.floor(Math.random() * 101),
			sensorValue: Math.floor(Math.random() * 101) * 1000,
			gasWeight: Math.floor(Math.random() * 101) * 11000
		}
	];
	return new Response(JSON.stringify(responseBody), { status: 200 });
}
